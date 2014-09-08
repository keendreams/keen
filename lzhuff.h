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

extern void (*LZH_CompressDisplayVector)();
extern void (*LZH_DecompressDisplayVector)();



//===========================================================================
//
//											PROTOTYPES
//
//===========================================================================


long lzhCompress(void far *infile, void far *outfile,unsigned long DataLength,unsigned PtrTypes);
long lzhDecompress(void far *infile, void far *outfile, unsigned long OrginalLength, unsigned long CompressLength, unsigned PtrTypes);



