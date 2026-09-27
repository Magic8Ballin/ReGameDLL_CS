#include "precompiled.h"

LINK_ENTITY_TO_CLASS(weapon_m4a1, CM4A1, CCSM4A1)

static const gw::WeaponMechanicsConfig &M4A1Mechanics()
{
	static bool loaded = false;
	static gw::WeaponMechanicsConfig config;
	if (!loaded)
	{
		loaded = true;
		config = gw::Defaults(true);
		int length = 0;
		char *json = (char *)LOAD_FILE_FOR_ME("configs/weapons/m4a1.json", &length);
		gw::WeaponMechanicsConfig parsed;
		if (json && gw::ParseConfig(json, parsed)) config = parsed;
		else ALERT(at_console, "Gloveworks: invalid/missing configs/weapons/m4a1.json; using safe defaults\n");
		if (json) FREE_FILE(json);
	}
	return config;
}

static void ResetM4A1Mechanics(CM4A1 *weapon)
{
	weapon->m_ModernState.firePenalty = 0.0f;
	weapon->m_ModernState.recoilIndex = 0.0f;
	weapon->m_ModernState.lastShotTime = 0.0f;
}

void CM4A1::Spawn()
{
	Precache();

	m_iId = WEAPON_M4A1;
	SET_MODEL(edict(), "models/w_m4a1.mdl");

	m_iDefaultAmmo = M4A1_DEFAULT_GIVE;
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	m_bDelayFire = true;
	ResetM4A1Mechanics(this);

#ifdef REGAMEDLL_API
	CSPlayerWeapon()->m_flBaseDamage = M4A1_DAMAGE;
	m_flBaseDamageSil = M4A1_DAMAGE_SIL;
#endif

	// Get ready to fall down
	FallInit();

	// extend
	CBasePlayerWeapon::Spawn();
}

void CM4A1::Precache()
{
	PRECACHE_MODEL("models/v_m4a1.mdl");
	PRECACHE_MODEL("models/w_m4a1.mdl");

	PRECACHE_SOUND("weapons/m4a1-1.wav");
	PRECACHE_SOUND("weapons/m4a1_unsil-1.wav");
	PRECACHE_SOUND("weapons/m4a1_unsil-2.wav");
	PRECACHE_SOUND("weapons/m4a1_clipin.wav");
	PRECACHE_SOUND("weapons/m4a1_clipout.wav");
	PRECACHE_SOUND("weapons/m4a1_boltpull.wav");
	PRECACHE_SOUND("weapons/m4a1_deploy.wav");
	PRECACHE_SOUND("weapons/m4a1_silencer_on.wav");
	PRECACHE_SOUND("weapons/m4a1_silencer_off.wav");

	m_iShell = PRECACHE_MODEL("models/rshell.mdl");
	m_usFireM4A1 = PRECACHE_EVENT(1, "events/m4a1.sc");
}

int CM4A1::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "556Nato";
	p->iMaxAmmo1 = MAX_AMMO_556NATO;
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = M4A1_MAX_CLIP;
	p->iSlot = 0;
	p->iPosition = 6;
	p->iId = m_iId = WEAPON_M4A1;
	p->iFlags = 0;
	p->iWeight = M4A1_WEIGHT;

	return 1;
}

BOOL CM4A1::Deploy()
{
	m_bDelayFire = true;
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	ResetM4A1Mechanics(this);

	iShellOn = 1;

	if (m_iWeaponState & WPNSTATE_M4A1_SILENCED)
		return DefaultDeploy("models/v_m4a1.mdl", "models/p_m4a1.mdl", M4A1_DRAW, "rifle", UseDecrement() != FALSE);
	else
		return DefaultDeploy("models/v_m4a1.mdl", "models/p_m4a1.mdl", M4A1_UNSIL_DRAW, "rifle", UseDecrement() != FALSE);
}

void CM4A1::SecondaryAttack()
{
	if (m_iWeaponState & WPNSTATE_M4A1_SILENCED)
	{
		m_iWeaponState &= ~WPNSTATE_M4A1_SILENCED;
		SendWeaponAnim(M4A1_DETACH_SILENCER, UseDecrement() != FALSE);
		Q_strlcpy(m_pPlayer->m_szAnimExtention, "rifle");
	}
	else
	{
		m_iWeaponState |= WPNSTATE_M4A1_SILENCED;
		SendWeaponAnim(M4A1_ATTACH_SILENCER, UseDecrement() != FALSE);
		Q_strlcpy(m_pPlayer->m_szAnimExtention, "rifle");
	}

	m_flTimeWeaponIdle = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 2.0f;
	m_flNextPrimaryAttack = GetNextAttackDelay(2.0);
}

void CM4A1::PrimaryAttack()
{
	const gw::WeaponMechanicsConfig &config = M4A1Mechanics();
	gw::UpdateState(config, m_ModernState, gpGlobals->time, (m_pPlayer->pev->flags & FL_DUCKING) != 0);
	const float inaccuracy = gw::ComputeInaccuracy(config, m_ModernState, m_pPlayer->pev->velocity.Length2D(),
		GetMaxSpeed(), (m_pPlayer->pev->flags & FL_DUCKING) != 0, (m_pPlayer->pev->flags & FL_ONGROUND) != 0,
		m_pPlayer->pev->movetype == MOVETYPE_FLY);
	M4A1Fire(inaccuracy, config.cycleTime, FALSE);
}

void CM4A1::M4A1Fire(float flSpread, float flCycleTime, BOOL fUseAutoAim)
{
	Vector vecAiming, vecSrc, vecDir;
	int flag;

	m_bDelayFire = true;
	if (m_iClip <= 0)
	{
		if (m_fFireOnEmpty)
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = GetNextAttackDelay(0.2);
		}

		if (TheBots)
		{
			TheBots->OnEvent(EVENT_WEAPON_FIRED_ON_EMPTY, m_pPlayer);
		}

		return;
	}

	m_iShotsFired++;

	m_iClip--;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	vecSrc = m_pPlayer->GetGunPosition();
	vecAiming = gpGlobals->v_forward;

#ifdef REGAMEDLL_API
	float flBaseDamage = (m_iWeaponState & WPNSTATE_M4A1_SILENCED) ? m_flBaseDamageSil : CSPlayerWeapon()->m_flBaseDamage;
#else
	float flBaseDamage = (m_iWeaponState & WPNSTATE_M4A1_SILENCED) ? M4A1_DAMAGE_SIL : M4A1_DAMAGE;
#endif
	const gw::WeaponMechanicsConfig &config = M4A1Mechanics();
	const gw::ShotOffset offset = gw::ComputeShotOffset(m_pPlayer->random_seed, flSpread, config.baseSpread);
	vecAiming = vecAiming + gpGlobals->v_right * offset.x + gpGlobals->v_up * offset.y;
	if (m_iWeaponState & WPNSTATE_M4A1_SILENCED)
	{
		vecDir = m_pPlayer->FireBullets3(vecSrc, vecAiming, 0.0f, 8192, 2, BULLET_PLAYER_556MM,
			flBaseDamage, M4A1_RANGE_MODIFER_SIL, m_pPlayer->pev, false, m_pPlayer->random_seed);
	}
	else
	{
		vecDir = m_pPlayer->FireBullets3(vecSrc, vecAiming, 0.0f, 8192, 2, BULLET_PLAYER_556MM,
			flBaseDamage, M4A1_RANGE_MODIFER, m_pPlayer->pev, false, m_pPlayer->random_seed);

		m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	}
	vecDir = Vector(offset.x, offset.y, 0.0f);

#ifdef CLIENT_WEAPONS
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

#ifndef REGAMEDLL_FIXES
	m_pPlayer->ammo_556nato--;
#endif

	PLAYBACK_EVENT_FULL(flag, m_pPlayer->edict(), m_usFireM4A1, 0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y,
		int(m_pPlayer->pev->punchangle.x * 100), int(m_pPlayer->pev->punchangle.y * 100), (m_iWeaponState & WPNSTATE_M4A1_SILENCED) == WPNSTATE_M4A1_SILENCED, FALSE);

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(flCycleTime);

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	{
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", SUIT_SENTENCE, SUIT_REPEAT_OK);
	}

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.5f;

	const gw::RecoilPoint recoil = gw::GetRecoil(config, m_ModernState.recoilIndex);
	m_pPlayer->pev->vuser1.x -= recoil.vertical;
	m_pPlayer->pev->vuser1.y += recoil.horizontal;
	m_pPlayer->pev->fuser4 = 1.0f;
	gw::CommitShot(config, m_ModernState, gpGlobals->time);
}

void CM4A1::Reload()
{
#ifndef REGAMEDLL_FIXES
	if (m_pPlayer->ammo_556nato <= 0)
		return;
#endif

	if (DefaultReload(iMaxClip(), ((m_iWeaponState & WPNSTATE_M4A1_SILENCED) == WPNSTATE_M4A1_SILENCED) ? M4A1_RELOAD : M4A1_UNSIL_RELOAD, M4A1_RELOAD_TIME))
	{
		m_pPlayer->SetAnimation(PLAYER_RELOAD);

		m_flAccuracy = 0.2f;
		m_iShotsFired = 0;
		ResetM4A1Mechanics(this);
		m_bDelayFire = false;
	}
}

void CM4A1::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector(AUTOAIM_10DEGREES);

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
	{
		return;
	}

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 20.0f;
	SendWeaponAnim((m_iWeaponState & WPNSTATE_M4A1_SILENCED) == WPNSTATE_M4A1_SILENCED ? M4A1_IDLE : M4A1_UNSIL_IDLE, UseDecrement() != FALSE);
}

float CM4A1::GetMaxSpeed()
{
	return M4A1_MAX_SPEED;
}
