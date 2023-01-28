// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_consolecmds.c -- text commands typed in at the local console, or
// executed by a key binding

#include "cg_local.h"



static void CG_ObjectivesDown_f( void ) {
	cg.showObjectives = qtrue;
}

static void CG_ObjectivesUp_f( void )
{
	cg.showObjectives = qfalse;
}

void CG_TargetCommand_f( void ) {
	int		targetNum;
	char	test[4];

	targetNum = CG_CrosshairPlayer();
	if (!targetNum ) {
		return;
	}

	trap_Argv( 1, test, 4 );
	trap_SendConsoleCommand( va( "gc %i %i", targetNum, atoi( test ) ) );
}



/*
=================
CG_SizeUp_f

Keybinding command
=================
*/
static void CG_SizeUp_f (void) {
	trap_Cvar_Set("cg_viewsize", va("%i",(int)(cg_viewsize.integer+10)));
}


/*
=================
CG_SizeDown_f

Keybinding command
=================
*/
static void CG_SizeDown_f (void) {
	trap_Cvar_Set("cg_viewsize", va("%i",(int)(cg_viewsize.integer-10)));
}


/*
=============
CG_Viewpos_f

Debugging command to print the current position
=============
*/
static void CG_Viewpos_f (void) {
	CG_Printf ("%s (%i %i %i) : %i\n", cgs.mapname, (int)cg.refdef.vieworg[0],
		(int)cg.refdef.vieworg[1], (int)cg.refdef.vieworg[2],
		(int)cg.refdefViewAngles[YAW]);
}


static void CG_ScoresDown_f( void ) {
	if ( cg.scoresRequestTime + 2000 < cg.time ) {
		// the scores are more than two seconds out of data,
		// so request new ones
		cg.scoresRequestTime = cg.time;
		trap_SendClientCommand( "score" );

		// leave the current scores up if they were already
		// displayed, but if this is the first hit, clear them out
		if ( !cg.showScores ) {
			cg.showScores = qtrue;
			cg.numScores = 0;
		}
	} else {
		// show the cached contents even if they just pressed if it
		// is within two seconds
		cg.showScores = qtrue;
	}
}

static void CG_ScoresUp_f( void ) {
	cg.showScores = qfalse;
	cg.scoreFadeTime = cg.time;
}

static void CG_TellTarget_f( void ) {
	int		clientNum;
	char	command[128];
	char	message[128];

	clientNum = CG_CrosshairPlayer();
	if ( clientNum == -1 ) {
		return;
	}

	trap_Args( message, 128 );
	Com_sprintf( command, 128, "tell %i %s", clientNum, message );
	trap_SendClientCommand( command );
}

static void CG_TellAttacker_f( void ) {
	int		clientNum;
	char	command[128];
	char	message[128];

	clientNum = CG_LastAttacker();
	if ( clientNum == -1 ) {
		return;
	}

	trap_Args( message, 128 );
	Com_sprintf( command, 128, "tell %i %s", clientNum, message );
	trap_SendClientCommand( command );
}

static void CG_Ignore_f(void)
{
	int clientNum;
	char clientName[sizeof(cgs.clientinfo[0].name)] = "";
	char strsub[2];
	char tname[sizeof(clientName)];
	clientInfo_t *curclient;
	qboolean substring = qfalse;

	trap_Argv(1, clientName, sizeof(clientName));

	trap_Argv(2, strsub, sizeof(strsub));
	if(*strsub && atoi(strsub))
		substring = qtrue;

	if(*clientName)
	{
		//Q_StripColor(clientName);

		// user specified in console which nick to ignore
		for(curclient = cgs.clientinfo; (curclient - cgs.clientinfo) < MAX_CLIENTS; curclient++)
		{
			if(!curclient->infoValid)
				continue;

			Q_strncpyz(tname, curclient->name, sizeof(tname));
			//Q_StripColor(tname);

			if((!substring && !Q_stricmp(tname, clientName)) || (substring && Q_strstr(tname, clientName)))
			{
				curclient->ignore = qtrue;
				if(substring && CG_AddIgnore(tname))
					Com_Printf("Ignoring player %s\n", tname);
			}
		}

		if(!substring && CG_AddIgnore(clientName))
			Com_Printf("Ignoring player %s\n", clientName);
	}
	else
	{
		clientNum = CG_CrosshairPlayer();
		if(clientNum >= 0)
		{
			curclient = &cgs.clientinfo[clientNum];
			curclient->ignore = qtrue;

			Q_strncpyz(tname, curclient->name, sizeof(tname));
			//Q_StripColor(tname);

			if(CG_AddIgnore(tname))
				Com_Printf("Ignoring player %s\n", tname);
		}
	}
}

static void CG_Unignore_f(void)
{
	int clientNum;
	char clientName[sizeof(cgs.clientinfo[0].name)] = "";
	char strsub[2];
	char tname[sizeof(clientName)];
	clientInfo_t *curclient;
	qboolean substring = qfalse;

	trap_Argv(1, clientName, sizeof(clientName));
	trap_Argv(2, strsub, sizeof(strsub));
	if(*strsub && atoi(strsub))
		substring = qtrue;

	if(*clientName)
	{
		//Q_StripColor(clientName);

		// user specified in console which nick to ignore
		for(curclient = cgs.clientinfo; (curclient - cgs.clientinfo) < MAX_CLIENTS; curclient++)
		{
			if(!curclient->infoValid)
				continue;

			Q_strncpyz(tname, curclient->name, sizeof(tname));
			//Q_StripColor(tname);

			if((!substring && !Q_stricmp(tname, clientName)) || (substring && Q_strstr(tname, clientName)))
				curclient->ignore = qfalse;
		}

		CG_DelIgnore(clientName, substring);
	}
	else
	{
		clientNum = CG_CrosshairPlayer();
		if(clientNum >= 0)
		{
			curclient = &cgs.clientinfo[clientNum];
			curclient->ignore = qfalse;

			Q_strncpyz(tname, curclient->name, sizeof(tname));
			//Q_StripColor(tname);

			CG_DelIgnore(tname, 0);
		}
	}
}

static void CG_UnignoreAll_f(void)
{
	clientInfo_t *curclient;

	for(curclient = cgs.clientinfo; (curclient - cgs.clientinfo) < MAX_CLIENTS; curclient++)
		curclient->ignore = qfalse;

	trap_Cvar_Set(IGNORE_CVARNAME, "");
}

typedef struct {
	char	*cmd;
	void	(*function)(void);
} consoleCommand_t;

static consoleCommand_t	commands[] = {
	{ "testgun", CG_TestGun_f },
	{ "testmodel", CG_TestModel_f },
	{ "nextframe", CG_TestModelNextFrame_f },
	{ "prevframe", CG_TestModelPrevFrame_f },
	{ "nextskin", CG_TestModelNextSkin_f },
	{ "prevskin", CG_TestModelPrevSkin_f },
	{ "viewpos", CG_Viewpos_f },
	{ "+info", CG_ScoresDown_f },
	{ "-info", CG_ScoresUp_f },
	{ "+zoom", CG_ZoomDown_f },
	{ "-zoom", CG_ZoomUp_f },
	{ "sizeup", CG_SizeUp_f },
	{ "sizedown", CG_SizeDown_f },
	{ "weapnext", CG_NextWeapon_f },
	{ "weapprev", CG_PrevWeapon_f },
	{ "weapon", CG_Weapon_f },
	{ "tell_target", CG_TellTarget_f },
	{ "tell_attacker", CG_TellAttacker_f },
	{ "tcmd", CG_TargetCommand_f },
	{ "loaddefered", CG_LoadDeferredPlayers },	// spelled wrong, but not changing for demo...
	{ "+analysis", CG_ObjectivesDown_f },
	{ "-analysis", CG_ObjectivesUp_f },
	{ "ignore", CG_Ignore_f },
	{ "unignore", CG_Unignore_f },
	{ "unignoreall", CG_UnignoreAll_f },
};


/*
=================
CG_ConsoleCommand

The string has been tokenized and can be retrieved with
Cmd_Argc() / Cmd_Argv()
=================
*/
qboolean CG_ConsoleCommand( void ) {
	const char	*cmd;
	int		i;

	cmd = CG_Argv(0);

	for ( i = 0 ; i < sizeof( commands ) / sizeof( commands[0] ) ; i++ ) {
		if ( !Q_stricmp( cmd, commands[i].cmd ) ) {
			commands[i].function();
			return qtrue;
		}
	}

	return qfalse;
}


/*
=================
CG_InitConsoleCommands

Let the client system know about all of our commands
so it can perform tab completion
=================
*/
void CG_InitConsoleCommands( void ) {
	int		i;

	for ( i = 0 ; i < sizeof( commands ) / sizeof( commands[0] ) ; i++ ) {
		trap_AddCommand( commands[i].cmd );
	}

	//
	// the game server will interpret these commands, which will be automatically
	// forwarded to the server after they are not recognized locally
	//
	trap_AddCommand ("kill");
	trap_AddCommand ("say");
	trap_AddCommand ("say_team");
	trap_AddCommand ("give");
	trap_AddCommand ("god");
	trap_AddCommand ("notarget");
	trap_AddCommand ("noclip");
	trap_AddCommand ("team");
	trap_AddCommand ("class");
	trap_AddCommand ("follow");
	trap_AddCommand ("levelshot");
	trap_AddCommand ("addbot");
	trap_AddCommand ("setviewpos");
	trap_AddCommand ("vote");
	trap_AddCommand ("callvote");
	trap_AddCommand ("loaddefered");	// spelled wrong, but not changing for demo
	trap_AddCommand ("ignore");
	trap_AddCommand ("unignore");
	trap_AddCommand ("unignoreall");
}
