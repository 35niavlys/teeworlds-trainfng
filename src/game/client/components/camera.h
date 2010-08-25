#ifndef GAME_CLIENT_COMPONENTS_CAMERA_H
#define GAME_CLIENT_COMPONENTS_CAMERA_H
#include <base/vmath.h>
#include <game/client/component.h>
#include <game/client/gameclient.h>

class CCamera : public CComponent
{	

	static void ConZoomPlus(IConsole::IResult *pResult, void *pUserData);
	static void ConZoomMinus(IConsole::IResult *pResult, void *pUserData);
	static void ConZoomReset(IConsole::IResult *pResult, void *pUserData);
	static void ConCameraFree(IConsole::IResult *pResult, void *pUserData);
	
	CGameClient::CSnapState::CCharacterInfo* getCharacter(int Id);
	
public:
	vec2 m_Center;
	float m_Zoom;
	bool m_Free;
	int m_Follow;

	CCamera();
	virtual void OnRender();
	virtual void OnConsoleInit();
	bool SetFollow(int Id);
};

#endif
