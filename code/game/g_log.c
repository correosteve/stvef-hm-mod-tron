#include "g_local.h"

#define LOGGING_WEAPONS

/*
=================
G_LogPrintf

Print to the logfile with a time stamp if it is open
=================
*/
void QDECL G_LogPrintf( const char *fmt, ... ) {
	va_list		argptr;
	char		string[1024];
	int			min, tens, sec;

	sec = level.time / 1000;

	min = sec / 60;
	sec -= min * 60;
	tens = sec / 10;
	sec -= tens * 10;

	Com_sprintf( string, sizeof(string), "%3i:%i%i ", min, tens, sec );

	va_start( argptr, fmt );
	vsprintf( string +7 , fmt,argptr );
	va_end( argptr );

	if ( g_dedicated.integer ) {
		G_Printf( "%s", string + 7 );
	}

	if ( !level.logFile ) {
		return;
	}

	trap_FS_Write( string, strlen( string ), level.logFile );
}

/*
================
G_LogExit

Append information about this game to the log file
================
*/
void G_LogExit( const char *string ) {
	int				i, numSorted;
	gclient_t		*cl;

	G_LogPrintf( "Exit: %s\n", string );

	level.intermissionQueued = level.time;
	G_SetMatchState( MS_INTERMISSION_QUEUED );

	// this will keep the clients from playing any voice sounds
	// that will get cut off when the queued intermission starts
	trap_SetConfigstring( CS_INTERMISSION, "1" );

	// don't send more than 32 scores (FIXME?)
	numSorted = level.numConnectedClients;
	if ( numSorted > 32 ) {
		numSorted = 32;
	}

	if ( g_gametype.integer >= GT_TEAM ) {
		G_LogPrintf( "red:%i  blue:%i\n",
			level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE] );
	}

	for (i=0 ; i < numSorted ; i++) {
		int		ping;

		cl = &level.clients[level.sortedClients[i]];

		if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}
		if ( cl->pers.connected == CON_CONNECTING ) {
			continue;
		}

		ping = cl->ps.ping < 999 ? cl->ps.ping : 999;

		G_LogPrintf( "score: %i  ping: %i  client: %i %s\n",
			modfn.EffectiveScore( level.sortedClients[i], EST_REALSCORE ),
			ping, level.sortedClients[i], cl->pers.netname );
	}
}




// Weapon statistic logging.
// Nothing super-fancy here, I just want to keep track of, per player:
//		--hom many times a weapon/item is picked up
//		--how many times a weapon/item is used/fired
//		--the total damage done by that weapon
//		--the number of kills by that weapon
//		--the number of deaths while holding that weapon
//		--the time spent with each weapon
//
// Additionally,
//		--how many times each powerup or item is picked up


#ifdef LOGGING_WEAPONS
int G_WeaponLogPickups[MAX_CLIENTS][WP_NUM_WEAPONS];
int G_WeaponLogFired[MAX_CLIENTS][WP_NUM_WEAPONS];
int G_WeaponLogDamage[MAX_CLIENTS][MOD_MAX];
int G_WeaponLogKills[MAX_CLIENTS][MOD_MAX];
int G_WeaponLogDeaths[MAX_CLIENTS][WP_NUM_WEAPONS];
int G_WeaponLogFrags[MAX_CLIENTS][MAX_CLIENTS];
int G_WeaponLogTime[MAX_CLIENTS][WP_NUM_WEAPONS];
int G_WeaponLogLastTime[MAX_CLIENTS];
qboolean G_WeaponLogClientTouch[MAX_CLIENTS];
int G_WeaponLogPowerups[MAX_CLIENTS][HI_NUM_HOLDABLE];
int	G_WeaponLogItems[MAX_CLIENTS][PW_NUM_POWERUPS];

// MOD-weapon mapping array.
int weaponFromMOD[MOD_MAX] =
{
	WP_NONE,				// MOD_UNKNOWN,

	WP_NONE,				// MOD_WATER,
	WP_NONE,				// MOD_SLIME,
	WP_NONE,				// MOD_LAVA,
	WP_NONE,				// MOD_CRUSH,
	WP_NONE,				// MOD_TELEFRAG,
	WP_NONE,				// MOD_FALLING,
	WP_NONE,				// MOD_SUICIDE,
	WP_NONE,				// MOD_TARGET_LASER,
	WP_NONE,				// MOD_TRIGGER_HURT,

	WP_PHASER,				// MOD_PHASER,
	WP_PHASER,				// MOD_PHASER_ALT,
	WP_COMPRESSION_RIFLE,	// MOD_CRIFLE,
	WP_COMPRESSION_RIFLE,	// MOD_CRIFLE_SPLASH,
	WP_COMPRESSION_RIFLE,	// MOD_CRIFLE_ALT,
	WP_COMPRESSION_RIFLE,	// MOD_CRIFLE_ALT_SPLASH,
	WP_IMOD,				// MOD_IMOD,
	WP_IMOD,				// MOD_IMOD_ALT,
	WP_SCAVENGER_RIFLE,		// MOD_SCAVENGER,
	WP_SCAVENGER_RIFLE,		// MOD_SCAVENGER_ALT,
	WP_SCAVENGER_RIFLE,		// MOD_SCAVENGER_ALT_SPLASH,
	WP_STASIS,				// MOD_STASIS,
	WP_STASIS,				// MOD_STASIS_ALT,
	WP_GRENADE_LAUNCHER,	// MOD_GRENADE,
	WP_GRENADE_LAUNCHER,	// MOD_GRENADE_ALT,
	WP_GRENADE_LAUNCHER,	// MOD_GRENADE_SPLASH,
	WP_GRENADE_LAUNCHER,	// MOD_GRENADE_ALT_SPLASH,
	WP_TETRION_DISRUPTOR,	// MOD_TETRION,
	WP_TETRION_DISRUPTOR,	// MOD_TETRION_ALT,
	WP_DREADNOUGHT,			// MOD_DREADNOUGHT,
	WP_DREADNOUGHT,			// MOD_DREADNOUGHT_ALT,
	WP_QUANTUM_BURST,		// MOD_QUANTUM,
	WP_QUANTUM_BURST,		// MOD_QUANTUM_SPLASH,
	WP_QUANTUM_BURST,		// MOD_QUANTUM_ALT,
	WP_QUANTUM_BURST,		// MOD_QUANTUM_ALT_SPLASH,

	// careful!! don't assume that any given map entry refers to a weapon index. see
	//GetFavoriteWeaponForClient() as an example of what can go wrong.

	HI_DETPACK,				// MOD_DETPACK,
	PW_SEEKER,				// MOD_SEEKER

	//expansion pack
	WP_VOYAGER_HYPO,		// MOD_KNOCKOUT,
	WP_BORG_ASSIMILATOR,	// MOD_ASSIMILATE,
	WP_BORG_WEAPON,			// MOD_BORG,
	WP_BORG_WEAPON,			// MOD_BORG_ALT

	WP_NONE,				// MOD_RESPAWN,
	WP_NONE,				// MOD_EXPLOSION,
};

char *weaponNameFromIndex[WP_NUM_WEAPONS] =
{
	"No Weapon",
	"Phaser",
	"Compression",
	"IMOD",
	"Scavenger",
	"Stasis",
	"Grenade",
	"Tetrion",
	"Quantum",
	"Dreadnought",
	"Hypo",
	"Assimilator",
	"Borg Weapon"
};

extern char	*modNames[];

#endif //LOGGING_WEAPONS

/*
=================
G_LogWeaponInit
=================
*/
void G_LogWeaponInit(void) {
#ifdef LOGGING_WEAPONS
	memset(G_WeaponLogPickups, 0, sizeof(G_WeaponLogPickups));
	memset(G_WeaponLogFired, 0, sizeof(G_WeaponLogFired));
	memset(G_WeaponLogDamage, 0, sizeof(G_WeaponLogDamage));
	memset(G_WeaponLogKills, 0, sizeof(G_WeaponLogKills));
	memset(G_WeaponLogDeaths, 0, sizeof(G_WeaponLogDeaths));
	memset(G_WeaponLogFrags, 0, sizeof(G_WeaponLogFrags));
	memset(G_WeaponLogTime, 0, sizeof(G_WeaponLogTime));
	memset(G_WeaponLogLastTime, 0, sizeof(G_WeaponLogLastTime));
	memset(G_WeaponLogPowerups, 0, sizeof(G_WeaponLogPowerups));
	memset(G_WeaponLogItems, 0, sizeof(G_WeaponLogItems));
#endif //LOGGING_WEAPONS
}

void QDECL G_LogWeaponPickup(int client, int weaponid)
{
#ifdef LOGGING_WEAPONS
	G_WeaponLogPickups[client][weaponid]++;
	G_WeaponLogClientTouch[client] = qtrue;
#endif //_LOGGING_WEAPONS
}

void QDECL G_LogWeaponFire(int client, int weaponid)
{
#ifdef LOGGING_WEAPONS
	int dur;

	G_WeaponLogFired[client][weaponid]++;
	dur = level.time - G_WeaponLogLastTime[client];
	if (dur > 5000)		// 5 second max.
		G_WeaponLogTime[client][weaponid] += 5000;
	else
		G_WeaponLogTime[client][weaponid] += dur;
	G_WeaponLogLastTime[client] = level.time;
	G_WeaponLogClientTouch[client] = qtrue;
#endif //_LOGGING_WEAPONS
}

void QDECL G_LogWeaponDamage(int client, int mod, int amount)
{
#ifdef LOGGING_WEAPONS
	if (client>=MAX_CLIENTS)
		return;
	G_WeaponLogDamage[client][mod] += amount;
	G_WeaponLogClientTouch[client] = qtrue;
#endif //_LOGGING_WEAPONS
}

void QDECL G_LogWeaponKill(int client, int mod)
{
#ifdef LOGGING_WEAPONS
	if (client>=MAX_CLIENTS)
		return;
	G_WeaponLogKills[client][mod]++;
	G_WeaponLogClientTouch[client] = qtrue;
#endif //_LOGGING_WEAPONS
}

void QDECL G_LogWeaponFrag(int attacker, int deadguy)
{
#ifdef LOGGING_WEAPONS
	if ( (attacker>=MAX_CLIENTS) || (deadguy>=MAX_CLIENTS) )
		return;
	G_WeaponLogFrags[attacker][deadguy]++;
	G_WeaponLogClientTouch[attacker] = qtrue;
#endif //_LOGGING_WEAPONS
}

void QDECL G_LogWeaponDeath(int client, int weaponid)
{
#ifdef LOGGING_WEAPONS
	if (client>=MAX_CLIENTS)
		return;
	G_WeaponLogDeaths[client][weaponid]++;
	G_WeaponLogClientTouch[client] = qtrue;
#endif //_LOGGING_WEAPONS
}

void QDECL G_LogWeaponPowerup(int client, int powerupid)
{
#ifdef LOGGING_WEAPONS
	if (client>=MAX_CLIENTS)
		return;
	G_WeaponLogPowerups[client][powerupid]++;
	G_WeaponLogClientTouch[client] = qtrue;
#endif //_LOGGING_WEAPONS
}

void QDECL G_LogWeaponItem(int client, int itemid)
{
#ifdef LOGGING_WEAPONS
	if (client>=MAX_CLIENTS)
		return;
	G_WeaponLogItems[client][itemid]++;
	G_WeaponLogClientTouch[client] = qtrue;
#endif //_LOGGING_WEAPONS
}


// Run through each player.  Print out:
//	-- Most commonly picked up weapon.
//  -- Weapon with which the most time was spent.
//  -- Weapon that was most often died with.
//  -- Damage type with which the most damage was done.
//  -- Damage type with the most kills.
//  -- Weapon with which the most damage was done.
//	-- Weapon with which the most damage was done per shot.
//
// For the whole game, print out:
//  -- Total pickups of each weapon.
//  -- Total time spent with each weapon.
//  -- Total damage done with each weapon.
//  -- Total damage done for each damage type.
//  -- Number of kills with each weapon.
//  -- Number of kills for each damage type.
//  -- Damage per shot with each weapon.
//  -- Number of deaths with each weapon.

void G_LogWeaponOutput(void)
{
#if 0//LOGGING_WEAPONS
	int i,j,curwp;
	float pershot;
	fileHandle_t weaponfile;
	char string[1024];

	int totalpickups[WP_NUM_WEAPONS];
	int totaltime[WP_NUM_WEAPONS];
	int totaldeaths[WP_NUM_WEAPONS];
	int totaldamageMOD[MOD_MAX];
	int totalkillsMOD[MOD_MAX];
	int totaldamage[WP_NUM_WEAPONS];
	int totalkills[WP_NUM_WEAPONS];
	int totalshots[WP_NUM_WEAPONS];
	int percharacter[WP_NUM_WEAPONS];
	char info[1024];
	char mapname[128];
	char *nameptr, *unknownname="<Unknown>";

	G_LogPrintf("*****************************Weapon Log:\n" );

	memset(totalpickups, 0, sizeof(totalpickups));
	memset(totaltime, 0, sizeof(totaltime));
	memset(totaldeaths, 0, sizeof(totaldeaths));
	memset(totaldamageMOD, 0, sizeof(totaldamageMOD));
	memset(totalkillsMOD, 0, sizeof(totalkillsMOD));
	memset(totaldamage, 0, sizeof(totaldamage));
	memset(totalkills, 0, sizeof(totalkills));
	memset(totalshots, 0, sizeof(totalshots));

	for (i=0; i<MAX_CLIENTS; i++)
	{
		if (G_WeaponLogClientTouch[i])
		{	// Ignore any entity/clients we don't care about!
			for (j=0;j<WP_NUM_WEAPONS;j++)
			{
				totalpickups[j] += G_WeaponLogPickups[i][j];
				totaltime[j] += G_WeaponLogTime[i][j];
				totaldeaths[j] += G_WeaponLogDeaths[i][j];
				totalshots[j] += G_WeaponLogFired[i][j];
			}

			for (j=0;j<MOD_MAX;j++)
			{
				totaldamageMOD[j] += G_WeaponLogDamage[i][j];
				totalkillsMOD[j] += G_WeaponLogKills[i][j];
			}
		}
	}

	// Now total the weapon data from the MOD data.
	for (j=0; j<MOD_MAX; j++)
	{
		if (j <= MOD_QUANTUM_ALT_SPLASH)
		{
			curwp = weaponFromMOD[j];
			totaldamage[curwp] += totaldamageMOD[j];
			totalkills[curwp] += totalkillsMOD[j];
		}
	}

	G_LogPrintf(  "\n****Data by Weapon:\n" );
	for (j=0; j<WP_NUM_WEAPONS; j++)
	{
		G_LogPrintf("%15s:  Pickups: %4d,  Time:  %5d,  Deaths: %5d\n",
				weaponNameFromIndex[j], totalpickups[j], (int)(totaltime[j]/1000), totaldeaths[j]);
	}

	G_LogPrintf(  "\n****Combat Data by Weapon:\n" );
	for (j=0; j<WP_NUM_WEAPONS; j++)
	{
		if (totalshots[j] > 0)
		{
			pershot = (float)(totaldamage[j])/(float)(totalshots[j]);
		}
		else
		{
			pershot = 0;
		}
		G_LogPrintf("%15s:  Damage: %6d,  Kills: %5d,  Dmg per Shot: %f\n",
				weaponNameFromIndex[j], totaldamage[j], totalkills[j], pershot);
	}

	G_LogPrintf(  "\n****Combat Data By Damage Type:\n" );
	for (j=0; j<MOD_MAX; j++)
	{
		G_LogPrintf("%25s:  Damage: %6d,  Kills: %5d\n",
				modNames[j], totaldamageMOD[j], totalkillsMOD[j]);
	}

	G_LogPrintf("\n");



	// Write the whole weapon statistic log out to a file.
	trap_FS_FOpenFile( ""/*g_weaponlog.string*/, &weaponfile, FS_APPEND );
	if (!weaponfile) {	//failed to open file, let's not crash, shall we?
		return;
	}

	// Write out the level name
	trap_GetServerinfo(info, sizeof(info));
	strncpy(mapname, Info_ValueForKey( info, "mapname" ), sizeof(mapname)-1);
	mapname[sizeof(mapname)-1] = '\0';

	Com_sprintf(string, sizeof(string), "\n\n\nLevel:\t%s\n\n\n", mapname);
	trap_FS_Write( string, strlen( string ), weaponfile);


	// Combat data per character

	// Start with Pickups per character
	Com_sprintf(string, sizeof(string), "Weapon Pickups per Player:\n\n");
	trap_FS_Write( string, strlen( string ), weaponfile);

	Com_sprintf(string, sizeof(string), "Player");
	trap_FS_Write(string, strlen(string), weaponfile);

	for (j=0; j<WP_NUM_WEAPONS; j++)
	{
		Com_sprintf(string, sizeof(string), "\t%s", weaponNameFromIndex[j]);
		trap_FS_Write(string, strlen(string), weaponfile);
	}
	Com_sprintf(string, sizeof(string), "\n");
	trap_FS_Write(string, strlen(string), weaponfile);

	// Cycle through each player, give their name and the number of times they picked up each weapon.
	for (i=0; i<MAX_CLIENTS; i++)
	{
		if (G_WeaponLogClientTouch[i])
		{	// Ignore any entity/clients we don't care about!
			if ( g_entities[i].client )
			{
				nameptr = g_entities[i].client->pers.netname;
			}
			else
			{
				nameptr = unknownname;
			}
			trap_FS_Write(nameptr, strlen(nameptr), weaponfile);

			for (j=0;j<WP_NUM_WEAPONS;j++)
			{
				Com_sprintf(string, sizeof(string), "\t%d", G_WeaponLogPickups[i][j]);
				trap_FS_Write(string, strlen(string), weaponfile);
			}

			Com_sprintf(string, sizeof(string), "\n");
			trap_FS_Write(string, strlen(string), weaponfile);
		}
	}

	// Sum up the totals.
	Com_sprintf(string, sizeof(string), "\n***TOTAL:");
	trap_FS_Write(string, strlen(string), weaponfile);

	for (j=0;j<WP_NUM_WEAPONS;j++)
	{
		Com_sprintf(string, sizeof(string), "\t%d", totalpickups[j]);
		trap_FS_Write(string, strlen(string), weaponfile);
	}

	Com_sprintf(string, sizeof(string), "\n\n\n");
	trap_FS_Write(string, strlen(string), weaponfile);


	// Weapon fires per character
	Com_sprintf(string, sizeof(string), "Weapon Shots per Player:\n\n");
	trap_FS_Write( string, strlen( string ), weaponfile);

	Com_sprintf(string, sizeof(string), "Player");
	trap_FS_Write(string, strlen(string), weaponfile);

	for (j=0; j<WP_NUM_WEAPONS; j++)
	{
		Com_sprintf(string, sizeof(string), "\t%s", weaponNameFromIndex[j]);
		trap_FS_Write(string, strlen(string), weaponfile);
	}
	Com_sprintf(string, sizeof(string), "\n");
	trap_FS_Write(string, strlen(string), weaponfile);

	// Cycle through each player, give their name and the number of times they picked up each weapon.
	for (i=0; i<MAX_CLIENTS; i++)
	{
		if (G_WeaponLogClientTouch[i])
		{	// Ignore any entity/clients we don't care about!
			if ( g_entities[i].client )
			{
				nameptr = g_entities[i].client->pers.netname;
			}
			else
			{
				nameptr = unknownname;
			}
			trap_FS_Write(nameptr, strlen(nameptr), weaponfile);

			for (j=0;j<WP_NUM_WEAPONS;j++)
			{
				Com_sprintf(string, sizeof(string), "\t%d", G_WeaponLogFired[i][j]);
				trap_FS_Write(string, strlen(string), weaponfile);
			}

			Com_sprintf(string, sizeof(string), "\n");
			trap_FS_Write(string, strlen(string), weaponfile);
		}
	}

	// Sum up the totals.
	Com_sprintf(string, sizeof(string), "\n***TOTAL:");
	trap_FS_Write(string, strlen(string), weaponfile);

	for (j=0;j<WP_NUM_WEAPONS;j++)
	{
		Com_sprintf(string, sizeof(string), "\t%d", totalshots[j]);
		trap_FS_Write(string, strlen(string), weaponfile);
	}

	Com_sprintf(string, sizeof(string), "\n\n\n");
	trap_FS_Write(string, strlen(string), weaponfile);


	// Weapon time per character
	Com_sprintf(string, sizeof(string), "Weapon Use Time per Player:\n\n");
	trap_FS_Write( string, strlen( string ), weaponfile);

	Com_sprintf(string, sizeof(string), "Player");
	trap_FS_Write(string, strlen(string), weaponfile);

	for (j=0; j<WP_NUM_WEAPONS; j++)
	{
		Com_sprintf(string, sizeof(string), "\t%s", weaponNameFromIndex[j]);
		trap_FS_Write(string, strlen(string), weaponfile);
	}
	Com_sprintf(string, sizeof(string), "\n");
	trap_FS_Write(string, strlen(string), weaponfile);

	// Cycle through each player, give their name and the number of times they picked up each weapon.
	for (i=0; i<MAX_CLIENTS; i++)
	{
		if (G_WeaponLogClientTouch[i])
		{	// Ignore any entity/clients we don't care about!
			if ( g_entities[i].client )
			{
				nameptr = g_entities[i].client->pers.netname;
			}
			else
			{
				nameptr = unknownname;
			}
			trap_FS_Write(nameptr, strlen(nameptr), weaponfile);

			for (j=0;j<WP_NUM_WEAPONS;j++)
			{
				Com_sprintf(string, sizeof(string), "\t%d", G_WeaponLogTime[i][j]);
				trap_FS_Write(string, strlen(string), weaponfile);
			}

			Com_sprintf(string, sizeof(string), "\n");
			trap_FS_Write(string, strlen(string), weaponfile);
		}
	}

	// Sum up the totals.
	Com_sprintf(string, sizeof(string), "\n***TOTAL:");
	trap_FS_Write(string, strlen(string), weaponfile);

	for (j=0;j<WP_NUM_WEAPONS;j++)
	{
		Com_sprintf(string, sizeof(string), "\t%d", totaltime[j]);
		trap_FS_Write(string, strlen(string), weaponfile);
	}

	Com_sprintf(string, sizeof(string), "\n\n\n");
	trap_FS_Write(string, strlen(string), weaponfile);



	// Weapon deaths per character
	Com_sprintf(string, sizeof(string), "Weapon Deaths per Player:\n\n");
	trap_FS_Write( string, strlen( string ), weaponfile);

	Com_sprintf(string, sizeof(string), "Player");
	trap_FS_Write(string, strlen(string), weaponfile);

	for (j=0; j<WP_NUM_WEAPONS; j++)
	{
		Com_sprintf(string, sizeof(string), "\t%s", weaponNameFromIndex[j]);
		trap_FS_Write(string, strlen(string), weaponfile);
	}
	Com_sprintf(string, sizeof(string), "\n");
	trap_FS_Write(string, strlen(string), weaponfile);

	// Cycle through each player, give their name and the number of times they picked up each weapon.
	for (i=0; i<MAX_CLIENTS; i++)
	{
		if (G_WeaponLogClientTouch[i])
		{	// Ignore any entity/clients we don't care about!
			if ( g_entities[i].client )
			{
				nameptr = g_entities[i].client->pers.netname;
			}
			else
			{
				nameptr = unknownname;
			}
			trap_FS_Write(nameptr, strlen(nameptr), weaponfile);

			for (j=0;j<WP_NUM_WEAPONS;j++)
			{
				Com_sprintf(string, sizeof(string), "\t%d", G_WeaponLogDeaths[i][j]);
				trap_FS_Write(string, strlen(string), weaponfile);
			}

			Com_sprintf(string, sizeof(string), "\n");
			trap_FS_Write(string, strlen(string), weaponfile);
		}
	}

	// Sum up the totals.
	Com_sprintf(string, sizeof(string), "\n***TOTAL:");
	trap_FS_Write(string, strlen(string), weaponfile);

	for (j=0;j<WP_NUM_WEAPONS;j++)
	{
		Com_sprintf(string, sizeof(string), "\t%d", totaldeaths[j]);
		trap_FS_Write(string, strlen(string), weaponfile);
	}

	Com_sprintf(string, sizeof(string), "\n\n\n");
	trap_FS_Write(string, strlen(string), weaponfile);




	// Weapon damage per character

	Com_sprintf(string, sizeof(string), "Weapon Damage per Player:\n\n");
	trap_FS_Write( string, strlen( string ), weaponfile);

	Com_sprintf(string, sizeof(string), "Player");
	trap_FS_Write(string, strlen(string), weaponfile);

	for (j=0; j<WP_NUM_WEAPONS; j++)
	{
		Com_sprintf(string, sizeof(string), "\t%s", weaponNameFromIndex[j]);
		trap_FS_Write(string, strlen(string), weaponfile);
	}
	Com_sprintf(string, sizeof(string), "\n");
	trap_FS_Write(string, strlen(string), weaponfile);

	// Cycle through each player, give their name and the number of times they picked up each weapon.
	for (i=0; i<MAX_CLIENTS; i++)
	{
		if (G_WeaponLogClientTouch[i])
		{	// Ignore any entity/clients we don't care about!

			// We must grab the totals from the damage types for the player and map them to the weapons.
			memset(percharacter, 0, sizeof(percharacter));
			for (j=0; j<MOD_MAX; j++)
			{
				if (j <= MOD_QUANTUM_ALT_SPLASH)
				{
					curwp = weaponFromMOD[j];
					percharacter[curwp] += G_WeaponLogDamage[i][j];
				}
			}

			if ( g_entities[i].client )
			{
				nameptr = g_entities[i].client->pers.netname;
			}
			else
			{
				nameptr = unknownname;
			}
			trap_FS_Write(nameptr, strlen(nameptr), weaponfile);

			for (j=0;j<WP_NUM_WEAPONS;j++)
			{
				Com_sprintf(string, sizeof(string), "\t%d", percharacter[j]);
				trap_FS_Write(string, strlen(string), weaponfile);
			}

			Com_sprintf(string, sizeof(string), "\n");
			trap_FS_Write(string, strlen(string), weaponfile);
		}
	}

	// Sum up the totals.
	Com_sprintf(string, sizeof(string), "\n***TOTAL:");
	trap_FS_Write(string, strlen(string), weaponfile);

	for (j=0;j<WP_NUM_WEAPONS;j++)
	{
		Com_sprintf(string, sizeof(string), "\t%d", totaldamage[j]);
		trap_FS_Write(string, strlen(string), weaponfile);
	}

	Com_sprintf(string, sizeof(string), "\n\n\n");
	trap_FS_Write(string, strlen(string), weaponfile);



	// Weapon kills per character

	Com_sprintf(string, sizeof(string), "Weapon Kills per Player:\n\n");
	trap_FS_Write( string, strlen( string ), weaponfile);

	Com_sprintf(string, sizeof(string), "Player");
	trap_FS_Write(string, strlen(string), weaponfile);

	for (j=0; j<WP_NUM_WEAPONS; j++)
	{
		Com_sprintf(string, sizeof(string), "\t%s", weaponNameFromIndex[j]);
		trap_FS_Write(string, strlen(string), weaponfile);
	}
	Com_sprintf(string, sizeof(string), "\n");
	trap_FS_Write(string, strlen(string), weaponfile);

	// Cycle through each player, give their name and the number of times they picked up each weapon.
	for (i=0; i<MAX_CLIENTS; i++)
	{
		if (G_WeaponLogClientTouch[i])
		{	// Ignore any entity/clients we don't care about!

			// We must grab the totals from the damage types for the player and map them to the weapons.
			memset(percharacter, 0, sizeof(percharacter));
			for (j=0; j<MOD_MAX; j++)
			{
				if (j <= MOD_QUANTUM_ALT_SPLASH)
				{
					curwp = weaponFromMOD[j];
					percharacter[curwp] += G_WeaponLogKills[i][j];
				}
			}

			if ( g_entities[i].client )
			{
				nameptr = g_entities[i].client->pers.netname;
			}
			else
			{
				nameptr = unknownname;
			}
			trap_FS_Write(nameptr, strlen(nameptr), weaponfile);

			for (j=0;j<WP_NUM_WEAPONS;j++)
			{
				Com_sprintf(string, sizeof(string), "\t%d", percharacter[j]);
				trap_FS_Write(string, strlen(string), weaponfile);
			}

			Com_sprintf(string, sizeof(string), "\n");
			trap_FS_Write(string, strlen(string), weaponfile);
		}
	}

	// Sum up the totals.
	Com_sprintf(string, sizeof(string), "\n***TOTAL:");
	trap_FS_Write(string, strlen(string), weaponfile);

	for (j=0;j<WP_NUM_WEAPONS;j++)
	{
		Com_sprintf(string, sizeof(string), "\t%d", totalkills[j]);
		trap_FS_Write(string, strlen(string), weaponfile);
	}

	Com_sprintf(string, sizeof(string), "\n\n\n");
	trap_FS_Write(string, strlen(string), weaponfile);



	// Damage type damage per character
	Com_sprintf(string, sizeof(string), "Typed Damage per Player:\n\n");
	trap_FS_Write( string, strlen( string ), weaponfile);

	Com_sprintf(string, sizeof(string), "Player");
	trap_FS_Write(string, strlen(string), weaponfile);

	for (j=0; j<MOD_MAX; j++)
	{
		Com_sprintf(string, sizeof(string), "\t%s", modNames[j]);
		trap_FS_Write(string, strlen(string), weaponfile);
	}
	Com_sprintf(string, sizeof(string), "\n");
	trap_FS_Write(string, strlen(string), weaponfile);

	// Cycle through each player, give their name and the number of times they picked up each weapon.
	for (i=0; i<MAX_CLIENTS; i++)
	{
		if (G_WeaponLogClientTouch[i])
		{	// Ignore any entity/clients we don't care about!
			if ( g_entities[i].client )
			{
				nameptr = g_entities[i].client->pers.netname;
			}
			else
			{
				nameptr = unknownname;
			}
			trap_FS_Write(nameptr, strlen(nameptr), weaponfile);

			for (j=0;j<MOD_MAX;j++)
			{
				Com_sprintf(string, sizeof(string), "\t%d", G_WeaponLogDamage[i][j]);
				trap_FS_Write(string, strlen(string), weaponfile);
			}

			Com_sprintf(string, sizeof(string), "\n");
			trap_FS_Write(string, strlen(string), weaponfile);
		}
	}

	// Sum up the totals.
	Com_sprintf(string, sizeof(string), "\n***TOTAL:");
	trap_FS_Write(string, strlen(string), weaponfile);

	for (j=0;j<MOD_MAX;j++)
	{
		Com_sprintf(string, sizeof(string), "\t%d", totaldamageMOD[j]);
		trap_FS_Write(string, strlen(string), weaponfile);
	}

	Com_sprintf(string, sizeof(string), "\n\n\n");
	trap_FS_Write(string, strlen(string), weaponfile);



	// Damage type kills per character
	Com_sprintf(string, sizeof(string), "Damage-Typed Kills per Player:\n\n");
	trap_FS_Write( string, strlen( string ), weaponfile);

	Com_sprintf(string, sizeof(string), "Player");
	trap_FS_Write(string, strlen(string), weaponfile);

	for (j=0; j<MOD_MAX; j++)
	{
		Com_sprintf(string, sizeof(string), "\t%s", modNames[j]);
		trap_FS_Write(string, strlen(string), weaponfile);
	}
	Com_sprintf(string, sizeof(string), "\n");
	trap_FS_Write(string, strlen(string), weaponfile);

	// Cycle through each player, give their name and the number of times they picked up each weapon.
	for (i=0; i<MAX_CLIENTS; i++)
	{
		if (G_WeaponLogClientTouch[i])
		{	// Ignore any entity/clients we don't care about!
			if ( g_entities[i].client )
			{
				nameptr = g_entities[i].client->pers.netname;
			}
			else
			{
				nameptr = unknownname;
			}
			trap_FS_Write(nameptr, strlen(nameptr), weaponfile);

			for (j=0;j<MOD_MAX;j++)
			{
				Com_sprintf(string, sizeof(string), "\t%d", G_WeaponLogKills[i][j]);
				trap_FS_Write(string, strlen(string), weaponfile);
			}

			Com_sprintf(string, sizeof(string), "\n");
			trap_FS_Write(string, strlen(string), weaponfile);
		}
	}

	// Sum up the totals.
	Com_sprintf(string, sizeof(string), "\n***TOTAL:");
	trap_FS_Write(string, strlen(string), weaponfile);

	for (j=0;j<MOD_MAX;j++)
	{
		Com_sprintf(string, sizeof(string), "\t%d", totalkillsMOD[j]);
		trap_FS_Write(string, strlen(string), weaponfile);
	}

	Com_sprintf(string, sizeof(string), "\n\n\n");
	trap_FS_Write(string, strlen(string), weaponfile);


	trap_FS_FCloseFile(weaponfile);


#endif //LOGGING_WEAPONS
}

//
// kef -- synchronized with ui_local.h
//
/*
typedef enum {
	AWARD_EFFICIENCY,		// Accuracy
	AWARD_SHARPSHOOTER,		// Most compression rifle frags
	AWARD_UNTOUCHABLE,		// Perfect (no deaths)
	AWARD_LOGISTICS,		// Most pickups
	AWARD_TACTICIAN,		// Kills with all weapons
	AWARD_DEMOLITIONIST,	// Most explosive damage kills
	AWARD_STREAK,			// Ace/Expert/Master/Champion
	AWARD_TEAM,				// MVP/Defender/Warrior/Carrier/Interceptor/Bravery
	AWARD_SECTION31,		// All-around god
	AWARD_MAX
} awardType_t;

typedef enum
{
	TEAM_NONE,				// ha ha! you suck!
	TEAM_MVP,				// most overall points
	TEAM_DEFENDER,			// killed the most baddies near your flag
	TEAM_WARRIOR,			// most frags
	TEAM_CARRIER,			// infected the most people with plague
	TEAM_INTERCEPTOR,		// returned your own flag the most
	TEAM_BRAVERY,			// Red Shirt Award (tm). you died more than anybody.
	TEAM_MAX
} teamAward_e;
*/

// did this player earn the efficiency award?
qboolean CalculateEfficiency(gentity_t *ent, int *efficiency)
{
#ifdef LOGGING_WEAPONS
	float		fAccuracyRatio = 0, fBestRatio = 0;
	int			i = 0, nShotsFired = 0, nShotsHit = 0, nBestPlayer = -1, tempEff = 0;
	gentity_t	*player = NULL;


	for (i = 0; i < g_maxclients.integer; i++)
	{
		player = g_entities + i;
		if (!player->inuse)
			continue;
		nShotsFired = player->client->ps.persistant[PERS_ACCURACY_SHOTS];
		nShotsHit = player->client->ps.persistant[PERS_ACCURACY_HITS];
		fAccuracyRatio = ( ((float)nShotsHit)/((float)nShotsFired) );
		if (fAccuracyRatio > fBestRatio)
		{
			fBestRatio = fAccuracyRatio;
			nBestPlayer = i;
		}
	}
	if (-1 == nBestPlayer)
	{
		// huh?
		return qfalse;
	}
	if (nBestPlayer == ent->s.number)
	{
		tempEff = (int)(100*fBestRatio);
		if (tempEff > 50)
		{
			*efficiency = tempEff;
			return qtrue;
		}
		return qfalse;
	}
#endif // LOGGING_WEAPONS
	return qfalse;
}

// did this player earn the sharpshooter award?
qboolean CalculateSharpshooter(gentity_t *ent, int *frags)
{
#ifdef LOGGING_WEAPONS
	int			i = 0, nBestPlayer = -1, nKills = 0, nMostKills = 0,
				playTime = (level.time - ent->client->pers.enterTime)/60000;
	gentity_t	*player = NULL;

	// if this guy didn't get one kill per minute, reject him right now
	if ( ((float)(G_WeaponLogKills[ent-g_entities][MOD_CRIFLE_ALT]))/((float)(playTime)) < 1.0 )
	{
		return qfalse;
	}

	for (i = 0; i < g_maxclients.integer; i++)
	{
		nKills = 0;
		player = g_entities + i;
		if (!player->inuse)
			continue;
		nKills = G_WeaponLogKills[i][MOD_CRIFLE_ALT];
		if (nKills > nMostKills)
		{
			nMostKills = nKills;
			nBestPlayer = i;
		}
	}
	if (-1 == nBestPlayer)
	{
		return qfalse;
	}
	if (nBestPlayer == ent->s.number)
	{
		*frags = nMostKills;
		return qtrue;
	}
#endif // LOGGING_WEAPONS
	return qfalse;
}

// did this player earn the untouchable award?
qboolean CalculateUntouchable(gentity_t *ent)
{
#ifdef LOGGING_WEAPONS
	int			playTime;
	playTime = (level.time - ent->client->pers.enterTime)/60000;

	if ( modfn.IsBorgQueen( ent - g_entities ) )
	{//Borg queen can only be killed once anyway
		return qfalse;
	}
	//------------------------------------------------------ MUST HAVE ACHIEVED 2 KILLS PER MINUTE
	if ( ((float)ent->client->pers.teamState.frags)/((float)(playTime)) < 2.0  || playTime==0)
		return qfalse;
	//------------------------------------------------------ MUST HAVE ACHIEVED 2 KILLS PER MINUTE


	// if this guy was never killed...  Award Away!!!
	if (ent->client->ps.persistant[PERS_KILLED]==0)
		return qtrue;

#endif // LOGGING_WEAPONS
	return qfalse;
}

// did this player earn the logistics award?
qboolean CalculateLogistics(gentity_t *ent, int *stuffUsed)
{
#ifdef LOGGING_WEAPONS
	int			i = 0, j = 0, nBestPlayer = -1, nStuffUsed = 0, nMostStuffUsed = 0,
				nDifferent = 0, nMostDifferent = 0;
	gentity_t	*player = NULL;

	for (i = 0; i < g_maxclients.integer; i++)
	{
		nStuffUsed = 0;
		nDifferent = 0;
		player = g_entities + i;
		if (!player->inuse)
			continue;
		for (j = HI_NONE+1; j < HI_NUM_HOLDABLE; j++)
		{
			if (G_WeaponLogPowerups[i][j])
			{
				nDifferent++;
			}
			nStuffUsed += G_WeaponLogPowerups[i][j];
		}
		for (j = PW_NONE+1; j < PW_NUM_POWERUPS; j++)
		{
			if (G_WeaponLogItems[i][j])
			{
				nDifferent++;
			}
			nStuffUsed += G_WeaponLogItems[i][j];
		}
		if ( (nDifferent >= 4) && (nDifferent >= nMostDifferent) )
		{
			if (nStuffUsed > nMostStuffUsed)
			{
				nMostDifferent = nDifferent;
				nMostStuffUsed = nStuffUsed;
				nBestPlayer = i;
			}
		}
	}
	if (-1 == nBestPlayer)
	{
		return qfalse;
	}
	if (nBestPlayer == ent->s.number)
	{
		*stuffUsed = nMostDifferent;
		return qtrue;
	}
#endif // LOGGING_WEAPONS
	return qfalse;
}




// did this player earn the tactician award?
qboolean CalculateTactician(gentity_t *ent, int *kills)
{
#ifdef LOGGING_WEAPONS
	int			i = 0, nBestPlayer = -1, nKills = 0, nMostKills = 0;
	int			person = 0, weapon = 0;
	gentity_t	*player = NULL;
	int			wasPickedUpBySomeone[WP_NUM_WEAPONS];
	int			killsWithWeapon[WP_NUM_WEAPONS];
	int			playTime = (level.time - ent->client->pers.enterTime)/60000;

	if ( modfn.AdjustGeneralConstant( GC_DISABLE_TACTICIAN, 0 ) )
	{
		return qfalse;
	}
	if ( modfn.IsBorgQueen( ent - g_entities ) )
	{//Borg queen has only 1 weapon
		return qfalse;
	}
	//------------------------------------------------------ MUST HAVE ACHIEVED 2 KILLS PER MINUTE
	if (playTime<0.3)
		return qfalse;

	if ( ((float)ent->client->pers.teamState.frags)/((float)(playTime)) < 2.0 )
		return qfalse;
	//------------------------------------------------------ MUST HAVE ACHIEVED 2 KILLS PER MINUTE




	//------------------------------------------------------ FOR EVERY WEAPON, ADD UP TOTAL PICKUPS
	for (weapon = 0; weapon<WP_NUM_WEAPONS; weapon++)
			wasPickedUpBySomeone[weapon] = 0;				// CLEAR

	for (person=0; person<g_maxclients.integer; person++)
	{
		for (weapon = 0; weapon<WP_NUM_WEAPONS; weapon++)
		{
			if (G_WeaponLogPickups[person][weapon]>0)
				wasPickedUpBySomeone[weapon]++;
		}
	}
	//------------------------------------------------------ FOR EVERY WEAPON, ADD UP TOTAL PICKUPS




	//------------------------------------------------------ FOR EVERY PERSON, CHECK FOR CANDIDATE
	for (person=0; person<g_maxclients.integer; person++)
	{
		player = g_entities + person;
		if (!player->inuse)			continue;

		nKills = 0;											// This Persons's Kills
		for (weapon=0; weapon<WP_NUM_WEAPONS; weapon++)
			killsWithWeapon[weapon] = 0;					// CLEAR

		for (i=0; i<MOD_MAX; i++)
		{
			weapon = weaponFromMOD[i];									// Select Weapon
			killsWithWeapon[weapon] += G_WeaponLogKills[person][i];		// Store Num Kills With Weapon
		}

		weapon=WP_PHASER;		// Start At Phaser
		//   keep looking through weapons if weapon is not on map, or if it is and we used it
		while( weapon<WP_NUM_WEAPONS && (!wasPickedUpBySomeone[weapon] || killsWithWeapon[weapon]>0) )
		{
			weapon++;
			nKills+=killsWithWeapon[weapon];							//  Update the number of kills
		}
		//
		// At this point we have either successfully gone through every weapon on the map and saw it had
		// been used, or we found one that WAS on the map and was NOT used
		//
		// so we look to see if the weapon==Max (i.e. we used every one) and then we check to see
		// if we got the most kills out of anyone else who did this.
		//
		if (weapon>=WP_NUM_WEAPONS && nKills>nMostKills)
		{
			// WE ARE A TACTICION CANDIDATE
			nMostKills  = nKills;
			nBestPlayer = person;
		}
	}
	//------------------------------------------------------ FOR EVERY PERSON, CHECK FOR CANDIDATE

	//Now, if we are the best player, return true and the number of kills we got
	if (nBestPlayer == ent->s.number)
	{
		*kills = nMostKills;
		return qtrue;
	}
#endif // LOGGING_WEAPONS
	return qfalse;
}




// did this player earn the demolitionist award?
qboolean CalculateDemolitionist(gentity_t *ent, int *kills)
{
#ifdef LOGGING_WEAPONS
	int			i = 0, nBestPlayer = -1, nKills = 0, nMostKills = 0,
				playTime = (level.time - ent->client->pers.enterTime)/60000;
	gentity_t	*player = NULL;

	for (i = 0; i < g_maxclients.integer; i++)
	{
		nKills = 0;
		player = g_entities + i;
		if (!player->inuse)
			continue;

		nKills = G_WeaponLogKills[i][MOD_GRENADE];
		nKills += G_WeaponLogKills[i][MOD_GRENADE_SPLASH];
		nKills += G_WeaponLogKills[i][MOD_GRENADE_ALT_SPLASH];
		nKills += G_WeaponLogKills[i][MOD_QUANTUM];
		nKills += G_WeaponLogKills[i][MOD_QUANTUM_SPLASH];
		nKills += G_WeaponLogKills[i][MOD_QUANTUM_ALT];
		nKills += G_WeaponLogKills[i][MOD_QUANTUM_ALT_SPLASH];
		nKills += G_WeaponLogKills[i][MOD_DETPACK];

		// if this guy didn't get two explosive kills per minute, reject him right now
		if ( ((float)nKills)/((float)(playTime)) < 2.0 )
		{
			continue;
		}

		if (nKills > nMostKills)
		{
			nMostKills = nKills;
			nBestPlayer = i;
		}
	}
	if (-1 == nBestPlayer)
	{
		return qfalse;
	}
	if (nBestPlayer == ent->s.number)
	{
		*kills = nMostKills;
		return qtrue;
	}
#endif // LOGGING_WEAPONS
	return qfalse;
}

int CalculateStreak(gentity_t *ent)
{
	if (ent->client->ps.persistant[PERS_STREAK_COUNT] >= STREAK_CHAMPION)
	{
		return STREAK_CHAMPION;
	}
	if (ent->client->ps.persistant[PERS_STREAK_COUNT] >= STREAK_MASTER)
	{
		return STREAK_MASTER;
	}
	if (ent->client->ps.persistant[PERS_STREAK_COUNT] >= STREAK_EXPERT)
	{
		return STREAK_EXPERT;
	}
	if (ent->client->ps.persistant[PERS_STREAK_COUNT] >= STREAK_ACE)
	{
		return STREAK_ACE;
	}
	return 0;
}

/*
==================
GetTeamMVP

Returns client number of MVP of specified team, or -1 if no match found.
==================
*/
int GetTeamMVP( team_t team ) {
	int i;

	// Always select borg queen as MVP
	for ( i = 0; i < level.numPlayingClients; ++i ) {
		int clientNum = level.sortedClients[i];
		if ( level.clients[clientNum].sess.sessionTeam == team && modfn.IsBorgQueen( clientNum ) ) {
			return clientNum;
		}
	}

	for ( i = 0; i < level.numPlayingClients; ++i ) {
		int clientNum = level.sortedClients[i];
		if ( level.clients[clientNum].sess.sessionTeam == team ) {
			return clientNum;
		}
	}

	return -1;
}

/*
==================
GetWinningTeamMVP

Returns client number of MVP of winning team, or overall MVP if teams are tied or winning team has no players.
Returns -1 if no match found.
==================
*/
int GetWinningTeamMVP( void ) {
	int MVP = -1;

	if ( level.winningTeam != TEAM_FREE ) {
		// Try to get MVP from winning team
		MVP = GetTeamMVP( level.winningTeam );
	}

	if ( MVP < 0 ) {
		// Tied or no valid MVP, so just pick the top player overall
		int i;
		for ( i = 0; i < level.numPlayingClients; ++i ) {
			int clientNum = level.sortedClients[i];
			if ( level.clients[clientNum].sess.sessionTeam != TEAM_SPECTATOR ) {
				MVP = clientNum;
				break;
			}
		}
	}

	return MVP;
}

qboolean CalculateTeamMVP(gentity_t *ent)
{
	// To get MVP award you need to be the team MVP and have a score higher than 0
	int MVP = GetTeamMVP( ent->client->sess.sessionTeam );
	return ent - g_entities == MVP && modfn.EffectiveScore( MVP, EST_REALSCORE ) > 0;
}

qboolean CalculateTeamDefender(gentity_t *ent)
{
	int			i = 0, nBestPlayer = -1, nScore = 0, nHighestScore = 0,
				team = ent->client->ps.persistant[PERS_TEAM];
	gentity_t	*player = NULL;

	/*
	if (CalculateTeamMVP(ent))
	{
		return qfalse;
	}
	*/
	for (i = 0; i < g_maxclients.integer; i++)
	{
		nScore = 0;
		player = g_entities + i;
		if (!player->inuse || (player->client->ps.persistant[PERS_TEAM] != team))
			continue;
		nScore = player->client->pers.teamState.basedefense;
		if (nScore > nHighestScore)
		{
			nHighestScore = nScore;
			nBestPlayer = i;
		}
	}
	if (-1 == nBestPlayer)
	{
		return qfalse;
	}
	if (nBestPlayer == ent->s.number)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean CalculateTeamWarrior(gentity_t *ent)
{
	int			i = 0, nBestPlayer = -1, nScore = 0, nHighestScore = 0,
				team = ent->client->ps.persistant[PERS_TEAM];
	gentity_t	*player = NULL;

	/*
	if (CalculateTeamMVP(ent) || CalculateTeamDefender(ent))
	{
		return qfalse;
	}
	*/
	for (i = 0; i < g_maxclients.integer; i++)
	{
		nScore = 0;
		player = g_entities + i;
		if (!player->inuse || (player->client->ps.persistant[PERS_TEAM] != team))
			continue;
		nScore = player->client->pers.teamState.frags;
		if (nScore > nHighestScore)
		{
			nHighestScore = nScore;
			nBestPlayer = i;
		}
	}
	if (-1 == nBestPlayer)
	{
		return qfalse;
	}
	if (nBestPlayer == ent->s.number)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean CalculateTeamCarrier(gentity_t *ent)
{
	int			i = 0, nBestPlayer = -1, nScore = 0, nHighestScore = 0,
				team = ent->client->ps.persistant[PERS_TEAM];
	gentity_t	*player = NULL;

	/*
	if (CalculateTeamMVP(ent) || CalculateTeamDefender(ent) || CalculateTeamWarrior(ent))
	{
		return qfalse;
	}
	*/
	for (i = 0; i < g_maxclients.integer; i++)
	{
		nScore = 0;
		player = g_entities + i;
		if (!player->inuse || (player->client->ps.persistant[PERS_TEAM] != team))
			continue;
		nScore = player->client->pers.teamState.captures;
		if (nScore > nHighestScore)
		{
			nHighestScore = nScore;
			nBestPlayer = i;
		}
	}
	if (-1 == nBestPlayer)
	{
		return qfalse;
	}
	if (nBestPlayer == ent->s.number)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean CalculateTeamInterceptor(gentity_t *ent)
{
	int			i = 0, nBestPlayer = -1, nScore = 0, nHighestScore = 0,
				team = ent->client->ps.persistant[PERS_TEAM];
	gentity_t	*player = NULL;

	/*
	if (CalculateTeamMVP(ent) || CalculateTeamDefender(ent) || CalculateTeamWarrior(ent) ||
		CalculateTeamCarrier(ent))
	{
		return qfalse;
	}
	*/
	for (i = 0; i < g_maxclients.integer; i++)
	{
		nScore = 0;
		player = g_entities + i;
		if (!player->inuse || (player->client->ps.persistant[PERS_TEAM] != team))
			continue;
		nScore = player->client->pers.teamState.flagrecovery;
		nScore += player->client->pers.teamState.fragcarrier;
		if (nScore > nHighestScore)
		{
			nHighestScore = nScore;
			nBestPlayer = i;
		}
	}
	if (-1 == nBestPlayer)
	{
		return qfalse;
	}
	if (nBestPlayer == ent->s.number)
	{
		return qtrue;
	}
	return qfalse;
}

qboolean CalculateTeamRedShirt(gentity_t *ent)
{
	int			i = 0, nBestPlayer = -1, nScore = 0, nHighestScore = 0,
				team = ent->client->ps.persistant[PERS_TEAM];
	gentity_t	*player = NULL;

	/*
	if (CalculateTeamMVP(ent) || CalculateTeamDefender(ent) || CalculateTeamWarrior(ent) ||
		CalculateTeamCarrier(ent) || CalculateTeamInterceptor(ent))
	{
		return qfalse;
	}
	*/
	for (i = 0; i < g_maxclients.integer; i++)
	{
		nScore = 0;
		player = g_entities + i;
		if (!player->inuse || (player->client->ps.persistant[PERS_TEAM] != team))
			continue;
		nScore = player->client->ps.persistant[PERS_KILLED];
		nScore -= player->client->pers.teamState.suicides; // suicides don't count, you big cheater.
		if (nScore > nHighestScore)
		{
			nHighestScore = nScore;
			nBestPlayer = i;
		}
	}
	if (-1 == nBestPlayer)
	{
		return qfalse;
	}
	if (nBestPlayer == ent->s.number)
	{
		return qtrue;
	}
	return qfalse;
}

int CalculateTeamAward(gentity_t *ent)
{
	int teamAwards = 0;

	if (CalculateTeamMVP(ent))
	{
		teamAwards |= (1<<TEAM_MVP);
	}
	if (GT_CTF == g_gametype.integer)
	{
		if (CalculateTeamDefender(ent))
		{
			teamAwards |= (1<<TEAM_DEFENDER);
		}
		if (CalculateTeamWarrior(ent))
		{
			teamAwards |= (1<<TEAM_WARRIOR);
		}
		if (CalculateTeamCarrier(ent))
		{
			teamAwards |= (1<<TEAM_CARRIER);
		}
		if (CalculateTeamInterceptor(ent))
		{
			teamAwards |= (1<<TEAM_INTERCEPTOR);
		}
	}
	if ( !teamAwards && CalculateTeamRedShirt(ent) )
	{//if you got nothing else and died a lot, at least get bravery
		teamAwards |= (1<<TEAM_BRAVERY);
	}
	return teamAwards;
}

qboolean CalculateSection31Award(gentity_t *ent)
{
	int			i = 0, frags = 0, efficiency = 0;
	gentity_t	*player = NULL;

	for (i = 0; i < g_maxclients.integer; i++)
	{
		player = g_entities + i;
		if (!player->inuse)
			continue;
//
//	kef -- heh.
//
//		if (strcmp("JaxxonPhred", ent->client->pers.netname))
//		{
//			continue;
//		}
		CalculateEfficiency(ent, &efficiency);
		if (!CalculateSharpshooter(ent, &frags) ||
			!CalculateUntouchable(ent) ||
			(CalculateStreak(ent) < STREAK_CHAMPION) ||
			(efficiency < 75))
		{
			continue;
		}
		return qtrue;
	}
	return qfalse;
}

/*
================
(ModFN) CalculateAwards

Generates awards message string for specified client.
================
*/
void ModFNDefault_CalculateAwards( int clientNum, char *msg ) {
#ifdef LOGGING_WEAPONS
	gentity_t	*ent = &g_entities[clientNum];
	char		buf1[AWARDS_MSG_LENGTH], buf2[AWARDS_MSG_LENGTH];
	int			awardFlags = 0, efficiency = 0, stuffUsed = 0, kills = 0, streak = 0, teamAwards = 0;
	int			awardCount = 0;		// maximum of 4 awards, to avoid issues with buggy cgame AW_SPPostgameMenu_f

	memset(buf1, 0, AWARDS_MSG_LENGTH);
	memset(buf2, 0, AWARDS_MSG_LENGTH);
	if (CalculateEfficiency(ent, &efficiency) && awardCount < 4)
	{
		awardFlags |= (1<<AWARD_EFFICIENCY);
		Com_sprintf(buf1, AWARDS_MSG_LENGTH, " %d", efficiency);
		++awardCount;
	}
	if (CalculateSharpshooter(ent, &kills) && awardCount < 4)
	{
		awardFlags |= (1<<AWARD_SHARPSHOOTER);
		strcpy(buf2, buf1);
		Com_sprintf(buf1, AWARDS_MSG_LENGTH, "%s %d", buf2, kills);
		++awardCount;
	}
	if (CalculateUntouchable(ent) && awardCount < 4)
	{
		awardFlags |= (1<<AWARD_UNTOUCHABLE);
		strcpy(buf2, buf1);
		Com_sprintf(buf1, AWARDS_MSG_LENGTH, "%s %d", buf2, 0);
		++awardCount;
	}
	if (CalculateLogistics(ent, &stuffUsed) && awardCount < 4)
	{
		awardFlags |= (1<<AWARD_LOGISTICS);
		strcpy(buf2, buf1);
		Com_sprintf(buf1, AWARDS_MSG_LENGTH, "%s %d", buf2, stuffUsed);
		++awardCount;
	}
	if (CalculateTactician(ent, &kills) && awardCount < 4)
	{
		awardFlags |= (1<<AWARD_TACTICIAN);
		strcpy(buf2, buf1);
		Com_sprintf(buf1, AWARDS_MSG_LENGTH, "%s %d", buf2, kills);
		++awardCount;
	}
	if (CalculateDemolitionist(ent, &kills) && awardCount < 4)
	{
		awardFlags |= (1<<AWARD_DEMOLITIONIST);
		strcpy(buf2, buf1);
		Com_sprintf(buf1, AWARDS_MSG_LENGTH, "%s %d", buf2, kills);
		++awardCount;
	}
	streak = CalculateStreak(ent);
	if (streak && awardCount < 4)
	{
		awardFlags |= (1<<AWARD_STREAK);
		strcpy(buf2, buf1);
		Com_sprintf(buf1, AWARDS_MSG_LENGTH, "%s %d", buf2, streak);
		++awardCount;
	}
	if (g_gametype.integer >= GT_TEAM)
	{
		teamAwards = CalculateTeamAward(ent);
		if (teamAwards && awardCount < 4)
		{
			awardFlags |= (1<<AWARD_TEAM);
			strcpy(buf2, buf1);
			Com_sprintf(buf1, AWARDS_MSG_LENGTH, "%s %d", buf2, teamAwards);
			++awardCount;
		}
	}
	if (CalculateSection31Award(ent) && awardCount < 4)
	{
		awardFlags |= (1<<AWARD_SECTION31);
		strcpy(buf2, buf1);
		Com_sprintf(buf1, AWARDS_MSG_LENGTH, "%s %d", buf2, 0);
		++awardCount;
	}
	strcpy(buf2, msg);
	Com_sprintf( msg, AWARDS_MSG_LENGTH, "%s %d%s", buf2, awardFlags, buf1);
#endif // LOGGING_WEAPONS
}

int GetMaxDeathsForClient(int nClient)
{
	int i = 0, nMostDeaths = 0;

	if ((nClient < 0) || (nClient >= MAX_CLIENTS))
	{
		return 0;
	}
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (G_WeaponLogFrags[i][nClient] > nMostDeaths)
		{
			nMostDeaths = G_WeaponLogFrags[i][nClient];
		}
	}
	return nMostDeaths;
}

int GetMaxKillsForClient(int nClient)
{
	int i = 0, nMostKills = 0;

	if ((nClient < 0) || (nClient >= MAX_CLIENTS))
	{
		return 0;
	}
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (G_WeaponLogFrags[nClient][i] > nMostKills)
		{
			nMostKills = G_WeaponLogFrags[nClient][i];
		}
	}
	return nMostKills;
}

int GetFavoriteTargetForClient(int nClient)
{
	int i = 0, nMostKills = 0, nFavoriteTarget = -1;

	if ((nClient < 0) || (nClient >= MAX_CLIENTS))
	{
		return 0;
	}
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (G_WeaponLogFrags[nClient][i] > nMostKills)
		{
			nMostKills = G_WeaponLogFrags[nClient][i];
			nFavoriteTarget = i;
		}
	}
	return nFavoriteTarget;
}

int GetWorstEnemyForClient(int nClient)
{
	int i = 0, nMostDeaths = 0, nWorstEnemy = -1;

	if ((nClient < 0) || (nClient >= MAX_CLIENTS))
	{
		return 0;
	}
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		// If there is a tie for most deaths, we want to choose anybody else
		// over the client...  I.E. Most deaths should not tie with yourself and
		// have yourself show up...

		if ( G_WeaponLogFrags[i][nClient] > nMostDeaths ||
			(G_WeaponLogFrags[i][nClient]== nMostDeaths && i!=nClient && nMostDeaths!=0) )
		{
			nMostDeaths = G_WeaponLogFrags[i][nClient];
			nWorstEnemy = i;
		}
	}
	return nWorstEnemy;
}

int GetFavoriteWeaponForClient(int nClient)
{
	int i = 0, nMostKills = 0, fav=0, weapon=WP_PHASER;
	int	killsWithWeapon[WP_NUM_WEAPONS];


	// First thing we need to do is cycle through all the MOD types and convert
	// number of kills to a single weapon.
	//----------------------------------------------------------------
	for (weapon=0; weapon<WP_NUM_WEAPONS; weapon++)
		killsWithWeapon[weapon] = 0;					// CLEAR

	for (i=MOD_PHASER; i<=MOD_BORG_ALT; i++)
	{
		weapon = weaponFromMOD[i];									// Select Weapon
		killsWithWeapon[weapon] += G_WeaponLogKills[nClient][i];	// Store Num Kills With Weapon
	}

	// now look through our list of kills per weapon and pick the biggest
	//----------------------------------------------------------------
	nMostKills=0;
	for (weapon=WP_PHASER; weapon<WP_NUM_WEAPONS; weapon++)
	{
		if (killsWithWeapon[weapon]>nMostKills)
		{
			nMostKills = killsWithWeapon[weapon];
			fav = weapon;
		}
	}
	return fav;
}

// kef -- if a client leaves the game, clear out all counters he may have set
void QDECL G_ClearClientLog(int client)
{
	int i = 0;

	for (i = 0; i < WP_NUM_WEAPONS; i++)
	{
		G_WeaponLogPickups[client][i] = 0;
	}
	for (i = 0; i < WP_NUM_WEAPONS; i++)
	{
		G_WeaponLogFired[client][i] = 0;
	}
	for (i = 0; i < MOD_MAX; i++)
	{
		G_WeaponLogDamage[client][i] = 0;
	}
	for (i = 0; i < MOD_MAX; i++)
	{
		G_WeaponLogKills[client][i] = 0;
	}
	for (i = 0; i < WP_NUM_WEAPONS; i++)
	{
		G_WeaponLogDeaths[client][i] = 0;
	}
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		G_WeaponLogFrags[client][i] = 0;
	}
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		G_WeaponLogFrags[i][client] = 0;
	}
	for (i = 0; i < WP_NUM_WEAPONS; i++)
	{
		G_WeaponLogTime[client][i] = 0;
	}
	G_WeaponLogLastTime[client] = 0;
	G_WeaponLogClientTouch[client] = qfalse;
	for (i = 0; i < HI_NUM_HOLDABLE; i++)
	{
		G_WeaponLogPowerups[client][i] = 0;
	}
	for (i = 0; i < PW_NUM_POWERUPS; i++)
	{
		G_WeaponLogItems[client][i] = 0;
	}
}
