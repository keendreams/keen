/* Keen Dreams Source Code
 * Copyright (C) 2014 Javier M. Chavez
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

// KD_DEMO.C

#include "KD_DEF.H"

#pragma hdrstop

#define RLETAG  0xABCD

/*
=============================================================================

						 LOCAL CONSTANTS

=============================================================================
*/

/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/


/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/

//===========================================================================

/*
=====================
=
= NewGame
=
= Set up new game to start from the beginning
=
=====================
*/

void NewGame (void)
{
	word    i;

	gamestate.worldx = 0;           // spawn keen at starting spot

	gamestate.mapon = 0;
	gamestate.score = 0;
	gamestate.nextextra = 20000;
	gamestate.lives = 3;
	gamestate.flowerpowers = gamestate.boobusbombs = 0;
	for (i = 0;i < GAMELEVELS;i++)
		gamestate.leveldone[i] = false;
}

//===========================================================================

/*
=====================
=
= WaitOrKey
=
=====================
*/

int WaitOrKey (int vbls)
{
	while (vbls--)
	{
		IN_ReadControl(0,&c);           // get player input
		if (LastScan || c.button0 || c.button1)
		{
			IN_ClearKeysDown ();
			return 1;
		}
		VW_WaitVBL(1);
	}
	return 0;
}

//===========================================================================

/*
=====================
=
= GameOver
=
=====================
*/

void
GameOver (void)
{
	VW_InitDoubleBuffer ();
	US_CenterWindow (16,3);

	US_PrintCentered("Game Over!");

	VW_UpdateScreen ();
	IN_ClearKeysDown ();
	IN_Ack ();

}


//===========================================================================

/*
==================
=
= StatusWindow
=
==================
*/

void StatusWindow (void)
{
	word    x;

	// DEBUG - make this look better

	US_CenterWindow(22,7);
	US_CPrint("Status Window");

	WindowX += 8;
	WindowW -= 8;
	WindowY += 20;
	WindowH -= 20;
	PrintX = WindowX;
	PrintY = WindowY;

	VWB_DrawTile8(PrintX,PrintY,26);
	VWB_DrawTile8(PrintX + 8,PrintY,27);
	PrintX += 24;
	US_PrintUnsigned(gamestate.lives);
	US_Print("\n");

	VWB_DrawTile8(PrintX,PrintY,32);
	VWB_DrawTile8(PrintX + 8,PrintY,33);
	VWB_DrawTile8(PrintX,PrintY + 8,34);
	VWB_DrawTile8(PrintX + 8,PrintY + 8,35);
	PrintX += 24;
	US_PrintUnsigned(gamestate.boobusbombs);
	US_Print("\n");

	WindowX += 50;
	WindowW -= 50;
	PrintX = WindowX;
	PrintY = WindowY;

	fontcolor = F_FIRSTCOLOR;
	US_Print("Next ");
	fontcolor = F_BLACK;
	x = PrintX;
	VWB_DrawTile8(PrintX,PrintY,26);
	VWB_DrawTile8(PrintX + 8,PrintY,27);
	PrintX += 24;
	US_PrintUnsigned(gamestate.nextextra);
	US_Print("\n");

	PrintX = x;
	VWB_DrawTile8(PrintX,PrintY,24);
	VWB_DrawTile8(PrintX + 8,PrintY,25);
	PrintX += 24;
	US_PrintUnsigned(gamestate.keys);
	US_Print("\n");

	// DEBUG - add flower powers (#36)

	VW_UpdateScreen();
	IN_Ack();
}

boolean
SaveGame(int file)
{
	word    i,size,compressed,expanded;
	objtype *o;
	memptr  bigbuffer;

	if (!CA_FarWrite(file,(void far *)&gamestate,sizeof(gamestate)))
		return(false);

	expanded = mapwidth * mapheight * 2;
	MM_GetPtr (&bigbuffer,expanded);

	for (i = 0;i < 3;i++)   // Write all three planes of the map
	{
//
// leave a word at start of compressed data for compressed length
//
		compressed = CA_RLEWCompress ((unsigned huge *)mapsegs[i]
			,expanded,((unsigned huge *)bigbuffer)+1,RLETAG);

		*(unsigned huge *)bigbuffer = compressed;

		if (!CA_FarWrite(file,(void far *)bigbuffer,compressed+2) )
		{
			MM_FreePtr (&bigbuffer);
			return(false);
		}
	}

	for (o = player;o;o = o->next)
		if (!CA_FarWrite(file,(void far *)o,sizeof(objtype)))
		{
			MM_FreePtr (&bigbuffer);
			return(false);
		}

	MM_FreePtr (&bigbuffer);
	return(true);
}


boolean
LoadGame(int file)
{
	word    i,j,size;
	objtype *o;
	int orgx,orgy;
	objtype         *prev,*next,*followed;
	unsigned        compressed,expanded;
	memptr  bigbuffer;

	if (!CA_FarRead(file,(void far *)&gamestate,sizeof(gamestate)))
		return(false);

// drop down a cache level and mark everything, so when the option screen
// is exited it will be cached

	ca_levelbit >>= 1;
	ca_levelnum--;

	SetupGameLevel (false);         // load in and cache the base old level
	titleptr[ca_levelnum] = levelnames[mapon];

	ca_levelbit <<= 1;
	ca_levelnum ++;

	expanded = mapwidth * mapheight * 2;
	MM_GetPtr (&bigbuffer,expanded);

	for (i = 0;i < 3;i++)   // Read all three planes of the map
	{
		if (!CA_FarRead(file,(void far *)&compressed,sizeof(compressed)) )
		{
			MM_FreePtr (&bigbuffer);
			return(false);
		}

		if (!CA_FarRead(file,(void far *)bigbuffer,compressed) )
		{
			MM_FreePtr (&bigbuffer);
			return(false);
		}

		CA_RLEWexpand ((unsigned huge *)bigbuffer,
			(unsigned huge *)mapsegs[i],compressed,RLETAG);
	}

	MM_FreePtr (&bigbuffer);

	// Read the object list back in - assumes at least one object in list

	InitObjArray ();
	new = player;
	prev = new->prev;
	next = new->next;
	if (!CA_FarRead(file,(void far *)new,sizeof(objtype)))
		return(false);
	new->prev = prev;
	new->next = next;
	new->needtoreact = true;
	new->sprite = NULL;
	new = scoreobj;
	while (true)
	{
		prev = new->prev;
		next = new->next;
		if (!CA_FarRead(file,(void far *)new,sizeof(objtype)))
			return(false);
		followed = new->next;
		new->prev = prev;
		new->next = next;
		new->needtoreact = true;
		new->sprite = NULL;

		if (followed)
			GetNewObj (false);
		else
			break;
	}

	*((long *)&(scoreobj->temp1)) = -1;             // force score to be updated
	scoreobj->temp3 = -1;                   // and flower power
	scoreobj->temp4 = -1;                   // and lives

	return(true);
}

void
ResetGame(void)
{
	NewGame ();

	ca_levelnum--;
	CA_ClearMarks();
	titleptr[ca_levelnum] = NULL;           // don't reload old level
	ca_levelnum++;
}

#if FRILLS
void
TEDDeath(void)
{
	ShutdownId();
	execlp("TED5.EXE","TED5.EXE","/LAUNCH","KDREAMS",NULL);
}
#endif

static boolean
MoveTitleTo(int offset)
{
	boolean         done;
	int                     dir,
				chunk,
				move;
	longword        lasttime,delay;

	if (offset < originxglobal)
		dir = -1;
	else
		dir = +1;

	chunk = dir * PIXGLOBAL;

	done = false;
	delay = 1;
	while (!done)
	{
		lasttime = TimeCount;
		move = delay * chunk;
		if (chunk < 0)
			done = originxglobal + move <= offset;
		else
			done = originxglobal + move >= offset;
		if (!done)
		{
			RF_Scroll(move,0);
			RF_Refresh();
		}
		if (IN_IsUserInput())
			return(true);
		delay = TimeCount - lasttime;
	}
	if (originxglobal != offset)
	{
		RF_Scroll(offset - originxglobal,0);
		RF_Refresh();
	}
	return(false);
}

static boolean
Wait(longword time)
{
	time += TimeCount;
	while ((TimeCount < time) && (!IN_IsUserInput()))
	{
		if (!(TimeCount % MINTICS))
			RF_Refresh();
	}
	return(IN_IsUserInput());
}

static boolean
ShowText(int offset,WindowRec *wr,char *s)
{
	if (MoveTitleTo(offset))
		return(true);

	US_RestoreWindow(wr);
	US_CPrint(s);
	VW_UpdateScreen();

	if (Wait(TickBase * 5))
		return(true);

	US_RestoreWindow(wr);
	US_CPrint(s);
	VW_UpdateScreen();
	return(false);
}

/*
=====================
=
= DemoLoop
=
=====================
*/

void
DemoLoop (void)
{
	char            *s;
	word            move;
	longword        lasttime;
	WindowRec       mywin;

#if FRILLS
//
// check for launch from ted
//
	if (tedlevel)
	{
		NewGame();
		gamestate.mapon = tedlevelnum;
		GameLoop();
		TEDDeath();
	}
#endif

//
// demo loop
//
	US_SetLoadSaveHooks(LoadGame,SaveGame,ResetGame);
	restartgame = gd_Continue;
	while (true)
	{
		// Load the Title map
		gamestate.mapon = 20;           // title map number
		loadedgame = false;
		SetupGameLevel(true);

		while (!restartgame && !loadedgame)
		{
			VW_InitDoubleBuffer();
			IN_ClearKeysDown();

			while (true)
			{
				// Display the Title map
				RF_NewPosition((5 * TILEGLOBAL) + (TILEGLOBAL / 2),
								(TILEGLOBAL * 2) + (TILEGLOBAL / 2)
								+ (TILEGLOBAL / 4));
				RF_ForceRefresh();
				RF_Refresh();
				RF_Refresh();

				if (Wait(TickBase * 2))
					break;

				mywin.x = (16 * 13) + 4;
				mywin.y = 0;
				mywin.w = 16 * 7;
				mywin.h = 200;
				mywin.px = mywin.x + 0;
				mywin.py = mywin.y + 10;
				s =             "Game\n"
						"John Carmack\n"
						"\n"
						"Utilities\n"
						"John Romero\n"
						"\n"
						"Interface/Sound\n"
						"Jason Blochowiak\n"
						"\n"
						"Creative Director\n"
						"Tom Hall\n"
						"\n"
						"Art\n"
						"Adrian Carmack\n";
				if (ShowText((9 * TILEGLOBAL) - (PIXGLOBAL * 2),&mywin,s))
					break;

				mywin.x = 4;
				mywin.y = 0;
				mywin.w = 16 * 7;
				mywin.h = 200;
				mywin.px = mywin.x + 0;
				mywin.py = mywin.y + 10;
				s =             "\n"
						"\"Keen Dreams\"\n"
						"Copyright 1991-93\n"
						"Softdisk, Inc.\n"
						"\n"
						"\n"
						"\n"
						"\n"
						"Commander Keen\n"
						"Copyright 1990-91\n"
						"Id Software, Inc.\n"
						"\n"
						"Press F1 for Help\n"
						"SPACE to Start\n";
				if (ShowText((2 * TILEGLOBAL) + (PIXGLOBAL * 2),&mywin,s))
					break;

				if (MoveTitleTo((5 * TILEGLOBAL) + (TILEGLOBAL / 2)))
					break;
				if (Wait(TickBase * 3))
					break;

				VWB_Bar(0,0,320,200,FIRSTCOLOR);
				US_DisplayHighScores(-1);

				if (IN_UserInput(TickBase * 8,false))
					break;
			}

			US_ControlPanel ();
		}

		if (!loadedgame)
			NewGame();
		GameLoop();
	}
}
