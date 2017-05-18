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
GRAPHICS.C - Routines that output to the screen.
*/


#include "bas51.h"
#include <stdarg.h>




// Change cursor position

void GotoXY(BYTE x, BYTE y)
{
    CurPosX = x;
    CurPosY = y;

    UPDATE_DISPLAY;   // macro to update display when running under windows
}



// Clear Screen Function

void VGA_ClrScrn(void)
{
    memset(VIDEO_MEMORY, ' ', sizeof(VIDEO_MEMORY));
    CurPosX = CurPosY = 0;

    UPDATE_DISPLAY;   // macro to update display when running under windows
}

// Clear the screen from the current position to the end of the screen
// Does not change cursor position.

void ClearEOS(void)
{
    int CurOffset;

    CurOffset = CurPosY * VID_COLS + CurPosX;
    memset(&VIDEO_MEMORY[CurOffset], ' ', (VID_ROWS * VID_COLS) - CurOffset);
    UPDATE_DISPLAY;   // macro to update display when running under windows
}

// Clear the screen from the current position to the end of the Line
// Does not change cursor position.

void ClearEOL(void)
{
    int CurOffset;

    CurOffset = CurPosY * VID_COLS + CurPosX;
    memset(&VIDEO_MEMORY[CurOffset], ' ', VID_COLS - CurPosX);
    UPDATE_DISPLAY;   // macro to update display when running under windows
}


// SROLL THE SCREEN UP ONE LINE
// Note: This does not adjust the cursor

void VGA_Scroll(void)
{
    memcpy(VIDEO_MEMORY, VIDEO_MEMORY + VID_COLS, (VID_COLS * (VID_ROWS - 1)));
    memset(VIDEO_MEMORY + (VID_COLS * (VID_ROWS - 1)), ' ', VID_COLS);
    FieldStart -= (BYTE) VID_COLS;   // adjust field start on scroll
    ScrollCnt++;      // Incremented ever time a scroll occurs
    UPDATE_DISPLAY;   // macro to update display when running under windows
}


// Output a single character to the screen at the cursor location without preprocessing

void VGA_PutRawChar(BYTE ch)
{
    if (CurPosX >= VID_COLS || CurPosY >= VID_ROWS)           // Check bounds
    {
        CurPosX = CurPosY = 0;
    }

    // Store char at cursor address in screen memory
    VIDEO_MEMORY[CurPosY * VID_COLS + CurPosX] = ch;

    // advance cursor
    CurPosX++;

    // check new bounds of cursor and scroll if necessary
    if (CurPosX >= VID_COLS)
    {
        CurPosX = 0;
        CurPosY++;
    }

    if (CurPosY >= VID_ROWS)
    {
        CurPosY--;
        VGA_Scroll();
    }

    UPDATE_DISPLAY;   // macro to update display when running under windows
}



// Output a single character to the screen at the cursor location

void VGA_putchar(BYTE ch)
{
    if (ch == '\n')     // special non-printing newline
    {
        CurPosX = 0;
        CurPosY++;
        if (CurPosY == VID_ROWS)
        {
            CurPosY--;
            VGA_Scroll();
        }
        ch = 0;         // special case - this routine does not print NULL chars
    }
    else if (ch == '\b')    // backspace
    {
        // Move cursor backwards one char if not on first col
        if (CurPosX) CurPosX--;
        ch = 0;
    }
    else if (ch == '\r')
    {
        CurPosX = 0;          // just return to col 0
        ch = 0;         // special case - this routine does not print NULL chars
    }
    else if (ch == '\t')    // tab
    {
        // find out how many spaces we need
        ch = (BYTE)(4 - (CurPosX & 0x03));
        for (; ch != 0; ch--)
            VGA_PutRawChar(' ');    // print that many spaces

        return;                   // no further processing for tab
    }


    if (ch)    // Regular printing char
    {
        VGA_PutRawChar(ch);
        FieldLen++;
    }
//    else SetCurPos(CurPosX,CurPosY);        // just set the new cursor location

}

// Print a string at the current cursor location.
// Note: Does not add automatic newline.

void VGA_print(char *str)
{
    BYTE b, Len;

    Len = (BYTE) strlen(str);
    for (b = 0; b != Len; b++)
        VGA_putchar(str[b]);
}



/* printf like routine, up to 10 parameters */

BYTE VGA_printf(char *fmt, ...)
{
    XDATA va_list arg_marker;
    BYTE  i;
    char XDATA buff[100];

    va_start(arg_marker, fmt);
    i = (BYTE) vsprintf(buff, fmt, arg_marker);
    VGA_print(buff);
    va_end(arg_marker);

    return(i);
}


// Print a basic string defined by uData
// Print exactly FieldSize characters, or to the end of string if FieldSize is 0

void VGA_PrintString(BYTE FieldSize)
{
    BYTE b, Len;

    Len = StringLen(UDATA);
    if (FieldSize && Len > FieldSize) Len = FieldSize;

    // Print string part
    for (b = 0; b != Len ; b++)
        VGA_putchar(ReadRandom51((WORD)(uData.sVal.sPtr + b)));

    // pad with spaces
    while (b++ < FieldSize) VGA_putchar(' ');
}


BIT GraphPlot(WORD X, WORD Y, BYTE Flags)
{
    WORD Offset;
    BYTE Shift, Mask, OldChar;

	// Char:  0, 1
	//        2, 3
	//        4, 5

    // test bounds - return FALSE if out of bounds
    if (X > 127 || Y > 74) return(FALSE);

	Offset = (WORD)((Y / 3) * 64 + (X / 2));
	Shift =  (BYTE)((Y % 3) * 2  + (X & 0x01));
	Mask = (BYTE)(1 << Shift);

	OldChar = VIDEO_MEMORY[Offset];
	if (!(OldChar & 0x80)) OldChar = 0x80;   // set graphics if not already set

    if (Flags == 0xFF)   // just testing point
        return(OldChar & Mask);

	if (Flags & 0x01)   // plotting
	    OldChar |= Mask;
	else            // unplotting
    	OldChar &= (BYTE) ~Mask;

    VIDEO_MEMORY[Offset] = OldChar;

    UPDATE_DISPLAY;   // macro to update display when running under windows

    return(0);
}

// Plot or unplot a point on the screen

void DoPlotCmd(BIT PlotFlag)
{
    WORD X, Y;

    // get parameters
    X = GetWordParameter();
    if (CurChar != TOKEN_COMMA) goto Error;

    Y = GetWordParameter();
    if (!TerminalChar()) goto Error;

    GraphPlot(X, Y, (BYTE) PlotFlag);

    return;

Error:
	SyntaxErrorCode = ERROR_SYNTAX;
    return;

}






