/*
* Specialties Mode
* 
* In this mode players can select from one of several classes: Heavy Weapons Specialist,
* Demolitionist, Technician, Medic, Sniper, and Infiltrator. It is usually played in
* combination with the CTF gametype.
*/

#include "mods/modes/specialties/specs_local.h"

#define PREFIX( x ) ModSpecialties_##x
#define MOD_STATE PREFIX( state )

typedef struct {
	// time of last class change
	int classChangeTime;
} specialties_client_t;

static struct {
	specialties_client_t clients[MAX_CLIENTS];
	trackedCvar_t g_classChangeDebounceTime;

	// For mod function stacking
	ModFNType_InitClientSession Prev_InitClientSession;
	ModFNType_PreClientSpawn Prev_PreClientSpawn;
	ModFNType_GenerateClientSessionStructure Prev_GenerateClientSessionStructure;
	ModFNType_ModClientCommand Prev_ModClientCommand;
	ModFNType_EffectiveHandicap Prev_EffectiveHandicap;
	ModFNType_SpawnConfigureClient Prev_SpawnConfigureClient;
	ModFNType_CheckItemSpawnDisabled Prev_CheckItemSpawnDisabled;
	ModFNType_CanItemBeDropped Prev_CanItemBeDropped;
	ModFNType_AddRegisteredItems Prev_AddRegisteredItems;
	ModFNType_KnockbackMass Prev_KnockbackMass;
	ModFNType_AdjustGeneralConstant Prev_AdjustGeneralConstant;
	ModFNType_AdjustWeaponConstant Prev_AdjustWeaponConstant;
	ModFNType_ModifyAmmoUsage Prev_ModifyAmmoUsage;
} *MOD_STATE;

/*
================
(ModFN) InitClientSession
================
*/
LOGFUNCTION_SVOID( PREFIX(InitClientSession), ( int clientNum, qboolean initialConnect, const info_string_t *info ),
		( clientNum, initialConnect, info ), "G_MODFN_INITCLIENTSESSION" ) {
	specialties_client_t *modclient = &MOD_STATE->clients[clientNum];

	MOD_STATE->Prev_InitClientSession( clientNum, initialConnect, info );
	memset( modclient, 0, sizeof( *modclient ) );
}

/*
================
(ModFN) PreClientSpawn

Reset class change time limit when player joins a new team.
================
*/
LOGFUNCTION_SVOID( PREFIX(PreClientSpawn), ( int clientNum, clientSpawnType_t spawnType ),
		( clientNum, spawnType ), "G_MODFN_PRECLIENTSPAWN" ) {
	specialties_client_t *modclient = &MOD_STATE->clients[clientNum];

	MOD_STATE->Prev_PreClientSpawn( clientNum, spawnType );

	if ( spawnType != CST_RESPAWN ) {
		modclient->classChangeTime = 0;
	}
}

/*
================
ModSpecialties_IsSpecialtiesClass
================
*/
static qboolean ModSpecialties_IsSpecialtiesClass( pclass_t pclass ) {
	if( pclass == PC_INFILTRATOR || pclass == PC_SNIPER || pclass == PC_HEAVY || pclass == PC_DEMO
				|| pclass == PC_MEDIC || pclass == PC_TECH )
		return qtrue;
	return qfalse;
}

/*
================
(ModFN) GenerateClientSessionStructure

Save specialties class for next round.
================
*/
LOGFUNCTION_SVOID( PREFIX(GenerateClientSessionStructure), ( int clientNum, clientSession_t *sess ),
		( clientNum, sess ), "G_MODFN_GENERATECLIENTSESSIONSTRUCTURE" ) {
	gclient_t *client = &level.clients[clientNum];
	pclass_t pclass;

	MOD_STATE->Prev_GenerateClientSessionStructure( clientNum, sess );

	pclass = modfn.RealSessionClass( clientNum );
	if ( ModSpecialties_IsSpecialtiesClass( pclass ) ) {
		sess->sessionClass = pclass;
	}
}

/*
================
ModSpecialties_TellClassCmd
================
*/
static void ModSpecialties_TellClassCmd( int clientNum ) {
	gclient_t *client = &level.clients[clientNum];
	const char *className = "";

	switch ( client->sess.sessionClass ) {
		case PC_INFILTRATOR: //fast: low attack
			className = "Infiltrator";
			break;
		case PC_SNIPER: //sneaky: snipe only
			className = "Sniper";
			break;
		case PC_HEAVY: //slow: heavy attack
			className = "Heavy";
			break;
		case PC_DEMO: //go boom
			className = "Demo";
			break;
		case PC_MEDIC: //heal
			className = "Medic";
			break;
		case PC_TECH: //operate
			className = "Tech";
			break;
		default:
			trap_SendServerCommand( clientNum, "print \"Unknown current class!\n\"" );
			return;
	}

	trap_SendServerCommand( clientNum, va( "print \"class: %s\n\"", className ) );
}

/*
=================
ModSpecialties_BroadCastClassChange

Let everyone know about a team change
=================
*/
static void ModSpecialties_BroadcastClassChange( gclient_t *client )
{
	switch( client->sess.sessionClass )
	{
	case PC_INFILTRATOR:
		trap_SendServerCommand( -1, va("cp \"%.15s" S_COLOR_WHITE " is now an Infiltrator.\n\"", client->pers.netname) );
		break;
	case PC_SNIPER:
		trap_SendServerCommand( -1, va("cp \"%.15s" S_COLOR_WHITE " is now a Sniper.\n\"", client->pers.netname) );
		break;
	case PC_HEAVY:
		trap_SendServerCommand( -1, va("cp \"%.15s" S_COLOR_WHITE " is now a Heavy Weapons Specialist.\n\"", client->pers.netname) );
		break;
	case PC_DEMO:
		trap_SendServerCommand( -1, va("cp \"%.15s" S_COLOR_WHITE " is now a Demolitionist.\n\"", client->pers.netname) );
		break;
	case PC_MEDIC:
		trap_SendServerCommand( -1, va("cp \"%.15s" S_COLOR_WHITE " is now a Medic.\n\"", client->pers.netname) );
		break;
	case PC_TECH:
		trap_SendServerCommand( -1, va("cp \"%.15s" S_COLOR_WHITE " is now a Technician.\n\"", client->pers.netname) );
		break;
	}
}

/*
================
ModSpecialties_SetClassCmd
================
*/
LOGFUNCTION_SVOID( ModSpecialties_SetClassCmd, ( int clientNum ), ( clientNum ), "" ) {
	gentity_t *ent = &g_entities[clientNum];
	gclient_t *client = &level.clients[clientNum];
	specialties_client_t *modclient = &MOD_STATE->clients[clientNum];
	pclass_t pclass = PC_NOCLASS;
	char s[MAX_TOKEN_CHARS];

	// Check if joining is disabled by mod
	if ( client->sess.sessionTeam != TEAM_SPECTATOR && !modfn.CheckJoinAllowed( clientNum, CJA_SETCLASS, TEAM_FREE ) ) {
		return;
	}

	// Apply time limit to repeat class changes
	if ( MOD_STATE->g_classChangeDebounceTime.integer > 0 && level.matchState == MS_ACTIVE &&
			modclient->classChangeTime && !modfn.SpectatorClient( clientNum ) &&
			modclient->classChangeTime + MOD_STATE->g_classChangeDebounceTime.integer * 1000 > level.time ) {
		int seconds, minutes = 0;
		seconds = ( modclient->classChangeTime + MOD_STATE->g_classChangeDebounceTime.integer * 1000 - level.time + 999 ) / 1000;
		if ( seconds >= 60 ) {
			minutes = floor( seconds / 60.0f );
			seconds -= minutes * 60;
			if ( minutes > 1 ) {
				if ( seconds ) {
					if ( seconds > 1 ) {
						trap_SendServerCommand( clientNum, va( "cp \"Cannot change classes again for %d minutes and %d seconds\"", minutes, seconds ) );
					} else {
						trap_SendServerCommand( clientNum, va( "cp \"Cannot change classes again for %d minutes\"", minutes ) );
					}
				} else {
					trap_SendServerCommand( clientNum, va( "cp \"Cannot change classes again for %d minutes\"", minutes ) );
				}
			} else {
				if ( seconds ) {
					if ( seconds > 1 ) {
						trap_SendServerCommand( clientNum, va( "cp \"Cannot change classes again for %d minute and %d seconds\"", minutes, seconds ) );
					} else {
						trap_SendServerCommand( clientNum, va( "cp \"Cannot change classes again for %d minute and %d second\"", minutes, seconds ) );
					}
				} else {
					trap_SendServerCommand( clientNum, va( "cp \"Cannot change classes again for %d minute\"", minutes ) );
				}
			}
		} else {
			if ( seconds > 1 ) {
				trap_SendServerCommand( clientNum, va( "cp \"Cannot change classes again for %d seconds\"", seconds ) );
			} else {
				trap_SendServerCommand( clientNum, va( "cp \"Cannot change classes again for %d second\"", seconds ) );
			}
		}
		return;
	}

	trap_Argv( 1, s, sizeof( s ) );

	if ( !Q_stricmp( s, "infiltrator" ) ) {
		pclass = PC_INFILTRATOR;
	} else if ( !Q_stricmp( s, "sniper" ) ) {
		pclass = PC_SNIPER;
	} else if ( !Q_stricmp( s, "heavy" ) ) {
		pclass = PC_HEAVY;
	} else if ( !Q_stricmp( s, "demo" ) ) {
		pclass = PC_DEMO;
	} else if ( !Q_stricmp( s, "medic" ) ) {
		pclass = PC_MEDIC;
	} else if ( !Q_stricmp( s, "tech" ) ) {
		pclass = PC_TECH;
	} else {
		pclass = irandom( PC_INFILTRATOR, PC_TECH );
	}

	if ( pclass == client->sess.sessionClass ) {
		// Make sure client has the correct class value in UI, just in case
		SetPlayerClassCvar( ent );
		return;
	}

	client->sess.sessionClass = pclass;
	ModSpecialties_BroadcastClassChange( client );
	ClientUserinfoChanged( clientNum );

	// Kill him (makes sure he loses flags, etc)
	player_die( ent, NULL, NULL, 100000, MOD_RESPAWN );

	ClientBegin( clientNum );

	modclient->classChangeTime = level.time;
}

/*
================
(ModFN) ModClientCommand

Handle class command.
================
*/
LOGFUNCTION_SRET( qboolean, PREFIX(ModClientCommand), ( int clientNum, const char *cmd ),
		( clientNum, cmd ), "G_MODFN_MODCLIENTCOMMAND" ) {
	if ( !Q_stricmp( cmd, "class" ) && !level.intermissiontime ) {
		if ( trap_Argc() != 2 ) {
			ModSpecialties_TellClassCmd( clientNum );
		} else {
			ModSpecialties_SetClassCmd( clientNum );
		}
		return qtrue;
	}

	return MOD_STATE->Prev_ModClientCommand( clientNum, cmd );
}

/*
================
(ModFN) UpdateSessionClass

Pick random class for players coming into the game.
================
*/
LOGFUNCTION_SVOID( PREFIX(UpdateSessionClass), ( int clientNum ),
		( clientNum ), "G_MODFN_UPDATESESSIONCLASS" ) {
	gentity_t *ent = &g_entities[clientNum];
	gclient_t *client = &level.clients[clientNum];

	if( !ModSpecialties_IsSpecialtiesClass( client->sess.sessionClass ) ) {
		client->sess.sessionClass = irandom( PC_INFILTRATOR, PC_TECH );
	}
}

/*
================
(ModFN) EffectiveHandicap

Force handicap values for infiltrator and heavy. Also ignore handicap for other classes because
the scoreboard doesn't support displaying it and for consistency with original behavior.
================
*/
LOGFUNCTION_SRET( int, PREFIX(EffectiveHandicap), ( int clientNum, effectiveHandicapType_t type ),
		( clientNum, type ), "G_MODFN_EFFECTIVEHANDICAP" ) {
	gclient_t *client = &level.clients[clientNum];

	if ( ModSpecialties_IsSpecialtiesClass( client->sess.sessionClass ) ) {
		if ( client->sess.sessionClass == PC_INFILTRATOR && ( type == EH_DAMAGE || type == EH_MAXHEALTH ) ) {
			return 50;
		}
		if ( client->sess.sessionClass == PC_HEAVY && ( type == EH_DAMAGE || type == EH_MAXHEALTH ) ) {
			return 200;
		}
		return 100;
	}

	return MOD_STATE->Prev_EffectiveHandicap( clientNum, type );
}

/*
================
(ModFN) SpawnConfigureClient
================
*/
LOGFUNCTION_SVOID( PREFIX(SpawnConfigureClient), ( int clientNum ), ( clientNum ), "G_MODFN_SPAWNCONFIGURECLIENT" ) {
	gentity_t *ent = &g_entities[clientNum];
	gclient_t *client = &level.clients[clientNum];
	specialties_client_t *modclient = &MOD_STATE->clients[clientNum];

	MOD_STATE->Prev_SpawnConfigureClient( clientNum );

	if ( ModSpecialties_IsSpecialtiesClass( client->sess.sessionClass ) ) {
		// STAT_MAX_HEALTH should already be determined via EffectiveHandicap
		ent->health = client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH];

		switch ( client->sess.sessionClass )
		{
		case PC_INFILTRATOR:
			client->ps.stats[STAT_WEAPONS] = ( 1 << WP_PHASER );
			client->ps.ammo[WP_PHASER] = PHASER_AMMO_MAX;

			client->ps.stats[STAT_ARMOR] = 0;

			client->ps.stats[STAT_HOLDABLE_ITEM] = BG_FindItemForHoldable( HI_TRANSPORTER ) - bg_itemlist;

			//INFILTRATOR gets permanent speed
			ent->client->ps.powerups[PW_HASTE] = level.time - ( level.time % 1000 );
			ent->client->ps.powerups[PW_HASTE] += 1800 * 1000;
			break;
		case PC_SNIPER:
			client->ps.stats[STAT_WEAPONS] = ( 1 << WP_IMOD );
			client->ps.ammo[WP_IMOD] = Max_Ammo[WP_IMOD];
			client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_COMPRESSION_RIFLE );
			client->ps.ammo[WP_COMPRESSION_RIFLE] = Max_Ammo[WP_COMPRESSION_RIFLE];

			client->ps.stats[STAT_ARMOR] = 25;

			client->ps.stats[STAT_HOLDABLE_ITEM] = BG_FindItemForHoldable( HI_DECOY ) - bg_itemlist;
			client->ps.stats[STAT_USEABLE_PLACED] = 0;
			break;
		case PC_HEAVY:
			client->ps.stats[STAT_WEAPONS] = ( 1 << WP_TETRION_DISRUPTOR );
			client->ps.ammo[WP_TETRION_DISRUPTOR] = Max_Ammo[WP_TETRION_DISRUPTOR];
			client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_QUANTUM_BURST );
			client->ps.ammo[WP_QUANTUM_BURST] = Max_Ammo[WP_QUANTUM_BURST];

			client->ps.stats[STAT_ARMOR] = 100;
			break;
		case PC_DEMO:
			// Start countdown to receive detpack
			ModPendingItem_Shared_SchedulePendingItem( clientNum, HI_DETPACK, 10000 );

			client->ps.stats[STAT_WEAPONS] = ( 1 << WP_SCAVENGER_RIFLE );
			client->ps.ammo[WP_SCAVENGER_RIFLE] = Max_Ammo[WP_SCAVENGER_RIFLE];
			client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_GRENADE_LAUNCHER );
			client->ps.ammo[WP_GRENADE_LAUNCHER] = Max_Ammo[WP_GRENADE_LAUNCHER];

			client->ps.stats[STAT_ARMOR] = 50;
			break;
		case PC_MEDIC:
			client->ps.stats[STAT_WEAPONS] = ( 1 << WP_PHASER );
			client->ps.ammo[WP_PHASER] = PHASER_AMMO_MAX;
			client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_VOYAGER_HYPO );
			client->ps.ammo[WP_VOYAGER_HYPO] = client->ps.ammo[WP_PHASER];

			client->ps.stats[STAT_ARMOR] = 75;

			client->ps.stats[STAT_HOLDABLE_ITEM] = BG_FindItemForHoldable( HI_MEDKIT ) - bg_itemlist;

			ent->client->ps.powerups[PW_REGEN] = level.time - ( level.time % 1000 );
			ent->client->ps.powerups[PW_REGEN] += 1800 * 1000;
			break;
		case PC_TECH:
			client->ps.stats[STAT_WEAPONS] = ( 1 << WP_PHASER );
			client->ps.ammo[WP_PHASER] = PHASER_AMMO_MAX;
			client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_DREADNOUGHT );
			client->ps.ammo[WP_DREADNOUGHT] = Max_Ammo[WP_DREADNOUGHT];

			client->ps.stats[STAT_ARMOR] = 50;

			client->ps.stats[STAT_HOLDABLE_ITEM] = BG_FindItemForHoldable( HI_SHIELD ) - bg_itemlist;

			//tech gets permanent seeker
			ent->client->ps.powerups[PW_SEEKER] = level.time - ( level.time % 1000 );
			ent->client->ps.powerups[PW_SEEKER] += 1800 * 1000;
			ent->count = 1;//can give away one invincibility
			//can also place ammo stations, register the model and sound for them
			G_ModelIndex( "models/mapobjects/dn/powercell.md3" );
			G_SoundIndex( "sound/player/suitenergy.wav" );
			break;
		}
	}
}

/*
================
(ModFN) CheckItemSpawnDisabled
================
*/
LOGFUNCTION_SRET( qboolean, PREFIX(CheckItemSpawnDisabled), ( gitem_t *item ), ( item ), "G_MODFN_CHECKITEMSPAWNDISABLED" ) {
	switch ( item->giType ) {
		case IT_ARMOR: //given to classes
		case IT_WEAPON: //spread out among classes
		case IT_HOLDABLE: //spread out among classes
		case IT_HEALTH: //given by medics
		case IT_AMMO: //given by technician
			return qtrue;
		case IT_POWERUP:
			switch ( item->giTag ) {
				case PW_HASTE:
				case PW_INVIS:
				case PW_REGEN:
				case PW_SEEKER:
					return qtrue;
			}
	}

	return MOD_STATE->Prev_CheckItemSpawnDisabled( item );
}

/*
============
(ModFN) CanItemBeDropped

Disable dropping any items except flag.
============
*/
LOGFUNCTION_SRET( qboolean, PREFIX(CanItemBeDropped), ( gitem_t *item, int clientNum ),
		( item, clientNum ), "G_MODFN_CANITEMBEDROPPED" ) {
	if ( item->giType != IT_TEAM ) {
		return qfalse;
	}

	return MOD_STATE->Prev_CanItemBeDropped( item, clientNum );
}

/*
================
(ModFN) AddRegisteredItems
================
*/
LOGFUNCTION_SVOID( PREFIX(AddRegisteredItems), ( void ), (), "G_MODFN_ADDREGISTEREDITEMS" ) {
	MOD_STATE->Prev_AddRegisteredItems();

	//weapons
	RegisterItem( BG_FindItemForWeapon( WP_IMOD ) );	//PC_SNIPER
	RegisterItem( BG_FindItemForWeapon( WP_TETRION_DISRUPTOR ) );	//PC_HEAVY
	RegisterItem( BG_FindItemForWeapon( WP_QUANTUM_BURST ) );	//PC_HEAVY
	RegisterItem( BG_FindItemForWeapon( WP_SCAVENGER_RIFLE ) );	//PC_DEMO
	RegisterItem( BG_FindItemForWeapon( WP_GRENADE_LAUNCHER ) );//PC_DEMO
	RegisterItem( BG_FindItemForWeapon( WP_DREADNOUGHT ) );		//PC_TECH
	RegisterItem( BG_FindItemForWeapon( WP_VOYAGER_HYPO ) );	//PC_MEDIC
	//items
	RegisterItem( BG_FindItemForHoldable( HI_TRANSPORTER ) );	//PC_INFILTRATOR, PC_BORG
	RegisterItem( BG_FindItemForHoldable( HI_DECOY ) );	//PC_SNIPER
	RegisterItem( BG_FindItemForHoldable( HI_DETPACK ) );	//PC_DEMO
	RegisterItem( BG_FindItemForHoldable( HI_MEDKIT ) );	//PC_MEDIC
	RegisterItem( BG_FindItemForHoldable( HI_SHIELD ) );	//PC_TECH
	//power ups
	RegisterItem( BG_FindItemForPowerup( PW_HASTE ) );	//PC_INFILTRATOR
	RegisterItem( BG_FindItemForPowerup( PW_REGEN ) );	//PC_MEDIC, PC_ACTIONHERO
	RegisterItem( BG_FindItemForPowerup( PW_SEEKER ) );	//PC_TECH. PC_VIP
	RegisterItem( BG_FindItemForPowerup( PW_INVIS ) );	//PC_TECH
	//Tech power stations
	G_ModelIndex( "models/mapobjects/dn/powercell.md3" );
	G_ModelIndex( "models/mapobjects/dn/powercell2.md3" );
	G_SoundIndex( "sound/player/suitenergy.wav" );
	G_SoundIndex( "sound/weapons/noammo.wav" );
	G_SoundIndex( "sound/weapons/explosions/cargoexplode.wav" );
	G_SoundIndex( "sound/items/respawn1.wav" );
}

/*
================
(ModFN) KnockbackMass

Determine class-specific mass value for knockback calculations.
================
*/
LOGFUNCTION_SRET( float, PREFIX(KnockbackMass), ( gentity_t *targ, gentity_t *inflictor, gentity_t *attacker,
		vec3_t dir, vec3_t point, int damage, int dflags, int mod ),
		( targ, inflictor, attacker, dir, point, damage, dflags, mod ), "G_MODFN_KNOCKBACKMASS G_DAMAGE" ) {
	if ( targ->client->sess.sessionClass == PC_INFILTRATOR )
		return 100;
	if ( targ->client->sess.sessionClass == PC_HEAVY )
		return 400;
	return MOD_STATE->Prev_KnockbackMass( targ, inflictor, attacker, dir, point, damage, dflags, mod );
}

/*
==================
(ModFN) AdjustGeneralConstant
==================
*/
static int PREFIX(AdjustGeneralConstant)( generalConstant_t gcType, int defaultValue ) {
	if ( gcType == GC_ALLOW_BOT_CLASS_SPECIFIER )
		return 1;

	return MOD_STATE->Prev_AdjustGeneralConstant( gcType, defaultValue );
}

/*
======================
(ModFN) AdjustWeaponConstant

Enable tripmines for the demolitionist's grenade launcher.
======================
*/
static int PREFIX(AdjustWeaponConstant)( weaponConstant_t wcType, int defaultValue ) {
	if ( wcType == WC_USE_TRIPMINES )
		return 1;

	return MOD_STATE->Prev_AdjustWeaponConstant( wcType, defaultValue );
}

/*
==============
(ModFN) ModifyAmmoUsage

Tripmines use more ammo.
==============
*/
LOGFUNCTION_SRET( int, PREFIX(ModifyAmmoUsage), ( int defaultValue, int weapon, qboolean alt ),
		( defaultValue, weapon, alt ), "G_MODFN_MODIFYAMMOUSAGE" ) {
	if ( weapon == WP_GRENADE_LAUNCHER && alt ) {
		return 3;
	}

	return MOD_STATE->Prev_ModifyAmmoUsage( defaultValue, weapon, alt );
}

/*
================
ModSpecialties_Init
================
*/

#define INIT_FN_STACKABLE( name ) \
	MOD_STATE->Prev_##name = modfn.name; \
	modfn.name = PREFIX(name);

#define INIT_FN_OVERRIDE( name ) \
	modfn.name = PREFIX(name);

#define INIT_FN_BASE( name ) \
	EF_WARN_ASSERT( modfn.name == ModFNDefault_##name ); \
	modfn.name = PREFIX(name);

LOGFUNCTION_VOID( ModSpecialties_Init, ( void ), (), "G_MOD_INIT G_SPECIALTIES" ) {
	if ( EF_WARN_ASSERT( !MOD_STATE ) ) {
		modcfg.mods_enabled.specialties = qtrue;
		MOD_STATE = G_Alloc( sizeof( *MOD_STATE ) );

		G_RegisterTrackedCvar( &MOD_STATE->g_classChangeDebounceTime, "g_classChangeDebounceTime", "180", CVAR_ARCHIVE, qfalse );

		// Register mod functions
		INIT_FN_STACKABLE( InitClientSession );
		INIT_FN_STACKABLE( PreClientSpawn );
		INIT_FN_STACKABLE( GenerateClientSessionStructure );
		INIT_FN_STACKABLE( ModClientCommand );
		INIT_FN_BASE( UpdateSessionClass );
		INIT_FN_STACKABLE( EffectiveHandicap );
		INIT_FN_STACKABLE( SpawnConfigureClient );
		INIT_FN_STACKABLE( CheckItemSpawnDisabled );
		INIT_FN_STACKABLE( CanItemBeDropped );
		INIT_FN_STACKABLE( AddRegisteredItems );
		INIT_FN_STACKABLE( KnockbackMass );
		INIT_FN_STACKABLE( AdjustGeneralConstant );
		INIT_FN_STACKABLE( AdjustWeaponConstant );
		INIT_FN_STACKABLE( ModifyAmmoUsage );

		// Pending item support for demolitionist detpack
		ModPendingItem_Init();
	}
}