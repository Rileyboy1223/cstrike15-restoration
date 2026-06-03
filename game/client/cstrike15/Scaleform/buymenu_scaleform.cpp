//fbr

#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "hud.h"
#include "hudelement.h"

#include "ienginevgui.h"
#include "buymenu_scaleform.h"
#include "HUD/sfhudflashinterface.h"
#include "game/client/iviewport.h"
#include "filesystem.h"
#include "gameui_util.h"
#include "inputsystem/iinputsystem.h"

#include "vstdlib/vstrtools.h"
#include "strtools.h"

#include <keyvalues.h>

#include <vgui_controls/Panel.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/AnalogBar.h>

#include "HUD/sfhudinfopanel.h"

#include "c_cs_player.h"
#include "weapon_csbasegun.h"

#include "econ_item_inventory.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

SFUI_BEGIN_GAME_API_DEF
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

CCSBuyMenuScaleform::CCSBuyMenuScaleform(CounterStrikeViewport* pViewport) :
	m_bVisible(false),
	m_bInitialized(false),
	m_pViewport(pViewport),
	m_iSelectedWeapon(-1),
	m_iPrimaryWeapon(0),
	m_iSecondaryWeapon(0),
	m_iArmorValue(0),
	m_iCurrentMoney(0),
	m_iUnknownState(-1)
{
	std::memset(m_Padding, 0, sizeof(m_Padding));

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

	if (shouldInit)
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
}

void CCSBuyMenuScaleform::FlashLoaded()
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

void CCSBuyMenuScaleform::FlashReady()
{
	CGameUiSetActiveSplitScreenPlayerGuard guard(this->m_nPlayerSlot - 2);

	C_CSPlayer *LocalCSPlayer = C_CSPlayer::GetLocalCSPlayer();

	if (LocalCSPlayer)
	{
		SetPlayerIsCT(LocalCSPlayer->GetTeamNumber() == 3);
	}

	CalculateBestStats();

	ListenForGameEvent("cs_game_disconnected");
	ListenForGameEvent("player_team");
	ListenForGameEvent("player_death");
	ListenForGameEvent("item_death");

	m_bLoading = false;

	if (m_bVisible)
	{
		Show();
	}
	else
	{
		Hide();
	}
}

void CCSBuyMenuScaleform::SetPlayerIsCT(bool isCT)
{
	CGameUiSetActiveSplitScreenPlayerGuard guard(this->m_iSplitScreenSlot - 2);

	if (m_bIsLoaded)
	{
		m_pScaleformUI->LockSlot(m_iSplitScreenSlot);
		m_pScaleformUI->Value_SetArraySize(value, 1);
		m_pScaleformUI->Value_SetValue(value, isCT);
		m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "setPlayerIsCT", value, 1);
		m_pScaleformUI->ReleaseValue(value);
		m_pScaleformUI->UnlockSlot(m_iSplitScreenSlot);
	}
}

bool CCSBuyMenuScaleform::PreUnloadFlash()
{
	// WIP
	return true;
}

void CCSBuyMenuScaleform::SetHostageMatch(bool bHostageMatch)
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD(m_iSplitScreenSlot);

	if (m_bIsLoaded)
	{
		SFVALUEARRAY args;

		//m_pScaleformUI->PushScaleformDataContext(m_iSplitScreenSlot);

		m_pScaleformUI->CreateValueArray(args, 1);
		m_pScaleformUI->ValueArray_SetElement(args, true, bHostageMatch);
		m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "setIsHostageMatch", args, 1);

		//m_pScaleformUI->PopScaleformDataContext();
	}
}

void CCSBuyMenuScaleform::ShowPanel( bool bShow )
{
	Msg("Calling ShowPanel for PANEL_BUY\n");
	if ( bShow == m_bVisible )
	{
		return;
	}

	if ( bShow )
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
	//g_pInputSystem->AttachToInputContext("RadialControls", this);

	// If the movie hasn't been loaded yet, start loading it.
	if (!m_bMovieLoaded)
	{
		if (!m_bLoadingMovie)
		{
			m_bMovieLoaded = true;

			g_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "BuyMenu", NULL, 0);
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
		//if (m_pScaleformFlashInterface)
			//m_pScaleformFlashInterface->PrePanelShow(m_iSplitScreenSlot);

		g_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "showPanel", NULL, NULL);

		//if (m_pScaleformFlashInterface)
			//m_pScaleformFlashInterface->PostPanelShow(m_iSplitScreenSlot);

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

void CCSBuyMenuScaleform::Hide()
{
	m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", NULL, 0 );
}

bool CCSBuyMenuScaleform::FillInPlayerLoadout(CCSLoadout &loadout)
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD(m_iSplitScreenSlot);

	C_CSPlayer *pCSPlayer = GetHudPlayer();
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if (!pPlayer)
		return false;

	if (!pPlayer->IsAlive())
		return false;

	Q_memset(&loadout, 0, sizeof(loadout));

	//
	// Armor
	//

	int grenadeIndex = 0;

	if (!CSGameRules()->IsArmorFree() && pPlayer->ArmorValue() > 0)
	{
		if (CSGameRules()->IsPlayingCoopMission())
		{
			loadout.m_Items[0].m_nWeaponID = (pPlayer->HasHeavyArmor() ? 1 : 0) | 0x34;;
			loadout.m_Items[0].m_nLoadoutSlot = 0x21;
		}
		else
		{
			loadout.m_Items[0].m_nWeaponID = (pPlayer->HasHelmet() ? 1 : 0) + 0x33;
			loadout.m_Items[0].m_nLoadoutSlot = (pPlayer->HasHelmet() ? 1 : 0) | 0x20;
		}

		loadout.m_Items[0].m_nCount = pPlayer->ArmorValue();

		grenadeIndex = 1;

		if (!loadout.m_Items[0].m_nCount)
		{
			loadout.m_Items[0].m_nWeaponID = 0;
			loadout.m_Items[0].m_nLoadoutSlot = 0;
		}
	}

	//
	// Defuser
	//

	if (pPlayer->HasDefuser())
		loadout.m_nFlags |= 4;

	//
	// Inventory scan
	//

	for (int i = 0; i < MAX_WEAPONS; ++i)
	{
		CWeaponCSBase *pWeapon = (CWeaponCSBase*)pCSPlayer->GetWeapon(i);

		if (!pWeapon)
			continue;

		int weaponID = pWeapon->GetCSWeaponID();

		//
		// Zeus
		//

		if (weaponID == WEAPON_TASER)
		{
			loadout.m_nFlags |= 1;
			continue;
		}

		const CCSWeaponInfo *pInfo = GetWeaponInfo(pWeapon->GetCSWeaponID());

		if (!pInfo)
			continue;

		CSWeaponType type = pInfo->GetWeaponType();

		//
		// Grenades
		//

		if (type == WEAPONTYPE_GRENADE)
		{
			if (grenadeIndex >= ARRAYSIZE(loadout.m_Items))
				continue;

			loadout.m_Items[grenadeIndex].m_nWeaponID = weaponID;
			loadout.m_Items[grenadeIndex].m_nLoadoutSlot = 5;
			loadout.m_Items[grenadeIndex].m_nCount = 1;

			if (loadout.m_Items[grenadeIndex].m_nCount <= 0)
			{
				loadout.m_Items[grenadeIndex].m_nWeaponID = 0;
				loadout.m_Items[grenadeIndex].m_nLoadoutSlot = 0;
			}

			++grenadeIndex;
			continue;
		}

		if (type == WEAPONTYPE_C4)
		{
			loadout.m_nFlags |= 2;
			continue;
		}
	}

	//
	// Determine active loadout selections
	//

	auto *pInventory = CSInventoryManager()->GetLocalInventory();

	int team = pPlayer->GetTeamNumber();

	for (int slot = LOADOUT_POSITION_SECONDARY0; slot < LOADOUT_POSITION_COUNT; ++slot)
	{
		C_EconItemView *pLoadoutItem = pInventory->GetItemInLoadout(team, slot);

		if (!pLoadoutItem)
			continue;

		uint16 loadoutDefIndex = pLoadoutItem->GetStaticData()->GetDefinitionIndex();

		for (int weaponIndex = 0; weaponIndex < MAX_WEAPONS; ++weaponIndex)
		{
			CWeaponCSBase *pWeapon = (CWeaponCSBase*)pCSPlayer->GetWeapon(weaponIndex);

			if (!pWeapon)
				continue;

			C_EconItemView *pWeaponItem = pWeapon->GetEconItemView();

			if (!pWeaponItem)
				continue;

			if (pWeaponItem->GetStaticData()->GetDefinitionIndex() != loadoutDefIndex)
			{
				continue;
			}

			int weaponID = pWeapon->GetCSWeaponID();

			const CCSWeaponInfo *pInfo = GetWeaponInfo(pWeapon->GetCSWeaponID());

			if (pInfo->GetWeaponType() == WEAPONTYPE_PISTOL)
			{
				loadout.m_nSecondaryWeaponDefIndex = weaponID;
				loadout.m_nSecondaryLoadoutSlot = slot;
			}
			else
			{
				loadout.m_nPrimaryWeaponDefIndex = weaponID;
				loadout.m_nPrimaryLoadoutSlot = slot;
			}
		}
	}

	return true;
}

SFVALUE CCSBuyMenuScaleform::CreateFlashLoadout(const CCSLoadout &loadout)
{
	SFVALUE flashObj = CreateFlashObject();

	int grenadeCount = 0;

	for (int i = 0; i < 6; ++i)
	{
		if (!loadout.m_Items[i].m_nWeaponID)
			break;

		if (!loadout.m_Items[i].m_nCount)
			break;

		++grenadeCount;
	}

	const int totalItems = grenadeCount + (loadout.m_nPrimaryWeaponID != 0) + (loadout.m_nSecondaryWeaponID != 0) + ((loadout.m_nFlags && loadout.m_nFlags == 1) ? 1 : 0);

	SFVALUE weaponsArray = CreateFlashArray(totalItems);
	SFVALUE slotsArray = CreateFlashArray(totalItems);
	SFVALUE countsArray = CreateFlashArray(totalItems);

	int idx = 0;
	int totalPrice = 0;

	//
	// primary
	//
	if (loadout.m_nPrimaryWeaponID)
	{
		m_pScaleformUI->Value_SetArrayElement(weaponsArray, idx, loadout.m_nPrimaryWeaponID);
		m_pScaleformUI->Value_SetArrayElement(slotsArray, idx, loadout.m_nPrimaryLoadoutSlot);
		m_pScaleformUI->Value_SetArrayElement(countsArray, idx, 1);
		m_pScaleformUI->Value_SetMember(flashObj, "primary", idx);

		++idx;
	}

	//
	// secondary
	//
	if (loadout.m_nSecondaryWeaponID)
	{
		m_pScaleformUI->Value_SetArrayElement(weaponsArray, idx, loadout.m_nSecondaryWeaponID);
		m_pScaleformUI->Value_SetArrayElement(slotsArray, idx, loadout.m_nSecondaryLoadoutSlot);
		m_pScaleformUI->Value_SetArrayElement(countsArray, idx, 1);
		m_pScaleformUI->Value_SetMember(flashObj, "secondary", idx);

		++idx;
	}

	//
	// taser
	//
	if (loadout.m_nFlags && loadout.m_nFlags == 1)
	{
		m_pScaleformUI->Value_SetArrayElement(weaponsArray, idx, WEAPON_TASER);
		m_pScaleformUI->Value_SetArrayElement(slotsArray, idx, 3);
		m_pScaleformUI->Value_SetArrayElement(countsArray, idx, 1);
		m_pScaleformUI->Value_SetMember(flashObj, "taser", idx);

		++idx;
	}

	//
	// grenades
	//
	for (int i = 0; i < 6; ++i)
	{
		const BuyMenuGrenadeEntry &gren = loadout.m_Items[i];

		if (!gren.m_nWeaponID || !gren.m_nCount)
			break;

		m_pScaleformUI->Value_SetArrayElement(weaponsArray, idx, gren.m_nWeaponID);
		m_pScaleformUI->Value_SetArrayElement(slotsArray, idx, 	gren.m_nLoadoutSlot);
		m_pScaleformUI->Value_SetArrayElement(countsArray, idx, gren.m_nCount);

		++idx;
	}

	m_pScaleformUI->Value_SetMember(flashObj, "weapons", weaponsArray);
	m_pScaleformUI->Value_SetMember(flashObj, "weapons_position", slotsArray);
	m_pScaleformUI->Value_SetMember(flashObj, "counts", countsArray);
	m_pScaleformUI->Value_SetMember(flashObj, "price", totalPrice);
	m_pScaleformUI->Value_SetMember(flashObj, "bomb", (loadout.m_nFlags && loadout.m_nFlags == 2) != 0);
	m_pScaleformUI->Value_SetMember(flashObj, "defuse", (loadout.m_nFlags && loadout.m_nFlags == 999) != 0); // Rileyboy1223 -- 999 for now lol until I find the right flag number.

	SafeReleaseSFVALUE(weaponsArray);
	SafeReleaseSFVALUE(slotsArray);
	SafeReleaseSFVALUE(countsArray);

	return flashObj;
}

void CCSBuyMenuScaleform::UpdatePlayerLoadout(bool bForceUpdate, bool bUpdateFlash)
{
	if (!FlashAPIIsValid())
		return;

	CCSLoadout loadout;

	if (!FillInPlayerLoadout(loadout))
		return;

	if (!bForceUpdate && loadout == m_PlayerLoadout)
		return;

	m_PlayerLoadout = loadout;

	if (!bUpdateFlash)
		return;

	WITH_SLOT_LOCKED
	{
		WITH_SFVALUEARRAY(args, 1)
	{
		SFVALUE flashLoadout = CreateFlashLoadout(m_PlayerLoadout);

		m_pScaleformUI->ValueArray_SetElement(args, 0, flashLoadout);

		m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "updatePlayerInventory", args);

		SafeReleaseSFVALUE(flashLoadout);
	}
	}
}

void CCSBuyMenuScaleform::FireGameEvent(IGameEvent *event)
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD(m_iSplitScreenSlot);

	const char *pszEvent = event->GetName();

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	if (!V_strcmp(pszEvent, "cs_game_disconnected"))
	{
		if (m_bShowOnReady)
		{
			if (!m_bVisible)
			{
				GetHud().EnableHud();
			}

			if (m_bShowOnReady)
			{
				m_pScaleformUI->RemoveElement(m_iSplitScreenSlot, m_FlashAPI);
			}
		}

		return;
	}

	if (!V_strcmp(pszEvent, "player_team"))
	{
		if (pLocalPlayer)
		{
			int team = event->GetInt("team");
			int userid = event->GetInt("userid");

			if (userid == pLocalPlayer->GetUserID())
			{
				Msg("Player switched to CT team");
				SetPlayerIsCT(team == TEAM_CT);
			}
		}

		return;
	}

	if (!V_strcmp(pszEvent, "player_death"))
	{
		if (pLocalPlayer)
		{
			int userid = event->GetInt("userid");

			if (userid == pLocalPlayer->GetUserID())
			{
				if (m_bVisible)
				{
					CSGameRules()->CloseBuyMenu(pLocalPlayer->GetUserID());
				}
			}
		}

		return;
	}

	if (!V_strcmp(pszEvent, "item_equip"))
	{
		int userid = event->GetInt("userid");

		if (pLocalPlayer)
		{
			if (userid == pLocalPlayer->GetUserID())
			{
				if (m_bVisible)
				{
					UpdatePlayerLoadout(true, true);
				}
			}
		}
	}
}

void CCSBuyMenuScaleform::SetRadialSelection(int radialSlot, int categorySlot)
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

	m_pScaleformUI->CreateValueArray( args, 2 );
	m_pScaleformUI->ValueArray_SetElement( args, 0, radialSlot );
	m_pScaleformUI->ValueArray_SetElement( args, 1, categorySlot );

	WITH_SLOT_LOCKED
	{
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setWedgeHighlight", args, 2 );
	}
}

void CCSBuyMenuScaleform::UpdateRadialSelection()
{
	float x = 0.0f;
	float y = 0.0f;

	//
	// Read stick position from Scaleform
	//
	if (m_bVisible)
	{
		y = inputsystem->GetAnalogValue( (AnalogCode_t)JOYSTICK_AXIS( GET_ACTIVE_SPLITSCREEN_SLOT(), 0 ) );
		x = -inputsystem->GetAnalogValue( (AnalogCode_t)JOYSTICK_AXIS( GET_ACTIVE_SPLITSCREEN_SLOT(), 0 ) );
	}

	//
	// Steam Controller / Steam Input
	//
	if (steamapicontext && steamapicontext->SteamController())
	{
		ControllerDigitalActionHandle_t hNavigate = steamapicontext->SteamController()->GetDigitalActionHandle("Navigate");

		ControllerHandle_t handles[16];
		int count = steamapicontext->SteamController()->GetConnectedControllers(handles);

		for (int i = 0; i < count; ++i)
		{
			ControllerAnalogActionData_t data = steamapicontext->SteamController()->GetAnalogActionData(handles[i], hNavigate);

			if (data.bActive)
			{
				y += data.x;
				x += data.y;
			}
		}
	}

	int wheelSelection = -1;
	int categorySelection = -1;

	//
	// Deadzone
	//
	if (sqrtf(x * x + y * y) > 0.25f)
	{
		float angle = RAD2DEG(atan2f(y, x));

		if (angle < 0.0f)
			angle += 360.0f;

		//
		// 8-way radial wedge
		//
		wheelSelection = (static_cast<int>(floorf(angle / 45.0f)) + 1) & 7;

		//
		// 6 buy categories
		//
		categorySelection = (static_cast<int>(floorf(angle / 60.0f)) + 1) % 6;
	}

	SetRadialSelection(wheelSelection, categorySelection);
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

		m_pScaleformUI->CreateValueArray(args, 1);
		m_pScaleformUI->ValueArray_SetElement(args, 0, pszAlias);
		m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "InvalidateWeapon", args, 1);
		m_pScaleformUI->ReleaseValueArray(args);
	}
}

bool CCSBuyMenuScaleform::UpdateTimeLeft(bool bForce)
{
	if (!m_bShowOnReady)
		return false;

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	float flTimeLeft = 0.0f;

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

		WITH_SFVALUEARRAY_SLOT_LOCKED(args, 2)
		{
			//args[0].SetNumber(floorf(flTimeLeft));
			//args[1].SetBoolean((nScaledTime % 5) < 3);

			m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "setBuyTimeLeft", args, 2);
		}
	}

	return false;
}

void CCSBuyMenuScaleform::OnCancel()
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD(m_iSplitScreenSlot);

	C_BasePlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	int nUserID = -1;
	if (pPlayer)
	{
		nUserID = pPlayer->GetUserID();
	}

	CSGameRules()->CloseBuyMenu(nUserID);
}

void CCSBuyMenuScaleform::UpdatePlayerCash(bool bForce)
{
	if (!m_bShowOnReady)
		return;

	CGameUiSetActiveSplitScreenPlayerGuard guard(
		m_iSplitScreenSlot - 2);

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	int nAccount = 0;
	if (pPlayer)
		nAccount = pPlayer->GetAccount();

	if (nAccount == m_nLastAccount && !bForce)
		return;

	SFVALUEARRAY args;

	//m_pScaleformUI->CreateValueArray(args, 1);

	//WITH_SFVALUEARRAY_SLOT_LOCKED(args, 1)
	//{
		//m_pScaleformUI->ValueArray_SetElement(args, 0, nAccount);
		//m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "setPlayerCash", args, 1);
	//}

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
		InvalidateWeapon(WEAPON_KEVLAR);
		InvalidateWeapon(WEAPON_ASSAULTSUIT);
		InvalidateWeapon(53); // 53 for now

		m_iCachedArmorState = armorState;
	}

	// Buy time expired
	if (UpdateTimeLeft(false))
	{
		OnCancel();

		if (!CSGameRules()->IsPlayingCooperativeGametype())
			PrintBuyTimeOverMessage();

		return;
	}

	// Player left buyzone
	if (!pPlayer->IsInBuyZone())
	{
		OnCancel();

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