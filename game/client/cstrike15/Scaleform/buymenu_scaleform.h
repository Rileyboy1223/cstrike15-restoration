#ifndef SFBUYMENU_H
#define SFBUYMENU_H
#pragma once

#include "cbase.h"

#include "scaleformui/scaleformui.h"

#include "../VGUI/counterstrikeviewport.h"

#include "game/client/iviewport.h"
#include "gameeventlistener.h"

class CounterStrikeViewport;
class CItemModelPanel;

struct BuyMenuWeaponSlot
{
	int m_iWeaponDefIndex;
	int m_iSlot;
};

struct BuyMenuWeaponData
{
	int m_iWeaponDefIndex;
	int m_iSlot;

	int m_iTeam;
	int m_iPrice;

	char m_szWeaponName[0x2C];

	uint64_t m_Unknown1;
	uint64_t m_Unknown2;
	uint64_t m_Unknown3;
	uint64_t m_Unknown4;
	uint64_t m_Unknown5;
	uint64_t m_Unknown6;
};
extern BuyMenuWeaponData g_LoadoutArray[2][4];

enum BuyMenuEquipmentFlags
{
	BUYMENU_EQUIPMENT_KEVLAR = 0x01,
	BUYMENU_EQUIPMENT_ZEUS = 0x02,
	BUYMENU_EQUIPMENT_DEFUSER = 0x04,

	WEAPON_ASSAULTSUIT = 0x05,
	WEAPON_KEVLAR = 0x06,
};

struct BuyMenuGrenadeEntry
{
	int m_nWeaponID;       // CSWeaponID
	int m_nLoadoutSlot;    // inventory loadout slot
	int m_nCount;          // reserve ammo / quantity

	BuyMenuGrenadeEntry()
	{
		m_nWeaponID = 0;
		m_nLoadoutSlot = 0;
		m_nCount = 0;
	}

	bool operator==(const BuyMenuGrenadeEntry &rhs) const
	{
		return m_nWeaponID == rhs.m_nWeaponID && m_nLoadoutSlot == rhs.m_nLoadoutSlot && m_nCount == rhs.m_nCount;
	}
};

struct CCSLoadout
{
	// Primary weapon currently equipped
	int m_nPrimaryWeaponID;
	int m_nSecondaryWeaponID;

	// Loadout slots selected in inventory
	int m_nPrimaryLoadoutSlot;
	int m_nSecondaryLoadoutSlot;

	// Index slots
	int m_nPrimaryWeaponDefIndex;
	int m_nSecondaryWeaponDefIndex;

	// Armor / grenades / utility
	BuyMenuGrenadeEntry m_Items[6];

	// Zeus / Knife / Defuser flags
	uint8 m_nFlags;

	CCSLoadout()
	{
		Q_memset(this, 0, sizeof(*this));
	}

	bool operator==(const CCSLoadout &rhs) const
	{
		return memcmp(this, &rhs, sizeof(CCSLoadout)) == 0;
	}

	bool operator!=(const CCSLoadout &rhs) const
	{
		return !(*this == rhs);
	}
};

struct InputAnalogActionData_t
{
	bool bActive;

	int x;
	int y;
};

class CCSBuyMenuScaleform : public ScaleformFlashInterface, public IViewPortPanel, public CGameEventListener
{
public:
	explicit CCSBuyMenuScaleform( CounterStrikeViewport *pViewport );
	virtual ~CCSBuyMenuScaleform();

	/****************************************
	* functionality
	*/

	bool FillInPlayerLoadout( CCSLoadout &loadout );
	bool UpdateTimeLeft( bool bForce );
	void SetPlayerIsCT( bool isCT );
	void CalculateBestStats();
	void UpdatePlayerLoadout( bool bForceUpdate, bool bUpdateFlash );
	void UpdateRadialSelection();
	void SetRadialSelection( int radialSlot, int categorySlot );
	void InvalidateWeapon( int weaponID );
	void OnCancel();
	void UpdatePlayerCash( bool bForce );
	void SetHostageMatch( bool bHostageMatch );

	SFVALUE CreateFlashLoadout( const CCSLoadout &loadout );

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

	virtual const char *GetName(void) { return PANEL_BUY; }
	virtual void SetData( KeyValues *data ) {};

	virtual void Reset( void ) {}  // hibernate
	virtual void Update( void ) {}	// updates all ( size, position, content, etc )
	virtual bool NeedsUpdate( void ) { return false; } // query panel if content needs to be updated
	virtual bool HasInputElements( void ) { return true; }
	virtual void ReloadScheme( void ) {}
	virtual bool CanReplace( const char *panelName ) const { return true; } // returns true if this panel can appear on top of the given panel
	virtual bool CanBeReopened( void ) const { return true; } // returns true if this panel can be re-opened after being hidden by another panel
	virtual void ViewportThink( void );

	void ShowPanel( bool bShow );

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
	CCSLoadout m_PlayerLoadout;
	SFVALUE value;

private:

	int   m_iSelectedWeapon;
	int   m_iSplitScreenSlot;
	int   m_iCachedArmorState;
	int   m_iCurrentRadialSelection;
	int   m_iSelectedCategory;
	int   m_iWheelSelection;
	int	  m_nLastTimeLeft;
	int   m_fGuardianBuyUntilTime;
	int   m_nLastAccount;

	bool  m_bMovieLoaded;
	bool  m_bLoadingMovie;
	bool  m_bVisible;
	bool  m_bNeedUpdate;
	bool  m_bLoading;
	bool  m_bRegisteredEvents;
	bool  m_bShowOnReady;
	bool  m_bIsLoaded;

	char  m_Padding[0x1C8];

	bool  m_bInitialized;

	int   m_iPrimaryWeapon;
	int   m_iSecondaryWeapon;

	int   m_iArmorValue;
	int   m_iCurrentMoney;

	int	  m_nPlayerSlot;

	float m_flNextLoadoutUpdate;

	float m_flMinX;
	float m_flMaxX;

	float m_flMinY;
	float m_flMaxY;

	float m_flMinZ;
	float m_flMaxZ;

	float m_flUnknown0;
	float m_flUnknown1;

	int   m_iUnknownState;

	vgui::EditablePanel *m_pItemPanelParent;
	CItemModelPanel   *m_pItemModelPanel;
};

#endif // SFBUYMENU_H