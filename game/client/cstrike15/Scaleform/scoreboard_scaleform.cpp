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

CScoreboardScaleform::CScoreboardScaleform( CounterStrikeViewport *pViewport ) :
	m_bVisible( false ),
	m_bLoading( true )
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

	// If the movie hasn't been loaded yet, start loading it.
	if ( m_bLoading )
	{
		if ( !FlashAPIIsValid() )
		{
			m_bLoading = true;
			SFUI_REQUEST_ELEMENT( SF_SS_SLOT( m_iSplitScreenSlot ), g_pScaleformUI, CScoreboardScaleform, this, Scoreboard );
		}

		m_bVisible = true;
		return;
	}
}

void CScoreboardScaleform::Hide()
{

}

void CScoreboardScaleform::FlashLoaded( void )
{

}

bool CScoreboardScaleform::PreUnloadFlash()
{

}

void CScoreboardScaleform::ShowPanel( bool state )
{
	if ( state == m_bVisible )
	{
		return;
	}

	if ( state )
	{
		Show();
	}
	else
	{
		Hide();
	}
}

void CScoreboardScaleform::FireGameEvent( IGameEvent *event )
{

}

void CScoreboardScaleform::ViewportThink( void )
{

}

#endif // INCLUDE_SCALEFORM