// RILEY WALLIS

#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "ienginevgui.h"
#include "scoreboard_scaleform.h"
#include "gameui_util.h"
#include "inputsystem/iinputsystem.h"
#include "../gameui/cstrike15/cstrike15basepanel.h"

#include <keyvalues.h>

#include <vgui_controls/EditablePanel.h>

#include "HUD/sfhudinfopanel.h"

#include "c_cs_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

SFUI_BEGIN_GAME_API_DEF
SFUI_END_GAME_API_DEF( CScoreboardScaleform, Scoreboard );

CScoreboardScaleform::CScoreboardScaleform( void ) :
	m_bVisible(false)
{

}

CScoreboardScaleform::~CScoreboardScaleform()
{

}

void CScoreboardScaleform::FlashReady()
{

}

void CScoreboardScaleform::Show()
{
	// Register for radial input
	g_pInputSystem->SetSteamControllerMode("RadialControls", this);

	// If the movie hasn't been loaded yet, start loading it.
	if (m_bLoading)
	{
		if (!FlashAPIIsValid())
		{
			m_bLoading = true;
			SFUI_REQUEST_ELEMENT(SF_SS_SLOT(m_iSplitScreenSlot), g_pScaleformUI, CScoreboardScaleform, this, BuyMenu);
		}

		m_bVisible = true;
		return;
	}

	SF_FORCE_SPLITSCREEN_PLAYER_GUARD(m_iSplitScreenSlot);

	m_bNeedUpdate = false;
	m_iSelectedCategory = 0;
	m_iCurrentRadialSelection = 0;

	UpdatePlayerCash(true);
	UpdateTimeLeft(true);

#if defined( CEG_ALLOW_BUYMENU )
	if (true)
#endif
	{
		g_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "showPanel", 0, NULL);

		m_iWheelSelection = -1;

		GetHud().DisableHud();

		C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
		if (pPlayer)
			pPlayer->SetBuyMenuOpen(true);

		bool bHostageMap = false;
		if (CSGameRules())
			bHostageMap = CSGameRules()->IsHostageRescueMap();

		SetHostageMatch(bHostageMap);

		m_bVisible = true;
	}
}

void CScoreboardScaleform::Hide()
{
	// Unregister radial controls
	g_pInputSystem->SetSteamControllerMode(NULL, this);

	if (!m_bLoading && FlashAPIIsValid() && m_bVisible)
	{
		SF_FORCE_SPLITSCREEN_PLAYER_GUARD(m_iSplitScreenSlot);

		g_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "hidePanel", nullptr, 0);

		GetHud().EnableHud();
	}

	m_bVisible = false;

	// Clear selected weapon model
	//if ( m_pWeaponModelPanel )
	//m_pWeaponModelPanel->SetWeapon( nullptr );

	{
		SF_FORCE_SPLITSCREEN_PLAYER_GUARD(m_iSplitScreenSlot);

		C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
		if (pPlayer)
			pPlayer->SetBuyMenuOpen(false);
	}
}

void CScoreboardScaleform::FlashLoaded(void)
{

}

bool CScoreboardScaleform::PreUnloadFlash()
{

}

void CScoreboardScaleform::ShowPanel(bool state)
{
	if (state == m_bVisible)
	{
		return;
	}

	if (state)
	{
		Show();
	}
	else
	{
		Hide();
	}
}

void CScoreboardScaleform::FireGameEvent(IGameEvent *event)
{

}

void CScoreboardScaleform::ViewportThink(void)
{

}

#endif // INCLUDE_SCALEFORM