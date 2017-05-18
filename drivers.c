/*
This source file is part of the TRS-8051 project.  BAS51 is a work alike
rewrite of the Microsoft Level 2 BASIC for the TRS-80 that is intended to run
on an 8051 microprocessor.  This version is in intended to also run under
Windows for testing purposes.

Copyright (C) 2017  Dennis Hawkins

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

If you use this code in any way, I would love to hear from you.  My email
address is: dennis@galliform.com
*/

/*
DRIVERS.C - Contains hardware dependent code to interface TRS-8051 BASIC with
the hardware its running on.
*/



#include "bas51.h"

extern BIT  StreamDirtyFlag;

// Hardware drivers to interface actual hardware or simulation


#ifdef __WIN32__

//#include <windows.h>
#include <conio.h>

extern BYTE FontLines[14][256]; // The actual FONT used for memory mapped display
//extern HDC G_hdc;
extern BIT ForcePaintFlag;
static BYTE MemBuf[0x10000];  // Simulates 64K external spi RAM
static BYTE LastScrn[64*25];  // Used to simulate memory maped display
extern FLOAT WinScaleX, WinScaleY;



// MEMORY        

// read a block into buffer - dest <= RAM[addr]
// Len = 0 is interpreted as Len = 256
void ReadBlock51(BYTE *dest, WORD addr, BYTE Len)
{
    memcpy(dest, &MemBuf[addr], Len ? Len : 256 );
}

// Write buffer to RAM - RAM[addr] <= src
// Len = 0 is interpreted as Len = 256
void WriteBlock51(WORD addr, BYTE *src, BYTE Len)
{
    if (addr < BasicVars.VarStart ||
        addr >= BasicVars.CmdLine) StreamDirtyFlag = TRUE;

    memcpy(&MemBuf[addr], src, Len ? Len : 256);
}

// Hardware specific memory clear.
// Writes Len bytes of zeros to memory starting at Dest.

void MemClear51(WORD Dest, WORD Len)
{
    memset(&MemBuf[Dest], 0, Len);
}



// SCREEN

/*void SetCurPos(BYTE x, BYTE y)
{
    CurPosX = x;
    CurPosY = y;
} */


// Win32 primitive to draw a char
void DrawChar(HDC hdc, int x, int y, BYTE ch)
{
    WORD CharBitmap[14];
    int i;
    HBITMAP map;
    HDC src;
    int CharSizeX, CharSizeY;


    CharSizeX = (int)(8.0 * WinScaleX);
    CharSizeY = (int)(14.0 * WinScaleY);

    // fill in array with a single char
    for (i = 0; i != 14; i++)
        CharBitmap[i] = (WORD)(FontLines[i][ch]);


	// Creating temp bitmap
	map = CreateBitmap(8, 14, 1, 1, (void *) CharBitmap);
    if (!map) return;   // GDI failure

    // Temp HDC to copy picture
	src = CreateCompatibleDC(hdc);
	SelectObject(src, map); // Inserting picture into our temp HDC
	// Copy image from temp HDC to window
    StretchBlt(hdc,	// handle of destination device context
    			x * CharSizeX,	// x-coordinate of upper-left corner of dest. rect.
    			y * CharSizeY,	// y-coordinate of upper-left corner of dest. rect.
    			CharSizeX,		// width of destination rectangle
    			CharSizeY,		// height of destination rectangle
    			src,	// handle of source device context
    			0,		// x-coordinate of upper-left corner of source rectangle
    			0,		// y-coordinate of upper-left corner of source rectangle
    			8,		// width of source rectangle
    			14,		// height of source rectangle
				SRCCOPY); 	// raster operation code


	DeleteObject(map);
	DeleteDC(src); // Deleting temp HDC



}


// Compare Screen Buffer with LastScrn and write differences
// Super inefficient, but it works, called from a timer routine.
// This is necessary in order to simulate the way the TRS8051 video works.

void DrawScreen(HDC hdc)
{
    int i;
    BYTE *Lptr, *Vptr;
    BYTE ch;
    HPEN hPen;
    int CharSizeX, CharSizeY;


    CharSizeX = (int)(8.0 * WinScaleX);
    CharSizeY = (int)(14.0 * WinScaleY);

    Lptr = LastScrn;
    Vptr = VIDEO_MEMORY;

    // Write new chars
    for (i = 0; i != sizeof(LastScrn); i++)
    {
        if (ForcePaintFlag || Lptr[i] != Vptr[i])
        {

           Lptr[i] = ch = Vptr[i];
           DrawChar(hdc, (BYTE)(i % 64), (BYTE)(i / 64), ch);
        }
    }


    // Erase old cursor
    hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
	SelectObject(hdc, hPen);
	MoveToEx(hdc, LastCurX * CharSizeX, (LastCurY + 1)* CharSizeY, NULL);
	LineTo(hdc, (LastCurX + 1) * CharSizeX, (LastCurY + 1) * CharSizeY);
    DeleteObject(hPen);

    // Get the
    LastCurX = CurPosX;
    LastCurY = CurPosY;

    // Draw the NEW cursor
    hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
	SelectObject(hdc, hPen);
	MoveToEx(hdc, LastCurX * CharSizeX, (LastCurY + 1)* CharSizeY, NULL);
	LineTo(hdc, (LastCurX + 1) * CharSizeX, (LastCurY + 1) * CharSizeY);
//	MoveToEx(hdc, LastCurX*16, LastCurY*28+28, NULL);
//	LineTo(hdc, LastCurX*16+16, LastCurY*28+28);
    DeleteObject(hPen);

    ForcePaintFlag = FALSE;
}



// KEYBOARD

// The keyboard code crosses threads and is not particularly thread safe.
// It will not survive a thrashing, but is good enough for this simulator code.

extern WORD KeyBuf[8];
extern int  KeyIn, KeyOut;


// Check for break key pressed
// If found, return TRUE and clear the Q.

BIT CheckBreak(void)
{
    WORD wch;
    int i;

    i = KeyOut;
    while (KeyIn != i)   // while something is in the Q
    {
        wch = KeyBuf[i++];
        i &= 0x07;

        if (wch == KEY51_BREAK || wch == KEY51_CTRL_C)
        {
            KeyOut = KeyIn;    // clear Q
            return(TRUE);
        }
    }

    return(FALSE);
}
// Return TRUE if key available

BIT KeyAvailable(void)
{
    return((BIT)(KeyIn != KeyOut ? TRUE : FALSE));
}

// Wait for and Get enhanced key from keyboard.

WORD GetKeyWord(void)
{
    WORD out;

    while (!KeyAvailable()) Sleep(100);
    out = KeyBuf[KeyOut++];
    KeyOut &= 0x7;

    return(out);
}



// Other

void SysWait(WORD Ms)
{
    Sleep(Ms);
}

DWORD GetScreenCount(void)   // Return Milliseconds since program was started
{
    return(GetTickCount() - ScreenTicks);
}

void InitScreenCount(void)
{
    ScreenTicks = GetTickCount();
}


void FindBasFile(BYTE *buffer)
{
    WIN32_FIND_DATA ffd;
    static HANDLE hFind = NULL;
    int rt;

    if (buffer == NULL)   // first call initializes stuff
    {
        hFind = NULL;
        return;
    }
    else   // regular call
    {
        buffer[0] = 0;
        while(1)
        {
	        if (hFind)       // not the first call
    	    {
		        rt = FindNextFile(hFind, &ffd);
	            if (rt == 0)   // No more files
	            {
	        	    FindClose(hFind);
	                hFind = NULL;
	                return;
	            }
	        }
	        else   // This is the first call
	        {
	            hFind = FindFirstFile(".\\BasFiles\\*.*", &ffd);
//	            hFind = FindFirstFile(".\\*.*", &ffd);
	            if (hFind == INVALID_HANDLE_VALUE)  // no files
	                return;
	        }

            // go back if not a real file
            if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) break;
		}

    }

    // If we are here, we found a file.
    strcpy((char *) buffer, ffd.cFileName);

    return;

}


static FILE *bfp = NULL;

// Close file if opened

void BasFileClose(void)
{
    if (bfp)
    {
    	fclose(bfp);    // already open so close
        bfp = NULL;
	}
}

// Open file pointed to by FileName.
// uData points to filename in secondary RAM.
// Returns TRUE on error opening file, else FALSE.

#define ROOT_DIR   ".\\BasFiles\\"

BIT BasFileOpen(BIT ReadFlag)
{
    char FStr[64];
    int Len;

    BasFileClose();

    if (!uData.sVal.sPtr) return(TRUE);    // null file name
    memset(FStr, 0, sizeof(FStr));
    strcpy(FStr, ROOT_DIR);
    Len = StringLen(UDATA);
    if (Len == 0 || Len + 1 >= (sizeof(FStr) + sizeof(ROOT_DIR))) return(TRUE);
    ReadBlock51((BYTE *) FStr + strlen(FStr),
    		(WORD)(uData.sVal.sPtr), (BYTE) Len);

    bfp = fopen(FStr, ReadFlag ? "rb" : "wb");

    return((BIT)(bfp ? FALSE : TRUE));
}

// Read next byte from open file.  Return 0xFFFF on error or EOF.

WORD BasReadByte(void)
{
    WORD w;

    do
    {
        w = (WORD) fgetc(bfp);
    } while (w == '\r');     // ignore returns

    return(w);
}

// Write byte to open file.

void BasWriteByte(BYTE b)
{
    if (b == '\n') fputc('\r',bfp);
    fputc(b, bfp);
}


#endif   // WIN32

















#ifdef __C51__


// read a block into buffer - dest <= RAM[addr]
// Len = 0 is interpreted as Len = 256
void ReadBlock51(BYTE *dest, WORD addr, BYTE Len)
{
//
}

// Write buffer to RAM - RAM[addr] <= src
// Len = 0 is interpreted as Len = 256
void WriteBlock51(WORD addr, BYTE *src, BYTE Len)
{
    if (addr < BasicVars.VarStart ||
        addr >= BasicVars.CmdLine) StreamDirtyFlag = TRUE;

//
}

// Hardware specific memory clear.
// Writes Len bytes of zeros to memory starting at Dest.

void MemClear51(WORD Dest, WORD Len)
{
//
}


// SCREEN







// KEYBOARD


// Wait for and Get enhanced key from keyboard.

WORD GetKeyWord(void)
{
//
}


BIT KeyAvailable(void)
{
//
}



#endif    // C51



