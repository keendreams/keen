# Commander Keen in Keen Dreams

This repository contains the source for Commander Keen in Keen Dreams.  It is released under the GNU GPLv2+.  See LICENSE for more details.

The release of the source code does not affect the licensing of the game data files, which you must still legally acquire.  This includes the static data included in this repository for your convenience.  However, you are permitted to link and distribute that data for the purposes of compatibility with the original game.

This release was made possible by a crowdfunding effort.  It is brought to you by Javier M. Chavez and Chuck Naaden with additional support from:

* Dave Allen
* Kirill Illenseer
* Michael Jurich
* Tom Laermans
* Jeremy Newman
* Braden Obrzut
* Evan Ramos
* Sam Schultz
* Matt Stath
* Ian Williams
* Steven Zakulec
* et al

## Compiling

The code is designed for Borland C++ 2.0, but all revisions compiled fine under 3.1 at the time of release.

There is some data that must be compiled into the binary.  This data is located in the static directory.  To prepare the source for building, make sure Borland C++ is in your *PATH* and then run `make.bat`.

You may now go to the root directory and type `bc` to open the project and build.  You may need to configure your directories in Borland for it to compile properly.

### EGA/CGA Version

Version 1.00 can be built for either EGA or CGA by changing a constant.  All later versions are specific to one mode.  The constant is `GRMODE` in ID_HEADS.H and ID_ASM.EQU.  Finally ensure that the proper static data files are being linked.  KDREDICT.OBJ/KDREHEAD.OBJ for EGA and KDRCDICT.OBJ/KDRCHEAD.OBJ for CGA.

## Revision History

> A little confusing because revisions have proceeded in three different sequences, the regular (EGA) version, the CGA version, and the shareware version.  At present, 1.05 is the latest CGA version, 1.93 is the latest EGA version, and 1.20 is the latest shareware version.  Also, some versions with suffixed letters have been used when text and other items changed without recompilation of the program itself.

* 1.00 (not numbered): Original release.
* 1.01: Version used on Tiger Software marketing deal.
* 1.01-360: Specially adapted version to fit on 360K disk for Tiger Software marketing deal.
* 1.01S: (mistakenly labeled 1.01): Shareware version.
* 1.02: Registered version.
* 1.03: Registered version (re-mastered edition).
* 1.04: CGA version.
* 1.05: Re-master of CGA version without Gamer's Edge references.

*New CGA versions should be numbered 1.06 through 1.12*

* 1.13: Shareware version (re-mastered edition).
* 1.20: Re-master of shareware version without Gamer's Edge references.

*New shareware versions should be numbered 1.21 through 1.90*

* 1.91: Version for Prodigy Download Superstore.
* 1.92 [rev 0] : Version for Good Times. The shell is not on this version.
* 1.93 [rev 1] : Version for catalog.  Uses DocView Shell instead of old GE shell.  Copyrights updated to "1991-1993" with Softdisk Publishing instead of Softdisk, Inc., to suit our present guidelines.  Otherwise the same as Good Times version.

*New EGA versions should be numbered 1.94 and up.*
