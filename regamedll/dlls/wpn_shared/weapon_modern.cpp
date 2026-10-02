#include "precompiled.h"

namespace
{
const char *ProfileName(int id)
{
	switch (id)
	{
	case WEAPON_P228: return "p228"; case WEAPON_SCOUT: return "scout"; case WEAPON_XM1014: return "xm1014";
	case WEAPON_MAC10: return "mac10"; case WEAPON_AUG: return "aug"; case WEAPON_ELITE: return "elite";
	case WEAPON_FIVESEVEN: return "fiveseven"; case WEAPON_UMP45: return "ump45"; case WEAPON_SG550: return "sg550";
	case WEAPON_GALIL: return "galil"; case WEAPON_FAMAS: return "famas"; case WEAPON_USP: return "usp";
	case WEAPON_GLOCK18: return "glock18"; case WEAPON_AWP: return "awp"; case WEAPON_MP5N: return "mp5navy";
	case WEAPON_M249: return "m249"; case WEAPON_M3: return "m3"; case WEAPON_M4A1: return "m4a1";
	case WEAPON_TMP: return "tmp"; case WEAPON_G3SG1: return "g3sg1"; case WEAPON_DEAGLE: return "deagle";
	case WEAPON_SG552: return "sg552"; case WEAPON_AK47: return "ak47"; case WEAPON_P90: return "p90";
	default: return 0;
	}
}
gw::WeaponMechanicsConfig g_profiles[WEAPON_P90 + 1];
bool g_loaded[WEAPON_P90 + 1] = { false };

struct NamedProfile
{
	const char *name;
	bool loaded;
	gw::WeaponMechanicsConfig config;
};

NamedProfile g_namedProfiles[] = {
	{ "famas_burst", false }, { "glock18_burst", false }, { "usp_suppressed", false },
	{ "awp_scoped", false }, { "scout_scoped", false }, { "g3sg1_scoped", false }, { "sg550_scoped", false }
};
}

const gw::WeaponMechanicsConfig &CBasePlayerWeapon::ModernMechanics()
{
	const int id = m_iId >= 0 && m_iId <= WEAPON_P90 ? m_iId : 0;
	if (!g_loaded[id])
	{
		g_loaded[id] = true;
		g_profiles[id] = gw::Defaults(true);
		const char *name = ProfileName(id);
		if (name)
		{
			char path[96]; sprintf(path, "configs/weapons/%s.json", name);
			int length = 0; char *json = (char *)LOAD_FILE_FOR_ME(path, &length);
			gw::WeaponMechanicsConfig parsed;
			if (json && gw::ParseConfig(json, parsed)) g_profiles[id] = parsed;
			else ALERT(at_console, "Gloveworks: invalid/missing %s; using safe defaults\n", path);
			if (json) FREE_FILE(json);
		}
	}
	return g_profiles[id];
}

const gw::WeaponMechanicsConfig &CBasePlayerWeapon::ModernMechanics(const char *profileName)
{
	for (unsigned int index = 0; index < sizeof(g_namedProfiles) / sizeof(g_namedProfiles[0]); ++index)
	{
		NamedProfile &profile = g_namedProfiles[index];
		if (strcmp(profile.name, profileName)) continue;
		if (!profile.loaded)
		{
			profile.loaded = true;
			profile.config = gw::Defaults(false);
			char path[96]; sprintf(path, "configs/weapons/%s.json", profile.name);
			int length = 0; char *json = (char *)LOAD_FILE_FOR_ME(path, &length);
			gw::WeaponMechanicsConfig parsed;
			if (json && gw::ParseConfig(json, parsed)) profile.config = parsed;
			else ALERT(at_console, "Gloveworks: invalid/missing %s; using safe defaults\n", path);
			if (json) FREE_FILE(json);
		}
		return profile.config;
	}
	return ModernMechanics();
}

float CBasePlayerWeapon::ModernInaccuracy()
{
	return ModernInaccuracy(ModernMechanics());
}

float CBasePlayerWeapon::ModernInaccuracy(const gw::WeaponMechanicsConfig &config)
{
	gw::UpdateState(config, m_ModernState, gpGlobals->time, (m_pPlayer->pev->flags & FL_DUCKING) != 0);
	return gw::ComputeInaccuracy(config, m_ModernState, m_pPlayer->pev->velocity.Length2D(), m_pPlayer->pev->velocity.z, GetMaxSpeed(),
		(m_pPlayer->pev->flags & FL_DUCKING) != 0, (m_pPlayer->pev->flags & FL_ONGROUND) != 0,
		m_pPlayer->pev->movetype == MOVETYPE_FLY);
}

void CBasePlayerWeapon::ApplyModernRecoil()
{
	ApplyModernRecoil(ModernMechanics());
}

void CBasePlayerWeapon::ApplyModernRecoil(const gw::WeaponMechanicsConfig &config, bool sequential)
{
	const gw::RecoilPoint recoil = gw::GetRecoil(config, m_ModernState.recoilIndex,
		(config.fullAuto || sequential) ? -1 : m_pPlayer->random_seed);
	m_pPlayer->pev->vuser1.x -= recoil.vertical;
	m_pPlayer->pev->vuser1.y += recoil.horizontal;
	m_pPlayer->pev->fuser4 = 1.0f;
	gw::CommitShot(config, m_ModernState, gpGlobals->time);
}

void CBasePlayerWeapon::ResetModernMechanics()
{
	m_ModernState.firePenalty = 0.0f; m_ModernState.recoilIndex = 0.0f; m_ModernState.lastShotTime = 0.0f;
}
