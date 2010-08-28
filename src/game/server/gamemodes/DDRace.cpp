/*Based on rajh's, Race mod stuff */
#include <engine/server.h>
#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include "DDRace.h"

CGameControllerDDRace::CGameControllerDDRace(class CGameContext *pGameServer) : IGameController(pGameServer)
{
	m_pGameType = "DDRace";
	
	InitTeleporter();
}

CGameControllerDDRace::~CGameControllerDDRace()
{
	delete[] m_pTeleporter;
}

void CGameControllerDDRace::Tick()
{
	IGameController::Tick();
}

void CGameControllerDDRace::InitTeleporter()
{
	int ArraySize = 0;
	if(GameServer()->Collision()->Layers()->TeleLayer())
	{
		for(int i = 0; i < GameServer()->Collision()->Layers()->TeleLayer()->m_Width*GameServer()->Collision()->Layers()->TeleLayer()->m_Height; i++)
		{
			// get the array size
			if(GameServer()->Collision()->TeleLayer()[i].m_Number > ArraySize)
				ArraySize = GameServer()->Collision()->TeleLayer()[i].m_Number;
		}
	}
	
	if(!ArraySize)
	{
		m_pTeleporter = 0x0;
		return;
	}
	
	m_pTeleporter = new vec2[ArraySize];
	mem_zero(m_pTeleporter, ArraySize*sizeof(vec2));
	
	// assign the values
	for(int i = 0; i < GameServer()->Collision()->Layers()->TeleLayer()->m_Width*GameServer()->Collision()->Layers()->TeleLayer()->m_Height; i++)
	{
		if(GameServer()->Collision()->TeleLayer()[i].m_Number > 0 && GameServer()->Collision()->TeleLayer()[i].m_Type == TILE_TELEOUT)
			m_pTeleporter[GameServer()->Collision()->TeleLayer()[i].m_Number-1] = vec2(i%GameServer()->Collision()->Layers()->TeleLayer()->m_Width*32+16, i/GameServer()->Collision()->Layers()->TeleLayer()->m_Width*32+16);
	}
}
