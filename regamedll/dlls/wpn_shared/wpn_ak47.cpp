#include "precompiled.h"

LINK_ENTITY_TO_CLASS(weapon_ak47, CAK47, CCSAK47)

static const gw::WeaponMechanicsConfig &AK47Mechanics()
{
	static bool loaded = false;
	static gw::WeaponMechanicsConfig config;
	if (!loaded)
	{
		loaded = true;
		config = gw::Defaults(true);
		int length = 0;
		char *json = (char *)LOAD_FILE_FOR_ME("configs/weapons/ak47.json", &length);
		gw::WeaponMechanicsConfig parsed;
		if (json && gw::ParseConfig(json, parsed)) config = parsed;
		else ALERT(at_console, "Gloveworks: invalid/missing configs/weapons/ak47.json; using safe defaults\n");
		if (json) FREE_FILE(json);
	}
	return config;
}

static void ResetAK47Mechanics(CAK47 *weapon)
{
	weapon->m_ModernState.firePenalty = 0.0f;
	weapon->m_ModernState.recoilIndex = 0.0f;
	weapon->m_ModernState.lastShotTime = 0.0f;
}

void CAK47::Spawn()
{
	Precache();

	m_iId = WEAPON_AK47;
	SET_MODEL(edict(), "models/w_ak47.mdl");

	m_iDefaultAmmo = AK47_DEFAULT_GIVE;
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	ResetAK47Mechanics(this);

#ifdef REGAMEDLL_API
	CSPlayerWeapon()->m_flBaseDamage = AK47_DAMAGE;
#endif

	// Get ready to fall down
	FallInit();

	// extend
	CBasePlayerWeapon::Spawn();
}

void CAK47::Precache()
{
	PRECACHE_MODEL("models/v_ak47.mdl");
	PRECACHE_MODEL("models/w_ak47.mdl");

	PRECACHE_SOUND("weapons/ak47-1.wav");
	PRECACHE_SOUND("weapons/ak47-2.wav");
	PRECACHE_SOUND("weapons/ak47_clipout.wav");
	PRECACHE_SOUND("weapons/ak47_clipin.wav");
	PRECACHE_SOUND("weapons/ak47_boltpull.wav");

	m_iShell = PRECACHE_MODEL("models/rshell.mdl");
	m_usFireAK47 = PRECACHE_EVENT(1, "events/ak47.sc");
}

int CAK47::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "762Nato";
	p->iMaxAmmo1 = MAX_AMMO_762NATO;
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = AK47_MAX_CLIP;
	p->iSlot = 0;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_AK47;
	p->iFlags = 0;
	p->iWeight = AK47_WEIGHT;

	return 1;
}

BOOL CAK47::Deploy()
{
	m_flAccuracy = 0.2f;
	m_iShotsFired = 0;
	ResetAK47Mechanics(this);
	iShellOn = 1;

	return DefaultDeploy("models/v_ak47.mdl", "models/p_ak47.mdl", AK47_DRAW, "ak47", UseDecrement() != FALSE);
}

void CAK47::SecondaryAttack()
{
	;
}

void CAK47::PrimaryAttack()
{
	const gw::WeaponMechanicsConfig &config = AK47Mechanics();
	gw::UpdateState(config, m_ModernState, gpGlobals->time, (m_pPlayer->pev->flags & FL_DUCKING) != 0);
	const float inaccuracy = gw::ComputeInaccuracy(config, m_ModernState, m_pPlayer->pev->velocity.Length2D(),
		GetMaxSpeed(), (m_pPlayer->pev->flags & FL_DUCKING) != 0, (m_pPlayer->pev->flags & FL_ONGROUND) != 0,
		m_pPlayer->pev->movetype == MOVETYPE_FLY);
	AK47Fire(inaccuracy, config.cycleTime, FALSE);
}

void CAK47::AK47Fire(float flSpread, float flCycleTime, BOOL fUseAutoAim)
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
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);

	vecSrc = m_pPlayer->GetGunPosition();
	vecAiming = gpGlobals->v_forward;

#ifdef REGAMEDLL_API
	float flBaseDamage = CSPlayerWeapon()->m_flBaseDamage;
#else
	float flBaseDamage = AK47_DAMAGE;
#endif
	const gw::WeaponMechanicsConfig &config = AK47Mechanics();
	const gw::ShotOffset offset = gw::ComputeShotOffset(m_pPlayer->random_seed, flSpread, config.baseSpread);
	vecAiming = vecAiming + gpGlobals->v_right * offset.x + gpGlobals->v_up * offset.y;
	vecDir = m_pPlayer->FireBullets3(vecSrc, vecAiming, 0.0f, 8192, 2, BULLET_PLAYER_762MM,
		flBaseDamage, AK47_RANGE_MODIFER, m_pPlayer->pev, false, m_pPlayer->random_seed);

#ifdef CLIENT_WEAPONS
	flag = FEV_NOTHOST;
#else
	flag = 0;
#endif

	PLAYBACK_EVENT_FULL(flag, m_pPlayer->edict(), m_usFireAK47, 0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y,
		int(m_pPlayer->pev->punchangle.x * 100), int(m_pPlayer->pev->punchangle.y * 100), FALSE, FALSE);

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(flCycleTime);

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	{
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", SUIT_SENTENCE, SUIT_REPEAT_OK);
	}

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.9f;

	const gw::RecoilPoint recoil = gw::GetRecoil(config, m_ModernState.recoilIndex);
	m_pPlayer->pev->punchangle.x -= recoil.vertical * config.viewScale;
	m_pPlayer->pev->punchangle.y += recoil.horizontal * config.viewScale;
	gw::CommitShot(config, m_ModernState, gpGlobals->time);
}

void CAK47::Reload()
{
	if (DefaultReload(iMaxClip(), AK47_RELOAD, AK47_RELOAD_TIME))
	{
		m_pPlayer->SetAnimation(PLAYER_RELOAD);

		m_flAccuracy = 0.2f;
		m_iShotsFired = 0;
		ResetAK47Mechanics(this);
		m_bDelayFire = false;
	}
}

void CAK47::WeaponIdle()
{
	ResetEmptySound();
	m_pPlayer->GetAutoaimVector(AUTOAIM_10DEGREES);

	if (m_flTimeWeaponIdle <= UTIL_WeaponTimeBase())
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 20.0f;
		SendWeaponAnim(AK47_IDLE1, UseDecrement() != FALSE);
	}
}
