#ifndef ENGINE_SERVER_DATABASES_CONNECTION_POOL_H
#define ENGINE_SERVER_DATABASES_CONNECTION_POOL_H

#include <base/types.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>

class IDbConnection;

struct ISqlResult
{
	// using atomic_bool to indicate completed sql query since usage_count
	// from shard_ptr isn't safe in multithreaded environment
	// the main thread must only access the remaining result data if set to true
	std::atomic_bool m_Completed{false};
	// indicate whether the thread indicated a successful completion (returned true)
	bool m_Success = false;

	virtual ~ISqlResult() = default;
};

struct ISqlData
{
	ISqlData(std::shared_ptr<ISqlResult> pResult) :
		m_pResult(std::move(pResult))
	{
	}
	virtual ~ISqlData() = default;

	mutable std::shared_ptr<ISqlResult> m_pResult;
};

class IConsole;

class CDbConnectionPool
{
public:
	CDbConnectionPool();
	~CDbConnectionPool();
	CDbConnectionPool &operator=(const CDbConnectionPool &) = delete;

	// Returns false on success.
	typedef bool (*FSqlFunc)(IDbConnection *, const ISqlData *, char *pError, int ErrorSize);

	enum Mode
	{
		READ,
		WRITE,
		NUM_MODES,
	};

	void Print(IConsole *pConsole, Mode DatabaseMode);

	void RegisterSqliteDatabase(Mode DatabaseMode, const char FileName[64]);

	void Execute(
		FSqlFunc pFunc,
		std::unique_ptr<const ISqlData> pSqlRequestData,
		const char *pName);
	void ExecuteWrite(
		FSqlFunc pFunc,
		std::unique_ptr<const ISqlData> pSqlRequestData,
		const char *pName);

	void OnShutdown();

	friend class CWorker;

private:
	void Enqueue(std::unique_ptr<struct CSqlExecData> pData);
	static bool ExecSqlFunc(IDbConnection *pConnection, struct CSqlExecData *pData);

	bool m_Shutdown = false;

	struct CSharedData
	{
		std::mutex m_Mutex;
		std::condition_variable m_Condition;
		std::condition_variable m_DoneCondition;
		std::deque<std::unique_ptr<struct CSqlExecData>> m_Queries;
		// Set when the server is shutting down. Queued queries are still
		// processed; read queries are dismissed.
		std::atomic_bool m_Shutdown{false};
		bool m_WorkerDone = false;
	};

	std::shared_ptr<CSharedData> m_pShared;
	void *m_pWorkerThread = nullptr;
};

#endif // ENGINE_SERVER_DATABASES_CONNECTION_POOL_H
