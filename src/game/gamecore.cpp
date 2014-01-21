/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "gamecore.h"

#include <engine/shared/config.h>
#include <engine/server/server.h>
const char *CTuningParams::m_apNames[] =
{
	#define MACRO_TUNING_PARAM(Name,ScriptName,Value) #ScriptName,
	#include "tuning.h"
	#undef MACRO_TUNING_PARAM
};


bool CTuningParams::Set(int Index, float Value)
{
	if(Index < 0 || Index >= Num())
		return false;
	((CTuneParam *)this)[Index] = Value;
	return true;
}

bool CTuningParams::Get(int Index, float *pValue)
{
	if(Index < 0 || Index >= Num())
		return false;
	*pValue = (float)((CTuneParam *)this)[Index];
	return true;
}

bool CTuningParams::Set(const char *pName, float Value)
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, m_apNames[i]) == 0)
			return Set(i, Value);
	return false;
}

bool CTuningParams::Get(const char *pName, float *pValue)
{
	for(int i = 0; i < Num(); i++)
		if(str_comp_nocase(pName, m_apNames[i]) == 0)
			return Get(i, pValue);

	return false;
}

float HermiteBasis1(float v)
{
	return 2*v*v*v - 3*v*v+1;
}

float VelocityRamp(float Value, float Start, float Range, float Curvature)
{
	if(Value < Start)
		return 1.0f;
	return 1.0f/powf(Curvature, (Value-Start)/Range);
}

void CCharacterCore::Init(CWorldCore *pWorld, CCollision *pCollision, CTeamsCore* pTeams)
{
	m_pWorld = pWorld;
	m_pCollision = pCollision;
	m_pTeleOuts = NULL;

	m_pTeams = pTeams;
	m_Id = -1;
	m_Hook = true;
	m_Collision = true;
	m_JumpedTotal = 0;
	m_Jumps = 2;
}

void CCharacterCore::Init(CWorldCore *pWorld, CCollision *pCollision, CTeamsCore* pTeams, std::map<int, std::vector<vec2> > *pTeleOuts)
{
	m_pWorld = pWorld;
	m_pCollision = pCollision;
	m_pTeleOuts = pTeleOuts;

	m_pTeams = pTeams;
	m_Id = -1;
	m_Hook = true;
	m_Collision = true;
	m_JumpedTotal = 0;
	m_Jumps = 2;
}

void CCharacterCore::Reset()
{
	m_Pos = vec2(0,0);
	m_Vel = vec2(0,0);
	m_NewHook = false;
	m_HookPos = vec2(0,0);
	m_HookDir = vec2(0,0);
	m_HookTick = 0;
	m_HookState = HOOK_IDLE;
	m_HookedPlayer = -1;
	m_Jumped = 0;
	m_JumpedTotal = 0;
	m_Jumps = 2;
	m_TriggeredEvents = 0;
	m_Hook = true;
	m_Collision = true;
}

void CCharacterCore::Tick(bool UseInput)
{
	float PhysSize = 28.0f;
	int MapIndex = Collision()->GetPureMapIndex(m_Pos);;
	int MapIndexL = Collision()->GetPureMapIndex(vec2(m_Pos.x + (28/2)+4,m_Pos.y));
	int MapIndexR = Collision()->GetPureMapIndex(vec2(m_Pos.x - (28/2)-4,m_Pos.y));
	int MapIndexT = Collision()->GetPureMapIndex(vec2(m_Pos.x,m_Pos.y + (28/2)+4));
	int MapIndexB = Collision()->GetPureMapIndex(vec2(m_Pos.x,m_Pos.y - (28/2)-4));
	//dbg_msg("","N%d L%d R%d B%d T%d",MapIndex,MapIndexL,MapIndexR,MapIndexB,MapIndexT);
	m_TileIndex = Collision()->GetTileIndex(MapIndex);
	m_TileFlags = Collision()->GetTileFlags(MapIndex);
	m_TileIndexL = Collision()->GetTileIndex(MapIndexL);
	m_TileFlagsL = Collision()->GetTileFlags(MapIndexL);
	m_TileIndexR = Collision()->GetTileIndex(MapIndexR);
	m_TileFlagsR = Collision()->GetTileFlags(MapIndexR);
	m_TileIndexB = Collision()->GetTileIndex(MapIndexB);
	m_TileFlagsB = Collision()->GetTileFlags(MapIndexB);
	m_TileIndexT = Collision()->GetTileIndex(MapIndexT);
	m_TileFlagsT = Collision()->GetTileFlags(MapIndexT);
	m_TileFIndex = Collision()->GetFTileIndex(MapIndex);
	m_TileFFlags = Collision()->GetFTileFlags(MapIndex);
	m_TileFIndexL = Collision()->GetFTileIndex(MapIndexL);
	m_TileFFlagsL = Collision()->GetFTileFlags(MapIndexL);
	m_TileFIndexR = Collision()->GetFTileIndex(MapIndexR);
	m_TileFFlagsR = Collision()->GetFTileFlags(MapIndexR);
	m_TileFIndexB = Collision()->GetFTileIndex(MapIndexB);
	m_TileFFlagsB = Collision()->GetFTileFlags(MapIndexB);
	m_TileFIndexT = Collision()->GetFTileIndex(MapIndexT);
	m_TileFFlagsT = Collision()->GetFTileFlags(MapIndexT);
	m_TileSIndex = (UseInput && IsRightTeam(MapIndex))?Collision()->GetDTileIndex(MapIndex):0;
	m_TileSFlags = (UseInput && IsRightTeam(MapIndex))?Collision()->GetDTileFlags(MapIndex):0;
	m_TileSIndexL = (UseInput && IsRightTeam(MapIndexL))?Collision()->GetDTileIndex(MapIndexL):0;
	m_TileSFlagsL = (UseInput && IsRightTeam(MapIndexL))?Collision()->GetDTileFlags(MapIndexL):0;
	m_TileSIndexR = (UseInput && IsRightTeam(MapIndexR))?Collision()->GetDTileIndex(MapIndexR):0;
	m_TileSFlagsR = (UseInput && IsRightTeam(MapIndexR))?Collision()->GetDTileFlags(MapIndexR):0;
	m_TileSIndexB = (UseInput && IsRightTeam(MapIndexB))?Collision()->GetDTileIndex(MapIndexB):0;
	m_TileSFlagsB = (UseInput && IsRightTeam(MapIndexB))?Collision()->GetDTileFlags(MapIndexB):0;
	m_TileSIndexT = (UseInput && IsRightTeam(MapIndexT))?Collision()->GetDTileIndex(MapIndexT):0;
	m_TileSFlagsT = (UseInput && IsRightTeam(MapIndexT))?Collision()->GetDTileFlags(MapIndexT):0;
	m_TriggeredEvents = 0;

	// get ground state
	bool Grounded = false;
	if(m_pCollision->CheckPoint(m_Pos.x+PhysSize/2, m_Pos.y+PhysSize/2+5))
		Grounded = true;
	if(m_pCollision->CheckPoint(m_Pos.x-PhysSize/2, m_Pos.y+PhysSize/2+5))
		Grounded = true;

	vec2 TargetDirection = normalize(vec2(m_Input.m_TargetX, m_Input.m_TargetY));

	m_Vel.y += m_pWorld->m_Tuning.m_Gravity;

	float MaxSpeed = Grounded ? m_pWorld->m_Tuning.m_GroundControlSpeed : m_pWorld->m_Tuning.m_AirControlSpeed;
	float Accel = Grounded ? m_pWorld->m_Tuning.m_GroundControlAccel : m_pWorld->m_Tuning.m_AirControlAccel;
	float Friction = Grounded ? m_pWorld->m_Tuning.m_GroundFriction : m_pWorld->m_Tuning.m_AirFriction;

	// handle input
	if(UseInput)
	{
		m_Direction = m_Input.m_Direction;

		// setup angle
		float a = 0;
		if(m_Input.m_TargetX == 0)
			a = atanf((float)m_Input.m_TargetY);
		else
			a = atanf((float)m_Input.m_TargetY/(float)m_Input.m_TargetX);

		if(m_Input.m_TargetX < 0)
			a = a+pi;

		m_Angle = (int)(a*256.0f);

		// handle jump
		if(m_Input.m_Jump)
		{
			if(!(m_Jumped&1))
			{
				if(Grounded)
				{
					m_TriggeredEvents |= COREEVENT_GROUND_JUMP;
					m_Vel.y = -m_pWorld->m_Tuning.m_GroundJumpImpulse;
					m_Jumped |= 1;
					m_JumpedTotal = 1;
				}
				else if(!(m_Jumped&2))
				{
					m_TriggeredEvents |= COREEVENT_AIR_JUMP;
					m_Vel.y = -m_pWorld->m_Tuning.m_AirJumpImpulse;
					m_Jumped |= 3;
					m_JumpedTotal++;
				}
			}
		}
		else
			m_Jumped &= ~1;

		// handle hook
		if(m_Input.m_Hook)
		{
			if(m_HookState == HOOK_IDLE)
			{
				m_HookState = HOOK_FLYING;
				m_HookPos = m_Pos+TargetDirection*PhysSize*1.5f;
				m_HookDir = TargetDirection;
				m_HookedPlayer = -1;
				m_HookTick = 0;
				m_TriggeredEvents |= COREEVENT_HOOK_LAUNCH;
			}
		}
		else
		{
			m_HookedPlayer = -1;
			m_HookState = HOOK_IDLE;
			m_HookPos = m_Pos;
		}
	}

	// add the speed modification according to players wanted direction
	if(m_Direction < 0)
		m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, m_Vel.x, -Accel);
	if(m_Direction > 0)
		m_Vel.x = SaturatedAdd(-MaxSpeed, MaxSpeed, m_Vel.x, Accel);
	if(m_Direction == 0)
		m_Vel.x *= Friction;

	// handle jumping
	// 1 bit = to keep track if a jump has been made on this input
	// 2 bit = to keep track if a air-jump has been made
	if(Grounded)
	{
		m_Jumped &= ~2;
		m_JumpedTotal = 0;
	}

	// do hook
	if(m_HookState == HOOK_IDLE)
	{
		m_HookedPlayer = -1;
		m_HookState = HOOK_IDLE;
		m_HookPos = m_Pos;
	}
	else if(m_HookState >= HOOK_RETRACT_START && m_HookState < HOOK_RETRACT_END)
	{
		m_HookState++;
	}
	else if(m_HookState == HOOK_RETRACT_END)
	{
		m_HookState = HOOK_RETRACTED;
		m_TriggeredEvents |= COREEVENT_HOOK_RETRACT;
		m_HookState = HOOK_RETRACTED;
	}
	else if(m_HookState == HOOK_FLYING)
	{
		vec2 NewPos = m_HookPos+m_HookDir*m_pWorld->m_Tuning.m_HookFireSpeed;
		if((!m_NewHook && distance(m_Pos, NewPos) > m_pWorld->m_Tuning.m_HookLength)
		|| (m_NewHook && distance(m_HookTeleBase, NewPos) > m_pWorld->m_Tuning.m_HookLength))
		{
			m_HookState = HOOK_RETRACT_START;
			NewPos = m_Pos + normalize(NewPos-m_Pos) * m_pWorld->m_Tuning.m_HookLength;
			m_pReset = true;
		}

		// make sure that the hook doesn't go though the ground
		bool GoingToHitGround = false;
		bool GoingToRetract = false;
		bool GoingThroughTele = false;
		int teleNr = 0;
		int Hit = m_pCollision->IntersectLineTeleHook(m_HookPos, NewPos, &NewPos, 0, &teleNr, true);

		//m_NewHook = false;

		if(Hit)
		{
			if(Hit&CCollision::COLFLAG_NOHOOK)
				GoingToRetract = true;
			else if (Hit&CCollision::COLFLAG_TELE)
				GoingThroughTele = true;
			else
				GoingToHitGround = true;
			m_pReset = true;
		}

		// Check against other players first
		if(this->m_Hook && m_pWorld && m_pWorld->m_Tuning.m_PlayerHooking)
		{
			float Distance = 0.0f;
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				CCharacterCore *pCharCore = m_pWorld->m_apCharacters[i];
				if(!pCharCore || pCharCore == this || !m_pTeams->CanCollide(i, m_Id))
					continue;

				vec2 ClosestPoint = closest_point_on_line(m_HookPos, NewPos, pCharCore->m_Pos);
				if(distance(pCharCore->m_Pos, ClosestPoint) < PhysSize+2.0f)
				{
					if (m_HookedPlayer == -1 || distance(m_HookPos, pCharCore->m_Pos) < Distance)
					{
						m_TriggeredEvents |= COREEVENT_HOOK_ATTACH_PLAYER;
						m_HookState = HOOK_GRABBED;
						m_HookedPlayer = i;
						Distance = distance(m_HookPos, pCharCore->m_Pos);
					}
				}
			}
		}

		if(m_HookState == HOOK_FLYING)
		{
			// check against ground
			if(GoingToHitGround)
			{
				m_TriggeredEvents |= COREEVENT_HOOK_ATTACH_GROUND;
				m_HookState = HOOK_GRABBED;
			}
			else if(GoingToRetract)
			{
				m_TriggeredEvents |= COREEVENT_HOOK_HIT_NOHOOK;
				m_HookState = HOOK_RETRACT_START;
			}

			if(GoingThroughTele && m_pTeleOuts && m_pTeleOuts->size() && (*m_pTeleOuts)[teleNr-1].size())
			{
				m_TriggeredEvents = 0;
				m_HookedPlayer = -1;

				m_NewHook = true;
				int Num = (*m_pTeleOuts)[teleNr-1].size();
				m_HookPos = (*m_pTeleOuts)[teleNr-1][(!Num)?Num:rand() % Num]+TargetDirection*PhysSize*1.5f;
				m_HookDir = TargetDirection;
        m_HookTeleBase = m_HookPos;
			}
			else
			{
				m_HookPos = NewPos;
			}
		}
	}

	if(m_HookState == HOOK_GRABBED)
	{
		if(m_HookedPlayer != -1)
		{
			CCharacterCore *pCharCore = m_pWorld->m_apCharacters[m_HookedPlayer];
			if(pCharCore)
				m_HookPos = pCharCore->m_Pos;
			else
			{
				// release hook
				m_HookedPlayer = -1;
				m_HookState = HOOK_RETRACTED;
				m_HookPos = m_Pos;
			}

			// keep players hooked for a max of 1.5sec
			//if(Server()->Tick() > hook_tick+(Server()->TickSpeed()*3)/2)
				//release_hooked();
		}

		// don't do this hook rutine when we are hook to a player
		if(m_HookedPlayer == -1 && distance(m_HookPos, m_Pos) > 46.0f)
		{
			vec2 HookVel = normalize(m_HookPos-m_Pos)*m_pWorld->m_Tuning.m_HookDragAccel;
			// the hook as more power to drag you up then down.
			// this makes it easier to get on top of an platform
			if(HookVel.y > 0)
				HookVel.y *= 0.3f;

			// the hook will boost it's power if the player wants to move
			// in that direction. otherwise it will dampen everything abit
			if((HookVel.x < 0 && m_Direction < 0) || (HookVel.x > 0 && m_Direction > 0))
				HookVel.x *= 0.95f;
			else
				HookVel.x *= 0.75f;

			vec2 NewVel = m_Vel+HookVel;

			// check if we are under the legal limit for the hook
			if(length(NewVel) < m_pWorld->m_Tuning.m_HookDragSpeed || length(NewVel) < length(m_Vel))
				m_Vel = NewVel; // no problem. apply

		}

		// release hook (max hook time is 1.25
		m_HookTick++;
		if(m_HookedPlayer != -1 && (m_HookTick > SERVER_TICK_SPEED+SERVER_TICK_SPEED/5 || !m_pWorld->m_apCharacters[m_HookedPlayer]))
		{
			m_HookedPlayer = -1;
			m_HookState = HOOK_RETRACTED;
			m_HookPos = m_Pos;
		}
	}

	if(m_pWorld)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CCharacterCore *pCharCore = m_pWorld->m_apCharacters[i];
			if(!pCharCore)
				continue;

			//player *p = (player*)ent;
			//if(pCharCore == this) // || !(p->flags&FLAG_ALIVE)

			if(pCharCore == this || (m_Id != -1 && !m_pTeams->CanCollide(m_Id, i)))
				continue; // make sure that we don't nudge our self

			// handle player <-> player collision
			float Distance = distance(m_Pos, pCharCore->m_Pos);
			vec2 Dir = normalize(m_Pos - pCharCore->m_Pos);
			if(pCharCore->m_Collision && this->m_Collision && m_pWorld->m_Tuning.m_PlayerCollision && Distance < PhysSize*1.25f && Distance > 0.0f)
			{
				float a = (PhysSize*1.45f - Distance);
				float Velocity = 0.5f;

				// make sure that we don't add excess force by checking the
				// direction against the current velocity. if not zero.
				if (length(m_Vel) > 0.0001)
					Velocity = 1-(dot(normalize(m_Vel), Dir)+1)/2;

				m_Vel += Dir*a*(Velocity*0.75f);
				m_Vel *= 0.85f;
			}

			// handle hook influence
			if(m_Hook && m_HookedPlayer == i && m_pWorld->m_Tuning.m_PlayerHooking)
			{
				if(Distance > PhysSize*1.50f) // TODO: fix tweakable variable
				{
					float Accel = m_pWorld->m_Tuning.m_HookDragAccel * (Distance/m_pWorld->m_Tuning.m_HookLength);
					float DragSpeed = m_pWorld->m_Tuning.m_HookDragSpeed;

					// add force to the hooked player
					vec2 Temp = pCharCore->m_Vel;
					Temp.x = SaturatedAdd(-DragSpeed, DragSpeed, pCharCore->m_Vel.x, Accel*Dir.x*1.5f);
					Temp.y = SaturatedAdd(-DragSpeed, DragSpeed, pCharCore->m_Vel.y, Accel*Dir.y*1.5f);
					if(Temp.x > 0 && ((pCharCore->m_TileIndex == TILE_STOP && pCharCore->m_TileFlags == ROTATION_270) || (pCharCore->m_TileIndexL == TILE_STOP && pCharCore->m_TileFlagsL == ROTATION_270) || (pCharCore->m_TileIndexL == TILE_STOPS && (pCharCore->m_TileFlagsL == ROTATION_90 || pCharCore->m_TileFlagsL ==ROTATION_270)) || (pCharCore->m_TileIndexL == TILE_STOPA) || (pCharCore->m_TileFIndex == TILE_STOP && pCharCore->m_TileFFlags == ROTATION_270) || (pCharCore->m_TileFIndexL == TILE_STOP && pCharCore->m_TileFFlagsL == ROTATION_270) || (pCharCore->m_TileFIndexL == TILE_STOPS && (pCharCore->m_TileFFlagsL == ROTATION_90 || pCharCore->m_TileFFlagsL == ROTATION_270)) || (pCharCore->m_TileFIndexL == TILE_STOPA) || (pCharCore->m_TileSIndex == TILE_STOP && pCharCore->m_TileSFlags == ROTATION_270) || (pCharCore->m_TileSIndexL == TILE_STOP && pCharCore->m_TileSFlagsL == ROTATION_270) || (pCharCore->m_TileSIndexL == TILE_STOPS && (pCharCore->m_TileSFlagsL == ROTATION_90 || pCharCore->m_TileSFlagsL == ROTATION_270)) || (pCharCore->m_TileSIndexL == TILE_STOPA)))
						Temp.x = 0;
					if(Temp.x < 0 && ((pCharCore->m_TileIndex == TILE_STOP && pCharCore->m_TileFlags == ROTATION_90) || (pCharCore->m_TileIndexR == TILE_STOP && pCharCore->m_TileFlagsR == ROTATION_90) || (pCharCore->m_TileIndexR == TILE_STOPS && (pCharCore->m_TileFlagsR == ROTATION_90 || pCharCore->m_TileFlagsR == ROTATION_270)) || (pCharCore->m_TileIndexR == TILE_STOPA) || (pCharCore->m_TileFIndex == TILE_STOP && pCharCore->m_TileFFlags == ROTATION_90) || (pCharCore->m_TileFIndexR == TILE_STOP && pCharCore->m_TileFFlagsR == ROTATION_90) || (pCharCore->m_TileFIndexR == TILE_STOPS && (pCharCore->m_TileFFlagsR == ROTATION_90 || pCharCore->m_TileFFlagsR == ROTATION_270)) || (pCharCore->m_TileFIndexR == TILE_STOPA) || (pCharCore->m_TileSIndex == TILE_STOP && pCharCore->m_TileSFlags == ROTATION_90) || (pCharCore->m_TileSIndexR == TILE_STOP && pCharCore->m_TileSFlagsR == ROTATION_90) || (pCharCore->m_TileSIndexR == TILE_STOPS && (pCharCore->m_TileSFlagsR == ROTATION_90 || pCharCore->m_TileSFlagsR == ROTATION_270)) || (pCharCore->m_TileSIndexR == TILE_STOPA)))
						Temp.x = 0;
					if(Temp.y < 0 && ((pCharCore->m_TileIndex == TILE_STOP && pCharCore->m_TileFlags == ROTATION_180) || (pCharCore->m_TileIndexB == TILE_STOP && pCharCore->m_TileFlagsB == ROTATION_180) || (pCharCore->m_TileIndexB == TILE_STOPS && (pCharCore->m_TileFlagsB == ROTATION_0 || pCharCore->m_TileFlagsB == ROTATION_180)) || (pCharCore->m_TileIndexB == TILE_STOPA) || (pCharCore->m_TileFIndex == TILE_STOP && pCharCore->m_TileFFlags == ROTATION_180) || (pCharCore->m_TileFIndexB == TILE_STOP && pCharCore->m_TileFFlagsB == ROTATION_180) || (pCharCore->m_TileFIndexB == TILE_STOPS && (pCharCore->m_TileFFlagsB == ROTATION_0 || pCharCore->m_TileFFlagsB == ROTATION_180)) || (pCharCore->m_TileFIndexB == TILE_STOPA) || (pCharCore->m_TileSIndex == TILE_STOP && pCharCore->m_TileSFlags == ROTATION_180) || (pCharCore->m_TileSIndexB == TILE_STOP && pCharCore->m_TileSFlagsB == ROTATION_180) || (pCharCore->m_TileSIndexB == TILE_STOPS && (pCharCore->m_TileSFlagsB == ROTATION_0 || pCharCore->m_TileSFlagsB == ROTATION_180)) || (pCharCore->m_TileSIndexB == TILE_STOPA)))
						Temp.y = 0;
					if(Temp.y > 0 && ((pCharCore->m_TileIndex == TILE_STOP && pCharCore->m_TileFlags == ROTATION_0) || (pCharCore->m_TileIndexT == TILE_STOP && pCharCore->m_TileFlagsT == ROTATION_0) || (pCharCore->m_TileIndexT == TILE_STOPS && (pCharCore->m_TileFlagsT == ROTATION_0 || pCharCore->m_TileFlagsT == ROTATION_180)) || (pCharCore->m_TileIndexT == TILE_STOPA) || (pCharCore->m_TileFIndex == TILE_STOP && pCharCore->m_TileFFlags == ROTATION_0) || (pCharCore->m_TileFIndexT == TILE_STOP && pCharCore->m_TileFFlagsT == ROTATION_0) || (pCharCore->m_TileFIndexT == TILE_STOPS && (pCharCore->m_TileFFlagsT == ROTATION_0 || pCharCore->m_TileFFlagsT == ROTATION_180)) || (pCharCore->m_TileFIndexT == TILE_STOPA) || (pCharCore->m_TileSIndex == TILE_STOP && pCharCore->m_TileSFlags == ROTATION_0) || (pCharCore->m_TileSIndexT == TILE_STOP && pCharCore->m_TileSFlagsT == ROTATION_0) || (pCharCore->m_TileSIndexT == TILE_STOPS && (pCharCore->m_TileSFlagsT == ROTATION_0 || pCharCore->m_TileSFlagsT == ROTATION_180)) || (pCharCore->m_TileSIndexT == TILE_STOPA)))
						Temp.y = 0;

					// add a little bit force to the guy who has the grip
					pCharCore->m_Vel = Temp;
					Temp.x = SaturatedAdd(-DragSpeed, DragSpeed, m_Vel.x, -Accel*Dir.x*0.25f);
					Temp.y = SaturatedAdd(-DragSpeed, DragSpeed, m_Vel.y, -Accel*Dir.y*0.25f);
					if(Temp.x > 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_270) || (m_TileIndexL == TILE_STOP && m_TileFlagsL == ROTATION_270) || (m_TileIndexL == TILE_STOPS && (m_TileFlagsL == ROTATION_90 || m_TileFlagsL ==ROTATION_270)) || (m_TileIndexL == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_270) || (m_TileFIndexL == TILE_STOP && m_TileFFlagsL == ROTATION_270) || (m_TileFIndexL == TILE_STOPS && (m_TileFFlagsL == ROTATION_90 || m_TileFFlagsL == ROTATION_270)) || (m_TileFIndexL == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_270) || (m_TileSIndexL == TILE_STOP && m_TileSFlagsL == ROTATION_270) || (m_TileSIndexL == TILE_STOPS && (m_TileSFlagsL == ROTATION_90 || m_TileSFlagsL == ROTATION_270)) || (m_TileSIndexL == TILE_STOPA)))
						Temp.x = 0;
					if(Temp.x < 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_90) || (m_TileIndexR == TILE_STOP && m_TileFlagsR == ROTATION_90) || (m_TileIndexR == TILE_STOPS && (m_TileFlagsR == ROTATION_90 || m_TileFlagsR == ROTATION_270)) || (m_TileIndexR == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_90) || (m_TileFIndexR == TILE_STOP && m_TileFFlagsR == ROTATION_90) || (m_TileFIndexR == TILE_STOPS && (m_TileFFlagsR == ROTATION_90 || m_TileFFlagsR == ROTATION_270)) || (m_TileFIndexR == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_90) || (m_TileSIndexR == TILE_STOP && m_TileSFlagsR == ROTATION_90) || (m_TileSIndexR == TILE_STOPS && (m_TileSFlagsR == ROTATION_90 || m_TileSFlagsR == ROTATION_270)) || (m_TileSIndexR == TILE_STOPA)))
						Temp.x = 0;
					if(Temp.y < 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_180) || (m_TileIndexB == TILE_STOP && m_TileFlagsB == ROTATION_180) || (m_TileIndexB == TILE_STOPS && (m_TileFlagsB == ROTATION_0 || m_TileFlagsB == ROTATION_180)) || (m_TileIndexB == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_180) || (m_TileFIndexB == TILE_STOP && m_TileFFlagsB == ROTATION_180) || (m_TileFIndexB == TILE_STOPS && (m_TileFFlagsB == ROTATION_0 || m_TileFFlagsB == ROTATION_180)) || (m_TileFIndexB == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_180) || (m_TileSIndexB == TILE_STOP && m_TileSFlagsB == ROTATION_180) || (m_TileSIndexB == TILE_STOPS && (m_TileSFlagsB == ROTATION_0 || m_TileSFlagsB == ROTATION_180)) || (m_TileSIndexB == TILE_STOPA)))
						Temp.y = 0;
					if(Temp.y > 0 && ((m_TileIndex == TILE_STOP && m_TileFlags == ROTATION_0) || (m_TileIndexT == TILE_STOP && m_TileFlagsT == ROTATION_0) || (m_TileIndexT == TILE_STOPS && (m_TileFlagsT == ROTATION_0 || m_TileFlagsT == ROTATION_180)) || (m_TileIndexT == TILE_STOPA) || (m_TileFIndex == TILE_STOP && m_TileFFlags == ROTATION_0) || (m_TileFIndexT == TILE_STOP && m_TileFFlagsT == ROTATION_0) || (m_TileFIndexT == TILE_STOPS && (m_TileFFlagsT == ROTATION_0 || m_TileFFlagsT == ROTATION_180)) || (m_TileFIndexT == TILE_STOPA) || (m_TileSIndex == TILE_STOP && m_TileSFlags == ROTATION_0) || (m_TileSIndexT == TILE_STOP && m_TileSFlagsT == ROTATION_0) || (m_TileSIndexT == TILE_STOPS && (m_TileSFlagsT == ROTATION_0 || m_TileSFlagsT == ROTATION_180)) || (m_TileSIndexT == TILE_STOPA)))
						Temp.y = 0;
					m_Vel = Temp;
				}
			}
		}

		if (m_HookState != HOOK_FLYING)
		{
			m_NewHook = false;
		}
	}

	// clamp the velocity to something sane
	if(length(m_Vel) > 6000)
		m_Vel = normalize(m_Vel) * 6000;
}

void CCharacterCore::Move()
{
	float RampValue = VelocityRamp(length(m_Vel)*50, m_pWorld->m_Tuning.m_VelrampStart, m_pWorld->m_Tuning.m_VelrampRange, m_pWorld->m_Tuning.m_VelrampCurvature);

	m_Vel.x = m_Vel.x*RampValue;

	vec2 NewPos = m_Pos;
	m_pCollision->MoveBox(&NewPos, &m_Vel, vec2(28.0f, 28.0f), 0);

	m_Vel.x = m_Vel.x*(1.0f/RampValue);

	if(m_pWorld && m_pWorld->m_Tuning.m_PlayerCollision && this->m_Collision)
	{
		// check player collision
		float Distance = distance(m_Pos, NewPos);
		int End = Distance+1;
		vec2 LastPos = m_Pos;
		for(int i = 0; i < End; i++)
		{
			float a = i/Distance;
			vec2 Pos = mix(m_Pos, NewPos, a);
			for(int p = 0; p < MAX_CLIENTS; p++)
			{
				CCharacterCore *pCharCore = m_pWorld->m_apCharacters[p];
				if(!pCharCore || pCharCore == this || (m_Id != -1 && !m_pTeams->CanCollide(m_Id, p)) || !pCharCore->m_Collision)
					continue;
				float D = distance(Pos, pCharCore->m_Pos);
				if(D < 28.0f && D > 0.0f)
				{
					if(a > 0.0f)
						m_Pos = LastPos;
					else if(distance(NewPos, pCharCore->m_Pos) > D)
						m_Pos = NewPos;
					return;
				}
				else if(D <= 0.001f && D >= -0.001f)
				{
					if(a > 0.0f)
						m_Pos = LastPos;
					else if(distance(NewPos, pCharCore->m_Pos) > D)
						m_Pos = NewPos;
					return;
				}
			}
			LastPos = Pos;
		}
	}

	m_Pos = NewPos;
}

void CCharacterCore::Write(CNetObj_CharacterCore *pObjCore)
{
	pObjCore->m_X = round(m_Pos.x);
	pObjCore->m_Y = round(m_Pos.y);

	pObjCore->m_VelX = round(m_Vel.x*256.0f);
	pObjCore->m_VelY = round(m_Vel.y*256.0f);
	pObjCore->m_HookState = m_HookState;
	pObjCore->m_HookTick = m_HookTick;
	pObjCore->m_HookX = round(m_HookPos.x);
	pObjCore->m_HookY = round(m_HookPos.y);
	pObjCore->m_HookDx = round(m_HookDir.x*256.0f);
	pObjCore->m_HookDy = round(m_HookDir.y*256.0f);
	pObjCore->m_HookedPlayer = m_HookedPlayer;
	pObjCore->m_Jumped = m_Jumped;
	pObjCore->m_Direction = m_Direction;
	pObjCore->m_Angle = m_Angle;
}

void CCharacterCore::Read(const CNetObj_CharacterCore *pObjCore)
{
	m_Pos.x = pObjCore->m_X;
	m_Pos.y = pObjCore->m_Y;
	m_Vel.x = pObjCore->m_VelX/256.0f;
	m_Vel.y = pObjCore->m_VelY/256.0f;
	m_HookState = pObjCore->m_HookState;
	m_HookTick = pObjCore->m_HookTick;
	m_HookPos.x = pObjCore->m_HookX;
	m_HookPos.y = pObjCore->m_HookY;
	m_HookDir.x = pObjCore->m_HookDx/256.0f;
	m_HookDir.y = pObjCore->m_HookDy/256.0f;
	m_HookedPlayer = pObjCore->m_HookedPlayer;
	m_Jumped = pObjCore->m_Jumped;
	m_Direction = pObjCore->m_Direction;
	m_Angle = pObjCore->m_Angle;
}

void CCharacterCore::Quantize()
{
	CNetObj_CharacterCore Core;
	Write(&Core);
	Read(&Core);
}

// DDRace

bool CCharacterCore::IsRightTeam(int MapIndex)
{
	if(Collision()->m_pSwitchers)
		if(m_pTeams->Team(m_Id) != m_pTeams->m_IsDDRace16 ? VANILLA_TEAM_SUPER : TEAM_SUPER)
			return Collision()->m_pSwitchers[Collision()->GetDTileNumber(MapIndex)].m_Status[m_pTeams->Team(m_Id)];
	return false;
}
