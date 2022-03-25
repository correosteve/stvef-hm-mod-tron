/*
Format:
CVAR_DEF( vmcvar, name, default, flags, announce )
*/

// don't override the cheat state set by the system
CVAR_DEF( g_cheats, "sv_cheats", "", 0, qfalse )

// noset vars
CVAR_DEF( gamename, "gamename", GAMEVERSION , CVAR_SERVERINFO | CVAR_ROM, qfalse )
CVAR_DEF( gamedate, "gamedate", __DATE__ , CVAR_ROM, qfalse )
CVAR_DEF( g_restarted, "g_restarted", "0", CVAR_ROM, qfalse  )

// latched vars
CVAR_DEF( g_gametype, "g_gametype", "0", CVAR_SERVERINFO | CVAR_LATCH, qfalse )
CVAR_DEF( g_pModAssimilation, "g_pModAssimilation", "0", CVAR_SERVERINFO | CVAR_LATCH, qfalse )
CVAR_DEF( g_pModDisintegration, "g_pModDisintegration", "0", CVAR_SERVERINFO | CVAR_LATCH, qfalse )
CVAR_DEF( g_pModActionHero, "g_pModActionHero", "0", CVAR_SERVERINFO | CVAR_LATCH, qfalse )
CVAR_DEF( g_pModSpecialties, "g_pModSpecialties", "0", CVAR_SERVERINFO | CVAR_LATCH, qfalse )
CVAR_DEF( g_pModElimination, "g_pModElimination", "0", CVAR_SERVERINFO | CVAR_LATCH, qfalse )

CVAR_DEF( g_maxclients, "sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, qfalse )
CVAR_DEF( g_maxGameClients, "g_maxGameClients", "0", CVAR_SERVERINFO | CVAR_LATCH | CVAR_ARCHIVE, qfalse )

// change anytime vars
CVAR_DEF( g_dmflags, "dmflags", "0", CVAR_SERVERINFO | CVAR_ARCHIVE, qtrue )
CVAR_DEF( g_fraglimit, "fraglimit", "20", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, qtrue )
CVAR_DEF( g_timelimit, "timelimit", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, qtrue )
CVAR_DEF( g_timelimitWinningTeam, "timelimitWinningTeam", "", CVAR_NORESTART, qtrue )
CVAR_DEF( g_capturelimit, "capturelimit", "8", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_NORESTART, qtrue )

CVAR_DEF( g_synchronousClients, "g_synchronousClients", "0", CVAR_SYSTEMINFO, qfalse )

CVAR_DEF( g_friendlyFire, "g_friendlyFire", "0", CVAR_SERVERINFO | CVAR_ARCHIVE, qtrue )

CVAR_DEF( g_teamAutoJoin, "g_teamAutoJoin", "0", CVAR_ARCHIVE, qfalse )
CVAR_DEF( g_teamForceBalance, "g_teamForceBalance", "1", CVAR_ARCHIVE, qfalse )

CVAR_DEF( g_intermissionTime, "g_intermissionTime", "20", CVAR_ARCHIVE, qtrue )
CVAR_DEF( g_warmup, "g_warmup", "20", CVAR_ARCHIVE, qtrue )
CVAR_DEF( g_doWarmup, "g_doWarmup", "0", CVAR_ARCHIVE, qtrue )
CVAR_DEF( g_log, "g_log", "games.log", CVAR_ARCHIVE, qfalse )
CVAR_DEF( g_logSync, "g_logSync", "0", CVAR_ARCHIVE, qfalse )

CVAR_DEF( g_password, "g_password", "", CVAR_USERINFO, qfalse )

CVAR_DEF( g_banIPs, "g_banIPs", "", CVAR_ARCHIVE, qfalse )
CVAR_DEF( g_filterBan, "g_filterBan", "1", CVAR_ARCHIVE, qfalse )

CVAR_DEF( g_needpass, "g_needpass", "0", CVAR_SERVERINFO | CVAR_ROM, qfalse )

CVAR_DEF( g_dedicated, "dedicated", "0", 0, qfalse )

CVAR_DEF( g_speed, "g_speed", "250", CVAR_SERVERINFO | CVAR_ARCHIVE, qtrue )				// Quake 3 default was 320.
CVAR_DEF( g_gravity, "g_gravity", "800", CVAR_SERVERINFO | CVAR_ARCHIVE, qtrue )
CVAR_DEF( g_knockback, "g_knockback", "500", 0, qtrue )
CVAR_DEF( g_dmgmult, "g_dmgmult", "1", 0, qtrue )
CVAR_DEF( g_weaponRespawn, "g_weaponrespawn", "5", 0, qtrue )		// Quake 3 default (with 1 ammo weapons) was 5.
CVAR_DEF( g_adaptRespawn, "g_adaptrespawn", "1", 0, qtrue )		// Make weapons respawn faster with a lot of players.
CVAR_DEF( g_forcerespawn, "g_forcerespawn", "0", 0, qtrue )		// Quake 3 default was 20. This is more "user friendly".
CVAR_DEF( g_inactivity, "g_inactivity", "0", 0, qtrue )
CVAR_DEF( g_debugMove, "g_debugMove", "0", 0, qfalse )
CVAR_DEF( g_debugDamage, "g_debugDamage", "0", 0, qfalse )
CVAR_DEF( g_debugAlloc, "g_debugAlloc", "0", 0, qfalse )
CVAR_DEF( g_motd, "g_motd", "", 0, qfalse )

CVAR_DEF( g_podiumDist, "g_podiumDist", "80", 0, qfalse )
CVAR_DEF( g_podiumDrop, "g_podiumDrop", "70", 0, qfalse )

CVAR_DEF( g_allowVote, "g_allowVote", "1", CVAR_SERVERINFO, qfalse )

#if 0
CVAR_DEF( g_debugForward, "g_debugForward", "0", 0, qfalse )
CVAR_DEF( g_debugRight, "g_debugRight", "0", 0, qfalse )
CVAR_DEF( g_debugUp, "g_debugUp", "0", 0, qfalse )
#endif

CVAR_DEF( g_language, "g_language", "", CVAR_ARCHIVE, qfalse )

CVAR_DEF( g_holoIntro, "g_holoIntro", "1", CVAR_ARCHIVE, qfalse )
CVAR_DEF( g_ghostRespawn, "g_ghostRespawn", "5", CVAR_ARCHIVE, qfalse )		// How long the player is ghosted, in seconds.
CVAR_DEF( g_team_group_red, "g_team_group_red", "", CVAR_LATCH, qfalse )		// Used to have CVAR_ARCHIVE
CVAR_DEF( g_team_group_blue, "g_team_group_blue", "", CVAR_LATCH, qfalse )		// Used to have CVAR_ARCHIVE
CVAR_DEF( g_random_skin_limit, "g_random_skin_limit", "4", CVAR_ARCHIVE, qfalse )
CVAR_DEF( g_noJoinTimeout, "g_noJoinTimeout", "120", CVAR_ARCHIVE, qfalse )
CVAR_DEF( g_classChangeDebounceTime, "g_classChangeDebounceTime", "180", CVAR_ARCHIVE, qfalse )

CVAR_DEF( g_pMoveFixed, "g_pMoveFixed", "1", CVAR_ARCHIVE, qfalse )
CVAR_DEF( g_pMoveMsec, "g_pMoveMsec", "8", CVAR_ARCHIVE, qfalse )
CVAR_DEF( g_noJumpKeySlowdown, "g_noJumpKeySlowdown", "0", CVAR_ARCHIVE, qfalse )
CVAR_DEF( g_infilJumpFactor, "g_infilJumpFactor", "0", CVAR_ARCHIVE, qfalse )
CVAR_DEF( g_infilAirAccelFactor, "g_infilAirAccelFactor", "0", CVAR_ARCHIVE, qfalse )
CVAR_DEF( g_altSwapSupport, "g_altSwapSupport", "1", CVAR_ARCHIVE, qfalse )