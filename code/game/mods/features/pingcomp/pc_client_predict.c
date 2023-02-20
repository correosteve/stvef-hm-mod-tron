/*
* Ping Compensation - Client Prediction
* 
* This module provides server-side support for weapon fire prediction features on the client.
*/

#define MOD_PREFIX( x ) ModPCClientPredict_##x

#include "mods/features/pingcomp/pc_local.h"

#define PREDICTION_ENABLED ( ModPingcomp_Static_PingCompensationEnabled() && MOD_STATE->g_unlaggedPredict.integer )

typedef struct {
	predictableRNG_t rng;
} PingcompClientPredict_client_t;

static struct {
	trackedCvar_t g_unlaggedPredict;

	PingcompClientPredict_client_t clients[MAX_CLIENTS];

	// For mod function stacking
	ModFNType_WeaponPredictableRNG Prev_WeaponPredictableRNG;
	ModFNType_AddModConfigInfo Prev_AddModConfigInfo;
	ModFNType_PostRunFrame Prev_PostRunFrame;
} *MOD_STATE;

/*
======================
(ModFN) WeaponPredictableRNG

Only use the predictable functions if prediction is actually enabled (it could have slight
cheating security implications).
======================
*/
LOGFUNCTION_SRET( unsigned int, MOD_PREFIX(WeaponPredictableRNG), ( int clientNum ), ( clientNum ), "G_MODFN_WEAPONPREDICTABLERNG" ) {
	if ( PREDICTION_ENABLED && G_AssertConnectedClient( clientNum ) ) {
		return BG_PredictableRNG_Rand( &MOD_STATE->clients[clientNum].rng, level.clients[clientNum].ps.commandTime );
	}

	return MOD_STATE->Prev_WeaponPredictableRNG( clientNum );
}

/*
==============
ModPCClientPredict_AddWeaponConstant
==============
*/
static void ModPCClientPredict_AddWeaponConstant( char *buffer, int bufSize, weaponConstant_t wc,
		int defaultValue, const char *infoKey ) {
	int value = modfn.AdjustWeaponConstant( wc, defaultValue );
	if ( value != defaultValue ) {
		Q_strcat( buffer, bufSize, va( " %s:%i", infoKey, value ) );
	}
}

/*
==============
ModPCClientPredict_AddInfo
==============
*/
static void ModPCClientPredict_AddInfo( char *info ) {
	char buffer[256];
	Q_strncpyz( buffer, "ver:" BG_WEAPON_PREDICT_VERSION, sizeof( buffer ) );

	if ( ModPingcomp_Static_ProjectileCompensationEnabled() ) {
		Q_strcat( buffer, sizeof( buffer ), " proj:1" );
		ModPCClientPredict_AddWeaponConstant( buffer, sizeof( buffer ), WC_USE_TRIPMINES, 0, "tm" );
	}

	Info_SetValueForKey( info, "weaponPredict", buffer );
}

/*
==============
ModPCClientPredict_SetNoDamageFlags

Set flags to communicate entity takedamage values to client for prediction purposes.
==============
*/
static void ModPCClientPredict_SetNoDamageFlags( void ) {
	int i;
	for(i=0; i<MAX_GENTITIES; ++i) {
		if ( g_entities[i].r.linked ) {
			gentity_t *ent = &g_entities[i];
			if ( !ent->takedamage && ent->s.solid ) {
				ent->s.eFlags |= EF_PINGCOMP_NO_DAMAGE;
			} else {
				ent->s.eFlags &= ~EF_PINGCOMP_NO_DAMAGE;
			}
		}
	}
}

/*
================
ModPCClientPredict_CvarCallback

Make sure any configstrings for client prediction are updated.
================
*/
static void ModPCClientPredict_CvarCallback( trackedCvar_t *cvar ) {
	ModModcfgCS_Static_Update();
}

/*
==============
(ModFN) AddModConfigInfo
==============
*/
LOGFUNCTION_SVOID( MOD_PREFIX(AddModConfigInfo), ( char *info ), ( info ), "G_MODFN_ADDMODCONFIGINFO" ) {
	MOD_STATE->Prev_AddModConfigInfo( info );

	if ( PREDICTION_ENABLED ) {
		ModPCClientPredict_AddInfo( info );
	}
}

/*
================
(ModFN) PostRunFrame
================
*/
LOGFUNCTION_SVOID( MOD_PREFIX(PostRunFrame), ( void ), (), "G_MODFN_POSTRUNFRAME" ) {
	MOD_STATE->Prev_PostRunFrame();

	if ( PREDICTION_ENABLED ) {
		ModPCClientPredict_SetNoDamageFlags();
	}
}

/*
================
ModPCClientPredict_Init
================
*/
LOGFUNCTION_VOID( ModPCClientPredict_Init, ( void ), (), "G_MOD_INIT" ) {
	if ( !MOD_STATE ) {
		MOD_STATE = G_Alloc( sizeof( *MOD_STATE ) );

		INIT_FN_STACKABLE( WeaponPredictableRNG );
		INIT_FN_STACKABLE_LCL( AddModConfigInfo );
		INIT_FN_STACKABLE( PostRunFrame );

		G_RegisterTrackedCvar( &MOD_STATE->g_unlaggedPredict, "g_unlaggedPredict", "1", 0, qfalse );
		G_RegisterCvarCallback( &MOD_STATE->g_unlaggedPredict, ModPCClientPredict_CvarCallback, qfalse );

		ModModcfgCS_Init();
	}
}
