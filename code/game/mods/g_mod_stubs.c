/*
* Mod Function Stubs
* 
* Default mod functions with minimal functionality can be defined here,
* to reduce clutter in the rest of the game code.
*/

#include "g_local.h"

void ModFNDefault_GeneralInit( void ) {
}

void ModFNDefault_PreRunFrame( void ) {
}

void ModFNDefault_PostRunFrame( void ) {
}

void ModFNDefault_PostGameShutdown( qboolean restart ) {
}

void ModFNDefault_MatchStateTransition( matchState_t oldState, matchState_t newState ) {
}

void ModFNDefault_PreClientConnect( int clientNum, qboolean firstTime, qboolean isBot ) {
}

void ModFNDefault_PreClientSpawn( int clientNum, clientSpawnType_t spawnType ) {
}

void ModFNDefault_PostClientSpawn( int clientNum, clientSpawnType_t spawnType ) {
}

void ModFNDefault_PrePlayerLeaveTeam( int clientNum, team_t oldTeam ) {
}

void ModFNDefault_PostPlayerDie( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int meansOfDeath, int *awardPoints ) {
}

void ModFNDefault_PatchClientSpawn( int clientNum, gentity_t **spawn, vec3_t origin, vec3_t angles ) {
}

int ModFNDefault_AdjustWeaponConstant( weaponConstant_t wcType, int defaultValue ) {
	return defaultValue;
}

void ModFNDefault_PostFireProjectile( gentity_t *projectile ) {
}

int ModFNDefault_ModifyDamageFlags( gentity_t *targ, gentity_t *inflictor, gentity_t *attacker,
		vec3_t dir, vec3_t point, int damage, int dflags, int mod ) {
	return dflags;
}

int ModFNDefault_ModifyAmmoUsage( int defaultValue, int weapon, qboolean alt ) {
	return defaultValue;
}

int ModFNDefault_AdjustGeneralConstant( generalConstant_t gcType, int defaultValue ) {
	return defaultValue;
}

qboolean ModFNDefault_ModConsoleCommand( const char *cmd ) {
	return qfalse;
}

qboolean ModFNDefault_ModClientCommand( int clientNum, const char *cmd ) {
	return qfalse;
}

int ModFNDefault_AdjustScoreboardAttributes( int clientNum, scoreboardAttribute_t saType, int defaultValue ) {
	return defaultValue;
}

void ModFNDefault_TrapTrace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs,
		const vec3_t end, int passEntityNum, int contentmask, int modFlags ) {
	trap_Trace( results, start, mins, maxs, end, passEntityNum, contentmask );
}

qboolean ModFNDefault_IsBorgQueen( int clientNum ) {
	return qfalse;
}

void ModFNDefault_PostModInit( void ) {
}
