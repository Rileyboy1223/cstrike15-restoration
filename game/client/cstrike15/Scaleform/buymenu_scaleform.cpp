//fbr

#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "ienginevgui.h"
#include "buymenu_scaleform.h"
#include "gameui_util.h"
#include "inputsystem/iinputsystem.h"
#include "../gameui/cstrike15/cstrike15basepanel.h"

#include <keyvalues.h>

#include <vgui_controls/EditablePanel.h>

#include "HUD/sfhudinfopanel.h"

#include "c_cs_player.h"
#include "cs_ammodef.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( OnCancel ),
	SFUI_DECL_METHOD( InitWeapon ),
	SFUI_DECL_METHOD( GetWeaponPriceScript ),
	SFUI_DECL_METHOD( GetWeaponPriceFromIDScript ),
	SFUI_DECL_METHOD( GetPlayerLoadout ),
	SFUI_DECL_METHOD( GetPlayerBuyMenuLoadout ),
	SFUI_DECL_METHOD( AreWeaponsFree ),
SFUI_END_GAME_API_DEF( CCSBuyMenuScaleform, BuyMenu );

static BuyMenuWeaponSlot s_defaultLoadoutWeapons[2][4] =
{
	{
		{ 1, 0 },
		{ 2, 0 },
		{ 3, 0 },
		{ 4, 0 }
	},

	{
		{ 5, 0 },
		{ 6, 0 },
		{ 7, 0 },
		{ 8, 0 }
	}
};

BuyMenuWeaponData g_LoadoutArray[2][4];

void InitBuyMenuLoadoutData()
{
	Msg("Init BuyMenu Loadout Data");
	for (int team = 0; team < 2; ++team)
	{
		for (int i = 0; i < 4; ++i)
		{
			BuyMenuWeaponData& data = g_LoadoutArray[team][i];

			data.m_iWeaponDefIndex = s_defaultLoadoutWeapons[team][i].m_iWeaponDefIndex;

			data.m_iSlot = s_defaultLoadoutWeapons[team][i].m_iSlot;

			data.m_iTeam = 0;
			data.m_iPrice = 0;

			data.m_szWeaponName[0] = '\0';

			data.m_Unknown1 = 0x2C;
			data.m_Unknown2 = 0x2B00000001ULL;
			data.m_Unknown3 = 0x100000000ULL;

			data.m_Unknown4 = 0x2D;
			data.m_Unknown5 = 1;
			data.m_Unknown6 = 0x100000000ULL;
		}
	}
}

CCSBuyMenuScaleform::CCSBuyMenuScaleform( CounterStrikeViewport* pViewport ) :
	m_bVisible( false ),
	m_bInitialized( false ),
	m_bLoading( true ),
	m_bIsLoaded( false ),
	m_bRegisteredEvents( false ),
	m_pViewport( pViewport ),
	m_iSelectedWeapon( -1 ),
	m_iPrimaryWeapon( 0 ),
	m_iSecondaryWeapon( 0 ),
	m_iArmorValue( 0 ),
	m_iCurrentMoney( 0 ),
	m_iUnknownState( -1 )
{
	m_iSplitScreenSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	std::memset( m_Padding, 0, sizeof( m_Padding ) );

	bool shouldInit = false;

	for (int i = 0; i < 2 && shouldInit; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			if (g_LoadoutArray[i][j].m_iWeaponDefIndex != 0 &&
				g_LoadoutArray[i][j].m_iSlot != 0)
			{
				shouldInit = true;
				break;
			}
		}
	}

	if ( shouldInit )
	{
		InitBuyMenuLoadoutData();
	}

	vgui::VPANEL pRoot = enginevgui->GetPanel( PANEL_CLIENTDLL );

	m_pItemPanelParent = new vgui::EditablePanel( pViewport, "buymenu_itempanel_parent" );
	m_pItemPanelParent->MakeReadyForUse();
	m_pItemPanelParent->LoadControlSettings( "Resource/UI/econ/buymenu_itempanel_parent.res" );

	vgui::Panel* m_pItemModelPanel = m_pItemPanelParent->FindChildByName( "buymenu_itempanel", false );

	if ( m_pItemModelPanel )
	{
		m_pItemModelPanel->SetParent( pRoot );
		m_pItemModelPanel->SetVisible( false );
		m_pItemModelPanel->SetMouseInputEnabled( false );
	}

	ListenForGameEvent( "round_start" );
}

CCSBuyMenuScaleform::~CCSBuyMenuScaleform()
{
	if (m_bRegisteredEvents)
	{
		if (gameeventmanager)
		{
			gameeventmanager->RemoveListener(this);
		}

		m_bRegisteredEvents = false;
	}

	// Base class destruction begins
	m_iSplitScreenSlot = 13;

	// Release Scaleform movie if loaded
	if (m_bIsLoaded)
	{
		//m_pScaleformUI->RemoveElement(m_iFlashHandle, m_FlashAPI);
	}
}

void CCSBuyMenuScaleform::SetPlayerIsCT( const bool isCT )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	if ( FlashAPIIsValid() )
	{
		SFVALUE value = m_pScaleformUI->CreateValue( 0 );

		m_pScaleformUI->Value_SetValue( value, isCT );
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setPlayerIsCT", value, 1 );
		m_pScaleformUI->ReleaseValue( value );
	}
}

void CCSBuyMenuScaleform::FlashReady()
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	C_CSPlayer *LocalCSPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( LocalCSPlayer )
	{
		SetPlayerIsCT(LocalCSPlayer->GetTeamNumber() == 3);
	}

	CalculateBestStats();

	ListenForGameEvent( "cs_game_disconnected" );
	ListenForGameEvent( "player_team" );
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "item_equip" );

	m_bLoading = false;

	if ( m_bVisible )
	{
		Show();
	}
	else
	{
		Hide();
	}
}

void CCSBuyMenuScaleform::Show()
{
	// Register for radial input
	g_pInputSystem->SetSteamControllerMode( "RadialControls", this );

	// If the movie hasn't been loaded yet, start loading it.
	if ( m_bLoading )
	{
		if ( !FlashAPIIsValid() )
		{
			m_bLoading = true;
			SFUI_REQUEST_ELEMENT( SF_SS_SLOT( m_iSplitScreenSlot ), g_pScaleformUI, CCSBuyMenuScaleform, this, BuyMenu );
		}

		m_bVisible = true;
		return;
	}

	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	m_bNeedUpdate = false;
	m_iSelectedCategory = 0;
	m_iCurrentRadialSelection = 0;

	UpdatePlayerCash( true );
	UpdateTimeLeft( true );

#if defined( CEG_ALLOW_BUYMENU )
	if (true)
#endif
	{
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", 0, NULL );

		m_iWheelSelection = -1;

		GetHud().DisableHud();

		C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pPlayer )
			pPlayer->SetBuyMenuOpen( true );

		bool bHostageMap = false;
		if ( CSGameRules() )
			bHostageMap = CSGameRules()->IsHostageRescueMap();

		SetHostageMatch( bHostageMap );

		m_bVisible = true;
	}
}

void CCSBuyMenuScaleform::Hide()
{
	// Unregister radial controls
	g_pInputSystem->SetSteamControllerMode( NULL, this );

	if ( !m_bLoading && FlashAPIIsValid() && m_bVisible )
	{
		SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", nullptr, 0 );

		GetHud().EnableHud();
	}

	m_bVisible = false;

	// Clear selected weapon model
	//if ( m_pWeaponModelPanel )
		//m_pWeaponModelPanel->SetWeapon( nullptr );

	{
		SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

		C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pPlayer )
			pPlayer->SetBuyMenuOpen( false );
	}
}

void CCSBuyMenuScaleform::FlashLoaded( void )
{

}

struct WeaponStatRange
{
	float maxDamage;      // +0x00
	float minDamage;      // +0x04

	float maxROF;         // +0x08
	float minROF;         // +0x0C

	float maxAccuracy;    // +0x10
	float minAccuracy;    // +0x14

	float maxMobility;    // +0x18
	float minMobility;    // +0x1C
};


void CCSBuyMenuScaleform::CalculateBestStats()
{
	WeaponStatRange *pSecondaryStats = reinterpret_cast<WeaponStatRange*>(reinterpret_cast<char*>(this) + 0x264);
	WeaponStatRange *pPrimaryStats = reinterpret_cast<WeaponStatRange*>(reinterpret_cast<char*>(this) + 0x284);

	for (int i = 1; i < WEAPON_MAX; ++i)
	{
		// Assembly skips weapon id 31
		if (i == 31)
			continue;

		const CCSWeaponInfo *pInfo = GetWeaponInfo((CSWeaponID)i);
		if (!pInfo)
			continue;

		if (!IsGunWeapon((CSWeaponType)pInfo->GetWeaponType(nullptr)))
		{
			continue;
		}

		bool bKnife = pInfo->GetWeaponType(nullptr) == WEAPONTYPE_KNIFE;

		WeaponStatRange *pStats = IsPrimaryWeapon((CSWeaponID)i) ? pPrimaryStats : pSecondaryStats;

		float damage = (float)pInfo->GetDamage(nullptr, 0, 1.0f);
		float armorDamage = pInfo->GetArmorRatio(nullptr, 0, 1.0f) * damage;

		pStats->maxDamage = MAX(pStats->maxDamage, armorDamage);
		pStats->minDamage = MIN(pStats->minDamage, armorDamage);

		float cycleTime = pInfo->GetCycleTime(nullptr, 0, 1.0f);

		if (cycleTime > 0.0f)
		{
			float rof = 1.0f / cycleTime;

			pStats->maxROF = MAX(pStats->maxROF, rof);
			pStats->minROF = MIN(pStats->minROF, rof);
		}

		float maxSpeed;

		if (bKnife)
		{
			maxSpeed = pInfo->GetMaxSpeed(nullptr, 1, 1.0f);
		}
		else
		{
			maxSpeed = pInfo->GetMaxSpeed(nullptr, 0, 1.0f);
		}

		pStats->maxMobility = MAX(pStats->maxMobility, maxSpeed);
		pStats->minMobility = MIN(pStats->minMobility, maxSpeed);

		float inaccuracy;

		if (bKnife)
		{
			inaccuracy = pInfo->GetInaccuracyStand(nullptr, 1, 0.0f);
		}
		else
		{
			inaccuracy = pInfo->GetInaccuracyStand(nullptr, 0, 0.0f);
		}

		float spread;

		if (bKnife)
		{
			spread = pInfo->GetSpread(nullptr, 1, 0.0f);
		}
		else
		{
		spread = pInfo->GetSpread(nullptr, 0, 	0.0f);}

		float accuracy = 100.0f / (spread + inaccuracy);
		pStats->maxAccuracy = MAX(pStats->maxAccuracy, accuracy);
		pStats->minAccuracy = MIN(pStats->minAccuracy, accuracy);
	}
}

bool CCSBuyMenuScaleform::PreUnloadFlash()
{
	// WIP
	return true;
}

void CCSBuyMenuScaleform::SetHostageMatch( bool bHostageMatch )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	if ( m_bIsLoaded )
	{
		SFVALUEARRAY args;

		m_pScaleformUI->CreateValueArray( args, 1 );
		m_pScaleformUI->ValueArray_SetElement( args, true, bHostageMatch );
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setIsHostageMatch", args, 1 );
	}
}

void CCSBuyMenuScaleform::ShowPanel( bool state )
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

void CCSBuyMenuScaleform::OnCancel( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	C_BasePlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	int nUserID = -1;
	if ( pPlayer )
	{
		nUserID = pPlayer->GetUserID();
	}

	CSGameRules()->CloseBuyMenu( nUserID );
}

void CCSBuyMenuScaleform::InitWeapon( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	CCSBuyMenuLoadout loadout;
	C_CSPlayer* pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	int loadoutSlot = -1;
	
	if ( pui->Params_GetArgAsNumber( obj ) != 0.0 )
		loadoutSlot = (int)pui->Params_GetArgAsNumber( obj );

	int team = pPlayer->GetTeamNumber();

	const C_EconItemView* pItem = nullptr;

	// Resolve inventory item for loadout slot
	if ( loadoutSlot >= 0 && loadoutSlot < 57 )
	{
		uint64 itemID = loadout.m_LoadoutItems[loadoutSlot];
	
		if ( itemID != INVALID_ITEM_ID )
		{
			CCSInventoryManager *pInv = CSInventoryManager();
	
			if ( pInv && pInv->GetLocalInventory() )
			{
				pItem = pInv->GetLocalInventory()->GetInventoryItemByItemID( itemID );
	
				if ( !pItem || !pItem->IsValid() )
				{
					if ( ( itemID >> 60 ) == 15 )
					{
						pItem = pInv->FindOrCreateReferenceEconItem( itemID );
					}
					else
					{
						pItem = pInv->GetItemInLoadoutForTeam( team, loadoutSlot, false );
					}
				}
			}
		}
	}

	// Translate buy-menu slot -> weapon ID
	//if ( (unsigned)(loadoutSlot - 26) > 11 )
		//return;

	CSWeaponID weaponID = WEAPON_NONE;

	switch ( loadoutSlot )
	{
	case LOADOUT_POSITION_EQUIPMENT0: weaponID = CSGameRules()->IsPlayingCoopMission() ? ITEM_ASSAULTSUIT : ITEM_KEVLAR;
		break;

	case LOADOUT_POSITION_EQUIPMENT1: weaponID = CSGameRules()->IsPlayingCoopMission() ? ITEM_HEAVYASSAULTSUIT : ITEM_ASSAULTSUIT;
		break;
	
	case LOADOUT_POSITION_EQUIPMENT2: weaponID = WEAPON_TASER;
		break;
	
	case LOADOUT_POSITION_EQUIPMENT3: weaponID = CSGameRules()->IsHostageRescueMap() ? ITEM_CUTTERS : ITEM_DEFUSER;
		break;
	}

	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( weaponID );

	if ( !pWeaponInfo )
		return;

	CSWeaponType weaponType = pWeaponInfo->GetWeaponType( pItem );

	int ammoType = pWeaponInfo->GetPrimaryAmmoType( pItem );
	int clipSize = pWeaponInfo->GetPrimaryClipSize( pItem );
	int maxCarry = GetCSAmmoDef()->MaxCarry( pWeaponInfo->GetPrimaryAmmoType(), pPlayer );

	// Create Scaleform object
	int weaponObj = 0;
	SFVALUE weaponData = m_pScaleformUI->CreateNewObject( weaponObj );

	/*const wchar_t* localizedName = g_pVGuiLocalize->Find( pWeaponInfo->szPrintName );*/

	//if ( pItem && pItem->IsValid() )
	//	localizedName = pItem->GetItemName();

	/*SFVALUE nameValue = m_pScaleformUI->CreateNewString( weaponObj, localizedName );*/
	SFVALUE nameValue = m_pScaleformUI->CreateNewString( weaponObj, pItem->GetItemName() );

	// Weapon type string
	m_pScaleformUI->Value_SetMember( weaponData, "maxCarry", 1 );

	if ( weaponID == WEAPON_TASER )
	{
		m_pScaleformUI->Value_SetMember( weaponData, "weaponType", "taser" );
	}
	else
	{
		switch ( weaponType )
		{
		case WEAPONTYPE_PISTOL:
			m_pScaleformUI->Value_SetMember( weaponData, "weaponType", "secondary" );
			break;

		case WEAPONTYPE_GRENADE:
			m_pScaleformUI->Value_SetMember( weaponData, "weaponType", "equipment" );

			m_pScaleformUI->Value_SetMember( weaponData, "maxCarry", maxCarry );
			break;

		case WEAPONTYPE_C4:
			m_pScaleformUI->Value_SetMember( weaponData, "weaponType", "equipment" );
			break;

		default:
			m_pScaleformUI->Value_SetMember( weaponData, "weaponType", "primary" );
			break;
		}
	}

	// Item ID
	uint64 itemID = (uint64)-1;

	if ( pItem && pItem->IsValid() )
		itemID = pItem->GetItemID();

	char itemIDBuf[256];
	V_sprintf_safe(itemIDBuf, "%llu", itemID);

	m_pScaleformUI->Value_SetMember( weaponData, "itemid", itemIDBuf );
	m_pScaleformUI->Value_SetMember( weaponData, "wepid", weaponID );

	// Price
	int price = -1;

	if ( pItem || weaponID )
	{
		price = 0;

		if ( C_CSPlayer* local = C_CSPlayer::GetLocalCSPlayer() )
		{
			price = local->GetWeaponPrice( weaponID );
		}
	}

	m_pScaleformUI->Value_SetMember( weaponData, "price", price );
	m_pScaleformUI->Value_SetMember( weaponData, "name", nameValue );
	m_pScaleformUI->Value_SetMember( weaponData, "clipSize", clipSize );
	m_pScaleformUI->Value_SetMember( weaponData, "maxRounds", GetAmmoDef()->MaxCarry( ammoType, pPlayer ) );

	// Stat bars
	m_pScaleformUI->Value_SetMember( weaponData, "armorPen", pWeaponInfo->GetArmorRatio( pItem ) * 50.0f );
	m_pScaleformUI->Value_SetMember( weaponData, "penetration", pWeaponInfo->GetPenetration( pItem ) );

	// Deathmatch points
	int dmScore = CSGameRules()->GetWeaponScoreForDeathmatch( weaponID );

	char dmBuf[64];
	V_snprintf( dmBuf, sizeof(dmBuf), " %d", dmScore );

	m_pScaleformUI->Value_SetMember( weaponData, "DMPoints", dmBuf );

	// Kill reward
	int killAward = pWeaponInfo->GetKillAward( pItem );
	int baseCash = 300;

	if ( CSGameRules() )
	{
		//baseCash = CSGameRules()->PlayerCashAwardValue( killAward );
	}

	char killBuf[64];
	V_snprintf( killBuf, sizeof( killBuf ), " %.2f", (float)killAward / (float)baseCash );

	m_pScaleformUI->Value_SetMember( weaponData, "KillAward", killBuf );

	// Fire rate
	float cycleTime = pWeaponInfo->GetCycleTime();
	float roundsPerSecond = cycleTime > 0.0f ? 1.0f / cycleTime : 0.0f;

	//m_pScaleformUI->Value_SetMember( weaponData, "fireRate", ComputeStatBar( roundsPerSecond, statTable ) );

	// Movement + accuracy
	float maxSpeed;
	float spread;
	float inaccuracy;

	if (weaponType == WEAPONTYPE_SNIPER_RIFLE)
	{
		maxSpeed = pWeaponInfo->GetMaxSpeed( pItem, true );
		inaccuracy = pWeaponInfo->GetInaccuracyStand( pItem, true );
		spread = pWeaponInfo->GetSpread( pItem, true );
	}
	else
	{
		maxSpeed = pWeaponInfo->GetMaxSpeed( pItem, false );
		inaccuracy = pWeaponInfo->GetInaccuracyStand( pItem, false );
		spread = pWeaponInfo->GetSpread( pItem, false );
	}

	//m_pScaleformUI->Value_SetMember( weaponData, "moveRate", ComputeStatBar( maxSpeed, statTable ) );
	//m_pScaleformUI->Value_SetMember( weaponData, "inaccuracy", ComputeStatBar( 100.0f / ( inaccuracy + spread ), statTable ) );

	// Store object in Scaleform array
	m_pScaleformUI->Params_SetResult( obj, weaponData );

	if ( nameValue )
		SafeReleaseSFVALUE( nameValue );

	if ( weaponData )
		SafeReleaseSFVALUE( weaponData );

	return;
}

void CCSBuyMenuScaleform::GetWeaponPriceScript( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	int nLoadoutPos = ( int )pui->Params_GetArgAsNumber( obj );

	int nWeaponID = WEAPON_NONE;
	bool bInvalid = true;

	int nTeam = pPlayer->GetTeamNumber();

	if ( nLoadoutPos <= 56 )
	{
		CCSPlayerInventory *pInventory = CSInventoryManager() ? CSInventoryManager()->GetLocalCSInventory() : nullptr;

		if ( pInventory )
		{
			itemid_t itemID = m_PlayerBuyMenuLoadout.m_WeaponID[nLoadoutPos];

			if ( itemID != INVALID_ITEM_ID )
			{
				CEconItemView *pItem = pInventory->GetInventoryItemByItemID( itemID );

				if ( !pItem || !pItem->IsValid() )
				{
					if ( CombinedItemIdIsDefIndexAndPaint( itemID ) )
					{
						pItem = CSInventoryManager()->FindOrCreateReferenceEconItem( itemID );
					}
					else
					{
						pItem = CSInventoryManager()->GetItemInLoadoutForTeam( nTeam, nLoadoutPos );
					}
				}

				if ( pItem && pItem->IsValid() )
				{
					const CEconItemDefinition *pDef = pItem->GetStaticData();

					nWeaponID = WeaponIdFromString( pDef->GetDefinitionName() );

					bInvalid = false;
				}
			}
		}
	}

	int nPrice = -1;

	if ( !bInvalid || nWeaponID )
	{
		if ( C_CSPlayer *pLocal = C_CSPlayer::GetLocalCSPlayer() )
		{
			nPrice = pLocal->GetWeaponPrice(static_cast<CSWeaponID>( nWeaponID ));
		}
		else
		{
			nPrice = 0;
		}
	}

	pui->Params_SetResult( obj, nPrice );
}

void CCSBuyMenuScaleform::GetWeaponPriceFromIDScript( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	if ( !C_CSPlayer::GetLocalCSPlayer() )
		return;

	CSWeaponID weaponID = static_cast<CSWeaponID>( static_cast<int>( m_pScaleformUI->Params_GetArgAsNumber( obj, 0 ) ) );

	int nPrice = -1;

	if ( weaponID != WEAPON_NONE )
	{
		C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

		nPrice = 0;

		if ( pPlayer )
		{
			nPrice = pPlayer->GetWeaponPrice( weaponID );
		}
	}

	pui->Params_SetResult( obj, nPrice );
}

void CCSBuyMenuScaleform::GetPlayerBuyMenuLoadout( SCALEFORM_CALLBACK_ARGS_DECL )
{
	CCSBuyMenuLoadout tempLoadout;
	memset( &tempLoadout, 0, sizeof( tempLoadout ) );

	if ( FillInPlayerBuyMenuLoadout( &tempLoadout ) )
	{
		memcpy( &m_PlayerBuyMenuLoadout, &tempLoadout, sizeof( tempLoadout ) );
	}

	SFVALUE pFlashLoadout = CreateFlashBuyMenuLoadout( m_PlayerBuyMenuLoadout );

	pui->Params_SetResult( obj, pFlashLoadout );

	if ( pFlashLoadout )
	{
		m_pScaleformUI->ReleaseValue( pFlashLoadout );
	}
}

void CCSBuyMenuScaleform::GetPlayerLoadout( SCALEFORM_CALLBACK_ARGS_DECL )
{
	UpdatePlayerLoadout( true, false );

	SFVALUE flashLoadout = CreateFlashLoadout( m_PlayerLoadout );

	C_CSPlayer* pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( pLocalPlayer && pLocalPlayer->IsAlive() && !CSGameRules()->IsArmorFree() && pLocalPlayer->ArmorValue() > 0 )
	{
		m_pScaleformUI->Value_SetMember( flashLoadout, "armor", pLocalPlayer->ArmorValue() );
	}

	pui->Params_SetResult( obj, flashLoadout );

	SafeReleaseSFVALUE( flashLoadout );
}

void CCSBuyMenuScaleform::AreWeaponsFree( SCALEFORM_CALLBACK_ARGS_DECL )
{
	bool IsPlayingGunGameDeathmatch = CSGameRules()->IsPlayingGunGameDeathmatch();

	//Msg("Gamemode is GunGame or Deathmatch: %s \n", IsPlayingGunGameDeathmatch ? "Yes, weapons are free!" : "No, sorry bro you gotta pay for your weapons!");

	return pui->Params_SetResult( obj, IsPlayingGunGameDeathmatch );
}

bool CCSBuyMenuScaleform::SetBuyMenuWeaponSliceEntry(uint32 weaponPosition, SFVALUE flashArray, uint32 arrayIndex, CCSBuyMenuLoadout &loadout)
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if (!pLocalPlayer)
		return false;

	SFVALUE flashObject = CreateFlashObject();

	m_pScaleformUI->Value_SetMember(flashObject, "sliceType", "weapon");

	uint64 itemID = weaponPosition;

	//Msg("ID: %s\n", itemID ? "" : "");

	//-------------------------------------------------------------------------
	// Equipped weapon
	//-------------------------------------------------------------------------
	if (itemID != INVALID_ITEM_ID)
	{
		C_EconItemView *pItem = nullptr;

		auto *pInventory = CSInventoryManager()->GetLocalInventory();

		if ((itemID >> 60) == 0xF)
		{
			pItem = CSInventoryManager()->FindOrCreateReferenceEconItem(itemID);
		}
		else if (pInventory)
		{
			pItem = pInventory->GetInventoryItemByItemID(itemID);
		}

		if (pItem)
		{
			char itemIDString[256];
			V_snprintf(itemIDString, sizeof(itemIDString), "%llu", itemID);

			m_pScaleformUI->Value_SetMember(flashObject, "weapon_itemid", itemIDString);
			m_pScaleformUI->Value_SetMember(flashObject, "weapon_position", (int)weaponPosition);

			const CEconItemDefinition *pDef = pItem->GetStaticData();

			const char *pszClassname = pDef ? pDef->GetItemClass() : "";

			if (IsWeaponClassname(pszClassname))
				pszClassname += 7;

			m_pScaleformUI->Value_SetMember(flashObject, "weapon", pszClassname);
		}
	}

	//-------------------------------------------------------------------------
	// Prohibited replacement weapon
	//-------------------------------------------------------------------------
	auto *pInventory = CSInventoryManager()->GetLocalInventory();

	if (pInventory)
	{
		C_EconItemView *pProhibitedItem = pInventory->GetItemInLoadout(pLocalPlayer->GetTeamNumber(), weaponPosition);

		if (pProhibitedItem)
		{
			if (CSGameRules()->IsWeaponAllowed(nullptr, pLocalPlayer->GetTeamNumber(), pProhibitedItem))
			{
				const CEconItemDefinition *pDef = pProhibitedItem->GetStaticData();

				if (pDef)
				{
					C_EconItemView *pReference = CSInventoryManager()->FindOrCreateReferenceEconItem(pDef->GetDefinitionIndex(), 0, 0);

					if (pReference)
					{
						uint64 prohibitedID = pReference->GetFauxItemIDFromDefinitionIndex();

						char prohibitedString[256];

						V_snprintf(prohibitedString, sizeof(prohibitedString), "%llu", prohibitedID);

						m_pScaleformUI->Value_SetMember(flashObject, "weapon_prohibiteditemid", prohibitedString);
					}
				}
			}
		}
	}

	m_pScaleformUI->Value_SetArrayElement(flashArray, arrayIndex, flashObject);

	SafeReleaseSFVALUE(flashObject);

	return true;
}

SFVALUE CCSBuyMenuScaleform::CreateFlashBuyMenuLoadout(CCSBuyMenuLoadout &loadout)
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if (!pPlayer)
		return SFVALUE();

	SFVALUE rootMenu = CreateFlashObject();

	// MAIN MENU

	SFVALUE mainMenu = CreateFlashObject();
	m_pScaleformUI->Value_SetMember( mainMenu, "centerText", "#SFUI_BuyMenu_SelectWeapon" );
	m_pScaleformUI->Value_SetMember( mainMenu, "menuType", "parent" );

	SFVALUE slices = CreateFlashArray( 24 );

	SFVALUE pistolSlice = CreateFlashObject();
	m_pScaleformUI->Value_SetMember(pistolSlice, "sliceType", "subMenu");
	m_pScaleformUI->Value_SetMember(pistolSlice, "name", "#SFUI_BuyMenu_Pistols");
	m_pScaleformUI->Value_SetMember(pistolSlice, "image", "glock");
	m_pScaleformUI->Value_SetMember(pistolSlice, "subMenu", "PistolMenu");
	m_pScaleformUI->Value_SetArrayElement(slices, 0, pistolSlice);

	SFVALUE heavySlice = CreateFlashObject();
	m_pScaleformUI->Value_SetMember(heavySlice, "sliceType", "subMenu");
	m_pScaleformUI->Value_SetMember(heavySlice, "name", "#SFUI_BuyMenu_HeavyWeapons");
	m_pScaleformUI->Value_SetMember(heavySlice, "image", "m249");
	m_pScaleformUI->Value_SetMember(heavySlice, "subMenu", "HeavyMenu");
	m_pScaleformUI->Value_SetArrayElement(slices, 1, heavySlice);

	SFVALUE smgSlice = CreateFlashObject();
	m_pScaleformUI->Value_SetMember(smgSlice, "sliceType", "subMenu");
	m_pScaleformUI->Value_SetMember(smgSlice, "name", "#SFUI_BuyMenu_SMGs");
	m_pScaleformUI->Value_SetMember(smgSlice, "image", "p90");
	m_pScaleformUI->Value_SetMember(smgSlice, "subMenu", "SMGMenu");
	m_pScaleformUI->Value_SetArrayElement(slices, 2, smgSlice);

	SFVALUE rifleSlice = CreateFlashObject();
	m_pScaleformUI->Value_SetMember(rifleSlice, "sliceType", "subMenu");
	m_pScaleformUI->Value_SetMember(rifleSlice, "name", "#SFUI_BuyMenu_Rifles");
	m_pScaleformUI->Value_SetMember(rifleSlice, "image", "m4a1");
	m_pScaleformUI->Value_SetMember(rifleSlice, "subMenu", "RifleMenu");
	m_pScaleformUI->Value_SetArrayElement(slices, 3, rifleSlice);

	SFVALUE equipmentSlice = CreateFlashObject();
	m_pScaleformUI->Value_SetMember(equipmentSlice, "sliceType", "subMenu");
	m_pScaleformUI->Value_SetMember(equipmentSlice, "name", "#SFUI_BuyMenu_Equipment");
	m_pScaleformUI->Value_SetMember(equipmentSlice, "image", "equipment");
	m_pScaleformUI->Value_SetMember(equipmentSlice, "subMenu", "EquipmentMenu");
	m_pScaleformUI->Value_SetArrayElement(slices, 4, equipmentSlice);

	SFVALUE grenadeSlice = CreateFlashObject();
	m_pScaleformUI->Value_SetMember(grenadeSlice, "sliceType", "subMenu");
	m_pScaleformUI->Value_SetMember(grenadeSlice, "name", "#SFUI_BuyMenu_Grenades");
	m_pScaleformUI->Value_SetMember(grenadeSlice, "image", "grenades");
	m_pScaleformUI->Value_SetMember(grenadeSlice, "subMenu", "GrenadeMenu");
	m_pScaleformUI->Value_SetArrayElement(slices, 5, grenadeSlice);

	m_pScaleformUI->Value_SetMember(mainMenu, "Slices", slices);
	m_pScaleformUI->Value_SetMember(rootMenu, "MainMenu", mainMenu);

	// PISTOL MENU

	{

		SFVALUE pistolMenu = CreateFlashObject();
		SFVALUE pistolSlices = CreateFlashArray( 6 );

		m_pScaleformUI->Value_SetMember(pistolMenu, "centerText", "#SFUI_BuyMenu_Pistols");
		m_pScaleformUI->Value_SetMember(pistolMenu, "menuType", "weapon");

		SetBuyMenuWeaponSliceEntry(2, pistolSlices, 0, loadout);
		SetBuyMenuWeaponSliceEntry(3, pistolSlices, 1, loadout);
		SetBuyMenuWeaponSliceEntry(4, pistolSlices, 2, loadout);
		SetBuyMenuWeaponSliceEntry(5, pistolSlices, 3, loadout);
		SetBuyMenuWeaponSliceEntry(6, pistolSlices, 4, loadout);
		SetBuyMenuWeaponSliceEntry(7, pistolSlices, 5, loadout);

		m_pScaleformUI->Value_SetMember(pistolMenu, "Slices", pistolSlices);
		m_pScaleformUI->Value_SetMember(rootMenu, "PistolMenu", pistolMenu);
	}

	// SMG MENU

	{

		SFVALUE smgMenu = CreateFlashObject();
		SFVALUE smgSlices = CreateFlashArray( 6 );

		m_pScaleformUI->Value_SetMember(smgMenu, "centerText", "#SFUI_BuyMenu_SMGs");
		m_pScaleformUI->Value_SetMember(smgMenu, "menuType", "weapon");

		SetBuyMenuWeaponSliceEntry(8, smgSlices, 0, loadout);
		SetBuyMenuWeaponSliceEntry(9, smgSlices, 1, loadout);
		SetBuyMenuWeaponSliceEntry(10, smgSlices, 2, loadout);
		SetBuyMenuWeaponSliceEntry(11, smgSlices, 3, loadout);
		SetBuyMenuWeaponSliceEntry(12, smgSlices, 4, loadout);
		SetBuyMenuWeaponSliceEntry(13, smgSlices, 5, loadout);

		m_pScaleformUI->Value_SetMember(smgMenu, "Slices", smgSlices);
		m_pScaleformUI->Value_SetMember(rootMenu, "SMGMenu", smgMenu);
	}

	// RIFLE MENU

	{

		SFVALUE rifleMenu = CreateFlashObject();
		SFVALUE rifleSlices = CreateFlashArray( 6 );

		m_pScaleformUI->Value_SetMember(rifleMenu, "centerText", "#SFUI_BuyMenu_Rifles");
		m_pScaleformUI->Value_SetMember(rifleMenu, "menuType", "weapon");

		SetBuyMenuWeaponSliceEntry(14, rifleSlices, 0, loadout);
		SetBuyMenuWeaponSliceEntry(15, rifleSlices, 1, loadout);
		SetBuyMenuWeaponSliceEntry(16, rifleSlices, 2, loadout);
		SetBuyMenuWeaponSliceEntry(17, rifleSlices, 3, loadout);
		SetBuyMenuWeaponSliceEntry(18, rifleSlices, 4, loadout);
		SetBuyMenuWeaponSliceEntry(19, rifleSlices, 5, loadout);

		m_pScaleformUI->Value_SetMember(rifleMenu, "Slices", rifleSlices);
		m_pScaleformUI->Value_SetMember(rootMenu, "RifleMenu", rifleMenu);
	}

	// HEAVY MENU

	{

		SFVALUE heavyMenu = CreateFlashObject();
		SFVALUE heavySlices = CreateFlashArray( 6 );

		m_pScaleformUI->Value_SetMember(heavyMenu, "centerText", "#SFUI_BuyMenu_HeavyWeapons");
		m_pScaleformUI->Value_SetMember(heavyMenu, "menuType", "weapon");

		SetBuyMenuWeaponSliceEntry(20, heavySlices, 0, loadout);
		SetBuyMenuWeaponSliceEntry(21, heavySlices, 1, loadout);
		SetBuyMenuWeaponSliceEntry(22, heavySlices, 2, loadout);
		SetBuyMenuWeaponSliceEntry(23, heavySlices, 3, loadout);
		SetBuyMenuWeaponSliceEntry(24, heavySlices, 4, loadout);
		SetBuyMenuWeaponSliceEntry(25, heavySlices, 5, loadout);

		m_pScaleformUI->Value_SetMember(heavyMenu, "Slices", heavySlices);
		m_pScaleformUI->Value_SetMember(rootMenu, "HeavyMenu", heavyMenu);
	}

	// EQUIPMENT MENU

	{

		SFVALUE equipmentMenu = CreateFlashObject();
		SFVALUE equipmentSlices = CreateFlashArray( 4 );

		m_pScaleformUI->Value_SetMember(equipmentMenu, "centerText", "#SFUI_BuyMenu_Equipment");
		m_pScaleformUI->Value_SetMember(equipmentMenu, "menuType", "weapon");
		m_pScaleformUI->Value_SetMember(equipmentMenu, "autoReturn", "false");

		SFVALUE armorObj = CreateFlashObject();

		bool coop = CSGameRules()->IsPlayingCoopMission();
		m_pScaleformUI->Value_SetMember(armorObj, "sliceType", "equipment");
		if (coop)
		{
			m_pScaleformUI->Value_SetMember(armorObj, "weapon", "assaultsuit");
			m_pScaleformUI->Value_SetMember(armorObj, "weapon_position", 32);
			m_pScaleformUI->Value_SetArrayElement(equipmentSlices, 0, armorObj);
			SafeReleaseSFVALUE(armorObj);
		}
		else
		{
			m_pScaleformUI->Value_SetMember(armorObj, "weapon", "kevlar");
			m_pScaleformUI->Value_SetMember(armorObj, "weapon_position", 32);
			m_pScaleformUI->Value_SetArrayElement(equipmentSlices, 0, armorObj);
			SafeReleaseSFVALUE(armorObj);
		}

		SFVALUE suitObj = CreateFlashObject();
		m_pScaleformUI->Value_SetMember(suitObj, "sliceType", "equipment");
		const char* sutiName = coop ? "heavyassaultsuit" : "assaultsuit";
		m_pScaleformUI->Value_SetMember(suitObj, "weapon", sutiName);
		m_pScaleformUI->Value_SetMember(suitObj, "weapon_position", 33);
		m_pScaleformUI->Value_SetArrayElement(equipmentSlices, 1, suitObj);
		SafeReleaseSFVALUE(suitObj);

		SFVALUE taserObj = CreateFlashObject();
		m_pScaleformUI->Value_SetMember(taserObj, "sliceType", "equipment");
		m_pScaleformUI->Value_SetMember(taserObj, "weapon", "taser");
		m_pScaleformUI->Value_SetMember(taserObj, "weapon_position", 34);
		m_pScaleformUI->Value_SetArrayElement(equipmentSlices, 2, taserObj);
		SafeReleaseSFVALUE(taserObj);

		SFVALUE weaponObj = CreateFlashObject();
		SFVALUE weaponObj2 = CreateFlashObject();
		m_pScaleformUI->Value_SetMember(weaponObj, "sliceType", "equipment");
		m_pScaleformUI->Value_SetMember(weaponObj2, "ct_hostage", "cutters");
		m_pScaleformUI->Value_SetMember(weaponObj2, "ct", "defuser");
		m_pScaleformUI->Value_SetMember(weaponObj2, "t", "null");
		m_pScaleformUI->Value_SetMember(weaponObj, "weapon", weaponObj2);
		m_pScaleformUI->Value_SetMember(weaponObj, "weapon_position", 35);
		m_pScaleformUI->Value_SetArrayElement(equipmentSlices, 3, weaponObj);
		SafeReleaseSFVALUE(weaponObj2);
		SafeReleaseSFVALUE(weaponObj);

		m_pScaleformUI->Value_SetMember(equipmentMenu, "Slices", equipmentSlices);
		m_pScaleformUI->Value_SetMember(rootMenu, "EquipmentMenu", equipmentMenu);
		SafeReleaseSFVALUE(equipmentMenu);
		SafeReleaseSFVALUE(equipmentSlices);
	}

	// GRENADE MENU

	{
		bool coop = CSGameRules()->IsPlayingCoopMission();

		SFVALUE grenadeMenu = CreateFlashObject();
		SFVALUE grenadeSlices = CreateFlashArray( 5 );

		m_pScaleformUI->Value_SetMember(grenadeMenu, "centerText", "#SFUI_BuyMenu_Grenades");
		m_pScaleformUI->Value_SetMember(grenadeMenu, "menuType", "weapon");
		m_pScaleformUI->Value_SetMember(grenadeMenu, "autoReturn", "false");

		SFVALUE fireGrenadeObj = CreateFlashObject();
		SFVALUE fireObj = CreateFlashObject();
		m_pScaleformUI->Value_SetMember(fireGrenadeObj, "sliceType", "equipment");
		m_pScaleformUI->Value_SetMember(fireObj, "ct", "Incgrenade");
		m_pScaleformUI->Value_SetMember(fireObj, "t", "molotov");
		m_pScaleformUI->Value_SetMember(fireGrenadeObj, "weapon", fireObj);
		m_pScaleformUI->Value_SetMember(fireGrenadeObj, "weapon_position", 26);
		m_pScaleformUI->Value_SetArrayElement(grenadeSlices, 0, fireGrenadeObj);
		SafeReleaseSFVALUE(fireObj);
		SafeReleaseSFVALUE(fireGrenadeObj);

		SFVALUE decoyObj = CreateFlashObject();
		m_pScaleformUI->Value_SetMember(decoyObj, "sliceType", "equipment");
		m_pScaleformUI->Value_SetMember(decoyObj, "weapon", "decoy");
		m_pScaleformUI->Value_SetMember(decoyObj, "weapon_position", 27);
		m_pScaleformUI->Value_SetArrayElement(grenadeSlices, 1, decoyObj);
		SafeReleaseSFVALUE(decoyObj);

		SFVALUE flashObj = CreateFlashObject();
		m_pScaleformUI->Value_SetMember(flashObj, "sliceType", "equipment");
		m_pScaleformUI->Value_SetMember(flashObj, "weapon", "flashbang");
		m_pScaleformUI->Value_SetMember(flashObj, "weapon_position", 28);
		m_pScaleformUI->Value_SetArrayElement(grenadeSlices, 2, flashObj);
		SafeReleaseSFVALUE(flashObj);

		SFVALUE heObj = CreateFlashObject();
		m_pScaleformUI->Value_SetMember(heObj, "sliceType", "equipment");
		m_pScaleformUI->Value_SetMember(heObj, "weapon", "hegrenade");
		m_pScaleformUI->Value_SetMember(heObj, "weapon_position", 29);
		m_pScaleformUI->Value_SetArrayElement(grenadeSlices, 3, heObj);
		SafeReleaseSFVALUE(heObj);

		SFVALUE smokeObj = CreateFlashObject();
		m_pScaleformUI->Value_SetMember(smokeObj, "sliceType", "equipment");
		m_pScaleformUI->Value_SetMember(smokeObj, "weapon", "smokegrenade");
		m_pScaleformUI->Value_SetMember(smokeObj, "weapon_position", 30);
		m_pScaleformUI->Value_SetArrayElement(grenadeSlices, 4, smokeObj);
		SafeReleaseSFVALUE(smokeObj);

		if (coop)
		{
			SFVALUE taObj = CreateFlashObject();
			m_pScaleformUI->Value_SetMember(taObj, "sliceType", "equipment");
			m_pScaleformUI->Value_SetMember(taObj, "weapon", "tagrenade");
			m_pScaleformUI->Value_SetMember(taObj, "weapon_position", 31);
			m_pScaleformUI->Value_SetArrayElement(grenadeSlices, 5, taObj);
			SafeReleaseSFVALUE(taObj);
		}

		m_pScaleformUI->Value_SetMember(grenadeMenu, "Slices", grenadeSlices);
		m_pScaleformUI->Value_SetMember(rootMenu, "GrenadeMenu", grenadeMenu);
	}

	return rootMenu;
}

bool CCSBuyMenuScaleform::FillInPlayerBuyMenuLoadout( CCSBuyMenuLoadout *pLoadout )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return false;

	if ( !pPlayer->IsAlive() )
		return false;

	int team = pPlayer->GetTeamNumber();

	if ( ( team & ~1 ) != TEAM_TERRORIST )
		return false;

	memset( pLoadout, 0, sizeof( *pLoadout ) );

	CCSPlayerInventory *pInventory = CSInventoryManager()->GetLocalCSInventory();

	for ( int slot = LOADOUT_POSITION_MELEE; slot < LOADOUT_POSITION_GRENADE5; ++slot )
	{
		pLoadout->m_LoadoutItems[slot] = INVALID_ITEM_ID;
		C_EconItemView *pItem = pInventory->GetItemInLoadoutFilteredByProhibition( team, slot );

		if ( !pItem )
			continue;

		uint64 itemID;

		if ( pItem->GetItemID() != 0 )
			itemID = pItem->GetItemID();
		else
			itemID = pItem->GetFauxItemIDFromDefinitionIndex();

		Msg( "Weapon: %s, ID: %s\n", pItem, itemID ? "":"" );
		pLoadout->m_LoadoutItems[slot] = itemID;
	}

	return true;
}

bool CCSBuyMenuScaleform::FillInPlayerLoadout( CCSLoadout &loadout )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	C_CSPlayer *pCSPlayer = GetHudPlayer();
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( !pPlayer )
		return false;

	if ( !pPlayer->IsAlive() )
		return false;

	Q_memset( &loadout, 0, sizeof(loadout) );

	int equipCount = 0;

	if ( !CSGameRules()->IsArmorFree() && pPlayer->ArmorValue() > 0 )
	{
		if ( CSGameRules()->IsPlayingCoopMission() )
		{
			loadout.m_EquipmentArray[0].m_EquipmentID = pPlayer->HasHeavyArmor() ? ITEM_HEAVYASSAULTSUIT : ITEM_KEVLAR;
			loadout.m_EquipmentArray[0].m_EquipmentPos = 33;
		}
		else
		{
			loadout.m_EquipmentArray[0].m_EquipmentID = pPlayer->HasHelmet() ? ITEM_ASSAULTSUIT : ITEM_KEVLAR;
			loadout.m_EquipmentArray[0].m_EquipmentPos = pPlayer->HasHelmet() ? 33 : 32;
		}

		loadout.m_EquipmentArray[0].m_Quantity = pPlayer->ArmorValue();

		equipCount = 1;

		if ( pPlayer->ArmorValue() == 0 )
		{
			loadout.m_EquipmentArray[0].m_EquipmentID = 0;
			loadout.m_EquipmentArray[0].m_EquipmentPos = 0;
		}
	}

	if ( pPlayer->HasDefuser() )
		loadout.m_flags |= 4;

	for ( int i = 0; i < MAX_WEAPONS; ++i )
	{
		CWeaponCSBase *pWeapon = (CWeaponCSBase*)pCSPlayer->GetWeapon( i );

		if ( !pWeapon )
			continue;

		int weaponID = pWeapon->GetCSWeaponID();

		if ( weaponID == WEAPON_TASER )
		{
			loadout.m_flags |= 1;
			continue;
		}

		const CCSWeaponInfo *pInfo = GetWeaponInfo( pWeapon->GetCSWeaponID() );

		if ( !pInfo )
			continue;

		CSWeaponType type = pInfo->GetWeaponType();

		if ( type == WEAPONTYPE_GRENADE )
		{
			if ( equipCount <= 5 )
			{
				loadout.m_EquipmentArray[equipCount].m_EquipmentID = weaponID;

				switch ( weaponID )
				{
				case WEAPON_FLASHBANG:
					loadout.m_EquipmentArray[equipCount].m_EquipmentPos = 28;
					break;

				case WEAPON_HEGRENADE:
					loadout.m_EquipmentArray[equipCount].m_EquipmentPos = 29;
					break;

				case WEAPON_SMOKEGRENADE:
					loadout.m_EquipmentArray[equipCount].m_EquipmentPos = 30;
					break;

				case WEAPON_MOLOTOV:
				case WEAPON_INCGRENADE:
					loadout.m_EquipmentArray[equipCount].m_EquipmentPos = 26;
					break;

				case WEAPON_DECOY:
					loadout.m_EquipmentArray[equipCount].m_EquipmentPos = 27;
					break;

				default:
					loadout.m_EquipmentArray[equipCount].m_EquipmentPos = 31;
					break;
				}

				loadout.m_EquipmentArray[equipCount].m_Quantity = pWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );

				++equipCount;

				if (loadout.m_EquipmentArray[equipCount].m_Quantity <= 0)
				{
					loadout.m_EquipmentArray[equipCount].m_EquipmentID = 0;
					loadout.m_EquipmentArray[equipCount].m_EquipmentPos = 0;
				}
			}

			continue;

			if ( type == WEAPONTYPE_C4 )
			{
				loadout.m_flags |= 2;
				continue;
			}
		}
	}

	// Determine active loadout selections
	CCSPlayerInventory *pInventory = CSInventoryManager()->GetLocalCSInventory();

	int team = pPlayer->GetTeamNumber();

	for ( int slot = LOADOUT_POSITION_SECONDARY0; slot < LOADOUT_POSITION_COUNT; ++slot )
	{
		C_EconItemView *pLoadoutItem = pInventory->GetItemInLoadoutFilteredByProhibition( team, slot );

		if ( !pLoadoutItem )
			continue;

		uint16 loadoutDefIndex = pLoadoutItem->GetStaticData()->GetDefinitionIndex();

		for ( int weaponIndex = 0; weaponIndex < MAX_WEAPONS; ++weaponIndex )
		{
			CWeaponCSBase *pWeapon = (CWeaponCSBase*)pCSPlayer->GetWeapon( weaponIndex );

			if ( !pWeapon )
				continue;

			C_EconItemView *pWeaponItem = pWeapon->GetEconItemView();

			if ( !pWeaponItem )
				continue;

			if ( pWeaponItem->GetStaticData()->GetDefinitionIndex() != loadoutDefIndex )
			{
				continue;
			}

			int weaponID = pWeapon->GetCSWeaponID();

			const CCSWeaponInfo *pInfo = GetWeaponInfo(pWeapon->GetCSWeaponID());

			if ( pInfo->GetWeaponType() == WEAPONTYPE_PISTOL )
			{
				loadout.m_nSecondaryWeaponDefIndex = weaponID;
				loadout.m_secondaryWeaponItemPos = slot;
			}
			else if ( pInfo->GetWeaponType() != WEAPONTYPE_KNIFE )
			{
				loadout.m_nPrimaryWeaponDefIndex = weaponID;
				loadout.m_primaryWeaponItemPos = slot;
			}
		}
	}

	return true;
}

SFVALUE CCSBuyMenuScaleform::CreateFlashLoadout( const CCSLoadout &loadout )
{
	SFVALUE flashObj = CreateFlashObject();

	int equipmentCount = 0;

	for ( int i = 0; i < 6; ++i )
	{
		if ( !loadout.m_EquipmentArray[i].m_EquipmentID )
			break;

		if ( !loadout.m_EquipmentArray[i].m_Quantity )
			break;

		++equipmentCount;
	}

	const uint32 totalItems = equipmentCount + ( loadout.m_flags & loadout.HAS_TASER ? 1 : 0 ) + ( loadout.m_primaryWeaponID ? 1 : 0 ) + ( loadout.m_secondaryWeaponID ? 1 : 0 );

	SFVALUE weaponsArray = CreateFlashArray( totalItems );
	SFVALUE slotsArray = CreateFlashArray( totalItems );
	SFVALUE countsArray = CreateFlashArray( totalItems );

	int index = 0;
	int totalPrice = 0;

	// PRIMARY
	if ( loadout.m_primaryWeaponID )
	{
		m_pScaleformUI->Value_SetArrayElement( weaponsArray, index, loadout.m_primaryWeaponID );
		m_pScaleformUI->Value_SetArrayElement( slotsArray, index, loadout.m_primaryWeaponItemPos );
		m_pScaleformUI->Value_SetArrayElement( countsArray, index, 1 );
		m_pScaleformUI->Value_SetMember( flashObj, "primary", index );

		if ( C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer() )
			totalPrice += pPlayer->GetWeaponPrice( static_cast<CSWeaponID>( loadout.m_primaryWeaponID ) );

		++index;
	}

	// SECONDARY
	if ( loadout.m_secondaryWeaponID )
	{
		m_pScaleformUI->Value_SetArrayElement( weaponsArray, index, loadout.m_secondaryWeaponID );
		m_pScaleformUI->Value_SetArrayElement( slotsArray, index, loadout.m_secondaryWeaponItemPos );
		m_pScaleformUI->Value_SetArrayElement( countsArray, index, 1 );
		m_pScaleformUI->Value_SetMember( flashObj, "secondary", index );

		if ( C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer() )
			totalPrice += pPlayer->GetWeaponPrice( static_cast<CSWeaponID>( loadout.m_secondaryWeaponID ) );

		++index;
	}

	// TASER
	if ( loadout.m_flags && loadout.m_flags == 1 )
	{
		m_pScaleformUI->Value_SetArrayElement( weaponsArray, index, WEAPON_TASER );
		m_pScaleformUI->Value_SetArrayElement( slotsArray, index, 3 );
		m_pScaleformUI->Value_SetArrayElement( countsArray, index, 1 );
		m_pScaleformUI->Value_SetMember( flashObj, "taser", index );

		if ( C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer() )
			totalPrice += pPlayer->GetWeaponPrice( WEAPON_TASER );

		++index;
	}

	// EQUIPMENT
	for ( int i = 0; i < 6; ++i )
	{
		const CCSEquipmentLoadout &slot = loadout.m_EquipmentArray[i];

		if ( !slot.m_EquipmentID || !slot.m_Quantity )
			break;

		m_pScaleformUI->Value_SetArrayElement( weaponsArray, index + i, slot.m_EquipmentID );
		m_pScaleformUI->Value_SetArrayElement( slotsArray, index + i, slot.m_EquipmentPos );
		m_pScaleformUI->Value_SetArrayElement( countsArray, index + i, slot.m_Quantity );

		if ( C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer() )
		{
			totalPrice += slot.m_Quantity * pPlayer->GetWeaponPrice( static_cast<CSWeaponID>( slot.m_EquipmentID ) );
		}

		++index;
	}

	m_pScaleformUI->Value_SetMember( flashObj, "weapons", weaponsArray );
	m_pScaleformUI->Value_SetMember( flashObj, "weapons_position", slotsArray );
	m_pScaleformUI->Value_SetMember( flashObj, "counts", countsArray );
	m_pScaleformUI->Value_SetMember( flashObj, "price", totalPrice );
	m_pScaleformUI->Value_SetMember( flashObj, "bomb", ( loadout.m_flags && loadout.HAS_BOMB ) != 0 );
	m_pScaleformUI->Value_SetMember( flashObj, "defuse", ( loadout.m_flags && loadout.HAS_DEFUSE ) != 0 );

	SafeReleaseSFVALUE( weaponsArray );
	SafeReleaseSFVALUE( slotsArray );
	SafeReleaseSFVALUE( countsArray );

	return flashObj;
}

void CCSBuyMenuScaleform::UpdatePlayerLoadout( bool bForceUpdate, bool bUpdateFlash )
{
	if ( !FlashAPIIsValid() )
		return;

	CCSLoadout loadout;

	if ( !FillInPlayerLoadout( loadout ) )
		return;

	if ( !bForceUpdate && loadout == m_PlayerLoadout )
		return;

	m_PlayerLoadout = loadout;

	if ( !bUpdateFlash )
		return;

	WITH_SLOT_LOCKED
	{
		WITH_SFVALUEARRAY( args, 1 )
	{
		SFVALUE flashLoadout = CreateFlashLoadout( m_PlayerLoadout );
		m_pScaleformUI->ValueArray_SetElement( args, 0, flashLoadout );
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "updatePlayerInventory", args );
		SafeReleaseSFVALUE( flashLoadout );
	}
	}
}

void CCSBuyMenuScaleform::FireGameEvent( IGameEvent *event )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	const char *pszEvent = event->GetName();

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( !V_strcmp( pszEvent, "cs_game_disconnected" ) )
	{
		if ( m_bShowOnReady )
		{
			if ( !m_bVisible )
			{
				GetHud().EnableHud();
			}

			if ( m_bShowOnReady )
			{
				m_pScaleformUI->RemoveElement( m_iSplitScreenSlot, m_FlashAPI );
			}
		}

		return;
	}

	if ( !V_strcmp( pszEvent, "player_team" ) )
	{
		if (pLocalPlayer)
		{
			int team = event->GetInt( "team" );
			int userid = event->GetInt( "userid" );

			if ( userid == pLocalPlayer->GetUserID() )
			{
				SetPlayerIsCT( team == TEAM_CT );
			}
		}

		return;
	}

	if ( !V_strcmp( pszEvent, "player_death" ) )
	{
		if ( pLocalPlayer )
		{
			int userid = event->GetInt( "userid" );

			if ( userid == pLocalPlayer->GetUserID() )
			{
				if ( m_bVisible )
				{
					CSGameRules()->CloseBuyMenu( pLocalPlayer->GetUserID() );
				}
			}
		}

		return;
	}

	if ( !V_strcmp( pszEvent, "item_equip" ) )
	{
		int userid = event->GetInt( "userid" );

		if ( pLocalPlayer )
		{
			if ( userid == pLocalPlayer->GetUserID() )
			{
				if ( m_bVisible )
				{
					UpdatePlayerLoadout( true, true );
				}
			}
		}
	}
}

void CCSBuyMenuScaleform::SetRadialSelection( int radialSlot, int categorySlot )
{
	if ( !m_bVisible )
		return;

	const int newSelection = radialSlot * 6 + categorySlot;

	if ( newSelection == m_iCurrentRadialSelection )
		return;

	if ( !m_bShowOnReady )
		return;

	m_iCurrentRadialSelection = newSelection;

	SFVALUEARRAY args;

	g_pScaleformUI->CreateValueArray( args, 2 );
	g_pScaleformUI->ValueArray_SetElement( args, 0, radialSlot );
	g_pScaleformUI->ValueArray_SetElement( args, 1, categorySlot );

	WITH_SLOT_LOCKED
	{
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setWedgeHighlight", args, 2 );
	}
}

void CCSBuyMenuScaleform::UpdateRadialSelection()
{
	float x = 0.0f;
	float y = 0.0f;

	// Read stick position from Scaleform
	if ( m_bVisible )
	{
		y = inputsystem->GetAnalogValue( (AnalogCode_t)JOYSTICK_AXIS( GET_ACTIVE_SPLITSCREEN_SLOT(), 0 ) );
		x = -inputsystem->GetAnalogValue( (AnalogCode_t)JOYSTICK_AXIS( GET_ACTIVE_SPLITSCREEN_SLOT(), 0 ) );
	}

	// Steam Controller / Steam Input
	if ( steamapicontext && steamapicontext->SteamController() )
	{
		ControllerDigitalActionHandle_t hNavigate = steamapicontext->SteamController()->GetDigitalActionHandle( "Navigate" );

		ControllerHandle_t handles[16];
		int count = steamapicontext->SteamController()->GetConnectedControllers( handles );

		for ( int i = 0; i < count; ++i )
		{
			ControllerAnalogActionData_t data = steamapicontext->SteamController()->GetAnalogActionData( handles[i], hNavigate );

			if ( data.bActive )
			{
				y += data.x;
				x += data.y;
			}
		}
	}

	int wheelSelection = -1;
	int categorySelection = -1;

	// Deadzone
	if ( sqrtf(x * x + y * y) > 0.25f )
	{
		float angle = RAD2DEG( atan2f( y, x ) );

		if ( angle < 0.0f )
			angle += 360.0f;

		// 8-way radial wedge
		wheelSelection = ( static_cast<int>( floorf( angle / 45.0f ) ) + 1 ) & 7;

		// 6 buy categories
		categorySelection = ( static_cast<int>( floorf( angle / 60.0f ) ) + 1 ) % 6;
	}

	SetRadialSelection( wheelSelection, categorySelection );
}

void CCSBuyMenuScaleform::InvalidateWeapon(int weaponID)
{
	CGameUiSetActiveSplitScreenPlayerGuard guard(m_iSplitScreenSlot - 2);

	const char *pszAlias = WeaponIDToAlias(weaponID);

	if (!m_bVisible || !pszAlias)
		return;

	WITH_SLOT_LOCKED
	{
		SFVALUEARRAY args;

		g_pScaleformUI->CreateValueArray(args, 1);
		g_pScaleformUI->ValueArray_SetElement(args, 0, pszAlias);
		g_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "InvalidateWeapon", args, 1);
		g_pScaleformUI->ReleaseValueArray(args);
	}
}

bool CCSBuyMenuScaleform::UpdateTimeLeft(bool bForce)
{
	if (!m_bShowOnReady)
		return false;

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	float flTimeLeft = 99.0f;

	if (CSGameRules()->IsPlayingCoopMission())
	{
		flTimeLeft = 9999.0f;
	}
	else if (CSGameRules()->IsWarmupPeriod() ||
		!CSGameRules()->IsPlayingCoopGuardian())
	{
		if (pPlayer && pPlayer->CanBuyDuringImmunity())
		{
			flTimeLeft = pPlayer->m_fImmuneToGunGameDamageTime - gpGlobals->curtime;
		}
		else if (CSGameRules()->IsWarmupPeriod() &&
			CSGameRules()->IsWarmupPeriodPaused())
		{
			flTimeLeft = CSGameRules()->GetWarmupPeriodEndTime() - CSGameRules()->GetWarmupPeriodStartTime();
		}
		else
		{
			flTimeLeft = CSGameRules()->GetBuyTimeLength() - CSGameRules()->GetRoundElapsedTime();
		}
	}
	else
	{
		float flEndTime;

		if (CSGameRules()->IsFreezePeriod())
			flEndTime = CSGameRules()->GetRoundStartTime();
		else
			flEndTime = CSGameRules()->GetWarmupPeriodEndTime();

		flTimeLeft = flEndTime - gpGlobals->curtime;

		if (flTimeLeft > 9999.0f)
			return true;
	}

	if (flTimeLeft <= 0.0f)
		return true;

	int nScaledTime = (int)floorf(flTimeLeft * 5.0f);

	if (bForce || nScaledTime != m_nLastTimeLeft)
	{
		m_nLastTimeLeft = nScaledTime;

		SFVALUEARRAY args = g_pScaleformUI->CreateValueArray(1);

		g_pScaleformUI->ValueArray_SetElement(args, 0, floorf(flTimeLeft));
		//g_pScaleformUI->ValueArray_SetElement(args, 1, (nScaledTime % 5) < 3);
		g_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "setBuyTimeLeft", args, 1);
	}

	return bForce == false && flTimeLeft == 0;
}

void CCSBuyMenuScaleform::UpdatePlayerCash(bool bForce)
{
	if (!m_bShowOnReady)
		return;

	SF_FORCE_SPLITSCREEN_PLAYER_GUARD(m_iSplitScreenSlot);

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	int nAccount = 0;
	if (pPlayer)
		nAccount = pPlayer->GetAccount();

	if (nAccount == m_nLastAccount && !bForce)
		return;

	SFVALUEARRAY args = g_pScaleformUI->CreateValueArray(1);

	WITH_SFVALUEARRAY_SLOT_LOCKED(args, 1)
	{
		m_pScaleformUI->ValueArray_SetElement(args, 0, nAccount);
		m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "setPlayerCash", args, 1);
	}

	m_nLastAccount = nAccount;
}

void CCSBuyMenuScaleform::ViewportThink( void )
{
	UpdateRadialSelection();

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if (!pPlayer)
		return;

	// Cache armor/helmet state
	int armorState = pPlayer->ArmorValue();

	if (pPlayer->HasHelmet())
		armorState += 1000;

	if (armorState != m_iCachedArmorState)
	{
		InvalidateWeapon(ITEM_KEVLAR);
		InvalidateWeapon(ITEM_ASSAULTSUIT);
		InvalidateWeapon(ITEM_HEAVYASSAULTSUIT);

		m_iCachedArmorState = armorState;
	}

	// Buy time expired
	if (UpdateTimeLeft(false))
	{
		Hide();

		if (!CSGameRules()->IsPlayingCooperativeGametype())
			PrintBuyTimeOverMessage();

		return;
	}

	// Player left buyzone
	if (!pPlayer->IsInBuyZone())
	{
		Hide();

		SFHudInfoPanel *pInfoPanel = GET_HUDELEMENT(SFHudInfoPanel);

		if (pInfoPanel)
		{
			pInfoPanel->SetPriorityText(g_pVGuiLocalize->Find("#SFUI_BuyMenu_NotInBuyZone"));
		}

		return;
	}

	// Periodic refresh
	float curTime = gpGlobals->curtime;

	if (curTime >= m_flNextLoadoutUpdate)
	{
		m_flNextLoadoutUpdate = curTime + 0.25f;

		UpdatePlayerCash(false);
		UpdatePlayerLoadout(false, true);
	}
}

#endif // INCLUDE_SCALEFORM