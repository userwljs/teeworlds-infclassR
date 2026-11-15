#include "connection_pool.h"
#include "connection.h"
#include <engine/shared/config.h>

#include <base/dbg.h>
#include <base/log.h>
#include <base/str.h>
#include <base/thread.h>
#include <cstring>
#include <engine/console.h>

#include <chrono>
#include <memory>
#include <vector>

// helper struct to hold thread data
struct CSqlExecData
{
	enum
	{
		SQL_ACCESS,
		ADD_SQLITE,
		PRINT,
	} m_Mode;

	CSqlExecData(
		CDbConnectionPool::FSqlFunc pFunc,
		std::unique_ptr<const ISqlData> pThreadData,
		const char *pName,
		bool Write);
	CSqlExecData(
		CDbConnectionPool::Mode m,
		const char aFileName[64]);
	CSqlExecData(IConsole *pConsole, CDbConnectionPool::Mode m);
	~CSqlExecData() = default;

	CDbConnectionPool::FSqlFunc m_pFunc = nullptr;
	bool m_Write = false;
	std::unique_ptr<const ISqlData> m_pThreadData;
	const char *m_pName = "";

	CDbConnectionPool::Mode m_DatabaseMode{};
	char m_aFileName[64]{};
	IConsole *m_pConsole = nullptr;
};

CSqlExecData::CSqlExecData(
	CDbConnectionPool::FSqlFunc pFunc,
	std::unique_ptr<const ISqlData> pThreadData,
	const char *pName,
	bool Write) :
	m_Mode(SQL_ACCESS),
	m_pFunc(pFunc),
	m_Write(Write),
	m_pThreadData(std::move(pThreadData)),
	m_pName(pName)
{
}

CSqlExecData::CSqlExecData(
	CDbConnectionPool::Mode m,
	const char aFileName[64]) :
	m_Mode(ADD_SQLITE),
	m_pName("add sqlite server"),
	m_DatabaseMode(m)
{
	str_copy(m_aFileName, aFileName);
}

CSqlExecData::CSqlExecData(IConsole *pConsole, CDbConnectionPool::Mode m) :
	m_Mode(PRINT),
	m_pName("print database server"),
	m_DatabaseMode(m),
	m_pConsole(pConsole)
{
}

void CDbConnectionPool::Enqueue(std::unique_ptr<CSqlExecData> pData)
{
	if(m_Shutdown)
	{
		// The worker has already been asked to stop. Complete the request as
		// failed instead of leaving its future pending forever.
		if(pData->m_pThreadData && pData->m_pThreadData->m_pResult)
		{
			pData->m_pThreadData->m_pResult->m_Success = false;
			pData->m_pThreadData->m_pResult->m_Completed.store(true);
		}
		return;
	}

	{
		const std::lock_guard<std::mutex> Lock(m_pShared->m_Mutex);
		m_pShared->m_Queries.push_back(std::move(pData));
	}
	m_pShared->m_Condition.notify_one();
}

void CDbConnectionPool::Print(IConsole *pConsole, Mode DatabaseMode)
{
	Enqueue(std::make_unique<CSqlExecData>(pConsole, DatabaseMode));
}

void CDbConnectionPool::RegisterSqliteDatabase(Mode DatabaseMode, const char aFileName[64])
{
	Enqueue(std::make_unique<CSqlExecData>(DatabaseMode, aFileName));
}

void CDbConnectionPool::Execute(
	FSqlFunc pFunc,
	std::unique_ptr<const ISqlData> pSqlRequestData,
	const char *pName)
{
	Enqueue(std::make_unique<CSqlExecData>(pFunc, std::move(pSqlRequestData), pName, false));
}

void CDbConnectionPool::ExecuteWrite(
	FSqlFunc pFunc,
	std::unique_ptr<const ISqlData> pSqlRequestData,
	const char *pName)
{
	Enqueue(std::make_unique<CSqlExecData>(pFunc, std::move(pSqlRequestData), pName, true));
}

void CDbConnectionPool::OnShutdown()
{
	if(m_Shutdown)
		return;
	m_Shutdown = true;

	std::unique_lock<std::mutex> Lock(m_pShared->m_Mutex);
	m_pShared->m_Shutdown.store(true);
	m_pShared->m_Condition.notify_all();
	while(!m_pShared->m_WorkerDone)
	{
		if(m_pShared->m_DoneCondition.wait_for(Lock, std::chrono::seconds(2)) == std::cv_status::timeout)
			dbg_msg("sql", "Waiting for database queries to complete");
	}
}

class CWorker
{
public:
	CWorker(std::shared_ptr<CDbConnectionPool::CSharedData> pShared, int DebugSql) :
		m_pShared(std::move(pShared)), m_DebugSql(DebugSql) {}
	static void Start(void *pUser);
	void ProcessQueries();

private:
	void Print(IConsole *pConsole, CDbConnectionPool::Mode DatabaseMode);

	std::shared_ptr<CDbConnectionPool::CSharedData> m_pShared;
	bool m_DebugSql;

	std::vector<std::unique_ptr<IDbConnection>> m_vpReadConnections;
	std::unique_ptr<IDbConnection> m_pWriteConnection;
};

/* static */
void CWorker::Start(void *pUser)
{
	CWorker *pThis = (CWorker *)pUser;
	pThis->ProcessQueries();
	delete pThis;
}

void CWorker::ProcessQueries()
{
	// remember last working server and try to connect to it first
	int ReadServer = 0;
	// enter fail mode when a sql request fails, skip read request during it
	bool FailMode = false;
	int JobNum = 0;
	while(true)
	{
		std::unique_ptr<CSqlExecData> pThreadData;
		{
			std::unique_lock<std::mutex> Lock(m_pShared->m_Mutex);
			m_pShared->m_Condition.wait(Lock, [this]() {
				return m_pShared->m_Shutdown.load() || !m_pShared->m_Queries.empty();
			});

			if(m_pShared->m_Shutdown.load() && m_pShared->m_Queries.empty())
			{
				m_pShared->m_WorkerDone = true;
				m_pShared->m_DoneCondition.notify_all();
				return;
			}

			pThreadData = std::move(m_pShared->m_Queries.front());
			m_pShared->m_Queries.pop_front();
		}

		bool Success = false;
		switch(pThreadData->m_Mode)
		{
		case CSqlExecData::SQL_ACCESS:
		{
			if(pThreadData->m_Write)
			{
				if(m_pWriteConnection && CDbConnectionPool::ExecSqlFunc(m_pWriteConnection.get(), pThreadData.get()))
				{
					if(m_DebugSql)
						dbg_msg("sql", "[%i] %s done on write database", JobNum, pThreadData->m_pName);
					Success = true;
				}
				else
				{
					FailMode = true;
				}
			}
			else
			{
				if(m_pShared->m_Shutdown.load())
				{
					dbg_msg("sql", "[%i] %s dismissed read request during shutdown", JobNum, pThreadData->m_pName);
				}
				else if(FailMode)
				{
					dbg_msg("sql", "[%i] %s dismissed read request during FailMode", JobNum, pThreadData->m_pName);
				}
				else
				{
					for(size_t i = 0; i < m_vpReadConnections.size(); i++)
					{
						const int CurServer = (ReadServer + i) % (int)m_vpReadConnections.size();
						if(CDbConnectionPool::ExecSqlFunc(m_vpReadConnections[CurServer].get(), pThreadData.get()))
						{
							ReadServer = CurServer;
							if(m_DebugSql)
								dbg_msg("sql", "[%i] %s done on read database %d", JobNum, pThreadData->m_pName, CurServer);
							Success = true;
							break;
						}
					}
					if(!Success)
						FailMode = true;
				}
			}
			break;
		}
		case CSqlExecData::ADD_SQLITE:
		{
			auto pSqlite = CreateSqliteConnection(pThreadData->m_aFileName, true);
			switch(pThreadData->m_DatabaseMode)
			{
			case CDbConnectionPool::Mode::READ:
				m_vpReadConnections.push_back(std::move(pSqlite));
				break;
			case CDbConnectionPool::Mode::WRITE:
				m_pWriteConnection = std::move(pSqlite);
				break;
			case CDbConnectionPool::Mode::NUM_MODES:
				break;
			}
			Success = true;
			break;
		}
		case CSqlExecData::PRINT:
			Print(pThreadData->m_pConsole, pThreadData->m_DatabaseMode);
			Success = true;
			break;
		}

		if(!Success)
			dbg_msg("sql", "[%i] %s failed on all databases", JobNum, pThreadData->m_pName);

		if(pThreadData->m_pThreadData != nullptr && pThreadData->m_pThreadData->m_pResult != nullptr)
		{
			pThreadData->m_pThreadData->m_pResult->m_Success = Success;
			pThreadData->m_pThreadData->m_pResult->m_Completed.store(true);
		}

		{
			const std::lock_guard<std::mutex> Lock(m_pShared->m_Mutex);
			if(m_pShared->m_Queries.empty())
				FailMode = false;
		}

		++JobNum;
	}
}

void CWorker::Print(IConsole *pConsole, CDbConnectionPool::Mode DatabaseMode)
{
	if(DatabaseMode == CDbConnectionPool::Mode::READ)
	{
		for(auto &pReadConnection : m_vpReadConnections)
			pReadConnection->Print(pConsole, "Read");
		if(m_vpReadConnections.empty())
			pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "There are no read databases");
	}
	else if(DatabaseMode == CDbConnectionPool::Mode::WRITE)
	{
		if(m_pWriteConnection)
			m_pWriteConnection->Print(pConsole, "Write");
		else
			pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "There are no write databases");
	}
}

/* static */
bool CDbConnectionPool::ExecSqlFunc(IDbConnection *pConnection, CSqlExecData *pData)
{
	if(pConnection == nullptr)
	{
		dbg_msg("sql", "No database given");
		return false;
	}
	char aError[256] = "unknown error";
	if(pConnection->Connect(aError, sizeof(aError)))
	{
		dbg_msg("sql", "failed connecting to db: %s", aError);
		return false;
	}
	const bool Success = !pData->m_pFunc(pConnection, pData->m_pThreadData.get(), aError, sizeof(aError));
	pConnection->Disconnect();
	if(!Success)
	{
		dbg_msg("sql", "%s failed: %s", pData->m_pName, aError);
	}
	return Success;
}

CDbConnectionPool::CDbConnectionPool()
{
	m_pShared = std::make_shared<CSharedData>();
	m_pWorkerThread = thread_init(CWorker::Start, new CWorker(m_pShared, g_Config.m_DbgSql), "database worker thread");
}

CDbConnectionPool::~CDbConnectionPool()
{
	OnShutdown();
	if(m_pWorkerThread)
		thread_wait(m_pWorkerThread);
}
