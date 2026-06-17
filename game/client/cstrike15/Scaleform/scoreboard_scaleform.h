#ifndef SFSCOREBOARD_H
#define SFSCOREBOARD_H
#pragma once

#include "scaleformui/scaleformui.h"
#include "../VGUI/counterstrikeviewport.h"
#include "GameEventListener.h"

class CounterStrikeViewport;

class CScoreboardScaleform : public ScaleformFlashInterface, public IViewPortPanel, public CGameEventListener
{
public:
	explicit CScoreboardScaleform( CounterStrikeViewport *pViewport );
	virtual ~CScoreboardScaleform();

	/************************************
	* callbacks from scaleform
	*/

	// Function goes here.

	/****************************************
	* functionality
	*/

	// Function goes here.

	/************************************************************
	*  Flash Interface methods
	*/

	virtual void FlashReady( void );
	virtual void FlashLoaded( void );

	void Show( void );

	// if bRemove, then remove all elements after hide animation completes
	void Hide( void );

	bool PreUnloadFlash( void );

	/*************************************************************
	* IViewPortPanel interface
	*/

	virtual const char *GetName( void ) { return PANEL_SCOREBOARD; }
	virtual void SetData( KeyValues *data ) {};

	virtual void Reset( void ) {}  // hibernate
	virtual void Update( void ) {}	// updates all ( size, position, content, etc )
	virtual bool NeedsUpdate( void ) { return false; } // query panel if content needs to be updated
	virtual bool HasInputElements( void ) { return true; }
	virtual void ReloadScheme( void ) {}
	virtual bool CanReplace( const char *panelName ) const { return true; } // returns true if this panel can appear on top of the given panel
	virtual bool CanBeReopened( void ) const { return true; } // returns true if this panel can be re-opened after being hidden by another panel
	virtual void ViewportThink( void );

	void ShowPanel( bool state );

	// VGUI functions:
	virtual vgui::VPANEL GetVPanel( void ) { return 0; } // returns VGUI panel handle
	virtual bool IsVisible( void ) { return m_bVisible; }  // true if panel is visible
	virtual void SetParent( vgui::VPANEL parent ) {}

	virtual bool WantsBackgroundBlurred( void ) { return false; }

	/********************************************
	* CGameEventListener methods
	*/

	virtual void FireGameEvent( IGameEvent *event );

protected:
	CounterStrikeViewport *m_pViewport;

private:
	int   m_iSplitScreenSlot;

	bool  m_bVisible;
	bool  m_bLoading;

	vgui::EditablePanel *m_pItemPanelParent;
};

#endif // SFSCOREBOARD_H