/* (c) Rajh, Redix and Sushi. */

#include <engine/ghost.h>
#include <engine/graphics.h>
#include <engine/serverbrowser.h>
#include <engine/storage.h>
#include <engine/shared/config.h>

#include "players.h"
#include "race.h"
#include "skins.h"
#include "menus.h"
#include "ghost.h"

const char *CGhost::ms_pGhostDir = "ghosts";

CGhost::CGhost() : m_StartRenderTick(-1), m_LastDeathTick(-1), m_Recording(false), m_Rendering(false) {}

void CGhost::GetGhostSkin(CGhostSkin *pSkin, const char *pSkinName, int UseCustomColor, int ColorBody, int ColorFeet)
{
	StrToInts(&pSkin->m_Skin0, 6, pSkinName);
	pSkin->m_UseCustomColor = UseCustomColor;
	pSkin->m_ColorBody = ColorBody;
	pSkin->m_ColorFeet = ColorFeet;
}

void CGhost::GetGhostCharacter(CGhostCharacter *pGhostChar, const CNetObj_Character *pChar)
{
	pGhostChar->m_X = pChar->m_X;
	pGhostChar->m_Y = pChar->m_Y;
	pGhostChar->m_VelX = pChar->m_VelX;
	pGhostChar->m_VelY = 0;
	pGhostChar->m_Angle = pChar->m_Angle;
	pGhostChar->m_Direction = pChar->m_Direction;
	pGhostChar->m_Weapon = pChar->m_Weapon;
	pGhostChar->m_HookState = pChar->m_HookState;
	pGhostChar->m_HookX = pChar->m_HookX;
	pGhostChar->m_HookY = pChar->m_HookY;
	pGhostChar->m_AttackTick = pChar->m_AttackTick;
	pGhostChar->m_Tick = pChar->m_Tick;
}

void CGhost::GetNetObjCharacter(CNetObj_Character *pChar, const CGhostCharacter *pGhostChar)
{
	mem_zero(pChar, sizeof(CNetObj_Character));
	pChar->m_X = pGhostChar->m_X;
	pChar->m_Y = pGhostChar->m_Y;
	pChar->m_VelX = pGhostChar->m_VelX;
	pChar->m_VelY = 0;
	pChar->m_Angle = pGhostChar->m_Angle;
	pChar->m_Direction = pGhostChar->m_Direction;
	pChar->m_Weapon = pGhostChar->m_Weapon == WEAPON_GRENADE ? WEAPON_GRENADE : WEAPON_GUN;
	pChar->m_HookState = pGhostChar->m_HookState;
	pChar->m_HookX = pGhostChar->m_HookX;
	pChar->m_HookY = pGhostChar->m_HookY;
	pChar->m_AttackTick = pGhostChar->m_AttackTick;
	pChar->m_HookedPlayer = -1;
	pChar->m_Tick = pGhostChar->m_Tick;
}

CGhost::CGhostPath::CGhostPath(CGhostPath &&Other)
	: m_ChunkSize(Other.m_ChunkSize), m_NumItems(Other.m_NumItems), m_lChunks(std::move(Other.m_lChunks))
{
	Other.m_NumItems = 0;
	Other.m_lChunks.clear();
}

CGhost::CGhostPath &CGhost::CGhostPath::operator = (CGhostPath &&Other)
{
	Reset(Other.m_ChunkSize);
	m_NumItems = Other.m_NumItems;
	m_lChunks = std::move(Other.m_lChunks);
	Other.m_NumItems = 0;
	Other.m_lChunks.clear();
	return *this;
}

void CGhost::CGhostPath::Reset(int ChunkSize)
{
	for(int i = 0; i < m_lChunks.size(); i++)
		mem_free(m_lChunks[i]);
	m_lChunks.clear();
	m_ChunkSize = ChunkSize;
	m_NumItems = 0;
}

void CGhost::CGhostPath::SetSize(int Items)
{
	int Chunks = m_lChunks.size();
	int NeededChunks = (Items + m_ChunkSize - 1) / m_ChunkSize;

	if(NeededChunks > Chunks)
	{
		m_lChunks.resize(NeededChunks);
		for(int i = Chunks; i < NeededChunks; i++)
			m_lChunks[i] = (CGhostCharacter*)mem_alloc(sizeof(CGhostCharacter) * m_ChunkSize, 1);
	}

	m_NumItems = Items;
}

void CGhost::CGhostPath::Add(CGhostCharacter Char)
{
	SetSize(m_NumItems + 1);
	*Get(m_NumItems - 1) = Char;
}

CGhostCharacter *CGhost::CGhostPath::Get(int Index)
{
	if(Index < 0 || Index >= m_NumItems)
		return 0;

	int Chunk = Index / m_ChunkSize;
	int Pos = Index % m_ChunkSize;
	return &m_lChunks[Chunk][Pos];
}

void CGhost::GetPath(char *pBuf, int Size, const char *pPlayerName, int Time) const
{
	const char *pMap = Client()->GetCurrentMap();
	unsigned Crc = Client()->GetMapCrc();

	char aPlayerName[MAX_NAME_LENGTH];
	str_copy(aPlayerName, pPlayerName, sizeof(aPlayerName));
	str_sanitize_filename(aPlayerName);

	if(Time < 0)
		str_format(pBuf, Size, "%s/%s_%s_%08x_tmp_%d.gho", ms_pGhostDir, pMap, aPlayerName, Crc, pid());
	else
		str_format(pBuf, Size, "%s/%s_%s_%d.%03d_%08x.gho", ms_pGhostDir, pMap, aPlayerName, Time / 1000, Time % 1000, Crc);
}

void CGhost::AddInfos(const CNetObj_Character *pChar)
{
	int NumTicks = m_CurGhost.m_Path.Size();

	// do not start writing to file as long as we still touch the start line
	if(g_Config.m_ClRaceSaveGhost && !GhostRecorder()->IsRecording() && NumTicks > 0)
	{
		GetPath(m_aTmpFilename, sizeof(m_aTmpFilename), m_CurGhost.m_aPlayer);
		Client()->GhostRecorder_Start(m_aTmpFilename, m_CurGhost.m_aPlayer);

		GhostRecorder()->WriteData(GHOSTDATA_TYPE_START_TICK, (const char*)&m_CurGhost.m_StartTick, sizeof(int));
		GhostRecorder()->WriteData(GHOSTDATA_TYPE_SKIN, (const char*)&m_CurGhost.m_Skin, sizeof(CGhostSkin));
		for(int i = 0; i < NumTicks; i++)
			GhostRecorder()->WriteData(GHOSTDATA_TYPE_CHARACTER, (const char*)m_CurGhost.m_Path.Get(i), sizeof(CGhostCharacter));
	}

	CGhostCharacter GhostChar;
	GetGhostCharacter(&GhostChar, pChar);
	m_CurGhost.m_Path.Add(GhostChar);
	if(GhostRecorder()->IsRecording())
		GhostRecorder()->WriteData(GHOSTDATA_TYPE_CHARACTER, (const char*)&GhostChar, sizeof(CGhostCharacter));
}

int CGhost::GetSlot() const
{
	for(int i = 0; i < MAX_ACTIVE_GHOSTS; i++)
		if(m_aActiveGhosts[i].Empty())
			return i;
	return -1;
}

int CGhost::FreeSlot() const
{
	int Num = 0;
	for(int i = 0; i < MAX_ACTIVE_GHOSTS; i++)
		if(m_aActiveGhosts[i].Empty())
			Num++;
	return Num;
}

void CGhost::OnRender()
{
	// only for race
	CServerInfo ServerInfo;
	Client()->GetServerInfo(&ServerInfo);
	if(!IsRace(&ServerInfo) || !g_Config.m_ClRaceGhost || !m_pClient->m_Snap.m_pGameInfoObj || Client()->State() != IClient::STATE_ONLINE)
		return;

	if(m_pClient->m_Snap.m_pLocalCharacter && m_pClient->m_Snap.m_pLocalPrevCharacter)
	{
		bool RaceFlag = m_pClient->m_Snap.m_pGameInfoObj->m_GameStateFlags&GAMESTATEFLAG_RACETIME;
		bool ServerControl = RaceFlag && g_Config.m_ClRaceGhostServerControl;
		int RaceTick = -m_pClient->m_Snap.m_pGameInfoObj->m_WarmupTimer;

		static int s_NewRenderTick = -1;
		int RenderTick = s_NewRenderTick;

		if(!ServerControl && m_pClient->m_NewPredictedTick)
		{
			vec2 PrevPos = m_pClient->m_PredictedPrevChar.m_Pos;
			vec2 Pos = m_pClient->m_PredictedChar.m_Pos;
			bool Rendering = m_Rendering || RenderTick != -1;
			if((!Rendering || m_AllowRestart) && CRaceHelper::IsStart(m_pClient, PrevPos, Pos))
				RenderTick = Client()->PredGameTick();
		}

		if(m_pClient->m_NewTick)
		{
			static int s_LastRaceTick = -1;

			if(ServerControl && s_LastRaceTick != RaceTick && Client()->GameTick() - RaceTick < Client()->GameTickSpeed())
			{
				if(m_Recording && s_LastRaceTick != -1)
					m_AllowRestart = true;
				if(GhostRecorder()->IsRecording())
					GhostRecorder()->Stop(0, -1);
				int StartTick = RaceTick;
				if(IsDDRace(&ServerInfo))
					StartTick--;
				StartRecord(StartTick);
				RenderTick = StartTick;
			}
			else if(!ServerControl)
			{
				int PrevTick = m_pClient->m_Snap.m_pLocalPrevCharacter->m_Tick;
				int CurTick = m_pClient->m_Snap.m_pLocalCharacter->m_Tick;
				vec2 PrevPos = vec2(m_pClient->m_Snap.m_pLocalPrevCharacter->m_X, m_pClient->m_Snap.m_pLocalPrevCharacter->m_Y);
				vec2 Pos = vec2(m_pClient->m_Snap.m_pLocalCharacter->m_X, m_pClient->m_Snap.m_pLocalCharacter->m_Y);

				// detecting death, needed because race allows immediate respawning
				if((!m_Recording || m_AllowRestart) && m_LastDeathTick < PrevTick)
				{
					// estimate the exact start tick
					int RecordTick = -1;
					int TickDiff = CurTick - PrevTick;
					for(int i = 0; i < TickDiff; i++)
					{
						if(CRaceHelper::IsStart(m_pClient, mix(PrevPos, Pos, (float)i/TickDiff), mix(PrevPos, Pos, (float)(i+1)/TickDiff)))
						{
							RecordTick = PrevTick + i + 1;
							if(!m_AllowRestart)
								break;
						}
					}
					if(RecordTick != -1)
					{
						if(GhostRecorder()->IsRecording())
							GhostRecorder()->Stop(0, -1);
						StartRecord(RecordTick);
					}
				}
			}

			if(m_Recording)
				AddInfos(m_pClient->m_Snap.m_pLocalCharacter);

			s_LastRaceTick = RaceFlag ? RaceTick : -1;
		}

		if((ServerControl && m_pClient->m_NewTick) || (!ServerControl && m_pClient->m_NewPredictedTick))
		{
			// only restart rendering if it did not change since last tick to prevent stuttering
			if(s_NewRenderTick != -1 && s_NewRenderTick == RenderTick)
			{
				StartRender(RenderTick);
				RenderTick = -1;
			}
			s_NewRenderTick = RenderTick;
		}
	}

	// Play the ghost
	if(!m_Rendering || !g_Config.m_ClRaceShowGhost)
		return;

	int ActiveGhosts = 0;
	int PlaybackTick = Client()->PredGameTick() - m_StartRenderTick;

	for(int i = 0; i < MAX_ACTIVE_GHOSTS; i++)
	{
		CGhostItem *pGhost = &m_aActiveGhosts[i];
		if(pGhost->Empty())
			continue;

		bool End = false;
		int GhostTick = pGhost->m_StartTick + PlaybackTick;
		while(pGhost->m_Path.Get(pGhost->m_PlaybackPos)->m_Tick < GhostTick && !End)
		{
			if(pGhost->m_PlaybackPos < pGhost->m_Path.Size() - 1)
				pGhost->m_PlaybackPos++;
			else
				End = true;
		}

		if(End)
			continue;

		ActiveGhosts++;

		int CurPos = pGhost->m_PlaybackPos;
		int PrevPos = max(0, CurPos - 1);
		if(pGhost->m_Path.Get(PrevPos)->m_Tick > GhostTick)
			continue;

		CNetObj_Character Player, Prev;
		GetNetObjCharacter(&Player, pGhost->m_Path.Get(CurPos));
		GetNetObjCharacter(&Prev, pGhost->m_Path.Get(PrevPos));

		int TickDiff = Player.m_Tick - Prev.m_Tick;
		float IntraTick = 0.f;
		if(TickDiff > 0)
			IntraTick = (GhostTick - Prev.m_Tick - 1 + Client()->PredIntraGameTick()) / TickDiff;

		Player.m_AttackTick += Client()->GameTick() - GhostTick;

		m_pClient->m_pPlayers->RenderHook(&Prev, &Player, &pGhost->m_RenderInfo , -2, vec2(), vec2(), IntraTick);
		m_pClient->m_pPlayers->RenderPlayer(&Prev, &Player, &pGhost->m_RenderInfo, -2, vec2(), IntraTick);
	}

	if(!ActiveGhosts)
		StopRender();
}

void CGhost::InitRenderInfos(CGhostItem *pGhost)
{
	char aSkinName[64];
	IntsToStr(&pGhost->m_Skin.m_Skin0, 6, aSkinName);
	CTeeRenderInfo *pRenderInfo = &pGhost->m_RenderInfo;

	int SkinId = m_pClient->m_pSkins->Find(aSkinName);
	if(SkinId < 0)
	{
		SkinId = m_pClient->m_pSkins->Find("default");
		if(SkinId < 0)
			SkinId = 0;
	}

	if(pGhost->m_Skin.m_UseCustomColor)
	{
		pRenderInfo->m_Texture = m_pClient->m_pSkins->Get(SkinId)->m_ColorTexture;
		pRenderInfo->m_ColorBody = m_pClient->m_pSkins->GetColorV4(pGhost->m_Skin.m_ColorBody);
		pRenderInfo->m_ColorFeet = m_pClient->m_pSkins->GetColorV4(pGhost->m_Skin.m_ColorFeet);
	}
	else
	{
		pRenderInfo->m_Texture = m_pClient->m_pSkins->Get(SkinId)->m_OrgTexture;
		pRenderInfo->m_ColorBody = vec4(1, 1, 1, 1);
		pRenderInfo->m_ColorFeet = vec4(1, 1, 1, 1);
	}

	pRenderInfo->m_ColorBody.a = 0.5f;
	pRenderInfo->m_ColorFeet.a = 0.5f;
	pRenderInfo->m_Size = 64;
}

void CGhost::StartRecord(int Tick)
{
	m_Recording = true;
	m_CurGhost.Reset();
	m_CurGhost.m_StartTick = Tick;

	const CGameClient::CClientData *pData = &m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID];
	str_copy(m_CurGhost.m_aPlayer, g_Config.m_PlayerName, sizeof(m_CurGhost.m_aPlayer));
	GetGhostSkin(&m_CurGhost.m_Skin, pData->m_aSkinName, pData->m_UseCustomColor, pData->m_ColorBody, pData->m_ColorFeet);
	InitRenderInfos(&m_CurGhost);
}

void CGhost::StopRecord(int Time)
{
	m_Recording = false;
	bool RecordingToFile = GhostRecorder()->IsRecording();

	if(RecordingToFile)
		GhostRecorder()->Stop(m_CurGhost.m_Path.Size(), Time);

	CMenus::CGhostItem *pOwnGhost = m_pClient->m_pMenus->GetOwnGhost();
	if(Time > 0 && (!pOwnGhost || Time < pOwnGhost->m_Time))
	{
		if(pOwnGhost && pOwnGhost->Active())
			Unload(pOwnGhost->m_Slot);

		// add to active ghosts
		int Slot = GetSlot();
		if(Slot != -1)
			m_aActiveGhosts[Slot] = std::move(m_CurGhost);

		// create ghost item
		CMenus::CGhostItem Item;
		if(RecordingToFile)
			GetPath(Item.m_aFilename, sizeof(Item.m_aFilename), m_CurGhost.m_aPlayer, Time);
		str_copy(Item.m_aPlayer, m_CurGhost.m_aPlayer, sizeof(Item.m_aPlayer));
		Item.m_Time = Time;
		Item.m_Slot = Slot;

		// save new ghost file
		if(Item.HasFile())
			Storage()->RenameFile(m_aTmpFilename, Item.m_aFilename, IStorage::TYPE_SAVE);

		// add item to menu list
		m_pClient->m_pMenus->UpdateOwnGhost(Item);
	}
	else if(RecordingToFile) // no new record
		Storage()->RemoveFile(m_aTmpFilename, IStorage::TYPE_SAVE);

	m_aTmpFilename[0] = 0;

	m_CurGhost.Reset();
}

void CGhost::StartRender(int Tick)
{
	m_Rendering = true;
	m_StartRenderTick = Tick;
	for(int i = 0; i < MAX_ACTIVE_GHOSTS; i++)
		m_aActiveGhosts[i].m_PlaybackPos = 0;
}

void CGhost::StopRender()
{
	m_Rendering = false;
}

int CGhost::Load(const char *pFilename)
{
	int Slot = GetSlot();
	if(Slot == -1)
		return -1;

	if(!Client()->GhostLoader_Load(pFilename))
		return -1;

	const CGhostHeader *pHeader = GhostLoader()->GetHeader();

	int NumTicks = GhostLoader()->GetTicks(pHeader);
	int Time = GhostLoader()->GetTime(pHeader);
	if(NumTicks <= 0 || Time <= 0)
	{
		GhostLoader()->Close();
		return -1;
	}

	// select ghost
	CGhostItem *pGhost = &m_aActiveGhosts[Slot];
	pGhost->Reset();
	pGhost->m_Path.SetSize(NumTicks);

	str_copy(pGhost->m_aPlayer, pHeader->m_aOwner, sizeof(pGhost->m_aPlayer));

	int Index = 0;
	bool FoundSkin = false;
	bool NoTick = false;
	bool Error = false;

	int Type;
	while(!Error && GhostLoader()->ReadNextType(&Type))
	{
		if(Index == NumTicks && (Type == GHOSTDATA_TYPE_CHARACTER || Type == GHOSTDATA_TYPE_CHARACTER_NO_TICK))
		{
			Error = true;
			break;
		}

		if(Type == GHOSTDATA_TYPE_SKIN && !FoundSkin)
		{
			FoundSkin = true;
			if(!GhostLoader()->ReadData(Type, (char*)&pGhost->m_Skin, sizeof(CGhostSkin)))
				Error = true;
		}
		else if(Type == GHOSTDATA_TYPE_CHARACTER_NO_TICK)
		{
			NoTick = true;
			if(!GhostLoader()->ReadData(Type, (char*)pGhost->m_Path.Get(Index++), sizeof(CGhostCharacter_NoTick)))
				Error = true;
		}
		else if(Type == GHOSTDATA_TYPE_CHARACTER)
		{
			if(!GhostLoader()->ReadData(Type, (char*)pGhost->m_Path.Get(Index++), sizeof(CGhostCharacter)))
				Error = true;
		}
		else if(Type == GHOSTDATA_TYPE_START_TICK)
		{
			if(!GhostLoader()->ReadData(Type, (char*)&pGhost->m_StartTick, sizeof(int)))
				Error = true;
		}
	}

	GhostLoader()->Close();

	if(Error || Index != NumTicks)
	{
		pGhost->Reset();
		return -1;
	}

	if(NoTick)
	{
		int StartTick = 0;
		for(int i = 1; i < NumTicks; i++) // estimate start tick
			if(pGhost->m_Path.Get(i)->m_AttackTick != pGhost->m_Path.Get(i - 1)->m_AttackTick)
				StartTick = pGhost->m_Path.Get(i)->m_AttackTick - i;
		for(int i = 0; i < NumTicks; i++)
			pGhost->m_Path.Get(i)->m_Tick = StartTick + i;
	}

	if(pGhost->m_StartTick == -1)
		pGhost->m_StartTick = pGhost->m_Path.Get(0)->m_Tick;

	if(!FoundSkin)
		GetGhostSkin(&pGhost->m_Skin, "default", 0, 0, 0);
	InitRenderInfos(pGhost);

	return Slot;
}

void CGhost::Unload(int Slot)
{
	m_aActiveGhosts[Slot].Reset();
}

void CGhost::UnloadAll()
{
	for(int i = 0; i < MAX_ACTIVE_GHOSTS; i++)
		Unload(i);
}

void CGhost::SaveGhost(CMenus::CGhostItem *pItem)
{
	int Slot = pItem->m_Slot;
	if(!pItem->Active() || pItem->HasFile() || m_aActiveGhosts[Slot].Empty() || GhostRecorder()->IsRecording())
		return;

	CGhostItem *pGhost = &m_aActiveGhosts[Slot];

	int NumTicks = pGhost->m_Path.Size();
	GetPath(pItem->m_aFilename, sizeof(pItem->m_aFilename), pItem->m_aPlayer, pItem->m_Time);
	Client()->GhostRecorder_Start(pItem->m_aFilename, pItem->m_aPlayer);

	GhostRecorder()->WriteData(GHOSTDATA_TYPE_START_TICK, (const char*)&pGhost->m_StartTick, sizeof(int));
	GhostRecorder()->WriteData(GHOSTDATA_TYPE_SKIN, (const char*)&pGhost->m_Skin, sizeof(CGhostSkin));
	for(int i = 0; i < NumTicks; i++)
		GhostRecorder()->WriteData(GHOSTDATA_TYPE_CHARACTER, (const char*)pGhost->m_Path.Get(i), sizeof(CGhostCharacter));

	GhostRecorder()->Stop(NumTicks, pItem->m_Time);
}

void CGhost::ConGPlay(IConsole::IResult *pResult, void *pUserData)
{
	CGhost *pGhost = (CGhost *)pUserData;
	pGhost->StartRender(pGhost->Client()->PredGameTick());
}

void CGhost::OnConsoleInit()
{
	m_pGhostLoader = Kernel()->RequestInterface<IGhostLoader>();
	m_pGhostRecorder = Kernel()->RequestInterface<IGhostRecorder>();

	Console()->Register("gplay", "", CFGFLAG_CLIENT, ConGPlay, this, "");
}

void CGhost::OnMessage(int MsgType, void *pRawMsg)
{
	// only for race
	CServerInfo ServerInfo;
	Client()->GetServerInfo(&ServerInfo);

	if(!IsRace(&ServerInfo) || m_pClient->m_Snap.m_SpecInfo.m_Active || Client()->State() != IClient::STATE_ONLINE)
		return;

	// check for messages from server
	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;
		if(pMsg->m_Victim == m_pClient->m_Snap.m_LocalClientID)
		{
			if(m_Recording)
				StopRecord();
			StopRender();
			m_LastDeathTick = Client()->GameTick();
		}
	}
	else if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		if(pMsg->m_ClientID == -1 && m_Recording)
		{
			char aName[MAX_NAME_LENGTH];
			int Time = CRaceHelper::TimeFromFinishMessage(pMsg->m_pMessage, aName, sizeof(aName));
			if(Time > 0 && str_comp(aName, m_pClient->m_aClients[m_pClient->m_Snap.m_LocalClientID].m_aName) == 0)
			{
				StopRecord(Time);
				StopRender();
			}
		}
	}
}

void CGhost::OnReset()
{
	StopRecord();
	StopRender();
	m_LastDeathTick = -1;
}

void CGhost::OnMapLoad()
{
	OnReset();
	UnloadAll();
	m_pClient->m_pMenus->GhostlistPopulate();
	m_AllowRestart = false;
}
