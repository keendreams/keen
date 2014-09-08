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

//
//	ID Engine
//	ID_US.c - User Manager
//	v1.0d1
//	By Jason Blochowiak
//

//
//	This module handles dealing with user input & feedback
//
//	Depends on: Input Mgr, View Mgr, some variables from the Sound, Caching,
//		and Refresh Mgrs, Memory Mgr for background save/restore
//
//	Globals:
//		ingame - Flag set by game indicating if a game is in progress
//      abortgame - Flag set if the current game should be aborted (if a load
//			game fails)
//		loadedgame - Flag set if a game was loaded
//		abortprogram - Normally nil, this points to a terminal error message
//			if the program needs to abort
//		restartgame - Normally set to gd_Continue, this is set to one of the
//			difficulty levels if a new game should be started
//		PrintX, PrintY - Where the User Mgr will print (global coords)
//		WindowX,WindowY,WindowW,WindowH - The dimensions of the current
//			window
//

// DEBUG - handle LPT3 for Sound Source

#include "ID_HEADS.H"

#define CTL_M_ADLIBUPPIC	CTL_S_ADLIBUPPIC
#define CTL_M_ADLIBDNPIC	CTL_S_ADLIBDNPIC

#pragma	hdrstop

#pragma	warn	-pia

#define	MaxX	320
#define	MaxY	200

#define	MaxHelpLines	500

#define	MaxHighName	57
#define	MaxScores	10
typedef	struct
		{
			char	name[MaxHighName + 1];
			long	score;
			word	completed;
		} HighScore;

#define	MaxGameName		32
#define	MaxSaveGames	7
typedef	struct
		{
			char	signature[4];
			boolean	present;
			char	name[MaxGameName + 1];
		} SaveGame;

//	Hack import for TED launch support
extern	boolean		tedlevel;
extern	word		tedlevelnum;
extern	void		TEDDeath(void);
static	char		*ParmStrings[] = {"TEDLEVEL",""};


//	Global variables
		boolean		ingame,abortgame,loadedgame;
		char		*abortprogram;
		GameDiff	restartgame = gd_Continue;
		word		PrintX,PrintY;
		word		WindowX,WindowY,WindowW,WindowH;

//	Internal variables
static	boolean		US_Started;
static	boolean		GameIsDirty,
					HighScoresDirty,
					QuitToDos,
					ResumeGame;

static	memptr		LineOffsets;

static	boolean		Button0,Button1,
					CursorBad;
static	int			CursorX,CursorY;

static	void		(*USL_MeasureString)(char far *,word *,word *) = VW_MeasurePropString,
					(*USL_DrawString)(char far *) = VWB_DrawPropString;

static	boolean		(*USL_SaveGame)(int),(*USL_LoadGame)(int);
static	void		(*USL_ResetGame)(void);
static	SaveGame	Games[MaxSaveGames];
static	HighScore	Scores[MaxScores] =
					{
						{"",10000},
						{"",10000},
						{"",10000},
						{"",10000},
						{"",10000},
						{"",10000},
						{"",10000},
						{"",10000},
						{"",10000},
						{"",10000}
					};

//	Internal routines

//	Public routines

///////////////////////////////////////////////////////////////////////////
//
//	USL_HardError() - Handles the Abort/Retry/Fail sort of errors passed
//			from DOS.
//
///////////////////////////////////////////////////////////////////////////
#pragma	warn	-par
#pragma	warn	-rch
int
USL_HardError(word errval,int ax,int bp,int si)
{
#define IGNORE  0
#define RETRY   1
#define	ABORT   2
extern	void	ShutdownId(void);

static	char		buf[32];
static	WindowRec	wr;
static	boolean		oldleavedriveon;
		int			di;
		char		c,*s,*t;


	di = _DI;

	oldleavedriveon = LeaveDriveOn;
	LeaveDriveOn = false;

	if (ax < 0)
		s = "Device Error";
	else
	{
		if ((di & 0x00ff) == 0)
			s = "Drive ~ is Write Protected";
		else
			s = "Error on Drive ~";
		for (t = buf;*s;s++,t++)	// Can't use sprintf()
			if ((*t = *s) == '~')
				*t = (ax & 0x00ff) + 'A';
		*t = '\0';
		s = buf;
	}

	c = peekb(0x40,0x49);	// Get the current screen mode
	if ((c < 4) || (c == 7))
		goto oh_kill_me;

	// DEBUG - handle screen cleanup

	US_SaveWindow(&wr);
	US_CenterWindow(30,3);
	US_CPrint(s);
	US_CPrint("(R)etry or (A)bort?");
	VW_UpdateScreen();
	IN_ClearKeysDown();

asm	sti	// Let the keyboard interrupts come through

	while (true)
	{
		switch (IN_WaitForASCII())
		{
		case key_Escape:
		case 'a':
		case 'A':
			goto oh_kill_me;
			break;
		case key_Return:
		case key_Space:
		case 'r':
		case 'R':
			US_ClearWindow();
			VW_UpdateScreen();
			US_RestoreWindow(&wr);
			LeaveDriveOn = oldleavedriveon;
			return(RETRY);
			break;
		}
	}

oh_kill_me:
	abortprogram = s;
	ShutdownId();
	fprintf(stderr,"Terminal Error: %s\n",s);
	if (tedlevel)
		fprintf(stderr,"You launched from TED. I suggest that you reboot...\n");

	return(ABORT);
#undef	IGNORE
#undef	RETRY
#undef	ABORT
}
#pragma	warn	+par
#pragma	warn	+rch

///////////////////////////////////////////////////////////////////////////
//
//	USL_GiveSaveName() - Returns a pointer to a static buffer that contains
//		the filename to use for the specified save game
//
///////////////////////////////////////////////////////////////////////////
static char *
USL_GiveSaveName(word game)
{
static	char	filename[32];
		char	*s,*t;

	for (s = "SAVEGM",t = filename;*s;)
		*t++ = *s++;
	*t++ = game + '0';
	for (s = "."EXTENSION;*s;)
		*t++ = *s++;
	*t = '\0';

	return(filename);
}

///////////////////////////////////////////////////////////////////////////
//
//	US_SetLoadSaveHooks() - Sets the routines that the User Mgr calls after
//		reading or writing the save game headers
//
///////////////////////////////////////////////////////////////////////////
void
US_SetLoadSaveHooks(boolean (*load)(int),boolean (*save)(int),void (*reset)(void))
{
	USL_LoadGame = load;
	USL_SaveGame = save;
	USL_ResetGame = reset;
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_ReadConfig() - Reads the configuration file, if present, and sets
//		things up accordingly. If it's not present, uses defaults. This file
//		includes the high scores.
//
///////////////////////////////////////////////////////////////////////////
static void
USL_ReadConfig(void)
{
	boolean		gotit;
	int			file;
	SDMode		sd;
	SMMode		sm;
	ControlType	ctl;

	if ((file = open("KDREAMS.CFG",O_BINARY | O_RDONLY)) != -1)
	{
		read(file,Scores,sizeof(HighScore) * MaxScores);
		read(file,&sd,sizeof(sd));
		read(file,&sm,sizeof(sm));
		read(file,&ctl,sizeof(ctl));
		read(file,&(KbdDefs[0]),sizeof(KbdDefs[0]));
		close(file);

		HighScoresDirty = false;
		gotit = true;
	}
	else
	{
		sd = sdm_Off;
		sm = smm_Off;
		ctl = ctrl_Keyboard;

		gotit = false;
		HighScoresDirty = true;
	}

	SD_Default(gotit,sd,sm);
	IN_Default(gotit,ctl);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_WriteConfig() - Writes out the current configuration, including the
//		high scores.
//
///////////////////////////////////////////////////////////////////////////
static void
USL_WriteConfig(void)
{
	int	file;

	file = open("KDREAMS.CFG", O_CREAT | O_BINARY | O_WRONLY,
				S_IREAD | S_IWRITE | S_IFREG);
	if (file != -1)
	{
		write(file,Scores,sizeof(HighScore) * MaxScores);
		write(file,&SoundMode,sizeof(SoundMode));
		write(file,&MusicMode,sizeof(MusicMode));
		write(file,&(Controls[0]),sizeof(Controls[0]));
		write(file,&(KbdDefs[0]),sizeof(KbdDefs[0]));
		close(file);
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_CheckSavedGames() - Checks to see which saved games are present
//		& valid
//
///////////////////////////////////////////////////////////////////////////
static void
USL_CheckSavedGames(void)
{
	boolean		ok;
	char		*filename;
	word		i;
	int			file;
	SaveGame	*game;

	USL_SaveGame = 0;
	USL_LoadGame = 0;

	for (i = 0,game = Games;i < MaxSaveGames;i++,game++)
	{
		filename = USL_GiveSaveName(i);
		ok = false;
		if ((file = open(filename,O_BINARY | O_RDONLY)) != -1)
		{
			if
			(
				(read(file,game,sizeof(*game)) == sizeof(*game))
			&&	(!strcmp(game->signature,EXTENSION))
			)
				ok = true;

			close(file);
		}

		if (ok)
			game->present = true;
		else
		{
			strcpy(game->signature,EXTENSION);
			game->present = false;
			strcpy(game->name,"Empty");
		}
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	US_Startup() - Starts the User Mgr
//
///////////////////////////////////////////////////////////////////////////
void
US_Startup(void)
{
	if (US_Started)
		return;

	harderr(USL_HardError);	// Install the fatal error handler

	US_InitRndT(true);		// Initialize the random number generator

	USL_ReadConfig();		// Read config file

	US_Started = true;
}

///////////////////////////////////////////////////////////////////////////
//
//	US_Setup() - Does the disk access part of the User Mgr's startup
//
///////////////////////////////////////////////////////////////////////////
void
US_Setup(void)
{
	USL_CheckSavedGames();	// Check which saved games are present
}

///////////////////////////////////////////////////////////////////////////
//
//	US_Shutdown() - Shuts down the User Mgr
//
///////////////////////////////////////////////////////////////////////////
void
US_Shutdown(void)
{
	if (!US_Started)
		return;

	if (!abortprogram)
		USL_WriteConfig();

	US_Started = false;
}

///////////////////////////////////////////////////////////////////////////
//
//	US_CheckParm() - checks to see if a string matches one of a set of
//		strings. The check is case insensitive. The routine returns the
//		index of the string that matched, or -1 if no matches were found
//
///////////////////////////////////////////////////////////////////////////
int
US_CheckParm(char *parm,char **strings)
{
	char	cp,cs,
			*p,*s;
	int		i;

	while (!isalpha(*parm))	// Skip non-alphas
		parm++;

	for (i = 0;*strings && **strings;i++)
	{
		for (s = *strings++,p = parm,cs = cp = 0;cs == cp;)
		{
			cs = *s++;
			if (!cs)
				return(i);
			cp = *p++;

			if (isupper(cs))
				cs = tolower(cs);
			if (isupper(cp))
				cp = tolower(cp);
		}
	}
	return(-1);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_ScreenDraw() - Draws a chunk of the text screen (called only by
//		US_TextScreen())
//
///////////////////////////////////////////////////////////////////////////
static void
USL_ScreenDraw(word x,word y,char *s,byte attr)
{
	byte	far *screen;

	screen = MK_FP(0xb800,(x * 2) + (y * 80 * 2));
	while (*s)
	{
		*screen++ = *s++;
		*screen++ = attr;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_ClearTextScreen() - Makes sure the screen is in text mode, clears it,
//		and moves the cursor to the leftmost column of the bottom line
//
///////////////////////////////////////////////////////////////////////////
static void
USL_ClearTextScreen(void)
{
	// Set to 80x25 color text mode
	_AL = 3;				// Mode 3
	_AH = 0x00;
	geninterrupt(0x10);

	// Use BIOS to move the cursor to the bottom of the screen
	_AH = 0x0f;
	geninterrupt(0x10);		// Get current video mode into _BH
	_DL = 0;				// Lefthand side of the screen
	_DH = 24;				// Bottom row
	_AH = 0x02;
	geninterrupt(0x10);
}

///////////////////////////////////////////////////////////////////////////
//
//	US_TextScreen() - Puts up the startup text screen
//	Note: These are the only User Manager functions that can be safely called
//		before the User Mgr has been started up
//
///////////////////////////////////////////////////////////////////////////
void
US_TextScreen(void)
{
	word	i,
			sx,sy;

	USL_ClearTextScreen();

#define	scr_rowcol(y,x)	{sx = (x) - 1;sy = (y) - 1;}
#define	scr_aputs(s,a)	USL_ScreenDraw(sx,sy,(s),(a))
#include "ID_US_S.c"
#undef	scr_rowcol
#undef	scr_aputs

	// Check for TED launching here
	for (i = 1;i < _argc;i++)
	{
		if (US_CheckParm(_argv[i],ParmStrings) == 0)
		{
			tedlevelnum = atoi(_argv[i + 1]);
			if (tedlevelnum >= 0)
			{
				tedlevel = true;
				return;
			}
			else
				break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_Show() - Changes the appearance of one of the fields on the text
//		screen. Possibly adds a checkmark in front of it and highlights it
//
///////////////////////////////////////////////////////////////////////////
static void
USL_Show(word x,word y,word w,boolean show,boolean hilight)
{
	byte	far *screen;

	screen = MK_FP(0xb800,((x - 1) * 2) + (y * 80 * 2));
	*screen++ = show? 251 : ' ';	// Checkmark char or space
	*screen = 0x48;
	if (show && hilight)
	{
		for (w++;w--;screen += 2)
			*screen = 0x4f;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_ShowMem() - Right justifies a longword in one of the memory fields on
//		the text screen
//
///////////////////////////////////////////////////////////////////////////
static void
USL_ShowMem(word x,word y,long mem)
{
	char	buf[16];
	word	i;

	for (i = strlen(ltoa(mem,buf,10));i < 5;i++)
		USL_ScreenDraw(x++,y," ",0x48);
	USL_ScreenDraw(x,y,buf,0x48);
}


#if 0
///////////////////////////////////////////////////////////////////////////
//
//	US_UpdateTextScreen() - Called after the ID libraries are started up.
//		Displays what hardware is present.
//
///////////////////////////////////////////////////////////////////////////
void
US_UpdateTextScreen(void)
{
	boolean		b;
	byte		far *screen;
	word		i;
	longword	totalmem;

	// Show video card info
	b = (grmode == CGAGR);
	USL_Show(21,7,4,(videocard >= CGAcard) && (videocard <= VGAcard),b);
	b = (grmode == EGAGR);
	USL_Show(21,8,4,(videocard >= EGAcard) && (videocard <= VGAcard),b);
	b = (grmode == VGAGR);
	USL_Show(21,9,4,videocard == VGAcard,b);
	if (compatability)
		USL_ScreenDraw(5,10,"SVGA Compatibility Mode Enabled.",0x4f);

	// Show input device info
	USL_Show(60,7,8,true,true);
	USL_Show(60,8,11,JoysPresent[0],true);
	USL_Show(60,9,11,JoysPresent[1],true);
	USL_Show(60,10,5,MousePresent,true);

	// Show sound hardware info
	USL_Show(21,14,11,true,SoundMode == sdm_PC);
	b = (SoundMode == sdm_AdLib) || (MusicMode == smm_AdLib);
	USL_Show(21,15,5,AdLibPresent && !SoundBlasterPresent,
				b && !SoundBlasterPresent);
	USL_Show(21,16,13,SoundBlasterPresent,
			SoundBlasterPresent && (b || (SoundMode == sdm_SoundBlaster)));
	USL_Show(21,17,13,SoundSourcePresent,SoundMode == sdm_SoundSource);

	// Show memory available/used
	USL_ShowMem(63,15,mminfo.mainmem / 1024);
	USL_Show(53,15,23,true,true);
	USL_ShowMem(63,16,mminfo.EMSmem / 1024);
	USL_Show(53,16,23,mminfo.EMSmem? true : false,true);
	USL_ShowMem(63,17,mminfo.XMSmem / 1024);
	USL_Show(53,17,23,mminfo.XMSmem? true : false,true);
	totalmem = mminfo.mainmem + mminfo.EMSmem + mminfo.XMSmem;
	USL_ShowMem(63,18,totalmem / 1024);
	screen = MK_FP(0xb800,1 + (((63 - 1) * 2) + (18 * 80 * 2)));
	for (i = 0;i < 13;i++,screen += 2)
		*screen = 0x4f;

	// Change Initializing... to Loading...
	USL_ScreenDraw(27,22,"  Loading...   ",0x9c);
}

#endif

///////////////////////////////////////////////////////////////////////////
//
//	US_FinishTextScreen() - After the main program has finished its initial
//		loading, this routine waits for a keypress and then clears the screen
//
///////////////////////////////////////////////////////////////////////////
void
US_FinishTextScreen(void)
{
	// Change Loading... to Press a Key
	USL_ScreenDraw(30, 18, "Ready - Press a Key",0xCE);

	if (!tedlevel)
	{
		IN_ClearKeysDown();
		IN_Ack();
	}
	IN_ClearKeysDown();

	USL_ClearTextScreen();
}

//	Window/Printing routines

///////////////////////////////////////////////////////////////////////////
//
//	US_SetPrintRoutines() - Sets the routines used to measure and print
//		from within the User Mgr. Primarily provided to allow switching
//		between masked and non-masked fonts
//
///////////////////////////////////////////////////////////////////////////
void
US_SetPrintRoutines(void (*measure)(char far *,word *,word *),void (*print)(char far *))
{
	USL_MeasureString = measure;
	USL_DrawString = print;
}

///////////////////////////////////////////////////////////////////////////
//
//	US_Print() - Prints a string in the current window. Newlines are
//		supported.
//
///////////////////////////////////////////////////////////////////////////
void
US_Print(char *s)
{
	char	c,*se;
	word	w,h;

	while (*s)
	{
		se = s;
		while ((c = *se) && (c != '\n'))
			se++;
		*se = '\0';

		USL_MeasureString(s,&w,&h);
		px = PrintX;
		py = PrintY;
		USL_DrawString(s);

		s = se;
		if (c)
		{
			*se = c;
			s++;

			PrintX = WindowX;
			PrintY += h;
		}
		else
			PrintX += w;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	US_PrintUnsigned() - Prints an unsigned long
//
///////////////////////////////////////////////////////////////////////////
void
US_PrintUnsigned(longword n)
{
	char	buffer[32];

	US_Print(ultoa(n,buffer,10));
}

///////////////////////////////////////////////////////////////////////////
//
//	US_PrintSigned() - Prints a signed long
//
///////////////////////////////////////////////////////////////////////////
void
US_PrintSigned(long n)
{
	char	buffer[32];

	US_Print(ltoa(n,buffer,10));
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_PrintInCenter() - Prints a string in the center of the given rect
//
///////////////////////////////////////////////////////////////////////////
static void
USL_PrintInCenter(char *s,Rect r)
{
	word	w,h,
			rw,rh;

	USL_MeasureString(s,&w,&h);
	rw = r.lr.x - r.ul.x;
	rh = r.lr.y - r.ul.y;

	px = r.ul.x + ((rw - w) / 2);
	py = r.ul.y + ((rh - h) / 2);
	USL_DrawString(s);
}

///////////////////////////////////////////////////////////////////////////
//
//	US_PrintCentered() - Prints a string centered in the current window.
//
///////////////////////////////////////////////////////////////////////////
void
US_PrintCentered(char *s)
{
	Rect	r;

	r.ul.x = WindowX;
	r.ul.y = WindowY;
	r.lr.x = r.ul.x + WindowW;
	r.lr.y = r.ul.y + WindowH;

	USL_PrintInCenter(s,r);
}

///////////////////////////////////////////////////////////////////////////
//
//	US_CPrintLine() - Prints a string centered on the current line and
//		advances to the next line. Newlines are not supported.
//
///////////////////////////////////////////////////////////////////////////
void
US_CPrintLine(char *s)
{
	word	w,h;

	USL_MeasureString(s,&w,&h);

	if (w > WindowW)
		Quit("US_CPrintLine() - String exceeds width");
	px = WindowX + ((WindowW - w) / 2);
	py = PrintY;
	USL_DrawString(s);
	PrintY += h;
}

///////////////////////////////////////////////////////////////////////////
//
//	US_CPrint() - Prints a string in the current window. Newlines are
//		supported.
//
///////////////////////////////////////////////////////////////////////////
void
US_CPrint(char *s)
{
	char	c,*se;
	word	w,h;

	while (*s)
	{
		se = s;
		while ((c = *se) && (c != '\n'))
			se++;
		*se = '\0';

		US_CPrintLine(s);

		s = se;
		if (c)
		{
			*se = c;
			s++;
		}
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	US_ClearWindow() - Clears the current window to white and homes the
//		cursor
//
///////////////////////////////////////////////////////////////////////////
void
US_ClearWindow(void)
{
	VWB_Bar(WindowX,WindowY,WindowW,WindowH,WHITE);
	PrintX = WindowX;
	PrintY = WindowY;
}

///////////////////////////////////////////////////////////////////////////
//
//	US_DrawWindow() - Draws a frame and sets the current window parms
//
///////////////////////////////////////////////////////////////////////////
void
US_DrawWindow(word x,word y,word w,word h)
{
	word	i,
			sx,sy,sw,sh;

	WindowX = x * 8;
	WindowY = y * 8;
	WindowW = w * 8;
	WindowH = h * 8;

	PrintX = WindowX;
	PrintY = WindowY;

	sx = (x - 1) * 8;
	sy = (y - 1) * 8;
	sw = (w + 1) * 8;
	sh = (h + 1) * 8;

	US_ClearWindow();

	VWB_DrawTile8M(sx,sy,0),VWB_DrawTile8M(sx,sy + sh,6);
	for (i = sx + 8;i <= sx + sw - 8;i += 8)
		VWB_DrawTile8M(i,sy,1),VWB_DrawTile8M(i,sy + sh,7);
	VWB_DrawTile8M(i,sy,2),VWB_DrawTile8M(i,sy + sh,8);

	for (i = sy + 8;i <= sy + sh - 8;i += 8)
		VWB_DrawTile8M(sx,i,3),VWB_DrawTile8M(sx + sw,i,5);
}

///////////////////////////////////////////////////////////////////////////
//
//	US_CenterWindow() - Generates a window of a given width & height in the
//		middle of the screen
//
///////////////////////////////////////////////////////////////////////////
void
US_CenterWindow(word w,word h)
{
	US_DrawWindow(((MaxX / 8) - w) / 2,((MaxY / 8) - h) / 2,w,h);
}

///////////////////////////////////////////////////////////////////////////
//
//	US_CenterSaveWindow() - Generates a window of a given width & height in
//		the middle of the screen, saving the background
//
///////////////////////////////////////////////////////////////////////////
void
US_CenterSaveWindow(word w,word h,memptr *save)
{
	word	x,y,
			screen;

	x = ((MaxX / 8) - w) / 2;
	y = ((MaxY / 8) - h) / 2;
	MM_GetPtr(save,(w * h) * CHARWIDTH);
	screen = bufferofs + panadjust + ylookup[y] + (x * CHARWIDTH);
	VW_ScreenToMem(screen,*save,w * CHARWIDTH,h);
	US_DrawWindow(((MaxX / 8) - w) / 2,((MaxY / 8) - h) / 2,w,h);
}

///////////////////////////////////////////////////////////////////////////
//
//	US_RestoreSaveWindow() - Restores the background of the size of the
//		current window from the memory specified by save
//
///////////////////////////////////////////////////////////////////////////
void
US_RestoreSaveWindow(memptr *save)
{
	word	screen;

	screen = bufferofs + panadjust + ylookup[WindowY] + (WindowX * CHARWIDTH);
	VW_MemToScreen(*save,screen,WindowW * CHARWIDTH,WindowH);
	MM_FreePtr(save);
}

///////////////////////////////////////////////////////////////////////////
//
//	US_SaveWindow() - Saves the current window parms into a record for
//		later restoration
//
///////////////////////////////////////////////////////////////////////////
void
US_SaveWindow(WindowRec *win)
{
	win->x = WindowX;
	win->y = WindowY;
	win->w = WindowW;
	win->h = WindowH;

	win->px = PrintX;
	win->py = PrintY;
}

///////////////////////////////////////////////////////////////////////////
//
//	US_RestoreWindow() - Sets the current window parms to those held in the
//		record
//
///////////////////////////////////////////////////////////////////////////
void
US_RestoreWindow(WindowRec *win)
{
	WindowX = win->x;
	WindowY = win->y;
	WindowW = win->w;
	WindowH = win->h;

	PrintX = win->px;
	PrintY = win->py;
}

//	Cursor routines

///////////////////////////////////////////////////////////////////////////
//
//	US_StartCursor() - Sets up the cursor for User Mgr use
//
///////////////////////////////////////////////////////////////////////////
void
US_StartCursor(void)
{
	CursorInfo	info;

	VW_SetCursor(CURSORARROWSPR);
	CursorX = MaxX / 2;
	CursorY = MaxY / 2;
	VW_MoveCursor(CursorX,CursorY);
	VW_ShowCursor();

	IN_ReadCursor(&info);	// Dispose of any accumulated movement
}

///////////////////////////////////////////////////////////////////////////
//
//	US_ShutCursor() - Cleans up after US_StartCursor()
//
///////////////////////////////////////////////////////////////////////////
void
US_ShutCursor(void)
{
	VW_HideCursor();
}

///////////////////////////////////////////////////////////////////////////
//
//	US_UpdateCursor() - Gets the new cursor position & button states from
//		the Input Mgr and tells the View Mgr where the cursor is
//
///////////////////////////////////////////////////////////////////////////
boolean
US_UpdateCursor(void)
{
	CursorInfo	info;

	IN_ReadCursor(&info);
	if (info.x || info.y || CursorBad)
	{
		CursorX += info.x;
		if (CursorX >= MaxX)
			CursorX = MaxX - 1;
		else if (CursorX < 0)
			CursorX = 0;

		CursorY += info.y;
		if (CursorY >= MaxY)
			CursorY = MaxY - 1;
		else if (CursorY < 0)
			CursorY = 0;

		VW_MoveCursor(CursorX,CursorY);
		CursorBad = false;
	}
	Button0 = info.button0;
	Button1 = info.button1;
	return(Button0 || Button1);
}

//	Input routines

///////////////////////////////////////////////////////////////////////////
//
//	USL_XORICursor() - XORs the I-bar text cursor. Used by US_LineInput()
//
///////////////////////////////////////////////////////////////////////////
static void
USL_XORICursor(int x,int y,char *s,word cursor)
{
	char	buf[MaxString];
	word	w,h;

	strcpy(buf,s);
	buf[cursor] = '\0';
	USL_MeasureString(buf,&w,&h);

	px = x + w - 1;
	py = y;
	USL_DrawString("\x80");
}

///////////////////////////////////////////////////////////////////////////
//
//	US_LineInput() - Gets a line of user input at (x,y), the string defaults
//		to whatever is pointed at by def. Input is restricted to maxchars
//		chars or maxwidth pixels wide. If the user hits escape (and escok is
//		true), nothing is copied into buf, and false is returned. If the
//		user hits return, the current string is copied into buf, and true is
//		returned
//
///////////////////////////////////////////////////////////////////////////
boolean
US_LineInput(int x,int y,char *buf,char *def,boolean escok,
				int maxchars,int maxwidth)
{
	boolean		redraw,
				cursorvis,cursormoved,
				done,result;
	ScanCode	sc;
	char		c,
				s[MaxString],olds[MaxString];
	word		i,
				cursor,
				w,h,
				len;
	longword	lasttime;

	VW_HideCursor();

	if (def)
		strcpy(s,def);
	else
		*s = '\0';
	*olds = '\0';
	cursor = strlen(s);
	cursormoved = redraw = true;

	cursorvis = done = false;
	lasttime = TimeCount;
	LastASCII = key_None;
	LastScan = sc_None;

	while (!done)
	{
		if (cursorvis)
			USL_XORICursor(x,y,s,cursor);

	asm	pushf
	asm	cli

		sc = LastScan;
		LastScan = sc_None;
		c = LastASCII;
		LastASCII = key_None;

	asm	popf

		switch (sc)
		{
		case sc_LeftArrow:
			if (cursor)
				cursor--;
			c = key_None;
			cursormoved = true;
			break;
		case sc_RightArrow:
			if (s[cursor])
				cursor++;
			c = key_None;
			cursormoved = true;
			break;
		case sc_Home:
			cursor = 0;
			c = key_None;
			cursormoved = true;
			break;
		case sc_End:
			cursor = strlen(s);
			c = key_None;
			cursormoved = true;
			break;

		case sc_Return:
			strcpy(buf,s);
			done = true;
			result = true;
			c = key_None;
			break;
		case sc_Escape:
			if (escok)
			{
				done = true;
				result = false;
			}
			c = key_None;
			break;

		case sc_BackSpace:
			if (cursor)
			{
				strcpy(s + cursor - 1,s + cursor);
				cursor--;
				redraw = true;
			}
			c = key_None;
			cursormoved = true;
			break;
		case sc_Delete:
			if (s[cursor])
			{
				strcpy(s + cursor,s + cursor + 1);
				redraw = true;
			}
			c = key_None;
			cursormoved = true;
			break;

		case 0x4c:	// Keypad 5
		case sc_UpArrow:
		case sc_DownArrow:
		case sc_PgUp:
		case sc_PgDn:
		case sc_Insert:
			c = key_None;
			break;
		}

		if (c)
		{
			len = strlen(s);
			USL_MeasureString(s,&w,&h);

			if
			(
				isprint(c)
			&&	(len < MaxString - 1)
			&&	((!maxchars) || (len < maxchars))
			&&	((!maxwidth) || (w < maxwidth))
			)
			{
				for (i = len + 1;i > cursor;i--)
					s[i] = s[i - 1];
				s[cursor++] = c;
				redraw = true;
			}
		}

		if (redraw)
		{
			px = x;
			py = y;
			USL_DrawString(olds);
			strcpy(olds,s);

			px = x;
			py = y;
			USL_DrawString(s);

			redraw = false;
		}

		if (cursormoved)
		{
			cursorvis = false;
			lasttime = TimeCount - TickBase;

			cursormoved = false;
		}
		if (TimeCount - lasttime > TickBase / 2)
		{
			lasttime = TimeCount;

			cursorvis ^= true;
		}
		if (cursorvis)
			USL_XORICursor(x,y,s,cursor);

		VW_UpdateScreen();
	}

	if (cursorvis)
		USL_XORICursor(x,y,s,cursor);
	if (!result)
	{
		px = x;
		py = y;
		USL_DrawString(olds);
	}
	VW_ShowCursor();
	VW_UpdateScreen();

	IN_ClearKeysDown();
	return(result);
}

//	Control panel routines

static	boolean		FlushHelp;
static	WindowRec	HelpWindow,BottomWindow;
typedef	enum
		{
			uic_Draw,uic_Hit
		} UserCall;
typedef	enum
		{
			uii_Bad,uii_Button,uii_RadioButton,uii_CheckBox,uii_KeyCap
		} UIType;
#define	ui_Normal	0
#define	ui_Selected	1
#define	ui_Disabled	2

					// Prototype the custom routines
static	boolean		USL_CtlButtonCustom(UserCall,word,word),
					USL_CtlPButtonCustom(UserCall,word,word),
					USL_CtlPSButtonCustom(UserCall,word,word),
					USL_CtlPRButtonCustom(UserCall,word,word),
					USL_CtlHButtonCustom(UserCall,word,word),
					USL_CtlDButtonCustom(UserCall,word,word),
					USL_CtlDEButtonCustom(UserCall,word,word),
					USL_CtlDLButtonCustom(UserCall,word,word),
					USL_CtlDSButtonCustom(UserCall,word,word),
					USL_CtlSButtonCustom(UserCall,word,word),
					USL_CtlCButtonCustom(UserCall,word,word),
					USL_CtlCKbdButtonCustom(UserCall,word,word),
					USL_CtlCJoyButtonCustom(UserCall,word,word);

					// The structure of a user interaction item
typedef	struct	{
					Rect		r;				// The enclosing rectangle
					UIType		type;			// The type of item
					int			picup,picdown;	// What to draw when up/down
					char		*help;			// Floating help string
					ScanCode	key;			// Key equiv
					word		sel;			// Interaction flags (ui_XXX)
					boolean		(*custom)(UserCall,word,word);	// Custom routine
					char		*text;			// Text for some items
				} UserItem;
typedef	struct	{
					ScanCode	key;
					word		i,n,		// Hit CtlPanels2[i][n]
								toi,ton;	// Move to CtlPanels2[toi][ton]
				} HotKey;	// MARK

static	ScanCode	*KeyMaps[] =
					{
						&KbdDefs[0].button0,&KbdDefs[0].button1,
						&KbdDefs[0].upleft,&KbdDefs[0].up,&KbdDefs[0].upright,
						&KbdDefs[0].left, &KbdDefs[0].right,
						&KbdDefs[0].downleft,&KbdDefs[0].down,&KbdDefs[0].downright,
					};

// Some macros to make rectangle definition quite a bit less unpleasant
#define	CtlPanelX	8
#define	CtlPanelY	4
#define	CtlPanel2X	(8*8)
#define	CtlPanel2Y	(2*8)
#define	CtlPanel3X	(8*8)
#define	CtlPanel3Y	(7*8)

#define	CtlPanelR(n)	{	CtlPanelX,CtlPanelY+(32 * (n)),\
							CtlPanelX+40,CtlPanelY+(32 * (n)) + 32}
#define	CtlPanel2R(x,y)	{	CtlPanel2X+(x)*8,CtlPanel2Y+(y)*8,\
							CtlPanel2X+32+(x)*8,CtlPanel2Y+24+(y)*8}
#define	CtlPanel3R(x,y)	{	CtlPanel3X+(x)*8,CtlPanel3Y+(y)*8,\
							CtlPanel3X+32+(x)*8,CtlPanel3Y+24+(y)*8}
static	UserItem	CtlPanels[] =
					{
{CtlPanelR(0),uii_RadioButton,CTL_STARTUPPIC,CTL_STARTDNPIC,"Start or Resume a Game",sc_None,ui_Normal,USL_CtlButtonCustom},
{CtlPanelR(1),uii_RadioButton,CTL_HELPUPPIC,CTL_HELPDNPIC,"Get Help With Commander Keen",sc_None,ui_Normal,USL_CtlButtonCustom},
{CtlPanelR(2),uii_RadioButton,CTL_DISKUPPIC,CTL_DISKDNPIC,"Load / Save / Quit",sc_None,ui_Normal,USL_CtlButtonCustom},
{CtlPanelR(3),uii_RadioButton,CTL_CONTROLSUPPIC,CTL_CONTROLSDNPIC,"Choose Controls",sc_C,ui_Normal,USL_CtlButtonCustom},
{CtlPanelR(4),uii_RadioButton,CTL_SOUNDUPPIC,CTL_SOUNDDNPIC,"Select Sound Device",sc_F2,ui_Normal,USL_CtlButtonCustom},
{CtlPanelR(5),uii_RadioButton,CTL_MUSICUPPIC,CTL_MUSICDNPIC,"Turn Music On / Off",sc_F7,ui_Normal,USL_CtlButtonCustom},
{-1,-1,-1,-1,uii_Bad}
					},
					CtlPPanels[] =
					{
{CtlPanel2R(10,0),uii_RadioButton,CTL_P_NEWGAMEUPPIC,CTL_P_NEWGAMEDNPIC,"Choose Difficulty for the New Game",sc_F5,ui_Normal,USL_CtlPButtonCustom},
{CtlPanel2R(15,0),uii_RadioButton,CTL_P_RESUMEUPPIC,CTL_P_RESUMEDNPIC,"Go Back to Current Game",sc_None,ui_Normal,USL_CtlPButtonCustom},
{-1,-1,-1,-1,uii_Bad}
					},
					CtlPSPanels[] =
					{
{CtlPanel3R(13,5),uii_Button,CTL_P_MEDUPPIC,CTL_P_MEDDNPIC,"Start New Game in Normal Mode",sc_None,ui_Normal,USL_CtlPSButtonCustom},
{CtlPanel3R(8,5),uii_Button,CTL_P_EASYUPPIC,CTL_P_EASYDNPIC,"Start New Game in Easy Mode",sc_None,ui_Normal,USL_CtlPSButtonCustom},
{CtlPanel3R(18,5),uii_Button,CTL_P_HARDUPPIC,CTL_P_HARDDNPIC,"Start New Game in Hard Mode",sc_None,ui_Normal,USL_CtlPSButtonCustom},
{-1,-1,-1,-1,uii_Bad}
					},
					CtlPRPanels[] =
					{
{CtlPanel3R(13,5),uii_Button,CTL_P_GORESUMEUPPIC,CTL_P_GORESUMEDNPIC,"Resume Current Game",sc_None,ui_Normal,USL_CtlPRButtonCustom},
{-1,-1,-1,-1,uii_Bad}
					},
					CtlHPanels[] =
					{
{CtlPanel2R(8,0),uii_Button,CTL_H_LOSTUPPIC,CTL_H_LOSTDNPIC,"Help Me, I'm Lost!",sc_F1,ui_Normal,USL_CtlHButtonCustom},
{CtlPanel2R(13,0),uii_Button,CTL_H_CTRLUPPIC,CTL_H_CTRLDNPIC,"Get Help with Controls",sc_None,ui_Normal,USL_CtlHButtonCustom},
{CtlPanel2R(18,0),uii_Button,CTL_H_STORYUPPIC,CTL_H_STORYDNPIC,"Read Story & Game Tips",sc_None,ui_Normal,USL_CtlHButtonCustom},
{-1,-1,-1,-1,uii_Bad}
					},
					CtlDPanels[] =
					{
{CtlPanel2R(9,0),uii_RadioButton,CTL_D_LSGAMEUPPIC,CTL_D_LSGAMEDNPIC,"Load or Save a Game",sc_F6,ui_Normal,USL_CtlDButtonCustom},
{CtlPanel2R(15,0),uii_RadioButton,CTL_D_DOSUPPIC,CTL_D_DOSDNPIC,"Exit to DOS",sc_Q,ui_Normal,USL_CtlDButtonCustom},
{-1,-1,-1,-1,uii_Bad}
					},
					CtlDLSPanels[] =
					{
#define	CtlPanel3LSR(x,y)	{	CtlPanel3X+(x)*8,CtlPanel3Y+(y)*8,\
								CtlPanel3X+32+(x)*8,CtlPanel3Y+16+(y)*8}
{CtlPanel3LSR(1,0),uii_Button,CTL_D_LOADUPPIC,CTL_D_LOADDNPIC,"Load This Game",sc_None,ui_Normal,USL_CtlDLButtonCustom},
{CtlPanel3LSR(6,0),uii_Button,CTL_D_SAVEUPPIC,CTL_D_SAVEDNPIC,"Save Current Game Here",sc_None,ui_Normal,USL_CtlDSButtonCustom},
{CtlPanel3LSR(1,2),uii_Button,CTL_D_LOADUPPIC,CTL_D_LOADDNPIC,"Load This Game",sc_None,ui_Normal,USL_CtlDLButtonCustom},
{CtlPanel3LSR(6,2),uii_Button,CTL_D_SAVEUPPIC,CTL_D_SAVEDNPIC,"Save Current Game Here",sc_None,ui_Normal,USL_CtlDSButtonCustom},
{CtlPanel3LSR(1,4),uii_Button,CTL_D_LOADUPPIC,CTL_D_LOADDNPIC,"Load This Game",sc_None,ui_Normal,USL_CtlDLButtonCustom},
{CtlPanel3LSR(6,4),uii_Button,CTL_D_SAVEUPPIC,CTL_D_SAVEDNPIC,"Save Current Game Here",sc_None,ui_Normal,USL_CtlDSButtonCustom},
{CtlPanel3LSR(1,6),uii_Button,CTL_D_LOADUPPIC,CTL_D_LOADDNPIC,"Load This Game",sc_None,ui_Normal,USL_CtlDLButtonCustom},
{CtlPanel3LSR(6,6),uii_Button,CTL_D_SAVEUPPIC,CTL_D_SAVEDNPIC,"Save Current Game Here",sc_None,ui_Normal,USL_CtlDSButtonCustom},
{CtlPanel3LSR(1,8),uii_Button,CTL_D_LOADUPPIC,CTL_D_LOADDNPIC,"Load This Game",sc_None,ui_Normal,USL_CtlDLButtonCustom},
{CtlPanel3LSR(6,8),uii_Button,CTL_D_SAVEUPPIC,CTL_D_SAVEDNPIC,"Save Current Game Here",sc_None,ui_Normal,USL_CtlDSButtonCustom},
{CtlPanel3LSR(1,10),uii_Button,CTL_D_LOADUPPIC,CTL_D_LOADDNPIC,"Load This Game",sc_None,ui_Normal,USL_CtlDLButtonCustom},
{CtlPanel3LSR(6,10),uii_Button,CTL_D_SAVEUPPIC,CTL_D_SAVEDNPIC,"Save Current Game Here",sc_None,ui_Normal,USL_CtlDSButtonCustom},
{CtlPanel3LSR(1,12),uii_Button,CTL_D_LOADUPPIC,CTL_D_LOADDNPIC,"Load This Game",sc_None,ui_Normal,USL_CtlDLButtonCustom},
{CtlPanel3LSR(6,12),uii_Button,CTL_D_SAVEUPPIC,CTL_D_SAVEDNPIC,"Save Current Game Here",sc_None,ui_Normal,USL_CtlDSButtonCustom},
{-1,-1,-1,-1,uii_Bad}
					},
					CtlDEPanels[] =
					{
#define	CtlPanel3ER(x,y)	{	CtlPanel3X+(x)*8,CtlPanel3Y+(y)*8,\
								CtlPanel3X+40+(x)*8,CtlPanel3Y+24+(y)*8}
{CtlPanel3ER(12,5),uii_Button,CTL_D_EXITUPPIC,CTL_D_EXITDNPIC,"Really Exit to DOS",sc_None,ui_Normal,USL_CtlDEButtonCustom},
{-1,-1,-1,-1,uii_Bad}
					},
					CtlCPanels[] =
					{
{CtlPanel2R(8,0),uii_RadioButton,CTL_C_KBDUPPIC,CTL_C_KBDDNPIC,"Use / Configure Keyboard",sc_F3,ui_Normal,USL_CtlCButtonCustom},
{CtlPanel2R(13,0),uii_RadioButton,CTL_C_JOY1UPPIC,CTL_C_JOY1DNPIC,"Use / Configure Joystick 1",sc_None,ui_Normal,USL_CtlCButtonCustom},
{CtlPanel2R(18,0),uii_RadioButton,CTL_C_JOY2UPPIC,CTL_C_JOY2DNPIC,"Use / Configure Joystick 2",sc_None,ui_Normal,USL_CtlCButtonCustom},
{-1,-1,-1,-1,uii_Bad}
					},
#define	CtlPanelKC3R(x,y)	{	CtlPanel3X+(x)*8,CtlPanel3Y+(y)*8,\
								CtlPanel3X+56+(x)*8,CtlPanel3Y+32+(y)*8}
					CtlCKbdPanels[] =
					{
{CtlPanelKC3R(1,2),uii_KeyCap,CTL_KEYCAPPIC,CTL_KEYCAPCURPIC,"Define Key for Jumping",sc_None,ui_Normal,USL_CtlCKbdButtonCustom},
{CtlPanelKC3R(1,6),uii_KeyCap,CTL_KEYCAPPIC,CTL_KEYCAPCURPIC,"Define Key for Throwing",sc_None,ui_Normal,USL_CtlCKbdButtonCustom},
{CtlPanelKC3R(8,0),uii_KeyCap,CTL_KEYCAPPIC,CTL_KEYCAPCURPIC,"Define Key to move Up & Left",sc_None,ui_Normal,USL_CtlCKbdButtonCustom},
{CtlPanelKC3R(15,0),uii_KeyCap,CTL_KEYCAPPIC,CTL_KEYCAPCURPIC,"Define Key to move Up",sc_None,ui_Normal,USL_CtlCKbdButtonCustom},
{CtlPanelKC3R(22,0),uii_KeyCap,CTL_KEYCAPPIC,CTL_KEYCAPCURPIC,"Define Key to move Up & Right",sc_None,ui_Normal,USL_CtlCKbdButtonCustom},
{CtlPanelKC3R(8,4),uii_KeyCap,CTL_KEYCAPPIC,CTL_KEYCAPCURPIC,"Define Key to move Left",sc_None,ui_Normal,USL_CtlCKbdButtonCustom},
{CtlPanelKC3R(22,4),uii_KeyCap,CTL_KEYCAPPIC,CTL_KEYCAPCURPIC,"Define Key to move Right",sc_None,ui_Normal,USL_CtlCKbdButtonCustom},
{CtlPanelKC3R(8,8),uii_KeyCap,CTL_KEYCAPPIC,CTL_KEYCAPCURPIC,"Define Key to move Down & Left",sc_None,ui_Normal,USL_CtlCKbdButtonCustom},
{CtlPanelKC3R(15,8),uii_KeyCap,CTL_KEYCAPPIC,CTL_KEYCAPCURPIC,"Define Key to move Down",sc_None,ui_Normal,USL_CtlCKbdButtonCustom},
{CtlPanelKC3R(22,8),uii_KeyCap,CTL_KEYCAPPIC,CTL_KEYCAPCURPIC,"Define Key to move Down & Right",sc_None,ui_Normal,USL_CtlCKbdButtonCustom},
{-1,-1,-1,-1,uii_Bad}
					},
					CtlCJoyPanels[] =
					{
{CtlPanel3R(13,5),uii_Button,CTL_C_CALIBRATEUPPIC,CTL_C_CALIBRATEDNPIC,"Configure Joystick",sc_None,ui_Normal,USL_CtlCJoyButtonCustom},
{-1,-1,-1,-1,uii_Bad}
					},
					CtlSPanels[] =
					{
{CtlPanel2R(3,0),uii_RadioButton,CTL_S_NOSNDUPPIC,CTL_S_NOSNDDNPIC,"Turn Sound Off",sc_None,ui_Normal,USL_CtlSButtonCustom},
{CtlPanel2R(8,0),uii_RadioButton,CTL_S_PCSNDUPPIC,CTL_S_PCSNDDNPIC,"Use PC Speaker",sc_None,ui_Normal,USL_CtlSButtonCustom},
{CtlPanel2R(13,0),uii_RadioButton,CTL_S_ADLIBUPPIC,CTL_S_ADLIBDNPIC,"Use AdLib Sound Effects",sc_None,ui_Normal,USL_CtlSButtonCustom},
{CtlPanel2R(18,0),uii_RadioButton,CTL_S_SNDBLUPPIC,CTL_S_SNDBLDNPIC,"Use SoundBlaster Sound Effects",sc_None,ui_Normal,USL_CtlSButtonCustom},
{CtlPanel2R(23,0),uii_RadioButton,CTL_S_SNDSRCUPPIC,CTL_S_SNDSRCDNPIC,"Use Sound Source Sound Effects",sc_None,ui_Normal,USL_CtlSButtonCustom},
{-1,-1,-1,-1,uii_Bad}
					},
					CtlSSSPanels[] =
					{
{CtlPanel3R(7,2),uii_CheckBox,CTL_CHECKUPPIC,CTL_CHECKDNPIC,"Turn Tandy Mode On / Off",sc_None,ui_Normal,0,"Tandy Mode"},
{CtlPanel3R(7,6),uii_CheckBox,CTL_CHECKUPPIC,CTL_CHECKDNPIC,"Switch between LPT1 & LPT2",sc_None,ui_Normal,0,"Use LPT2"},
{-1,-1,-1,-1,uii_Bad}
					},
					CtlMPanels[] =
					{
{CtlPanel2R(9,0),uii_RadioButton,CTL_M_NOMUSUPPIC,CTL_M_NOMUSDNPIC,"Background Music Off"},
{CtlPanel2R(15,0),uii_RadioButton,CTL_M_ADLIBUPPIC,CTL_M_ADLIBDNPIC,"Use AdLib/SoundBlaster Music"},
{-1,-1,-1,-1,uii_Bad}
					},
					*CtlPanels2[] =
					{
						CtlPPanels,	// Start
						CtlHPanels,	// Help
						CtlDPanels,	// Disk
						CtlCPanels,	// Controls
						CtlSPanels,	// Sound
						CtlMPanels	// Music
					},
					*TheItems[4] = {CtlPanels};
static	int			CtlPanelButton;

///////////////////////////////////////////////////////////////////////////
//
//	USL_TurnOff() - Goes through a list of UserItems and sets them all to
//		the normal state
//
///////////////////////////////////////////////////////////////////////////
static void
USL_TurnOff(UserItem *ip)
{
	while (ip->type != uii_Bad)
	{
		ip->sel = ui_Normal;
		ip++;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_FindDown() - Finds which UserItem, if any, is selected in the given
//		list
//
///////////////////////////////////////////////////////////////////////////
static int
USL_FindDown(UserItem *ip)
{
	int	i;

	for (i = 0;ip->type != uii_Bad;i++,ip++)
		if (ip->sel & ui_Selected)
			return(i);
	return(-1);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_ShowHelp() - Shows the specified string in the help window
//
///////////////////////////////////////////////////////////////////////////
static void
USL_ShowHelp(char *s)
{
	WindowRec	wr;

	if (!s)
		return;

	US_SaveWindow(&wr);
	US_RestoreWindow(&HelpWindow);

	US_ClearWindow();
	US_PrintCentered(s);

	US_RestoreWindow(&wr);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_HandleError() - Handles telling the user that there's been an error
//
///////////////////////////////////////////////////////////////////////////
static void
USL_HandleError(int num)
{
	char	buf[64];

	strcpy(buf,"Error: ");
	if (num < 0)
		strcat(buf,"Unknown");
	else if (num == ENOMEM)
		strcat(buf,"Disk is Full");
	else if (num == EINVFMT)
		strcat(buf,"File is Incomplete");
	else
		strcat(buf,sys_errlist[num]);

	VW_HideCursor();

	fontcolor = F_SECONDCOLOR;
	USL_ShowHelp(buf);
	fontcolor = F_BLACK;
	VW_UpdateScreen();

	IN_ClearKeysDown();
	IN_Ack();

	VW_ShowCursor();
	VW_UpdateScreen();
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_DrawItem() - Draws a UserItem. If there's a custom routine, this will
//		call it with a uic_Draw command. If the custom routine returns true,
//		then the routine handled all of the drawing. If it returns false,
//		then this routine does the default drawing.
//
///////////////////////////////////////////////////////////////////////////
static void
USL_DrawItem(word hiti,word hitn)
{
	boolean		handled,centered;
	char		*text;
	word		w,h;
	int			picup,picdown;
	Rect		r;
	UserItem	*ip;

	ip = &TheItems[hiti][hitn];
	if (ip->custom)
		handled = ip->custom(uic_Draw,hiti,hitn);
	else
		handled = false;

	if (!handled)
	{
		picup = ip->picup;
		picdown = ip->picdown;
		switch (ip->type)
		{
		case uii_CheckBox:
			px = ip->r.lr.x + 8;
			py = ip->r.ul.y + 8;
			text = ip->text;
			centered = false;
			break;
		case uii_KeyCap:
			if (!(ip->sel & ui_Selected))
			{
				text = ip->text;
				if (text)
				{
					r = ip->r;
					centered = true;
				}
			}
			else
				text = nil;
			break;
		default:
			text = nil;
			break;
		}

		VWB_DrawPic(ip->r.ul.x,ip->r.ul.y,
						(ip->sel & ui_Selected)? picdown : picup);
		if (text)
		{
			if (centered)
				USL_PrintInCenter(text,r);
			else
			{
				USL_MeasureString(text,&w,&h);
				VWB_Bar(px,py,w + 7,h,WHITE);
				USL_DrawString(text);
			}
		}
		if (ip->sel & ui_Disabled)
		{
			if ((picup == CTL_D_LOADUPPIC) || (picup == CTL_D_SAVEUPPIC))
				VWB_DrawMPic(ip->r.ul.x,ip->r.ul.y,CTL_LSMASKPICM);
			else
				VWB_DrawMPic(ip->r.ul.x,ip->r.ul.y,CTL_LITTLEMASKPICM);
		}
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_DoHit() - Handles a hit on a UserItem. If there's a custom routine,
//			it will be called. If it returns true, then don't do anything
//			more. If it returns false, then use the standard behaviour
//
///////////////////////////////////////////////////////////////////////////
static void
USL_DoHit(word hiti,word hitn)
{
	boolean		handled;
	word		i;
	UserItem	*ip;

	ip = &TheItems[hiti][hitn];
	if (ip->custom)
		handled = ip->custom(uic_Hit,hiti,hitn);
	else
		handled = false;

	if (!handled)
	{
		if (TheItems[hiti][hitn].sel & ui_Disabled)
		{
			fontcolor = F_SECONDCOLOR;
			USL_ShowHelp("This Item is Disabled");
			fontcolor = F_BLACK;
			return;
		}

		FlushHelp = true;

		switch (ip->type)
		{
		case uii_Button:
			// Must have a custom routine to handle hits - this just redraws
			ip->sel ^= ui_Selected;
			USL_DrawItem(hiti,hitn);
		case uii_CheckBox:
			ip->sel ^= ui_Selected;
			USL_DrawItem(hiti,hitn);
			break;
		case uii_RadioButton:
			for (i = 0,ip = TheItems[hiti];ip->type != uii_Bad;i++,ip++)
			{
				if
				(
					(i != hitn)
				&&	(ip->type == uii_RadioButton)
				&&	(ip->sel & ui_Selected)
				)
				{
					ip->sel &= ~ui_Selected;
					USL_DrawItem(hiti,i);
				}
			}
			TheItems[hiti][hitn].sel |= ui_Selected;
			USL_DrawItem(hiti,hitn);
			break;
		case uii_KeyCap:
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_IsInRect() - Checks to see if the coordinates given are within any
//		of the Rects in the UserItem list. If so, returns true & sets the
//		index & number for lookup. If not, returns false.
//
///////////////////////////////////////////////////////////////////////////
static boolean
USL_IsInRect(word x,word y,word *index,word *number)
{
	UserItem	*item,**items;

	items = TheItems;
	*index = 0;
	while (*items)
	{
		item = *items;
		*number = 0;
		while (item->type != uii_Bad)
		{
			if
			(
				(x >= item->r.ul.x)
			&&	(x <  item->r.lr.x)
			&&	(y >= item->r.ul.y)
			&&	(y <  item->r.lr.y)
			)
				return(true);
			(*number)++;
			item++;
		}

		(*index)++;
		items++;
	}
	return(false);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_TrackItem() - Tracks the given item. If the cursor is inside of the
//		item, it's redrawn as down. If the cursor is outside, the item is
//		drawn in its original state. Returns true if the button was released
//		while the cursor was inside the item, or false if it wasn't.
//
///////////////////////////////////////////////////////////////////////////
static boolean
USL_TrackItem(word hiti,word hitn)
{
	boolean		inside,last;
	word		ini,inn,
				on,
				sel,othersel;
	UserItem	*ip,*op;

	ip = &TheItems[hiti][hitn];
	sel = ip->sel;
	if (ip->type == uii_RadioButton)
	{
		inside = false;
		for (op = TheItems[hiti],on = 0;op->type != uii_Bad;op++,on++)
		{
			if (op->sel & ui_Selected)
			{
				inside = true;
				break;
			}
		}
		if (!inside)
			op = ip;
		othersel = op->sel;
	}
	else
		op = nil;

	if (ip->sel & ui_Disabled)
	{
		fontcolor = F_SECONDCOLOR;
		USL_ShowHelp("This item is disabled");
		fontcolor = F_BLACK;

		while (US_UpdateCursor())
			VW_UpdateScreen();

		FlushHelp = true;
		return(false);
	}

	last = false;
	do
	{
		USL_IsInRect(CursorX,CursorY,&ini,&inn);
		inside = (ini == hiti) && (inn == hitn);
		if (inside != last)
		{
			if (inside)
			{
				if (op)
				{
					op->sel &= ~ui_Selected;
					ip->sel |= ui_Selected;
				}
				else
					ip->sel = sel ^ ui_Selected;
			}
			else
			{
				if (op && (op != ip))
				{
					op->sel |= ui_Selected;
					ip->sel &= ~ui_Selected;
				}
				else
					ip->sel = sel;
			}

			USL_DrawItem(hiti,hitn);
			if (op && (op != ip))
				USL_DrawItem(hiti,on);

			last = inside;
		}
		VW_UpdateScreen();
	} while (US_UpdateCursor());

	if (op)
		op->sel = othersel;
	ip->sel = sel;
	if (!inside)
	{
		if (op && (op != ip))
			USL_DrawItem(hiti,on);
		USL_DrawItem(hiti,hitn);
		VW_UpdateScreen();
	}

	return(inside);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_GlideCursor() - Smoothly moves the cursor to the given location
//
///////////////////////////////////////////////////////////////////////////
static void
USL_GlideCursor(long newx,long newy)
{
	word	steps;
	long	x,y,
			dx,dy;

	if (grmode == CGAGR)
		steps = 1;
	else
		steps = 8;

	x = (long)CursorX << 16;
	dx = ((newx << 16) - x) / steps;
	y = (long)CursorY << 16;
	dy = ((newy << 16) - y) / steps;

	while ((CursorX != newx) || (CursorY != newy))
	{
		x += dx;
		y += dy;

		CursorX = x >> 16;
		CursorY = y >> 16;
		VW_MoveCursor(CursorX,CursorY);
		VW_UpdateScreen();
	}
	CursorBad = true;
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_FindRect() - Code so ugly you'll puke! Given a Rect and direction,
//      this routine will try to find a UserItem to move the cursor to
//
///////////////////////////////////////////////////////////////////////////
static void
USL_FindRect(Rect r,Motion xd,Motion yd)
{
	word		i,i1,i2,i3;
	Motion		m1,m2;
	Point		diffs[9],diff,*dp;
	Rect		*rp,*good,*goods[9];
	UserItem	*ip,**items;

	for (m1 = motion_Up,dp = diffs;m1 <= motion_Down;m1++)
	{
		for (m2 = motion_Left;m2 <= motion_Right;m2++,dp++)
		{
			dp->x = m2 * 1024;
			dp->y = m1 * 1024;
		}
	}
	for (i = 0;i < 9;i++)
		goods[i] = nil;

	// Find out which octants all of the rects (except r) are in
	for (items = TheItems;*items;items++)
	{
		for (ip = *items;ip->type != uii_Bad;ip++)
		{
			rp = &ip->r;
			diff.x = rp->ul.x - r.ul.x;
			diff.y = rp->ul.y - r.ul.y;
			if (!(diff.x || diff.y))
				continue;

			if	// 1,4,7
			(
				((rp->ul.x >= r.ul.x) && (rp->ul.x < r.lr.x))
			||	((rp->lr.x > r.ul.x) && (rp->lr.x <= r.lr.x))
			)
			{
				if (rp->lr.y <= r.ul.y)
				{
					if (!(goods[1] && (diff.y < diffs[1].y)))
					{
						goods[1] = rp;
						diffs[1] = diff;
					}
				}
				else if (rp->ul.y >= r.lr.y)
				{
					if (!(goods[7] && (diff.y > diffs[7].y)))
					{
						goods[7] = rp;
						diffs[7] = diff;
					}
				}
			}

			if	// 3,4,5
			(
				((rp->ul.y >= r.ul.y) && (rp->ul.y < r.lr.y))
			||	((rp->lr.y > r.ul.y) && (rp->lr.y <= r.lr.y))
			)
			{
				if (rp->lr.x <= r.ul.x)
				{
					if (!(goods[3] && (diff.x < diffs[3].x)))
					{
						goods[3] = rp;
						diffs[3] = diff;
					}
				}
				else if (rp->ul.x >= r.lr.x)
				{
					if (!(goods[5] && (diff.x > diffs[5].x)))
					{
						goods[5] = rp;
						diffs[5] = diff;
					}
				}
			}

			if (rp->ul.x < r.ul.x)	// 0,6
			{
				if (rp->lr.y <= r.ul.y)
				{
					if
					(
						(!goods[0])
					|| 	(diff.y > diffs[0].y)
					||	(diff.x > diffs[6].x)
					)
					{
						goods[0] = rp;
						diffs[0] = diff;
					}
				}
				else if (rp->ul.y >= r.lr.y)
				{
					if
					(
						(!goods[6])
					|| 	(diff.y < diffs[6].y)
					||	(diff.x > diffs[6].x)
					)
					{
						goods[6] = rp;
						diffs[6] = diff;
					}
				}
			}

			if (rp->lr.x > r.lr.x)	// 2,8
			{
				if (rp->lr.y <= r.ul.y)
				{
					if
					(
						(!goods[2])
					|| 	(diff.y > diffs[2].y)
					|| 	(diff.x < diffs[2].x)
					)
					{
						goods[2] = rp;
						diffs[2] = diff;
					}
				}
				else if (rp->ul.y >= r.lr.y)
				{
					if
					(
						(!goods[8])
					|| 	(diff.y < diffs[8].y)
					||	(diff.x < diffs[8].x)
					)
					{
						goods[8] = rp;
						diffs[8] = diff;
					}
				}
			}
		}
	}

	switch (yd)
	{
	case motion_Up:
		i1 = 1,i2 = 0,i3 = 2;
		break;
	case motion_None:
		switch (xd)
		{
		case motion_Left:
			i1 = 3,i2 = 0,i3 = 6;
			break;
		case motion_Right:
			i1 = 5,i2 = 8,i3 = 2;
			break;
		}
		break;
	case motion_Down:
		i1 = 7,i2 = 8,i3 = 6;
		break;
	}

	(
		(good = goods[i1])
	|| 	(good = goods[i2])
	|| 	(good = goods[i3])
	|| 	(good = &r)
	);
#if 0
	CursorX = good->lr.x - 8;
	CursorY = good->lr.y - 8;
	CursorBad = true;
	US_UpdateCursor();
#endif
	USL_GlideCursor(good->lr.x - 8,good->lr.y - 8);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_CtlButtonCustom() - The custom routine for all of the Control Panel
//		(leftmost) buttons. Clears all of the other item lists, clears the
//		large area, and draws the line dividing the top and bottom areas.
//		Then it sets up and draws the appropriate top row of icons.
//
///////////////////////////////////////////////////////////////////////////
static boolean
USL_CtlButtonCustom(UserCall call,word i,word n)
{
	word		j;
	UserItem	*ip;

	if (call != uic_Hit)
		return(false);

	if (n == CtlPanelButton)
		return(true);

	US_ClearWindow();
	for (j = 8;j < 38;j++)
	{
		VWB_DrawTile8M(j * 8,6 * 8,10);
		VWB_DrawTile8M(j * 8,21 * 8,10);
	}
	VWB_DrawTile8M(7 * 8,6 * 8,9);
	VWB_DrawTile8M(38 * 8,6 * 8,11);
	VWB_DrawTile8M(7 * 8,21 * 8,9);
	VWB_DrawTile8M(38 * 8,21 * 8,11);

	for (j = 1;j < 4;j++)
		TheItems[j] = nil;

	// Set to new button
	CtlPanelButton = n;

	// Draw new items
	TheItems[1] = ip = CtlPanels2[CtlPanelButton];
	j = 0;
	while (ip && (ip->type != uii_Bad))
	{
		USL_DrawItem(i + 1,j);
		if (ip->sel & ui_Selected)
			USL_DoHit(i + 1,j);
		j++;
		ip++;
	}

	return(false);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_CtlCKbdButtonCustom() - The custom routine for the keyboard keycaps.
//		This routine gets a scancode and puts it in the appropriate
//		KbdDefs[0] member.
//
///////////////////////////////////////////////////////////////////////////
static boolean
USL_CtlCKbdButtonCustom(UserCall call,word i,word n)
{
	boolean		state;
	word		j;
	ScanCode	scan;
	longword	time;
	UserItem	*ip;

	if (call != uic_Hit)
		return(false);

	ip = &TheItems[i][n];

	fontcolor = F_SECONDCOLOR;
	USL_ShowHelp(ip->help);
	fontcolor = F_BLACK;
	VW_HideCursor();
	VWB_DrawPic(ip->r.ul.x,ip->r.ul.y,ip->picdown);
	VW_UpdateScreen();

	LastScan = sc_None;
	time = TimeCount;
	state = true;
	do
	{
		if (TimeCount - time > 35)	// Half-second delays
		{
			state ^= true;
			VWB_DrawPic(ip->r.ul.x,ip->r.ul.y,state? ip->picdown : ip->picup);
			VW_UpdateScreen();
			time = TimeCount;
		}
		if (US_UpdateCursor())
		{
			while (US_UpdateCursor())
				;
			scan = sc_Escape;
			break;
		}

		asm	pushf
		asm	cli
		if (LastScan == sc_LShift)
			LastScan = sc_None;
		asm	popf
	} while (!(scan = LastScan));
	IN_ClearKey(scan);
	if (scan != sc_Escape)
	{
		for (j = 0,state = false;j < 10;j++)
		{
			if (j == n)
				continue;
			if (*(KeyMaps[j]) == scan)
			{
				state = true;
				break;
			}
		}
		if (state)
		{
			fontcolor = F_SECONDCOLOR;
			USL_ShowHelp("That Key is Already Used!");
			fontcolor = F_BLACK;
		}
		else
		{
			ip->text = IN_GetScanName(scan);
			*(KeyMaps[n]) = scan;
			FlushHelp = true;
		}
	}

	USL_DrawItem(i,n);
	VW_ShowCursor();
	VW_UpdateScreen();

	return(true);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_CtlCJoyButtonCustom() - The custom button routine for joystick
//		calibration
//
///////////////////////////////////////////////////////////////////////////
static boolean
USL_CtlCJoyButtonCustom(UserCall call,word i,word n)
{
	word	joy,
			minx,maxx,
			miny,maxy;

	i++,n++;	// Shut the compiler up

	if (call != uic_Hit)
		return(false);

	IN_ClearKeysDown();
	joy = USL_FindDown(CtlCPanels) - 1;

	VW_HideCursor();
	FlushHelp = true;
	fontcolor = F_SECONDCOLOR;

	USL_ShowHelp("Move Joystick to the Upper-Left");
	VW_UpdateScreen();
	while ((LastScan != sc_Escape) && !IN_GetJoyButtonsDB(joy))
		;
	if (LastScan != sc_Escape)
	{
		IN_GetJoyAbs(joy,&minx,&miny);
		while (IN_GetJoyButtonsDB(joy))
			;

		USL_ShowHelp("Move Joystick to the Lower-Right");
		VW_UpdateScreen();
		while ((LastScan != sc_Escape) && !IN_GetJoyButtonsDB(joy))
			;
		if (LastScan != sc_Escape)
		{
			IN_GetJoyAbs(0,&maxx,&maxy);
			IN_SetupJoy(joy,minx,maxx,miny,maxy);
		}
	}

	if (LastScan != sc_Escape)
		while (IN_GetJoyButtonsDB(joy))
			;

	if (LastScan)
		IN_ClearKeysDown();

	fontcolor = F_BLACK;
	VW_ShowCursor();

	return(false);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_ClearBottom() - Clears the bottom part of the window
//
///////////////////////////////////////////////////////////////////////////
static void
USL_ClearBottom(void)
{
	WindowRec	wr;

	US_SaveWindow(&wr);
	US_RestoreWindow(&BottomWindow);

	US_ClearWindow();

	US_RestoreWindow(&wr);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_FormatHelp() - Formats helptext. Runs through and calculates the
//		number of lines, and the offset for the start of each line. Stops
//		after len bytes or when it hits a tilde ('~'). Munges the text.
//
///////////////////////////////////////////////////////////////////////////
static word
USL_FormatHelp(char far *text,long len)
{
	word	line,
			w,h,
			far *off;
	char	c,
			far *s,far *l,far *le;

	WindowX += 4;
	WindowW -= 4;

	MM_GetPtr(&LineOffsets,MaxHelpLines * sizeof(word));
	off = (word far *)LineOffsets;
	for (line = 0,le = l = s = text;(s - text < len) && (*s != '~');s++)
	{
		if ((c = *s) == '\n')
		{
			*s = '\0';
			*off++ = l - text;	// Save offset of start of line
			line++;				// Bump line number
			le = l = s + 1;		// Set start of line ptr
		}

		if (c == '\r')
			c = *s = ' ';
		if						// Strip orphaned spaces
		(
			(c == ' ')
		&& 	(s == l)
		&& 	(*(s - 1) == '\0')
		&&	(*(s + 1) != ' ')
		&& 	(s > text)
		)
			le = l = s + 1;
		else if (c == ' ')
		{
			*s = '\0';
			USL_MeasureString(l,&w,&h);
			if (w >= WindowW)	// If string width exceeds window,
			{
				*s = c;			// Replace null char with proper char
				*le = '\0';		// Go back to last line end
				*off++ = l - text;	// Save offset of start of line
				line++;			// Bump line number
				l = s = le + 1;	// Start next time through after last line end
			}
			else
			{
				*s = c;			// Width still ok - put char back
				le = s;			// And save ptr to last ok end of word
			}
		}
	}

	WindowX -= 4;
	WindowW += 4;

	return(line);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_DrawHelp() - Draws helptext in the current window
//
///////////////////////////////////////////////////////////////////////////
static void
USL_DrawHelp(char far *text,word start,word end,word line,word h,word far *lp)
{
	px = WindowX + 4;
	py = WindowY + (line * h);
	for (lp += start;start < end;start++,px = WindowX + 4,py += h)
		USL_DrawString(text + *lp++);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_DoHelp() - Formats and displays the specified help
//
///////////////////////////////////////////////////////////////////////////
static void
USL_DoHelp(memptr text,long len)
{
	boolean		done,
				moved;
	int			scroll;
	word		i,
				pixdiv,
				w,h,
				lines,cur,page,
				top,num,loc,
				far *lp,
				base,srcbase,destbase;
	ScanCode	waitkey;
	longword	lasttime;
	WindowRec	wr;
	CursorInfo	info;

	USL_ShowHelp("Arrow Keys Move / Escape Exits");
	fontcolor = F_BLACK;

	US_SaveWindow(&wr);
	US_RestoreWindow(&BottomWindow);
	US_ClearWindow();

	VW_HideCursor();
	VW_UpdateScreen();

	lines = USL_FormatHelp((char far *)text,len);
	USL_MeasureString("",&w,&h);
	page = WindowH / h;
	cur = 0;
	lp = LineOffsets;

	IN_ClearKeysDown();
	moved = true;
	lasttime = 0;
	scroll = 0;
	done = false;
	waitkey = sc_None;
	while (!done)
	{
		if (moved)
		{
			while (TimeCount - lasttime < 5)
				;
			lasttime = TimeCount;

			if (scroll == -1)
			{
				top = cur;
				num = 1;
				loc = 0;
			}
			else if (scroll == +1)
			{
				num = 1;
				loc = page - 1;
				top = cur + loc;
			}
			else
			{
				top = cur;
				num = (page < lines)? page : lines;
				loc = 0;
			}
			if (scroll)
			{
				if (grmode == CGAGR)
				{
					pixdiv = 4;
					base = bufferofs + panadjust + (WindowX / pixdiv);
				}
				else if (grmode == EGAGR)
				{
					VWB_Bar(WindowX,WindowY + (loc * h),WindowW,num * h,WHITE);
					USL_DrawHelp((char far *)text,top,top + num,loc,h,lp);

					pixdiv = 8;
					base = displayofs + panadjust + (WindowX / pixdiv);
				}
				else if (grmode == VGAGR)
					pixdiv = 1;

				if (scroll == 1)
				{
					srcbase = base + ylookup[WindowY + h];
					destbase = base + ylookup[WindowY];
					if (grmode == EGAGR)
					{
						EGAWRITEMODE(1);
						VW_WaitVBL(1);
					}
					VW_ScreenToScreen(srcbase,destbase,WindowW / pixdiv,
										WindowH - h);
				}
				else
				{
					i = WindowY + (h * (page - 1));
					srcbase = base + ylookup[i - h];
					destbase = base + ylookup[i];
					base = ylookup[h];
					for (i = page - 1;i;i--,srcbase -= base,destbase -= base)
						VW_ScreenToScreen(srcbase,destbase,WindowW / pixdiv,h);
				}
				if (grmode == CGAGR)
				{
					VWB_Bar(WindowX,WindowY + (loc * h),WindowW,num * h,WHITE);
					USL_DrawHelp((char far *)text,top,top + num,loc,h,lp);
					VW_UpdateScreen();
				}
				else if (grmode == EGAGR)
				{
					base = panadjust + (WindowX / pixdiv) +
							ylookup[WindowY + (loc * h)];
					VW_ScreenToScreen(base + bufferofs,base + displayofs,
										WindowW / pixdiv,h);
				}
			}
			else
			{
				US_ClearWindow();
				USL_DrawHelp((char far *)text,top,top + num,loc,h,lp);
				VW_UpdateScreen();
			}

			moved = false;
			scroll = 0;
		}

		if (waitkey)
			while (IN_KeyDown(waitkey))
				;
		waitkey = sc_None;

		IN_ReadCursor(&info);
		if (info.y < 0)
		{
			if (cur > 0)
			{
				scroll = -1;
				cur--;
				moved = true;
			}
		}
		else if (info.y > 0)
		{
			if (cur + page < lines)
			{
				scroll = +1;
				cur++;
				moved = true;
			}
		}
		else if (info.button0 || info.button1)
			done = true;
		else if (IN_KeyDown(LastScan))
		{
			switch (LastScan)
			{
			case sc_Escape:
				done = true;
				break;
			case sc_UpArrow:
				if (cur > 0)
				{
					scroll = -1;
					cur--;
					moved = true;
				}
				break;
			case sc_DownArrow:
				if (cur + page < lines)
				{
					scroll = +1;
					cur++;
					moved = true;
				}
				break;
			case sc_PgUp:
				if (cur > page)
					cur -= page;
				else
					cur = 0;
				moved = true;
				waitkey = sc_PgUp;
				break;
			case sc_PgDn:
				if (cur + page < lines)
				{
					cur += page;
					if (cur + page >= lines)
						cur = lines - page;
					moved = true;
				}
				waitkey = sc_PgDn;
				break;
			}
		}
	}
	IN_ClearKeysDown();
	do
	{
		IN_ReadCursor(&info);
	} while (info.button0 || info.button1);

	VW_ShowCursor();
	US_ClearWindow();
	VW_UpdateScreen();
	US_RestoreWindow(&wr);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_CtlHButtonCustom() - The custom routine for all of the help buttons
//
///////////////////////////////////////////////////////////////////////////
static boolean
USL_CtlHButtonCustom(UserCall call,word i,word n)
{
	word		j;
	UserItem	*ip;

	if (call != uic_Hit)
		return(false);

	ip = &TheItems[i][n];
	if (ip->sel & ui_Disabled)
		return(false);

	ip->sel |= ui_Selected;
	USL_DrawItem(i,n);

	USL_ClearBottom();

	fontcolor = F_SECONDCOLOR;
	USL_ShowHelp("Loading & Formatting Text...");
	VW_UpdateScreen();

#ifdef	HELPTEXTLINKED	// Ugly hack because of lack of disk space...
	{
extern	char	far gametext,far context,far story;
		char	far *buf;
		memptr	dupe;

		switch (n)
		{
		case 0:
			buf = &gametext;
			break;
		case 1:
			buf = &context;
			break;
		case 2:
			buf = &story;
			break;
		}

		MM_GetPtr(&dupe,5600);
		_fmemcpy((char far *)dupe,buf,5600);

		USL_DoHelp(dupe,5600);

		MM_FreePtr(&dupe);
		if (LineOffsets)
			MM_FreePtr(&LineOffsets);
	}
#else
	{
		char	*name;
		int		file;
		long	len;
		memptr	buf;

		switch (n)
		{
		case 0:
			name = "GAMETEXT."EXTENSION;
			break;
		case 1:
			name = "CONTEXT."EXTENSION;
			break;
		case 2:
			name = "STORY."EXTENSION;
			break;
		default:
			Quit("Bad help button number");
		}

		if ((file = open(name,O_RDONLY | O_TEXT)) == -1)
			USL_HandleError(errno);
		else
		{
			len = filelength(file);
			MM_GetPtr(&buf,len);

			if (CA_FarRead(file,(byte far *)buf,len))
				USL_DoHelp(buf,len);
			else
				USL_HandleError(errno);

			close(file);
			MM_FreePtr(&buf);
		}

		if (LineOffsets)
			MM_FreePtr(&LineOffsets);
	}
#endif

	fontcolor = F_BLACK;

	ip->sel &= ~ui_Selected;
	USL_DrawItem(i,n);

	return(false);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_CtlDButtonCustom() - The custom routine for all of the disk buttons.
//		Sets up the bottom area of the window with the appropriate buttons
//
///////////////////////////////////////////////////////////////////////////
static boolean
USL_CtlDButtonCustom(UserCall call,word i,word n)
{
	word		j;
	UserItem	*ip;

	if (call != uic_Hit)
		return(false);

	ip = &TheItems[i][n];
	if (ip->sel & ui_Disabled)
		return(false);

	USL_ClearBottom();

	j = 0;
	TheItems[i + 1] = ip = n? CtlDEPanels : CtlDLSPanels;
	while (ip && (ip->type != uii_Bad))
	{
		USL_DrawItem(i + 1,j++);
		ip++;
	}

	return(false);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_DLSRect() - Draw the rectangle for the save game names
//
///////////////////////////////////////////////////////////////////////////
static Rect
USL_DLSRect(UserItem *ip)
{
	Rect	r;

	r.ul.x = ip->r.lr.x + 40 + 2;
	r.ul.y = ip->r.ul.y + 2;
	r.lr.x = WindowX + WindowW - 8 - 2;
	r.lr.y = ip->r.lr.y - 2;

	VWB_Bar(r.ul.x,r.ul.y,r.lr.x - r.ul.x,r.lr.y - r.ul.y,WHITE);

	VWB_Hlin(r.ul.x,r.lr.x,r.ul.y,BLACK);
	VWB_Hlin(r.ul.x,r.lr.x,r.lr.y,BLACK);
	VWB_Vlin(r.ul.y,r.lr.y,r.ul.x,BLACK);
	VWB_Vlin(r.ul.y,r.lr.y,r.lr.x,BLACK);

	px = r.ul.x + 2;
	py = r.ul.y + 2;

	return(r);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_CtlDLButtonCustom() - The load game custom routine
//
///////////////////////////////////////////////////////////////////////////
static boolean
USL_CtlDLButtonCustom(UserCall call,word i,word n)
{
	char		*filename,
				msg[MaxGameName + 12];
	word		err;
	int			file;
	UserItem	*ip;
	SaveGame	*game;
	WindowRec 	wr;

	// DEBUG - deal with warning user about loading a game causing abort

	game = &Games[n / 2];
	ip = &TheItems[i][n];

	switch (call)
	{
	case uic_Draw:
		if (!loadedgame)
		{
			USL_DLSRect(ip);
			fontcolor = game->present? F_BLACK : F_FIRSTCOLOR;
			USL_DrawString(game->present? game->name : "Empty");
			fontcolor = F_BLACK;
		}
		break;
	case uic_Hit:
		if (ip->sel & ui_Disabled)
			return(false);

		LeaveDriveOn++;
		filename = USL_GiveSaveName(n / 2);

		US_SaveWindow(&wr);
		US_CenterWindow(30,3);
		strcpy(msg,"Loading `");
		strcat(msg,game->name);
		strcat(msg,"\'");
		US_PrintCentered(msg);
		VW_HideCursor();
		VW_UpdateScreen();

		err = 0;
		if ((file = open(filename,O_BINARY | O_RDONLY)) != -1)
		{
			if (read(file,game,sizeof(*game)) == sizeof(*game))
			{
				if (USL_LoadGame)
					if (!USL_LoadGame(file))
						USL_HandleError(err = errno);
			}
			else
				USL_HandleError(err = errno);
			close(file);
		}
		else
			USL_HandleError(err = errno);
		if (err)
			abortgame = true;
		else
			loadedgame = true;
		game->present = true;

		if (loadedgame)
			Paused = true;

		VW_ShowCursor();
		US_RestoreWindow(&wr);

		LeaveDriveOn--;
		break;
	}
	return(false);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_CtlDSButtonCustom() - The save game custom routine
//
///////////////////////////////////////////////////////////////////////////
static boolean
USL_CtlDSButtonCustom(UserCall call,word i,word n)
{
	boolean		ok;
	char		*filename;
	word		err;
	int			file;
	Rect		r;
	UserItem	*ip;
	SaveGame	*game;
	WindowRec	wr;

	if (call != uic_Hit)
		return(false);

	game = &Games[n / 2];
	ip = &TheItems[i][n];
	if (ip->sel & ui_Disabled)
		return(false);

	FlushHelp = true;
	fontcolor = F_SECONDCOLOR;
	USL_ShowHelp("Enter Game Name / Escape Aborts");
	fontcolor = F_BLACK;

	r = USL_DLSRect(ip - 1);
	ok = US_LineInput(px,py,game->name,game->present? game->name : nil,true,
						MaxGameName,r.lr.x - r.ul.x - 8);
	if (!strlen(game->name))
		strcpy(game->name,"Untitled");
	if (ok)
	{
		US_SaveWindow(&wr);
		US_CenterWindow(10,3);
		US_PrintCentered("Saving");
		VW_HideCursor();
		VW_UpdateScreen();

		LeaveDriveOn++;
		filename = USL_GiveSaveName(n / 2);
		err = 0;
		file = open(filename,O_CREAT | O_BINARY | O_WRONLY,
					S_IREAD | S_IWRITE | S_IFREG);
		if (file != -1)
		{
			if (write(file,game,sizeof(*game)) == sizeof(*game))
			{
				if (USL_SaveGame)
					ok = USL_SaveGame(file);
				if (!ok)
					USL_HandleError(err = errno);
			}
			else
				USL_HandleError(err = ((errno == ENOENT)? ENOMEM : errno));
			close(file);
		}
		else
			USL_HandleError(err = ((errno == ENOENT)? ENOMEM : errno));
		if (err)
		{
			remove(filename);
			ok = false;
		}
		LeaveDriveOn--;

		VW_ShowCursor();
		US_RestoreWindow(&wr);
		USL_DoHit(i - 1,0);
		VW_UpdateScreen();
	}

	if (!game->present)
		game->present = ok;

	if (ok)
	{
		GameIsDirty = false;
		(ip - 1)->sel &= ~ui_Disabled;
	}

	USL_DrawItem(i,n - 1);
//	USL_CtlDLButtonCustom(uic_Draw,i,n - 1);

	return(true);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_CtlSButtonCustom() - The custom routine for all of the sound buttons
//
///////////////////////////////////////////////////////////////////////////
static boolean
USL_CtlSButtonCustom(UserCall call,word i,word n)
{
	word		j;
	UserItem	*ip;

	if (call != uic_Hit)
		return(false);

	ip = &TheItems[i][n];
	if (ip->sel & ui_Disabled)
		return(false);

	USL_ClearBottom();

	if (n == sdm_SoundSource)
	{
		j = 0;
		TheItems[i + 1] = ip = CtlSSSPanels;
		while (ip && (ip->type != uii_Bad))
		{
			USL_DrawItem(i + 1,j++);
			ip++;
		}
	}
	else
		TheItems[i + 1] = nil;

	return(false);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_CtlPButtonCustom() - The custom routine for all of the start game btns
//
///////////////////////////////////////////////////////////////////////////
static boolean
USL_CtlPButtonCustom(UserCall call,word i,word n)
{
	word		j;
	UserItem	*ip;

	if (call != uic_Hit)
		return(false);

	ip = &TheItems[i][n];
	if (ip->sel & ui_Disabled)
		return(false);

	USL_ClearBottom();

	j = 0;
	TheItems[i + 1] = ip = n? CtlPRPanels : CtlPSPanels;
	while (ip && (ip->type != uii_Bad))
	{
		USL_DrawItem(i + 1,j++);
		ip++;
	}

	return(false);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_GiveAbortWarning() - Draws a string that warns the user that an
//		action they're about to take will abort the game in progress
//
///////////////////////////////////////////////////////////////////////////
static void
USL_GiveAbortWarning(void)
{
	WindowRec	wr;

	if (!GameIsDirty)
		return;

	US_SaveWindow(&wr);
	US_RestoreWindow(&BottomWindow);
	US_HomeWindow();
	PrintY += 5;

	VWB_Bar(WindowX,WindowY,WindowW,30,WHITE);
	fontcolor = F_SECONDCOLOR;
	US_CPrint("Warning! If you do this, you'll");
	US_CPrint("abort the current game.");
	fontcolor = F_BLACK;

	US_RestoreWindow(&wr);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_CtlPSButtonCustom() - The custom routine for the start game button
//
///////////////////////////////////////////////////////////////////////////
static boolean
USL_CtlPSButtonCustom(UserCall call,word i,word n)
{
	boolean		result;
	UserItem	*ip;

	i++;	// Shut the compiler up

	switch (call)
	{
	case uic_Hit:
		switch (n)
		{
		case 0:
			restartgame = gd_Normal;
			break;
		case 1:
			restartgame = gd_Easy;
			break;
		case 2:
			restartgame = gd_Hard;
			break;
		}
		if (restartgame && ingame && USL_ResetGame)
			USL_ResetGame();
		result = false;
		break;
	case uic_Draw:
		USL_GiveAbortWarning();
		result = false;
		break;
	default:
		result = false;
		break;
	}
	return(result);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_CtlPRButtonCustom() - The custom routine for the resume game button
//
///////////////////////////////////////////////////////////////////////////
static boolean
USL_CtlPRButtonCustom(UserCall call,word i,word n)
{
	if (call != uic_Hit)
		return(false);

	i++,n++;	// Shut the compiler up
	ResumeGame = true;
	return(false);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_CtlDEButtonCustom() - The custom routine for the exit to DOS button
//
///////////////////////////////////////////////////////////////////////////
static boolean
USL_CtlDEButtonCustom(UserCall call,word i,word n)
{
	boolean		result;
	UserItem	*ip;

	i++,n++;	// Shut the compiler up

	switch (call)
	{
	case uic_Hit:
		QuitToDos = true;
		break;
	case uic_Draw:
		USL_GiveAbortWarning();
	default:
		result = false;
		break;
	}
	return(result);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_CtlCButtonCustom() - The custom routine for all of the control
//		buttons
//
///////////////////////////////////////////////////////////////////////////
static boolean
USL_CtlCButtonCustom(UserCall call,word i,word n)
{
	word		j;
	Point		p;
	UserItem	*ip;

	if (call != uic_Hit)
		return(false);

	ip = &TheItems[i][n];
	if (ip->sel & ui_Disabled)
		return(false);

	USL_ClearBottom();
	if (n == 0)	// Keyboard
	{
		TheItems[i + 1] = ip = CtlCKbdPanels;
		p = CtlCKbdPanels[2].r.lr;
		VWB_DrawPic(p.x,p.y,CTL_DIRSPIC);
	}
	else
		TheItems[i + 1] = ip = CtlCJoyPanels;

	j = 0;
	while (ip && (ip->type != uii_Bad))
	{
		USL_DrawItem(i + 1,j++);
		ip++;
	}

	return(false);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_HitHotKey() - After a hotkey was hit, move the cursor to the first
//		selected item in the group after the group containing the item
//		holding the hotkey
//
///////////////////////////////////////////////////////////////////////////
static void
USL_HitHotKey(int i,int n)
{
	UserItem	*ip;

	if (ip = TheItems[++i])
	{
		if ((n = USL_FindDown(TheItems[i])) == -1)
			n = 0;
		ip += n;
		CursorX = ip->r.lr.x - 8;
		CursorY = ip->r.lr.y - 8;
		CursorBad = true;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_CheckScan() - Checks to see if the scancode in LastScan corresponds
//		to anything in the list of useritems. If so, selects the item.
//
///////////////////////////////////////////////////////////////////////////
static boolean
USL_CheckScan(word *ci,word *cn)
{
	word		i,n;
	UserItem	*ip;

	if (!LastScan)
		return(false);

#if 1	// DEBUG - probably kill this code
	// Use 1..? for the items across the top row
	if (TheItems[1] && !IN_KeyDown(sc_RShift))
	{
		for (i = 0,ip = TheItems[1];(ip->type != uii_Bad) && (i < 9);i++,ip++)
			;
		for (n = 0;n < i;n++)
		{
			if (LastScan == 2 + n)	// Numbers from 1..9
			{
				if (!(TheItems[1][n].sel & ui_Disabled))
				{
					LastScan = sc_None;
					USL_DoHit(1,n);
					return(true);
				}
			}
		}
	}

	// Use Alt-1..6 for the items in the leftmost column
	if (IN_KeyDown(sc_RShift))
	{
		n = LastScan - 2;
		if (n < 6)	// Numbers from 1..6
		{
			USL_DoHit(0,n);
			LastScan = sc_None;
			return(true);
		}
	}
#endif

	// Check normal hotkeys for the leftmost column
	for (i = 0;CtlPanels[i].type != uii_Bad;i++)
	{
		if (CtlPanels[i].key == LastScan)
		{
			LastScan = sc_None;
			USL_DoHit(0,i);
			*ci = 0;
			*cn = i;
			USL_HitHotKey(0,i);
			return(true);
		}
	}

	// Check normal hotkeys for the top row
	for (i = 0;i < 6;i++)
	{
		for (n = 0,ip = CtlPanels2[i];ip && ip->type != uii_Bad;n++,ip++)
		{
			if ((ip->key == LastScan) && !(ip->sel & ui_Disabled))
			{
				LastScan = sc_None;
				USL_DoHit(0,i);
				USL_DoHit(1,n);
				*ci = 1;
				*cn = n;
				USL_HitHotKey(1,n);
				return(true);
			}
		}
	}
	return(false);
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_SetUpCtlPanel() - Sets the states of the UserItems to reflect the
//		values of all the appropriate variables
//
///////////////////////////////////////////////////////////////////////////
static void
USL_SetUpCtlPanel(void)
{
	word	i,j;

	GameIsDirty = ingame;

	// Set up restart game
	USL_TurnOff(CtlPPanels);
	CtlPPanels[0].sel = ingame? ui_Normal : ui_Selected;
	CtlPPanels[1].sel = ingame? ui_Selected : ui_Disabled;

	// Set up disk stuff - default to load/save game
	USL_TurnOff(CtlDPanels);
	CtlDPanels[0].sel = ui_Selected;

	// Set up load/save buttons
	USL_TurnOff(CtlDLSPanels);
	for (i = 0;i < MaxSaveGames;i++)
	{
		if (!Games[i].present)
			CtlDLSPanels[i * 2].sel = ui_Disabled;
		if (!ingame)
			CtlDLSPanels[(i * 2) + 1].sel = ui_Disabled;
	}

	// Set up Controls
	USL_TurnOff(CtlCPanels);
	CtlCPanels[1].sel = JoysPresent[0]? ui_Normal : ui_Disabled;
	CtlCPanels[2].sel = JoysPresent[1]? ui_Normal : ui_Disabled;
	if (Controls[0] == ctrl_Keyboard)
		i = 0;
	else
		i = (Controls[0] == ctrl_Joystick1)? 1 : 2;
	CtlCPanels[i].sel |= ui_Selected;
	if (JoysPresent[1] && !JoysPresent[0])
		CtlCPanels[2].key = sc_F4;
	else
		CtlCPanels[1].key = sc_F4;

	// Set up Keyboard
	for (i = 0;i < 10;i++)
		CtlCKbdPanels[i].text = IN_GetScanName(*(KeyMaps[i]));

	// Set up Sounds
	USL_TurnOff(CtlSPanels);
	CtlSPanels[sdm_AdLib].sel = AdLibPresent? ui_Normal : ui_Disabled;
#if 0	// DEBUG - hack because no space for digitized sounds on Keen Dreams
	CtlSPanels[sdm_SoundBlaster].sel =
		SoundBlasterPresent? ui_Normal : ui_Disabled;
	CtlSPanels[sdm_SoundSource].sel =
		SoundSourcePresent? ui_Normal : ui_Disabled;
#else
	CtlSPanels[sdm_SoundBlaster].sel = ui_Disabled;
	CtlSPanels[sdm_SoundSource].sel = ui_Disabled;
#endif
	CtlSPanels[SoundMode].sel |= ui_Selected;

	// Set up SoundSource
	USL_TurnOff(CtlSSSPanels);
	CtlSSSPanels[0].sel = ssIsTandy? ui_Selected : ui_Normal;
	CtlSSSPanels[1].sel = (ssPort == 2)? ui_Selected : ui_Normal;

	// Set up Music
	USL_TurnOff(CtlMPanels);
	CtlMPanels[smm_AdLib].sel = AdLibPresent? ui_Normal : ui_Disabled;
	CtlMPanels[MusicMode].sel |= ui_Selected;
}

///////////////////////////////////////////////////////////////////////////
//
//	USL_TearDownCtlPanel() - Given the state of the control panel, sets the
//		modes and values as appropriate
//
///////////////////////////////////////////////////////////////////////////
static void
USL_TearDownCtlPanel(void)
{
	int	i;

	i = USL_FindDown(CtlCPanels);
	if (i != -1)
	{
		i = i? (i == 1? ctrl_Joystick1 : ctrl_Joystick2) : ctrl_Keyboard;
		IN_SetControlType(0,i);
	}

	CtlCPanels[1].key = CtlCPanels[2].key = sc_None;

	i = USL_FindDown(CtlSPanels);
	if (i != -1)
		SD_SetSoundMode(i);

	ssIsTandy = CtlSSSPanels[0].sel & ui_Selected;
	ssPort = (CtlSSSPanels[1].sel & ui_Selected)? 2 : 1;

	i = USL_FindDown(CtlMPanels);
	if (i != -1)
	{
		SD_SetMusicMode(i);

		if (!QuitToDos)
		{
//			US_CenterWindow(20,8);		//NOLAN
//			US_CPrint("Loading");
#if 0
			fontcolor = F_SECONDCOLOR;
			US_CPrint("Sounds");
			fontcolor = F_BLACK;
#endif
//			VW_UpdateScreen();

			CA_LoadAllSounds();
		}
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	US_ControlPanel() - This is the main routine for the control panel
//
///////////////////////////////////////////////////////////////////////////
void
US_ControlPanel(void)
{
	char		gamename[MaxGameName + 10 + 1];
	ScanCode	c;
	boolean		done,
				buttondown,inrect;
	word		hiti,hitn,
				i,n,
				lasti,lastn,
				lastx,lasty;
	longword	lasttime;
	Point		p;
	Rect		userect;
	UserItem	*ip;

	c = LastScan;
	if (c == sc_Escape)	// Map escape from game to Exit to DOS
		c = sc_Q;

	CA_UpLevel();
	for (i = CONTROLS_LUMP_START;i <= CONTROLS_LUMP_END;i++)
		CA_MarkGrChunk(i);
	CA_MarkGrChunk(CTL_LITTLEMASKPICM);
	CA_MarkGrChunk(CTL_LSMASKPICM);
	CA_CacheMarks("Options Screen", 0);

	USL_SetUpCtlPanel();

	US_SetPrintRoutines(VW_MeasurePropString,VWB_DrawPropString);
	fontcolor = F_BLACK;

	VW_InitDoubleBuffer();

	VWB_Bar(0,0,MaxX,MaxY,FIRSTCOLOR);
	US_DrawWindow(8,22,30,2);
	US_SaveWindow(&HelpWindow);
	US_DrawWindow(8,7,30,14);
	US_SaveWindow(&BottomWindow);
	US_DrawWindow(8,1,30,20);

	for (ip = CtlPanels;ip->type != uii_Bad;ip++)
		VWB_DrawPic(ip->r.ul.x,ip->r.ul.y,ip->picup);

	US_StartCursor();
	CursorX = (8 * 8) + ((MaxX - (8 * 8)) / 2);
	CursorBad = true;

	CtlPanelButton = -1;
	LastScan = c;
	USL_CheckScan(&i,&n);
	if (CtlPanelButton == -1)
		USL_DoHit(0,0);

	ResumeGame = false;
	done = false;
	FlushHelp = true;
	lastx = lasty = -1;
	while
	(
		(restartgame == gd_Continue)
	&&	!(done || loadedgame || ResumeGame)
	)
	{
		VW_UpdateScreen();

		buttondown = US_UpdateCursor();
		inrect = USL_IsInRect(CursorX,CursorY,&i,&n);

		if (FlushHelp)
		{
			lasti = -2;
			lasttime = TimeCount;
			FlushHelp = false;
		}
		if (inrect)
		{
			if ((lasti != i) || (lastn != n))
			{
				// If over a Load button
				if
				(
					(CtlPanelButton == 2)
				&&	(i == 2)
				&&	(TheItems[1][0].sel & ui_Selected)
				&&	(Games[n / 2].present)
				&& 	!(n & 1)
				)
				{
					strcpy(gamename,"Load `");
					strcat(gamename,Games[n / 2].name);
					strcat(gamename,"'");
					USL_ShowHelp(gamename);
				}
				else
					USL_ShowHelp(TheItems[i][n].help);
				lasti = i;
				lastn = n;
			}
		}
		else if (lasti != (word)-1)
		{
			USL_ShowHelp("Select a Button");
			lasti = -1;
		}

		hiti = i;
		hitn = n;

		if (inrect)
			userect = TheItems[i][n].r;
		else
		{
			userect.ul.x = CursorX;
			userect.ul.y = CursorY;
			userect.lr = userect.ul;
		}

		if (IN_KeyDown(sc_UpArrow))
		{
			USL_FindRect(userect,motion_None,motion_Up);
			buttondown = false;
			IN_ClearKey(sc_UpArrow);
		}
		else if (IN_KeyDown(sc_DownArrow))
		{
			USL_FindRect(userect,motion_None,motion_Down);
			buttondown = false;
			IN_ClearKey(sc_DownArrow);
		}
		else if (IN_KeyDown(sc_LeftArrow))
		{
			USL_FindRect(userect,motion_Left,motion_None);
			buttondown = false;
			IN_ClearKey(sc_LeftArrow);
		}
		else if (IN_KeyDown(sc_RightArrow))
		{
			USL_FindRect(userect,motion_Right,motion_None);
			buttondown = false;
			IN_ClearKey(sc_RightArrow);
		}
		else if
		(
			IN_KeyDown(c = sc_Return)
		|| 	IN_KeyDown(c = KbdDefs[0].button0)
		|| 	IN_KeyDown(c = KbdDefs[0].button1)
		)
		{
			IN_ClearKey(c);
			if (inrect)
			{
				ip = &TheItems[hiti][hitn];

				if ((ip->type == uii_Button) && !(ip->sel & ui_Disabled))
				{
					lasttime = TimeCount;

					ip->sel |= ui_Selected;
					USL_DrawItem(hiti,hitn);
					VW_UpdateScreen();

					while (TimeCount - lasttime < TickBase / 4)
						;
					lasttime = TimeCount;

					ip->sel &= ~ui_Selected;
					USL_DrawItem(hiti,hitn);
					VW_UpdateScreen();

					while (TimeCount - lasttime < TickBase / 4)
						;
				}

				USL_DoHit(hiti,hitn);
			}
		}
		else if (USL_CheckScan(&i,&n))
			;
		else if (buttondown && inrect && USL_TrackItem(hiti,hitn))
			USL_DoHit(hiti,hitn);

		if (LastScan == sc_Escape)
		{
			IN_ClearKey(sc_Escape);
			done = true;
		}

		if (QuitToDos)
			done = true;

		if ((lastx != CursorX) || (lasty != CursorY))
		{
			lastx = CursorX;
			lasty = CursorY;
			lasttime = TimeCount;
		}
		if (TimeCount - lasttime > TickBase * 10)
		{
			if (((TimeCount - lasttime) / TickBase) & 2)
				fontcolor = F_SECONDCOLOR;
			USL_ShowHelp("Press F1 for Help");
			fontcolor = F_BLACK;
		}
	}

	US_ShutCursor();

	USL_TearDownCtlPanel();

	if (QuitToDos)
	{
#if FRILLS
		if (tedlevel)
			TEDDeath();
		else
#endif
		{
			US_CenterWindow(20,3);
			fontcolor = F_SECONDCOLOR;
			US_PrintCentered("Now Exiting to DOS...");
			fontcolor = F_BLACK;
			VW_UpdateScreen();
			Quit(nil);
		}
	}

	CA_DownLevel();
}

//	High score routines

///////////////////////////////////////////////////////////////////////////
//
//	US_DisplayHighScores() - Assumes that double buffering has been started.
//		If passed a -1 will just display the high scores, but if passed
//		a non-negative number will display that entry in red and let the
//		user type in a name
//
///////////////////////////////////////////////////////////////////////////
void
US_DisplayHighScores(int which)
{
	char		buffer[16],*str;
	word		i,
				w,h,
				x,y;
	HighScore	*s;

	US_CenterWindow(30,MaxScores + (MaxScores / 2));

	x = WindowX + (WindowW / 2);
	US_Print(" Name");
	PrintX = x + 20;
	US_Print("Score");
	PrintX = x + 60;
	US_Print("Done\n\n");
	PrintY -= 3;

	for (i = WindowX;i < WindowX + WindowW;i += 8)
		VWB_DrawTile8M(i,WindowY + 8,10);
	VWB_DrawTile8M(WindowX - 8,WindowY + 8,9);
	VWB_DrawTile8M(WindowX + WindowW,WindowY + 8,11);

	for (i = 0,s = Scores;i < MaxScores;i++,s++)
	{
		fontcolor = (i == which)? F_SECONDCOLOR : F_BLACK;

		if (i != which)
		{
			US_Print(" ");
			if (strlen(s->name))
				US_Print(s->name);
			else
				US_Print("-");
		}
		else
			y = PrintY;

		PrintX = x + (7 * 8);
		ultoa(s->score,buffer,10);
		for (str = buffer;*str;str++)
			*str = *str + (129 - '0');	// Used fixed-width numbers (129...)
		USL_MeasureString(buffer,&w,&h);
		PrintX -= w;
		US_Print(buffer);

		PrintX = x + 60;
		if (s->completed)
			US_PrintUnsigned(s->completed);
		else
			US_Print("-");

		US_Print("\n");
	}

	if (which != -1)
	{
		fontcolor = F_SECONDCOLOR;
		PrintY = y;
		PrintX = WindowX;
		US_Print(" ");
		strcpy(Scores[which].name,"");
		US_LineInput(PrintX,PrintY,Scores[which].name,nil,true,MaxHighName,
						(WindowW / 2) - 8);
	}
	fontcolor = F_BLACK;

	VW_UpdateScreen();
}

///////////////////////////////////////////////////////////////////////////
//
//	US_CheckHighScore() - Checks gamestate to see if the just-ended game
//		should be entered in the high score list. If so, lets the user
//		enter their name
//
///////////////////////////////////////////////////////////////////////////
void
US_CheckHighScore(long score,word other)
{
	word		i,j,
				n;
	HighScore	myscore;

	strcpy(myscore.name,"");
	myscore.score = score;
	myscore.completed = other;

	for (i = 0,n = -1;i < MaxScores;i++)
	{
		if
		(
			(myscore.score > Scores[i].score)
		||	(
				(myscore.score == Scores[i].score)
			&& 	(myscore.completed > Scores[i].completed)
			)
		)
		{
			for (j = MaxScores;--j > i;)
				Scores[j] = Scores[j - 1];
			Scores[i] = myscore;

			n = i;
			HighScoresDirty = true;
			break;
		}
	}

	VW_InitDoubleBuffer();
	VWB_Bar(0,0,MaxX,MaxY,FIRSTCOLOR);

	US_DisplayHighScores(n);
	IN_UserInput(5 * TickBase,false);
}
