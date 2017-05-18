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
RENUM.C - Handles the RENUM command.
*/

#include "bas51.h"

#define OnFlag uHash.b[0]

// Traverse program file and convert old line number to new line number.
// Returns new line number or 0xFFFF if not found.
// If OldNum == 0xFFFF, make line number changes permanent.

WORD RenumHelper(WORD OldNum)
{
    BYTE Len;
    WORD addr;
    WORD Line;
    WORD NewLine;

    addr = BasicVars.ProgStart;  // start program after basic vars

    NewLine = wArg[1];
    while (addr < BasicVars.VarStart)
    {
        Line = ReadRandomWord(addr);
        addr += (WORD) 2;
        Len = (BYTE) ReadRandom51(addr++);
        if (Line >= wArg[0])  // are we part the old starting line number?
        {
            if (OldNum == 0xFFFF)   // change line number
	        {
    	        WriteRandomWord((WORD)(addr - 3), NewLine);
        	}
            else if (Line >= OldNum) return(NewLine);


            // Increment new line number for the next line
            NewLine += wArg[2];
        }
        else   // haven't passed old line number yet
        {
            // Not renumbering yet, just return line number
            if (OldNum != 0xFFFF && Line >= OldNum) return(Line);
        }

        addr += Len;
    }

    return(0xFFFF);  // not found
}


// Uses ReadStream51 to scan the program Line.
// Returns NULL when end of Line is reached.
// Returns Line number address when INTL Line number when found.

WORD RenumScan(void)
{
    BIT LineNumFlag;
    WORD LineAddr;

    // if we exited and came back in, it was because of a line number, so
    // LineNumFlag is initially set to true.
    LineNumFlag = TRUE;

    // Are we starting a new line
    if (CurChar == '\r')    // need to skip three chars for line number
    {
        StreamSkip(3);    // skip line number and length

        // If our current char (LastToken) was a newline, then we know we
        // aren't following a GOTO, etc.
        LineNumFlag = FALSE;
        OnFlag = FALSE;
    }

    while (1)
    {
        // Read next token
        CurChar = ReadStream51();

        // process token
        switch(CurChar)
        {
            case '\r':    // Check to see if we are at end of Line.
                return(NULL);

            case TOKEN_STRING_CONST:
                StreamSkip(ReadStream51());    // skip string
                break;

            case TOKEN_INTL_CONST:   // possible line number
                if (LineNumFlag)
                {
                    LineAddr = GetStreamAddr();
                    StreamSkip(4);   // read over it
                    return(LineAddr);
                }

            case TOKEN_FLOAT_CONST:
                StreamSkip(4);   // read over it
                break;

            // skip over variable names and hashes
            case TOKEN_INTL_VAR:
            case TOKEN_FLOAT_VAR:
            case TOKEN_STRING_VAR:
            case TOKEN_INTL_ARRAY:
            case TOKEN_FLOAT_ARRAY:
            case TOKEN_STRING_ARRAY:
                StreamSkip((BYTE)(ReadStream51() + 4));    // skip var name and hash
                break;

            case TOKEN_COMMA:   // ignore commas
                if (OnFlag) LineNumFlag = TRUE;
                break;

            case TOKEN_RESTORE:
                CurChar = PeekStream51();
                LineNumFlag = (BIT) !TerminalChar();
                break;

            case TOKEN_GOTO:
            case TOKEN_GOSUB:
            case TOKEN_THEN:
            	LineNumFlag = TRUE;
                break;

            case TOKEN_ELSE:   // special case - skip jump bytes
                LineNumFlag = TRUE;   // possible line number token next
            case TOKEN_IF:
				ReadStream51();    // Skip jump pointer
                break;

            case TOKEN_ON:
                OnFlag = TRUE;
                break;

            case TOKEN_COLON:
            case TOKEN_ERROR:
                OnFlag = FALSE;
                break;

            default:
                LineNumFlag = FALSE;
                break;
        }
    }
}


// Renumber the program file
// RENUM [OldNum] [,[NewNum] [,Increment]
// Return TRUE if there was an error

BIT DoRenumCmd(void)
{
    BYTE Count;
    WORD addr;

    wArg[0] = wArg[1] = wArg[2] = 0xFFFF;  // defaults
    Count = GetLineNumberList(TOKEN_COMMA);
    if (Count == 0xFF) return(TRUE);

    // Correct Defaults
    if (wArg[0] == 0xFFFF) wArg[0] = 0;  // default starting OldNum
    if (wArg[1] == 0xFFFF)
    {
        wArg[1] = 10;  // default starting NewNum
        if (wArg[1] < wArg[0]) wArg[1] = wArg[0];
    }
    if (wArg[2] == 0xFFFF) wArg[2] = 10;  // default increment
    if (wArg[1] < wArg[0]) return(TRUE);  // New start line must be >= Old Start Line

    // Renumber algorithm
    // Start at the beginning of the program file.

    CurChar = '\r';   // Force first call to treat as new line
    SetStream51(BasicVars.ProgStart);  // start program after basic vars
    while (GetStreamAddr() < BasicVars.VarStart)
    {
	    // Scan each line for qualifying line numbers.
        addr = RenumScan();
        if (addr)
        {
            // Convert line number
            WriteRandomLong(addr, (long) RenumHelper((WORD) ReadRandomLong(addr)));
        }

    }

    // When all lines have been converted to new line numbers, traverse
    // program file again and set the line numbers to the new values.
    RenumHelper(0xFFFF);


    return(FALSE);
}

