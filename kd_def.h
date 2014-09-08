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

// KD_DEF.H

#include "ID_HEADS.H"
#include <BIOS.H>

#define FRILLS	0			// Cut out frills for 360K - MIKE MAYNARD


/*
=============================================================================

						GLOBAL CONSTANTS

=============================================================================
*/

#define	MAXACTORS	MAXSPRITES

#define ACCGRAVITY	3
#define SPDMAXY		80

#define BLOCKSIZE	(8*PIXGLOBAL)		// for positioning starting actors

//
// movement scrolling edges
//
#define SCROLLEAST (TILEGLOBAL*11)
#define SCROLLWEST (TILEGLOBAL*9)
#define SCROLLSOUTH (TILEGLOBAL*8)
#define SCROLLNORTH (TILEGLOBAL*4)

#define CLIPMOVE	24					// speed to push out of a solid wall

#define GAMELEVELS	17

/*
=============================================================================

							 TYPES

=============================================================================
*/

typedef	enum	{notdone,resetgame,levelcomplete,warptolevel,died,victorious}
				exittype;

typedef	enum	{nothing,keenobj,powerobj,doorobj,
	bonusobj,broccoobj,tomatobj,carrotobj,celeryobj,asparobj,grapeobj,
	taterobj,cartobj,frenchyobj,melonobj,turnipobj,cauliobj,brusselobj,
	mushroomobj,squashobj,apelobj,peapodobj,peabrainobj,boobusobj,
	shotobj,inertobj}	classtype;

typedef struct
{
  int 		leftshapenum,rightshapenum;
  enum		{step,slide,think,stepthink,slidethink} progress;
  boolean	skippable;

  boolean	pushtofloor;
  int tictime;
  int xmove;
  int ymove;
  void (*think) ();
  void (*contact) ();
  void (*react) ();
  void *nextstate;
} statetype;


typedef	struct
{
	unsigned	worldx,worldy;
	boolean	leveldone[GAMELEVELS];
	long	score,nextextra;
	int		flowerpowers;
	int		boobusbombs,bombsthislevel;
	int		keys;
	int		mapon;
	int		lives;
	int		difficulty;
} gametype;


typedef struct	objstruct
{
	classtype	obclass;
	enum		{no,yes,allways,removable} active;
	boolean		needtoreact,needtoclip;
	unsigned	nothink;
	unsigned	x,y;

	int			xdir,ydir;
	int			xmove,ymove;
	int			xspeed,yspeed;

	int			ticcount,ticadjust;
	statetype	*state;

	unsigned	shapenum;

	unsigned	left,top,right,bottom;	// hit rectangle
	unsigned	midx;
	unsigned	tileleft,tiletop,tileright,tilebottom;	// hit rect in tiles
	unsigned	tilemidx;

	int			hitnorth,hiteast,hitsouth,hitwest;	// wall numbers contacted

	int			temp1,temp2,temp3,temp4;

	void		*sprite;

	struct	objstruct	*next,*prev;
} objtype;


/*
=============================================================================

					 KD_MAIN.C DEFINITIONS

=============================================================================
*/

extern	char	str[80],str2[20];
extern	boolean	singlestep,jumpcheat,godmode,tedlevel;
extern	unsigned	tedlevelnum;

void	DebugMemory (void);
void	TestSprites(void);
int		DebugKeys (void);
void	StartupId (void);
void	ShutdownId (void);
void	Quit (char *error);
void	InitGame (void);
void	main (void);


/*
=============================================================================

					  KD_DEMO.C DEFINITIONS

=============================================================================
*/

void	Finale (void);
void	GameOver (void);
void	DemoLoop (void);
void	StatusWindow (void);
void	NewGame (void);
void	TEDDeath (void);

boolean	LoadGame (int file);
boolean	SaveGame (int file);
void	ResetGame (void);

/*
=============================================================================

					  KD_PLAY.C DEFINITIONS

=============================================================================
*/

extern	gametype	gamestate;
extern	exittype	playstate;
extern	boolean		button0held,button1held;
extern	unsigned	originxtilemax,originytilemax;
extern	objtype		*new,*check,*player,*scoreobj;

extern	ControlInfo	c;

extern	objtype dummyobj;

extern	char		*levelnames[21];

void	CheckKeys (void);
void	CalcInactivate (void);
void 	InitObjArray (void);
void 	GetNewObj (boolean usedummy);
void	RemoveObj (objtype *gone);
void 	ScanInfoPlane (void);
void 	PatchWorldMap (void);
void 	MarkTileGraphics (void);
void 	FadeAndUnhook (void);
void 	SetupGameLevel (boolean loadnow);
void 	ScrollScreen (void);
void 	MoveObjVert (objtype *ob, int ymove);
void 	MoveObjHoriz (objtype *ob, int xmove);
void 	GivePoints (unsigned points);
void 	ClipToEnds (objtype *ob);
void 	ClipToEastWalls (objtype *ob);
void 	ClipToWestWalls (objtype *ob);
void 	ClipToWalls (objtype *ob);
void	ClipToSprite (objtype *push, objtype *solid, boolean squish);
void	ClipToSpriteSide (objtype *push, objtype *solid);
int 	DoActor (objtype *ob,int tics);
void 	StateMachine (objtype *ob);
void 	NewState (objtype *ob,statetype *state);
void 	PlayLoop (void);
void 	GameLoop (void);

/*
=============================================================================

					  KD_KEEN.C DEFINITIONS

=============================================================================
*/

void	CalcSingleGravity (void);

void	ProjectileThink		(objtype *ob);
void	VelocityThink		(objtype *ob);
void	DrawReact			(objtype *ob);

void	SpawnScore (void);
void	FixScoreBox (void);
void	SpawnWorldKeen (int tilex, int tiley);
void	SpawnKeen (int tilex, int tiley, int dir);

void 	KillKeen (void);

extern	int	singlegravity;
extern	unsigned	bounceangle[8][8];

extern	statetype s_keendie1;

/*
=============================================================================

					  KD_ACT1.C DEFINITIONS

=============================================================================
*/

void WalkReact (objtype *ob);

void 	DoGravity (objtype *ob);
void	AccelerateX (objtype *ob,int dir,int max);
void 	FrictionX (objtype *ob);

void	ProjectileThink		(objtype *ob);
void	VelocityThink		(objtype *ob);
void	DrawReact			(objtype *ob);
void	DrawReact2			(objtype *ob);
void	DrawReact3			(objtype *ob);
void	ChangeState (objtype *ob, statetype *state);

void	ChangeToFlower (objtype *ob);

void	SpawnBonus (int tilex, int tiley, int type);
void	SpawnDoor (int tilex, int tiley);
void 	SpawnBrocco (int tilex, int tiley);
void 	SpawnTomat (int tilex, int tiley);
void 	SpawnCarrot (int tilex, int tiley);
void 	SpawnAspar (int tilex, int tiley);
void 	SpawnGrape (int tilex, int tiley);

extern	statetype s_doorraise;

extern	statetype s_bonus;
extern	statetype s_bonusrise;

extern	statetype s_broccosmash3;
extern	statetype s_broccosmash4;

extern	statetype s_grapefall;

/*
=============================================================================

					  KD_ACT2.C DEFINITIONS

=============================================================================
*/

void SpawnTater (int tilex, int tiley);
void SpawnCart (int tilex, int tiley);
void SpawnFrenchy (int tilex, int tiley);
void SpawnMelon (int tilex, int tiley,int dir);
void SpawnSquasher (int tilex, int tiley);
void SpawnApel (int tilex, int tiley);
void SpawnPeaPod (int tilex, int tiley);
void SpawnPeaBrain (int tilex, int tiley);
void SpawnBoobus (int tilex, int tiley);

extern	statetype s_taterattack2;
extern	statetype s_squasherjump2;
extern	statetype s_boobusdie;

extern	statetype s_deathwait1;
extern	statetype s_deathwait2;
extern	statetype s_deathwait3;
extern	statetype s_deathboom1;
extern	statetype s_deathboom2;
