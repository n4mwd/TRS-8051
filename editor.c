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
EDITOR.C - Line editor routines.
*/


#include "bas51.h"

// This is the main line editor routine.
// The line is accepted and added to the code file.
// Lines are stored in line number order.  Previously entered lines are moved
// around if necessary to make room.



// Edit a portion of the screen.
// On Entry, FieldStart and FieldLen describe the start of the edit field and
// its current length.  Their may be stuff in it already.  The edit field may
// extend past one line, but not more than 255 chars.  If the Edit field
// extends past the end of the screen, then it causes a scroll.  For this
// reason, FieldStart can change on exit.
// Returns with the key that terminated the edit.  Can be '\r', ESC,
// PgUp or PgDn.
// The cursor is initially placed in the position pointed to by 'CurOffset'.

BYTE EditScreen(BYTE CurOffset)
{
    WORD CurLoc;
    WORD wch;
    WORD Tmp;

    // Place cursor in the starting position.
    CurLoc = (WORD)(FieldStart + CurOffset);


    while (1)
    {
    	// Show cursor in the correct position
        GotoXY((BYTE)(CurLoc % VID_COLS), (BYTE)(CurLoc / VID_COLS));

        // Get a character from the keyboard
        wch = GetKeyWord();

        switch (wch)
        {
            case '\n':   // enter pressed input done
                wch = '\r';
            case '\r':
                return((BYTE) wch);

            case KEY51_PGUP:
            case KEY51_PGDN:
                return((BYTE)(wch >> 8));

            case '\b':   // Backspace
                if (CurLoc == FieldStart) break;
                CurLoc--;   // move cursor backwards then do delete
                // fall through

            case KEY51_DEL:  // DEL
                // Get number of bytes to move in Tmp
                Tmp = (WORD)(FieldStart + FieldLen - CurLoc);
                if (Tmp)  // only react if there is something to do
                {
                    memcpy(	&VIDEO_MEMORY[CurLoc],
                	    	&VIDEO_MEMORY[CurLoc + 1],
                            Tmp);
                    FieldLen--;
                }
                break;

            case KEY51_LEFT:     // Cursor Left
                if (CurLoc != FieldStart) CurLoc--;
                break;

            case KEY51_RIGHT:     // Cursor Right
                if (CurLoc != FieldStart + FieldLen) CurLoc++;
                break;

            case KEY51_END:   // End
                CurLoc = (WORD)(FieldStart + FieldLen);
                break;

            case KEY51_HOME:   // Home
                CurLoc = FieldStart;
                break;

            case KEY51_UP:    // UP
                CurLoc -= (BYTE) VID_COLS;
                if (CurLoc < FieldStart) CurLoc = FieldStart;
                break;

            case KEY51_DOWN:    // Down
                CurLoc += (BYTE) VID_COLS;
                if (CurLoc > FieldStart + FieldLen)
                	CurLoc = (WORD)(FieldStart + FieldLen);
                break;

            case KEY51_CTRL_C:
            case KEY51_BREAK:
                if (Running) BreakFlag = TRUE;

            case KEY51_ESC:       // ESC
                AutoLine = 0;
                return(27);


            default:
                if (isprint((char) wch) && FieldLen < 240)
                {
                    // Check if we need to scroll
                    Tmp = (WORD)(FieldStart + FieldLen);  // screen offset of end in Tmp
                    if (Tmp == (VID_COLS * VID_ROWS - 1))
                    {
                        // Scroll
                        VGA_Scroll();
                        // FieldStart -= (BYTE) VID_COLS;  - done by scroll
                        CurLoc -= (BYTE) VID_COLS;
                        Tmp -= (BYTE) VID_COLS;
                    }

                    // insert char at current position
                    memmove(&VIDEO_MEMORY[CurLoc + 1],
                            &VIDEO_MEMORY[CurLoc],
                            Tmp - CurLoc);

                    VIDEO_MEMORY[CurLoc++] = (char) wch;
                    FieldLen++;
                }
                break;
        }

        UPDATE_DISPLAY;   // For windows screen update

    } // end while()
}


// Get a basic line onto screen
// Return the key that terminated the edit.
// Edit field starts on CurPosY line.
// Cursor is initially positioned on CurOffset.
// Editing existing lines is possible but must be pre-printed on screen.

BYTE GetBasicLine(WORD LinePtr, BYTE CurOffset)
{
    GotoXY(0, CurPosY);
    if (CurOffset)    // We are re-editing a line with a syntax error.
    {
        BYTE SaveLen;
        WORD FieldStartOffset;

        SaveLen = FieldLen; // Save length

        // print prompt
        VGA_putchar('>');

        // Get the difference between old and new fieldstarts
        FieldStartOffset = (WORD)(CurPosY * VID_COLS + 1);
        FieldStartOffset -= FieldStart;

        // we need to copy the previous field and create a new one
        FieldLen = 0;
        while (FieldLen != SaveLen)   // FieldLen incremented by VGA_putchar()
            VGA_putchar(VIDEO_MEMORY[FieldStart + FieldLen]);

        // Adjust FieldStart to new position
        FieldStart += FieldStartOffset;

    }
    else if (LinePtr)    // If Lineptr is set, then we are editing an existing line.
    {
        // print prompt
        VGA_putchar('>');

        // Untokenize line and print in edit field
        // We do this only once.  Fieldstart and FieldLen get set here.
        PrintTokenizedLine(LinePtr);
        CurOffset = 6;   // set cursor past line number
    }
	else     // we are editing a blank line
    {
        ClearEOL();   // Clear line
        CurOffset = 0;
        if (AutoLine)
        {
		    BYTE PromptChar;

    	    PromptChar = '>';
            if (FindLinePtr(AutoLine, TRUE))
            {
                // If line already exists, then use special prompt instead
				PromptChar = '=';
            }
            CurOffset = (BYTE) VGA_printf("%c%u ", PromptChar, AutoLine);
            AutoLine += (WORD) AutoIncrement;
        }
        else VGA_putchar('>');

	    FieldStart = (WORD) (CurPosY * VID_COLS + 1);
        FieldLen = CurOffset;
    }

    return(EditScreen(CurOffset));
}




// Edit exisiting Line or create new one if LinePtr is NULL.
// Returns NULL if escape hit, 0xFFFF if no line number, or pointer in
// buffer to where it was stored.

WORD EditBasicLine(WORD LinePtr)
{
    BYTE CurOffset, SaveLen;

    CurOffset = 0;


    do   // Keep editing until tokenizer is happy
    {
        // Edit line on the screen directly
        if (GetBasicLine(LinePtr, CurOffset) == 27)  // ESC hit
            return(NULL);   // Don't save a line after escape hit

        // Tokenize line on screen starting at FieldStart for FieldLen bytes.
        // Tokenizer does not change screen memory or field descriptors.
        // Stores results in TokBuf[].
        SaveLen = FieldLen;
        CurOffset = TokenizeLine(&VIDEO_MEMORY[FieldStart], FieldLen);

        // Place cursor on last line of field
        GotoXY(CurPosX, (BYTE)((FieldStart + FieldLen) / VID_COLS));
        VGA_print("\n");

        if (SyntaxErrorCode != ERROR_NONE)    // there was an error
	    {
            // Print the error code, but it will also mess up FieldLen
            PrintErrorCode();
	    }
        FieldLen = SaveLen;   // Restore FieldLen

    } while (CurOffset);

    // Store / replace basic line in TokBuf[] to RAM file.
    return(StoreBasicLine(LinePtr));

}


// Scan the file and look for the specified line number.
// If ExactFlag = TRUE, LineNum must be an exact match.
// If ExactFlag = FALSE, will return LineNum if its there or the next line
// if not.
// Return a pointer to it or NULL if not found.
// NULL does not mean the program file is empty.

WORD FindLinePtr(WORD LineNum, BIT ExactFlag)
{
    BYTE Len;
    WORD addr;
    WORD Line;

    addr = BasicVars.ProgStart;  // start program after basic vars

    while (addr < BasicVars.VarStart)
    {
        Line = (WORD)((ReadRandom51(addr++) << 8) | ReadRandom51(addr++));
        Len = (BYTE) ReadRandom51(addr++);
        if (Line >= LineNum)
        {
            if (!ExactFlag || Line == LineNum) return((WORD)(addr - 3));
            else return(0);
        }
        addr += Len;
    }

    return(0);  // not found
}


void DeleteLine(WORD LinePtr)
{
	WORD Src;
    BYTE Len;

	Len = (BYTE)(ReadRandom51((WORD)(LinePtr + 2)) + 3);
    Src = (WORD)(LinePtr + Len);
    MemMove51(LinePtr, Src, (WORD)(BasicVars.VarStart - Src));
    BasicVars.VarStart -= Len;
}


// Get start line address or next line if not found
// If LineStart is zero it means to start at the beginning.
// Scan program file for LineEnd or line just before.
// If LineEnd == MAX_LINE_NUMBER, it means to go to end of file.
// If LineStart is zero and LineEnd is MAX_LINE_NUMBER, don't do anything.
// The start line must be less than the end line.

void DeleteLineRange(WORD StartLine, WORD EndLine)
{
    WORD Len;

    // Can't be used to delete all lines
    if (StartLine == 0 && EndLine == MAX_LINE_NUMBER) return;
    if (StartLine > EndLine) return;  // not an error but nothing to do

    // Get pointer to starting line
    if (StartLine == 0)
        StartLine = BasicVars.ProgStart;
    else  // Search For It
        StartLine = FindLinePtr(StartLine, FALSE);

    // Get pointer to last line
    EndLine = FindLinePtr((WORD)(EndLine + 1), FALSE);
    if (EndLine == 0) EndLine = BasicVars.VarStart;

    // Delete lines and collapse file
    Len = (WORD)(BasicVars.VarStart - EndLine);
    MemMove51(StartLine, EndLine, Len);
    BasicVars.VarStart -= (WORD)(EndLine - StartLine);
    ClearVariables();

    return;
}


// Store / replace basic line in TokBuf[] to RAM file.
// If LinePtr != NULL, it means that we are editing an existing line.
// If so, or if the line already exists, delete the old line first.
// Insert the new line in line number order shifting the program file as
// necessary.  Variable space is reset.
// Returns address of new line, else 0 + Errorcode in SyntaxErrorCode.
// Will return 0 + ERROR_NONE if line has number but is blank.
// Returns 0xFFFF if there was no line number and line is to be executed immediately.

WORD StoreBasicLine(WORD LinePtr)
{
    WORD Line;
    BYTE Len;

    SyntaxErrorCode = ERROR_NONE;

    // Get actual line number from TokBuf[]
    Line = (WORD)((TokBuf[0] << 8) | TokBuf[1]);
    if (Line == 0xFFFF)
    {
        // This line is to be executed immediately and not properly stored.
        // But in order to do that, it must be copied to external memory.
        WriteBlock51(BasicVars.CmdLine, (BYTE *) TokBuf, (BYTE) sizeof(TokBuf));
        return(0xFFFF);   // Indicate that line is to be Executed immediately.
    }
    // StreamDirtyFlag = TRUE;

    // If No pre-existing line pointer supplied, search for it.
    if (LinePtr == NULL)
    {
        LinePtr = FindLinePtr(Line, TRUE);
    }

    if (LinePtr) DeleteLine(LinePtr);   // delete old line first
/*    {
	    WORD Src;

        Len = (BYTE)(RandomRead51((WORD)(LinePtr + 2)) + 3);
        Src = (WORD)(LinePtr + Len);
        MemMove51(LinePtr, Src, (WORD)(BasicVars.VarStart - Src));
        BasicVars.VarStart -= Len;
    }
    */

    // Get True Length of new line from TokBuf[]
    LinePtr = 0;
    Len = (BYTE)(TokBuf[2] + 3);
    if (Len != 4)       // Add space for non-blank new line
	{
	    // Do We have enough RAM?
	    if (BytesFree() < Len)
	    {
	        SyntaxErrorCode = ERROR_INSUFFICIENT_MEMORY;
	        goto Error;   // Insufficient Memory
	    }

	    // Find the line just greater than Line
	    LinePtr = FindLinePtr(Line, FALSE);

	    // If we found one, then we need to open up space for the new line.
	    if (LinePtr)
	    {
            // Check for pre-existing line with same number and delete it
            if (ReadRandomWord(LinePtr) == Line) DeleteLine(LinePtr);

	        // Open up space
	        MemMove51((WORD)(LinePtr + Len), LinePtr, (WORD)(BasicVars.VarStart - LinePtr));
	    }
	    else // Just add to end of file.
	    {
    	    LinePtr = BasicVars.VarStart;  // First line in the program
	    }

	    // Copy New Line In
	    WriteBlock51(LinePtr, (BYTE *) TokBuf, Len);
	    BasicVars.VarStart += Len;
    }

Error:
   	ClearVariables();     // reset variable area

    return(LinePtr);

}

// Get an input line starting at the current cursor location

void GetInputLine(void)
{
    if (FieldStart != 9999) VGA_print("\n?");
    VGA_print("? ");   // Print input prompt

    FieldStart = (WORD) (CurPosY * VID_COLS + CurPosX);
    FieldLen = 0;
    EditScreen(0);  // Sets FieldStart and FieldLen to screen edit field
}

// Scan the input line for type compatable input.
// Returns TRUE on fatal input error.
// On syntax error, returns with SyntaxErrorCode non-zero.
// Set uData to input value of type VarType.
// Only TOKEN_INTL_CONST, TOKEN_FLOAT_CONST, & TOKEN_STRING_CONST types allowed.
// FieldStart is the start of the field and is adjusted for each call.
// The calling function needs to account for CurChar being used here.
// Returns TRUE and ErrorCode = 0 when there is no input.

BIT ReadInputLine(BYTE VarType)
{
    BYTE ret;

	if (FieldLen == 0)   // we need to get input from the screen
        GetInputLine();

    uData.LVal = 0;
    ret = FALSE;

    InBufPtr = &VIDEO_MEMORY[FieldStart];
    InBufPtr[FieldLen] = '\0';   // Temporary null terminator

    // Skip leading blanks.  CurChar set to char that stopped the scan.
    // InBufPtr points to the first non-blank - could be null.

    SkipBlanks();

    if (VarType == TOKEN_INTL_CONST)
    {
		uData.LVal = strtol((char *) InBufPtr, (char **) &InBufPtr, 0);
    }
    else if (VarType == TOKEN_FLOAT_CONST)
    {
        uData.fVal = strtod((char *) InBufPtr, (char **) &InBufPtr);
    }
    else if (VarType == TOKEN_STRING_CONST)
    {
        BYTE EndChar;
        BYTE *ptr;

        // String input - quotes optional
        EndChar = ',';
        if (*InBufPtr == '"')
        {
            EndChar = '"';
            InBufPtr++;
        }

        // Scan for endchar
        ptr = (BYTE *) strchr((char *) InBufPtr, (char) EndChar);

        // Calculate Length
        if (ptr) uData.sVal.sLen = (BYTE)(ptr - InBufPtr);  // Found EndChar
        else
        {
            // All the way to the end
            uData.sVal.sLen = (BYTE) strlen((char *) InBufPtr);
            EndChar = 0;
        }

        if (uData.sVal.sLen != 0)
        {
	        // allocate temp storage and copy
    	    uData.sVal.sPtr = TempAlloc(uData.sVal.sLen);
            if (!uData.sVal.sPtr) return(TRUE);
            WriteBlock51(uData.sVal.sPtr, InBufPtr, uData.sVal.sLen); // copy string

        }

        // Point to end
        InBufPtr += uData.sVal.sLen;
        if (EndChar == '"') InBufPtr++;  // point to char past quote
    }
    else   // illegal type
    {
        // This error is a syntax error and not because of the input
        SyntaxErrorCode = ERROR_TYPE_CONFLICT;
        goto EndScan;  //  Only SyntaxErrorCode is set with normal return value
    }

    {  // to enable private variables
    	BYTE ch;

    	ch = SkipBlanks();   // skip past any trailing blanks

	    // At this point, InBufPtr should point to either a comma or a null.

    	if (ch == '\0') goto EndScan;	// end of input
	    if (ch == ',')
    	{
        	InBufPtr++;    // get past comma
	        goto EndScan;
    	}
    }


//InputError:
    // if we are here, we need to do it over because the input wasn't right
    ret = TRUE;    // If we are here, then there was an error

EndScan:
    // Remove terminal null and reset Field descriptors
    FieldLen = (BYTE) strlen((char *) InBufPtr);
    FieldStart = (WORD)(InBufPtr - VIDEO_MEMORY);
    InBufPtr[FieldLen] = ' ';

    return(ret);   // no errors
}

























