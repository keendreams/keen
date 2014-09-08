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

#include <stdio.h>
#include <dos.h>
#include <mem.h>
#include <alloc.h>
#include <fcntl.h>
#include <conio.h>
#include <string.h>
#include <io.h>
#include <sys\stat.h>
#include <stdlib.h>
#include <process.h>

#include "jampak.h"
#include "sl_file.h"


#define VERSION_NUM	"1.10"
#define MESSAGE		"THIS VERSION IS MADE FOR USE IN G.E. TEXT SHELL"



//===========================================================================
//
//									 LOCAL PROTOTYPES
//
//===========================================================================


typedef enum {false=0,true} boolean;

int WritePtr(long outfile, unsigned char data, unsigned PtrType);
int ReadPtr(long infile, unsigned PtrType);
void far lzwDecompress(void far *infile, void far *outfile,unsigned long DataLength,unsigned PtrTypes);
boolean LoadLIBFile(char *LibName,char *FileName,char far **MemPtr);

long FileSize(char *filename);

boolean FarRead (int handle, char far *dest, long length);

int main(int argc,char **argv);
void TrashProg(char *OutMsg);



//=========================================================================
//
//									LOCAL VARIABLES
//
//=========================================================================


unsigned char far LZW_ring_buffer[LZW_N + LZW_F - 1];

	// ring buffer of size LZW_N, with extra LZW_F-1 bytes to facilitate
	//	string comparison




//==========================================================================
//
//
//							DECOMPRESSION/LIB ROUTINES
//
//
//==========================================================================


//---------------------------------------------------------------------------
// WritePtr()  -- Outputs data to a particular ptr type
//
//	PtrType MUST be of type DEST_TYPE.
//
// NOTE : For PtrTypes DEST_MEM a ZERO (0) is always returned.
//
//---------------------------------------------------------------------------
int WritePtr(long outfile, unsigned char data, unsigned PtrType)
{
	int returnval = 0;

	switch (PtrType & DEST_TYPES)
	{
		case DEST_FILE:
			write(*(int far *)outfile,(char *)&data,1);
		break;

		case DEST_FFILE:
			returnval = putc(data, *(FILE **)outfile);
		break;

		case DEST_MEM:
//			*(*(char far **)outfile++) = data;						// Do NOT delete
			*((char far *)*(char far **)outfile)++ = data;
		break;

		default:
			TrashProg("WritePtr() : Unknown DEST_PTR type");
		break;
	}

	return(returnval);

}


//---------------------------------------------------------------------------
// ReadPtr()  -- Reads data from a particular ptr type
//
//	PtrType MUST be of type SRC_TYPE.
//
// RETURNS :
//		The char read in or EOF for SRC_FFILE type of reads.
//
//
//---------------------------------------------------------------------------
int ReadPtr(long infile, unsigned PtrType)
{
	int returnval = 0;

	switch (PtrType & SRC_TYPES)
	{
		case SRC_FILE:
			read(*(int far *)infile,(char *)&returnval,1);
		break;

		case SRC_FFILE:
			returnval = fgetc((FILE far *)*(FILE far **)infile);
		break;


		case SRC_MEM:
			returnval = (char)*(*(char far **)infile++);
//			returnval = *((char far *)*(char far **)infile)++;	// DO NOT DELETE!
		break;

		default:
			TrashProg("ReadPtr() : Unknown SRC_PTR type");
		break;
	}

	return(returnval);
}





//--------------------------------------------------------------------------
//
// lzwDecompress() - Compresses data from an input ptr to a dest ptr
//
// PARAMS:
//		 infile     - Pointer at the BEGINNING of the compressed data (no header!)
//		 outfile    - Pointer to the destination.
// 	 DataLength - Length of compressed data.
//     PtrTypes   - Type of pointers being used (SRC_FILE,DEST_FILE,SRC_MEM etc).
//
// RETURNS:
//	    Length of compressed data.
//
//	COMPTYPE : ct_LZW
//
// NOTES    : Does not write ANY header information!
//
void far lzwDecompress(void far *infile, void far *outfile,unsigned long DataLength,unsigned PtrTypes)
{
	int  i, j, k, r, c;
	unsigned int flags;

	for (i = 0; i < LZW_N - LZW_F; i++)
		LZW_ring_buffer[i] = ' ';

	 r = LZW_N - LZW_F;
	 flags = 0;

	 for ( ; ; )
	 {
			if (((flags >>= 1) & 256) == 0)
			{
				c = ReadPtr((long)&infile,PtrTypes);
				if (!DataLength--)
					return;

				flags = c | 0xff00;      // uses higher byte cleverly to count 8
			}

			if (flags & 1)
			{
				c = ReadPtr((long)&infile,PtrTypes);		// Could test for EOF iff FFILE type
				if (!DataLength--)
					return;

				WritePtr((long)&outfile,c,PtrTypes);

				LZW_ring_buffer[r++] = c;
				r &= (LZW_N - 1);
			}
			else
			{
				i = ReadPtr((long)&infile,PtrTypes);
				if (!DataLength--)
					return;

				j = ReadPtr((long)&infile,PtrTypes);
				if (!DataLength--)
					return;

				i |= ((j & 0xf0) << 4);
				j = (j & 0x0f) + LZW_THRESHOLD;

				for (k = 0; k <= j; k++)
				{
					 c = LZW_ring_buffer[(i + k) & (LZW_N - 1)];

					 WritePtr((long)&outfile,c,PtrTypes);

					 LZW_ring_buffer[r++] = c;
					 r &= (LZW_N - 1);
				}
			}
	 }
}



//----------------------------------------------------------------------------
// LoadLIBFile() -- Copies a file from an existing archive to dos.
//
// PARAMETERS :
//
//			LibName 		 - Name of lib file created with SoftLib V1.0
//
//			FileName 	 - Name of file to load from lib file.
//
//			MemPtr  		 - Address of variable to store the file addr.
//							 - if VariablePtr is NULL then will allocmem before
//                      loading.
//       UseID_Memory - TRUE = use MM_GetPtr else farmalloc.
//
//
// RETURN :
//
//   		true   - Successfull load
//			false  - Error!
//
//----------------------------------------------------------------------------
boolean LoadLIBFile(char *LibName,char *FileName,char far **MemPtr)
{
	int handle;
	unsigned long header;
	struct ChunkHeader Header;
	unsigned long ChunkLen;
	short x;
	struct FileEntryHdr FileEntry;     			// Storage for file once found
	struct FileEntryHdr FileEntryHeader;		// Header used durring searching
	struct SoftLibHdr LibraryHeader;				// Library header - Version Checking
	boolean FileFound = false;
	unsigned long id_slib = ID_SLIB;
	unsigned long id_chunk = ID_CHUNK;
	boolean Success = true;


	//
	// OPEN SOFTLIB FILE
	//

	if ((handle = open(LibName,O_RDONLY | O_BINARY, S_IREAD)) == -1)
		TrashProg("LOADSCN ERROR : Error openning file.");


	//
	//	VERIFY it is a SOFTLIB (SLIB) file
	//

	if (read(handle,&header,4) == -1)
	{
		close(handle);
		TrashProg("LOADSCN ERROR : Error reading first header.");
	}

	if (header != id_slib)
	{
		close(handle);
		TrashProg("LOADSCN ERROR : Header on id_slib.");
	}


	//
	// CHECK LIBRARY HEADER VERSION NUMBER
	//

	if (read(handle, &LibraryHeader,sizeof(struct SoftLibHdr)) == -1)
		TrashProg("read error in LoadSLIBFile()");

	if (LibraryHeader.Version > SOFTLIB_VER)
		TrashProg("Unsupported file version ");


	//
	// MANAGE FILE ENTRY HEADERS...
	//

	for (x = 1;x<=LibraryHeader.FileCount;x++)
	{
		if (read(handle, &FileEntryHeader,sizeof(struct FileEntryHdr)) == -1)
		{
			close(handle);
			TrashProg("LOADSCN ERROR : Error reading second header.");
		}

		if (!stricmp(FileEntryHeader.FileName,FileName))
		{
			FileEntry = FileEntryHeader;
			FileFound = true;
		}
	}

	//
	// IF FILE HAS BEEN FOUND THEN SEEK TO POSITION AND EXTRACT
	//	ELSE RETURN WITH ERROR CODE...
	//

	if (FileFound)
	{
		if (lseek(handle,FileEntry.Offset,SEEK_CUR) == -1)
		{
			close(handle);
			TrashProg("LOADSCN ERROR : Error seeking through file.");
		}

		//
		// READ CHUNK HEADER - Verify we are at the beginning of a chunk..
		//

		if (read(handle,(char *)&Header,sizeof(struct ChunkHeader)) == -1)
			TrashProg("LIB File - Unable to read Header!");

		if (Header.HeaderID != id_chunk)
			TrashProg("LIB File - BAD HeaderID!");

		//
		// Allocate memory if Necessary...
		//

		if (*MemPtr == NULL)
		{
			delay(2000);
			if ((*MemPtr = farmalloc(FileEntry.OrginalLength)) == NULL)
				TrashProg("Can't get memory");
		}

		//
		//	Calculate the length of the data (without the chunk header).
		//

		ChunkLen = FileEntry.ChunkLen - sizeof(struct ChunkHeader);

		//
		// Extract Data from file
		//

		switch (Header.Compression)
		{
			case ct_LZW:
				lzwDecompress((void *)handle,*MemPtr,ChunkLen,(SRC_FILE|DEST_MEM));
				break;

			case ct_NONE:
				Success = FarRead(handle,*MemPtr,ChunkLen);
				break;

			default:
				close(handle);
				TrashProg("Unknown Chunk.Compression Type!");
				break;
		}
	}
	else
		Success = false;

	close(handle);

	return(Success);
}




//===========================================================================
//
//                         GENERAL FUNCTIONS
//
//===========================================================================


//---------------------------------------------------------------------------
//  FileSize() - Returns the size of a file on disk. (-1 = error)
//---------------------------------------------------------------------------
long FileSize(char *filename)
{
	  long filesize;
	  int handle;

	  if ((handle = open(filename,O_RDONLY)) != -1)
	  {
			filesize = filelength(handle) ;
			close(handle);
	  }
	  else
			filesize = 0;

	return(filesize);
}



//--------------------------------------------------------------------------
// FarRead()
//-------------------------------------------------------------------------
boolean FarRead (int handle, char far *dest, long length)
{
	if (length>0xffffl)
		TrashProg("FarRead doesn't support 64K reads yet!");

asm		push	ds
asm		mov	bx,[handle]
asm		mov	cx,[WORD PTR length]
asm		mov	dx,[WORD PTR dest]
asm		mov	ds,[WORD PTR dest+2]
asm		mov	ah,0x3f				// READ w/handle
asm		int	21h
asm		pop	ds
asm		jnc	good
//	errno = _AX;
	return	false;
good:
asm		cmp	ax,[WORD PTR length]
asm		je	done
//	errno = EINVFMT;			// user manager knows this is bad read
	return	false;
done:
	return	true;
}




//--------------------------------------------------------------------------
//  MAIN
//--------------------------------------------------------------------------
int main(int argc,char **argv)
{
	unsigned char huge *bufferptr = NULL;

	if (stricmp(argv[1], "/VER") == 0)
	{
		clrscr();
		printf("\nG.E. Load Text Screen.\n");
		printf("Copyright 1992 Softdisk Publishing\n");
		printf("Version Number %s\n", VERSION_NUM);
#ifdef MESSAGE
		printf("Note : %s\n", MESSAGE);
#endif
		return(0);
	}

	if ((argc < 2) || (argc > 3))
	{
		printf("\nG.E. Load Text Screen.\n");
		printf("Copyright 1992 Softdisk Publishing\n");
		printf("by Nolan Martin and Jim Row\n");
		printf("usage : LOADSCN libname Filename\n");
		return(1);
		}

	_setcursortype(_NOCURSOR);

	if (!LoadLIBFile(argv[1],argv[2], &bufferptr))
		TrashProg("Error loading TEXT_SCEENS");

	_fmemcpy(MK_FP(0xB800,0), bufferptr+7, 4000);

	_setcursortype(_NORMALCURSOR);

	gotoxy(1, 24);

	exit(0);

}  // main end


//---------------------------------------------------------------------------
//  TrashProg() --
//---------------------------------------------------------------------------
void TrashProg(char *OutMsg)
{
	int error = 0;

	_setcursortype(_NORMALCURSOR);

	if (OutMsg)
	{
		printf("%s\n",OutMsg);
		error = 1;
	}

	exit(error);
}

