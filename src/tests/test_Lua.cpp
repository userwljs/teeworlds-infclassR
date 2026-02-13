#include "test/test.h"
#include <gtest/gtest.h>

#include <base/logger.h>
#include <base/system.h>
#include <base/types.h>
#include <engine/engine.h>
#include <engine/kernel.h>
#include <engine/lua.h>
#include <engine/server/databases/connection.h>
#include <engine/server/databases/connection_pool.h>
#include <engine/server/register.h>
#include <engine/server/server.h>
#include <engine/server/server_logger.h>
#include <engine/shared/assertion_logger.h>
#include <engine/shared/config.h>
#include <game/generated/protocol.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/gameworld.h>
#include <game/server/infclass/entities/ic_entity.h>
#include <game/server/infclass/ic_gamecontroller.h>
#include <game/server/player.h>
#include <game/version.h>

#include <memory>

class CTestLuaServer : public ::testing::Test
{
public:
	IGameServer *m_pGameServer = nullptr;
	CServer *m_pServer = nullptr;
	std::unique_ptr<IKernel> m_pKernel;
	CTestInfo m_TestInfo;
	std::unique_ptr<IStorage> m_pStorage;
	ILua *m_pLua{};

	CGameContext *GameServer()
	{
		return (CGameContext *)m_pGameServer;
	}

	CTestLuaServer()
	{
		CServer *pServer = CreateServer();
		m_pServer = pServer;

		m_pKernel = std::unique_ptr<IKernel>(IKernel::Create());
		m_pKernel->RegisterInterface(m_pServer);

		IEngine *pEngine = CreateTestEngine(GAME_NAME);
		m_pKernel->RegisterInterface(pEngine);

		m_TestInfo.m_DeleteTestStorageFilesOnSuccess = true;
		m_pStorage = m_TestInfo.CreateTestStorage();
		EXPECT_NE(m_pStorage, nullptr);
		m_pKernel->RegisterInterface(m_pStorage.get(), false);

		IConsole *pConsole = CreateConsole(CFGFLAG_SERVER | CFGFLAG_ECON).release();
		m_pKernel->RegisterInterface(pConsole);

		IConfigManager *pConfigManager = CreateConfigManager();
		m_pKernel->RegisterInterface(pConfigManager);

		IEngineMap *pEngineMap = CreateEngineMap();
		m_pKernel->RegisterInterface(pEngineMap);
		m_pKernel->RegisterInterface(static_cast<IMap *>(pEngineMap), false);

		m_pLua = CreateLua();
		m_pKernel->RegisterInterface(m_pLua);

		m_pGameServer = CreateGameServer();
		m_pKernel->RegisterInterface(m_pGameServer);

		pEngine->Init();
		pConsole->Init();
		pConfigManager->Init();
		m_pLua->Init();

		m_pServer->RegisterCommands();

		EXPECT_NE(m_pServer->LoadMap("infc_empty"), 0);

		m_pServer->m_RunServer = CServer::RUNNING;

		m_pServer->InitPersistentData();
		EXPECT_NE(m_pServer->LoadMap("infc_empty"), 0);

		if(!pServer->m_Http.Init(CTestInfo::GetHttpShutdownDelay()))
		{
			log_error("server", "Failed to initialize the HTTP client.");
		}

		pServer->m_NetServer.SetCallbacks(
			CServer::NewClientCallback,
			CServer::NewClientNoAuthCallback,
			CServer::ClientRejoinCallback,
			CServer::DelClientCallback, pServer);

		pServer->m_Econ.Init(pServer->Config(), pServer->Console(), &pServer->m_ServerBan);

		GameServer()->OnInit(nullptr);
	};

	~CTestLuaServer() override
	{
		m_pServer->m_Econ.Shutdown();
		m_pGameServer->OnShutdown(nullptr);
		m_pServer->m_pMap->Unload();
		m_pServer->DbPool()->OnShutdown();
	};

	ILua *Lua() const { return m_pLua; }
};

TEST_F(CTestLuaServer, CharacterPosition)
{
	CPlayer *pPlayer = GameServer()->m_pController->CreatePlayer(0, false, nullptr);
	ASSERT_TRUE(pPlayer);
	GameServer()->m_apPlayers[0] = pPlayer;
	pPlayer->SetTeam(0);
	pPlayer->TryRespawn();

	CCharacter *pChr0 = pPlayer->GetCharacter();
	ASSERT_TRUE(pChr0);

	Lua()->ExecScript("ch = Game.Controller:GetCharacter(0)");
	Lua()->ExecScript("ch.Position = vec2(100, 200)");

	EXPECT_EQ(pChr0->GetPos(), vec2(100, 200));
}

TEST_F(CTestLuaServer, CallbackWithResult)
{
	CIcGameController *pIcGameController = static_cast<CIcGameController *>(GameServer()->m_pController);
	Lua()->ExecScript("function Get_hero_flag_position(player) return vec2(100, 200) end");
	std::optional<vec2> flagPosition = pIcGameController->GetHeroFlagPosition();
	ASSERT_TRUE(flagPosition.has_value());
	EXPECT_EQ(flagPosition.value(), vec2(100, 200));
}
