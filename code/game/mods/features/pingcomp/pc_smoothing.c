/*
* Player Smoothing
* 
* Normally, player movement is not in sync with server frames, and the player commandTime
* can increase by different amounts between each server frame. This can lead to jerky
* player movement, especially for laggy players.
* 
* This module compensates for this by shifting visible player positions slightly backwards
* in time such that the commandTime increases by roughly the same amount each server frame,
* resulting in the appeareance of smooth movement.
*/

#include "mods/features/pingcomp/pc_local.h"

#define PREFIX( x ) ModPCSmoothing_##x
#define MOD_STATE PREFIX( state )

#define WORKING_CLIENT_COUNT ( level.maxclients < MAX_SMOOTHING_CLIENTS ? level.maxclients : MAX_SMOOTHING_CLIENTS )
#define CLIENT_IN_RANGE( clientNum ) ( clientNum >= 0 && clientNum < WORKING_CLIENT_COUNT )

#define MAX_CLIENT_SNAPS 64
#define CLIENT_SNAP( clientNum, index ) MOD_STATE->clients[clientNum].snaps[ ( index ) % MAX_CLIENT_SNAPS ]

// Skip storing snaps less than this many milliseconds from the previous one
#define MINIMUM_SNAP_LENGTH 5

// Workaround for QVM compiler bugs when doing float/char conversion with regular VectorCopy
#define VECTORCOPY_INT(a,b) ((b)[0]=(int)(a)[0],(b)[1]=(int)(a)[1],(b)[2]=(int)(a)[2])

typedef struct {
	int commandTime;
	vec3_t origin;
	vec3_t viewangles;
	unsigned char liftCount;
	char mins[3];
	char maxs[3];
	char movementDir;
	char legsAnim;
} smoothing_client_snap_t;

typedef struct {
	smoothing_client_snap_t snaps[MAX_CLIENT_SNAPS];
	int snapCounter;
	qboolean teleportFlag;
	qboolean deadOnMover;
	unsigned char liftCount;
	float lastMoveZ;
} smoothing_client_t;

static struct {
	smoothing_client_t clients[MAX_SMOOTHING_CLIENTS];

	// For mod function stacking
	ModFNType_PostPmoveActions Prev_PostPmoveActions;
	ModFNType_CopyToBodyQue Prev_CopyToBodyQue;
	ModFNType_PostRunFrame Prev_PostRunFrame;
} *MOD_STATE;

/*
==============
ModPCSmoothing_ReadClientSnap

Generates a smoothing snap from the client's current position.
==============
*/
static void ModPCSmoothing_ReadClientSnap( int clientNum, smoothing_client_snap_t *snap ) {
	const gclient_t *client = &level.clients[clientNum];
	const gentity_t *ent = &g_entities[clientNum];
	smoothing_client_t *modclient = &MOD_STATE->clients[clientNum];

	snap->commandTime = client->ps.commandTime;
	VectorCopy( client->ps.origin, snap->origin );
	VectorCopy( client->ps.viewangles, snap->viewangles );
	VECTORCOPY_INT( ent->r.mins, snap->mins );
	VECTORCOPY_INT( ent->r.maxs, snap->maxs );
	snap->movementDir = client->ps.movementDir;
	snap->legsAnim = (char)client->ps.legsAnim;
	snap->liftCount = modclient->liftCount;
}

/*
==============
ModPCSmoothing_ApplyClientSnap

Apply stored client position to entity position that will be sent out to other clients.

Call with skipOriginZ true if the snap has not yet caught up to a recent lift movement, to avoid
overwriting the lifted position and e.g. shifting the client through the bottom of the lift.
==============
*/
static void ModPCSmoothing_ApplyClientSnap( int clientNum, const smoothing_client_snap_t *snap, qboolean skipOriginZ ) {
	gentity_t *ent = &g_entities[clientNum];

	ent->s.pos.trBase[0] = snap->origin[0];
	ent->s.pos.trBase[1] = snap->origin[1];
	if ( !skipOriginZ ) {
		ent->s.pos.trBase[2] = snap->origin[2];
	}
	VectorCopy( snap->viewangles, ent->s.apos.trBase );
	ent->s.angles2[1] = (int)snap->movementDir;
	ent->s.legsAnim = snap->legsAnim;
}

/*
==============
ModPCSmoothing_CheckLiftMoves

Check for lift movement during server frame. If movement was detected, store the Z origin
and increment the lift counter.
==============
*/
static void ModPCSmoothing_CheckLiftMoves( void ) {
	int i;
	int clientCount = WORKING_CLIENT_COUNT;

	for ( i = 0; i < clientCount; ++i ) {
		if ( level.clients[i].pers.connected == CON_CONNECTED &&
					MOD_STATE->clients[i].lastMoveZ != level.clients[i].ps.origin[2] ) {
			MOD_STATE->clients[i].lastMoveZ = level.clients[i].ps.origin[2];
			MOD_STATE->clients[i].liftCount++;
		}
	}
}

/*
==============
ModPCSmoothing_RecordClientMove
==============
*/
static void ModPCSmoothing_RecordClientMove( int clientNum ) {
	smoothing_client_t *modclient = &MOD_STATE->clients[clientNum];

	qboolean teleportFlag = ( level.clients[clientNum].ps.eFlags & EF_TELEPORT_BIT ) ? qtrue : qfalse;
	if ( modclient->teleportFlag != teleportFlag ) {
		modclient->teleportFlag = teleportFlag;
		modclient->snapCounter = 0;
	}

	modclient->lastMoveZ = level.clients[clientNum].ps.origin[2];

	if ( !modclient->snapCounter || CLIENT_SNAP( clientNum, modclient->snapCounter - 1 ).commandTime + MINIMUM_SNAP_LENGTH
			<= level.clients[clientNum].ps.commandTime ) {
		smoothing_client_snap_t *snap = &CLIENT_SNAP( clientNum, modclient->snapCounter );
		modclient->snapCounter++;
		ModPCSmoothing_ReadClientSnap( clientNum, snap );
	}
}

/*
==============
ModPCSmoothing_LerpVector

Creates interpolation between 'base' and 'next'.
Proportion is indicated by 'fraction' on a scale from 0 (only base) to 1 (only next).
==============
*/
static void ModPCSmoothing_LerpVector( vec3_t base, vec3_t next, float fraction, vec3_t target ) {
	target[0] = base[0] + ( next[0] - base[0] ) * fraction;
	target[1] = base[1] + ( next[1] - base[1] ) * fraction;
	target[2] = base[2] + ( next[2] - base[2] ) * fraction;
}

/*
==============
ModPCSmoothing_RetrieveClientSnap

Retrieves client position as close as possible to specified command time, interpolating if possible.
Returns qtrue on success, qfalse on error.
==============
*/
static qboolean ModPCSmoothing_RetrieveClientSnap( int clientNum, int time, smoothing_client_snap_t *snap ) {
	int counter = MOD_STATE->clients[clientNum].snapCounter;
	if ( counter > 0 ) {
		int maxSnapIndex = counter - 1;
		int minSnapIndex = counter - MAX_CLIENT_SNAPS > 0 ? counter - MAX_CLIENT_SNAPS : 0;
		int currentSnapIndex = maxSnapIndex;
		smoothing_client_snap_t *current_snap = &CLIENT_SNAP( clientNum, currentSnapIndex );

		// try to find the snap at or directly before target time
		while ( currentSnapIndex > minSnapIndex && current_snap->commandTime > time ) {
			--currentSnapIndex;
			current_snap = &CLIENT_SNAP( clientNum, currentSnapIndex );
		}

		// make sure time is reasonable (can fail if smoothing is switched off and on)
		if ( current_snap->commandTime + 200 < time ) {
			return qfalse;
		}

		*snap = *current_snap;

		// see if we have a valid next snap to interpolate to
		if ( current_snap->commandTime < time && currentSnapIndex + 1 <= maxSnapIndex ) {
			smoothing_client_snap_t *next_snap = &CLIENT_SNAP( clientNum, currentSnapIndex + 1 );
			float lerpFraction;

			EF_WARN_ASSERT( next_snap->commandTime > time );

			lerpFraction = (float)( time - current_snap->commandTime ) /
					(float)( next_snap->commandTime - current_snap->commandTime );

			ModPCSmoothing_LerpVector( current_snap->origin, next_snap->origin, lerpFraction, snap->origin );
		}

		return qtrue;
	}

	return qfalse;
}

/*
==============
ModPCSmoothing_Static_ShiftClient

Moves client's shared entity to "smoothed" position ahead of server snapshot.
Returns qtrue on success, qfalse on error or smoothing inactive.
On success, additional data will be written to info_out for collision handling purposes.
==============
*/
qboolean ModPCSmoothing_Static_ShiftClient( int clientNum, Smoothing_ShiftInfo_t *info_out ) {
	if ( MOD_STATE && ModPingcomp_Static_SmoothingEnabledForClient( clientNum ) ) {
		gclient_t *client = &level.clients[clientNum];
		smoothing_client_t *modclient = &MOD_STATE->clients[clientNum];
		smoothing_client_snap_t snap;

		if ( client->ps.pm_type == PM_DEAD ) {
			if ( modclient->deadOnMover ) {
				return qfalse;
			}

			if ( !ModPCDeadMove_Static_DeadMoveActive( clientNum ) ) {
				// Try to initiate dead move
				if ( ModPCSmoothing_RetrieveClientSnap( clientNum, level.time - ModPCSmoothingOffset_Shared_GetOffset( clientNum ), &snap ) ) {
					if ( snap.liftCount != modclient->liftCount ) {
						// If player died on a lift, just disable smoothing until they respawn. This can cause the
						// player to visually snap to the playerstate position a bit, but is usually pretty minor
						// and works adequately for this special case.
						modclient->deadOnMover = qtrue;
						return qfalse;
					}

					ModPCDeadMove_Static_InitDeadMove( clientNum, snap.origin );
				}
			}

			// Try to retrieve dead move position
			if ( ModPCDeadMove_Static_ShiftClient( clientNum, info_out ) ) {
				return qtrue;
			}
		} else {
			modclient->deadOnMover = qfalse;
		}

		if ( ModPCSmoothing_RetrieveClientSnap( clientNum, level.time - ModPCSmoothingOffset_Shared_GetOffset( clientNum ), &snap ) ) {
			qboolean liftMoved = snap.liftCount != modclient->liftCount ? qtrue : qfalse;
			ModPCSmoothing_ApplyClientSnap( clientNum, &snap, liftMoved );
			if ( info_out ) {
				VECTORCOPY_INT( snap.mins, info_out->mins );
				VECTORCOPY_INT( snap.maxs, info_out->maxs );
			}

			ModPCSmoothingDebug_Static_LogFrame( clientNum, snap.commandTime );
			return qtrue;
		}
	}

	return qfalse;
}

/*
==============
(ModFN) PostPmoveActions

Record client positions after each move fragment.
==============
*/
LOGFUNCTION_SVOID( PREFIX(PostPmoveActions), ( pmove_t *pmove, int clientNum, int oldEventSequence ),
		( pmove, clientNum, oldEventSequence ), "G_MODFN_POSTPMOVEACTIONS" ) {
	MOD_STATE->Prev_PostPmoveActions( pmove, clientNum, oldEventSequence );

	if ( ModPingcomp_Static_SmoothingEnabledForClient( clientNum ) ) {
		ModPCSmoothing_RecordClientMove( clientNum );
	}
}

/*
=============
(ModFN) CopyToBodyQue

Use smoothed position when creating body entity so it stays in the same visible position
as the player entity body.
=============
*/
LOGFUNCTION_SRET( gentity_t *, PREFIX(CopyToBodyQue), ( int clientNum ), ( clientNum ), "G_MODFN_COPYTOBODYQUE" ) {
	ModPCSmoothing_Static_ShiftClient( clientNum, NULL );
	return MOD_STATE->Prev_CopyToBodyQue( clientNum );
}

/*
================
(ModFN) PostRunFrame
================
*/
LOGFUNCTION_SVOID( PREFIX(PostRunFrame), ( void ), (), "G_MODFN_POSTRUNFRAME" ) {
	if ( ModPingcomp_Static_SmoothingEnabled() ) {
		ModPCSmoothing_CheckLiftMoves();
	}

	MOD_STATE->Prev_PostRunFrame();
}

/*
================
ModPCSmoothing_Init
================
*/

#define INIT_FN_STACKABLE( name ) \
	MOD_STATE->Prev_##name = modfn.name; \
	modfn.name = PREFIX(name);

#define INIT_FN_OVERRIDE( name ) \
	modfn.name = PREFIX(name);

LOGFUNCTION_VOID( ModPCSmoothing_Init, ( void ), (), "G_MOD_INIT" ) {
	if ( !MOD_STATE ) {
		MOD_STATE = G_Alloc( sizeof( *MOD_STATE ) );

		INIT_FN_STACKABLE( PostPmoveActions );
		INIT_FN_STACKABLE( CopyToBodyQue );
		INIT_FN_STACKABLE( PostRunFrame );

		ModPCSmoothingDebug_Init();
		ModPCSmoothingOffset_Init();
		ModPCProjectileImpact_Init();
		ModPCDeadMove_Init();
	}
}