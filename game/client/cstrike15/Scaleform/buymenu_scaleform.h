#ifndef SFBUYMENU_H
#define SFBUYMENU_H
#pragma once

#include "scaleformui/scaleformui.h"
#include "../VGUI/counterstrikeviewport.h"
#include "GameEventListener.h"

const int cMaxEquipment = 6;  // if this # changes, bump version # cl_titledataversionblock3 and loadoutData in TitleData1 block for title data storageCCSSteamStats::SyncCSLoadoutsToTitleData
const int cMaxLoadouts = 4;	// if this # changes, bump version # cl_titledataversionblock3 and loadoutData in TitleData1 block  for title data storageCCSSteamStats::SyncCSLoadoutsToTitleData

class CounterStrikeViewport;

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

struct CCSEquipmentLoadout
{
	int m_EquipmentID;
	int m_EquipmentPos;
	int m_Quantity;

	CCSEquipmentLoadout()
	{
		m_EquipmentID = 0;
		m_EquipmentPos = 0;
		m_Quantity = 0;
	}

	bool operator==(const CCSEquipmentLoadout &in_rhs) const
	{
		return m_EquipmentID == in_rhs.m_EquipmentID && m_EquipmentPos == in_rhs.m_EquipmentPos && m_Quantity == in_rhs.m_Quantity;
	}
};

struct CCSLoadout
{
	enum
	{
		HAS_TASER = 0x1,
		HAS_BOMB = 0x2,
		HAS_DEFUSE = 0x4,
	};

	int m_primaryWeaponID;
	int m_secondaryWeaponID;

	int m_primaryWeaponItemPos;
	int m_secondaryWeaponItemPos;

	int m_nPrimaryWeaponDefIndex;
	int m_nSecondaryWeaponDefIndex;

	// Data fields
	CCSEquipmentLoadout m_EquipmentArray[cMaxEquipment];
	uint8 m_flags;

	CCSLoadout()
	{
		Q_memset(this, 0, sizeof(*this));
	}

	bool operator==(const CCSLoadout &in_rhs) const
	{
		return memcmp(this, &in_rhs, sizeof(CCSLoadout)) == 0;
	}

	bool operator!=(const CCSLoadout &in_rhs) const
	{
		return !(*this == in_rhs);
	}
};

struct CCSBuyMenuLoadout
{
	CCSBuyMenuLoadout()
	{
		ClearLoadout();
	}

	void ClearLoadout(void)
	{
		for (int i = 0; i < ARRAYSIZE(m_WeaponID); i++)
		{
			m_WeaponID[i] = 0;
		}
	}

	CCSBuyMenuLoadout& operator=(const CCSBuyMenuLoadout& in_rhs)
	{
		for (int i = 0; i < ARRAYSIZE(m_WeaponID); i++)
		{
			m_WeaponID[i] = in_rhs.m_WeaponID[i];
		}

		return *this;
	}

	bool operator==(const CCSBuyMenuLoadout& in_rhs) const
	{
		for (int i = 0; i < ARRAYSIZE(m_WeaponID); i++)
		{
			if (m_WeaponID[i] != in_rhs.m_WeaponID[i])
			{
				return false;
			}
		}

		return true;
	}

	inline bool operator!=(const CCSBuyMenuLoadout& in_rhs) const
	{
		return !(*this == in_rhs);
	}

	//CEconItemView * GetWeaponView(loadout_positions_t pos, int nTeamNumber) const;

	// Data fields
	itemid_t          m_WeaponID[LOADOUT_POSITION_COUNT];
	uint64			  m_LoadoutItems[LOADOUT_POSITION_COUNT];
};

struct InputAnalogActionData_t
{
	bool bActive;

	int x;
	int y;
};

struct BuyMenuLoadout_t
{
	CUtlVector<int> weapons;
	CUtlVector<int> weapons_position;
	CUtlVector<int> counts;

	uint64 m_LoadoutItems[ LOADOUT_POSITION_COUNT ];

	int primary;
	int secondary;
	int taser;

	int price;
	int armor;
};

class CCSBuyMenuScaleform : public ScaleformFlashInterface, public IViewPortPanel, public CGameEventListener
{
public:
	explicit CCSBuyMenuScaleform( CounterStrikeViewport *pViewport );
	virtual ~CCSBuyMenuScaleform();

	/************************************
	* callbacks from scaleform
	*/

	void OnCancel( SCALEFORM_CALLBACK_ARGS_DECL );
	void InitWeapon( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponPriceScript( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponPriceFromIDScript( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetPlayerLoadout( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetPlayerBuyMenuLoadout( SCALEFORM_CALLBACK_ARGS_DECL );
	void AreWeaponsFree( SCALEFORM_CALLBACK_ARGS_DECL );

	/****************************************
	* functionality
	*/

	bool FillInPlayerLoadout( CCSLoadout &loadout );
	bool FillInPlayerBuyMenuLoadout( CCSBuyMenuLoadout *pLoadout );
	bool UpdateTimeLeft( bool bForce );
	bool SetBuyMenuWeaponSliceEntry( uint32 weaponPosition, SFVALUE flashArray, uint32 arrayIndex, CCSBuyMenuLoadout &loadout );

	void SetPlayerIsCT( const bool isCT );
	void CalculateBestStats();
	void UpdatePlayerLoadout( bool bForceUpdate, bool bUpdateFlash );
	void UpdateRadialSelection();
	void SetRadialSelection( int radialSlot, int categorySlot );
	void InvalidateWeapon( int weaponID );
	void UpdatePlayerCash( bool bForce );
	void SetHostageMatch( bool bHostageMatch );

	SFVALUE CreateFlashLoadout( const CCSLoadout &loadout );
	SFVALUE CreateFlashBuyMenuLoadout( CCSBuyMenuLoadout &loadout );

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

	virtual const char *GetName( void ) { return PANEL_BUY; }
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
	CCSLoadout m_PlayerLoadout;

	CCSBuyMenuLoadout m_PlayerBuyMenuLoadout;

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

	unsigned int m_iFlashHandle;

	bool  m_bLoadingMovie;
	bool  m_bVisible;
	bool  m_bNeedUpdate;
	bool  m_bLoading;
	bool  m_bRegisteredEvents;
	bool  m_bShowOnReady;
	bool  m_bIsLoaded;
	bool  m_bInitialized;

	char  m_Padding[0x1C8];

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
};

#endif // SFBUYMENU_H