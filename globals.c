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
GLOBALS.C - This is where all the globals are stored.
*/


#include "bas51.h"

BYTE SyntaxErrorCode;   // Set to error enum value defined in bas51.h
XDATA BYTE InBuf[256];
XDATA BYTE TokBuf[256];
BYTE *InBufPtr;       // pointer used to scan input buffer
BYTE CurChar;      // current char being evaluated
BYTE TmpChar;      // Temporary general purpose char
BYTE *TokBufPtr;
WORD AutoLine;     // Zero if AUTO line mode is off, else the next line number.
BYTE AutoIncrement;
BASIC_VARS_TYPE BasicVars;   // Local memory copy of Basic Vars (from prog file)
WORD EditLinePtr;
BIT Running;
WORD LineNo;      // Line number when Running
//WORD ExecLinePtr;   // pointer to line to execute
FOR_DESCRIPTOR gFor;
GOSUB_RETURN gReturn;
UVAL_HASH uHash;
WORD StopLinePtr;    // Temp Storage for STOP/CONT line
BIT BreakFlag;       // Break or Ctrl-C hit
BYTE CurPosX, CurPosY, LastCurX, LastCurY;   // Current/last cursor location
BYTE ScrollCnt;      // Incremented ever time a scroll occurs
WORD FieldStart;  // Index into screen memory  where edit field begins.
BYTE FieldLen;    // Length of edit field. (No trailing zero)
WORD DATAptr;     // DATA statement pointer.
UVAL_DATA uValArg[UDATA + 1];
BYTE uValTok[UDATA + 1];
WORD wArg[UDATA + 1];
BYTE DefTypes[26];   // default types
BYTE KeyFlags;       // Flags set with keystrokes
DWORD ScreenTicks;















// Token table - sorted by precedence - see OPERATOR_TOKEN enums.
// Similar names like "ELSE" and "ELSEIF" must have the longer name listed first.
// Some like '+' and '-' are listed twice on purpose.  MatchToken() will only
// return the first, but these are special set if appropriate.

CODE BYTE OperatorTokenTable[] =
{
    // OPERATORS

    '='+128,                           // = Assignment
    ','+128,                           // , comma
    '('+128,                           // ( Left Parentheses
    'O'+128,'R',          			   // Logical OR
    'A'+128,'N','D',                   // Logical AND
    '='+128,                           // = Equals (Comparison)
    '<'+128,'=',          			   // <=
    '>'+128,'=',          			   // >=
    '<'+128,'>',          			   // <>
    '>'+128,                           // >
    '<'+128,                           // <
    '+'+128,                           // + Numerical Addition
    '-'+128,                           // - Binary Minus
    '*'+128,                           // *
    '/'+128,                           // /
    'M'+128,'O','D',      			   // Modulus
    '-'+128,                           // - Unary Minus
    'N'+128,'O','T',      			   // Logical NOT
    '^'+128,              			   // ^ Raise to the power of

    // special characters
    ':'+128,                           // : colon
    ';'+128,                           // ; semicolon
    ')'+128,                           // ) Right Parentheses
    '@'+128,
    128               // End of table
};


// Sorted by the number of parameters

CODE BYTE FunctionTokenTable[] =
{
    // Zero parameters
    'P'+128,'I',                       		// PI - Returns 3.14159265359
    'M'+128,'E','M',                   		// MEM - Returns free space
    'P'+128,'O','S','X',               		// POSX - Cursor X position
    'P'+128,'O','S','Y',               		// POSY - Cursor Y Position
    'I'+128,'N','K','E','Y','$',       		// INKEY$ - Read key from keyboard
    'E'+128,'X','T','K','E','Y',            // EXTKEY - Extended keyboard status
    'E'+128,'R','R',               			// ERR  - Error code
    'E'+128,'R','L',               			// ERL  - Error Line number
    'T'+128,'I','M','E','$',                // TIME - Returns time
    'T'+128,'I','M','E','R',            	// TIMER - Returns timer ticks


    // One Paramerter
    'A'+128,'B','S',      					// ABS() - absolute value
    'A'+128,'T','N',      					// ATN() - ArcTangent in radians
    'C'+128,'O','S',      					// COS() - Cosine in radians
    'E'+128,'X','P',      					// EXP() - Exponent e^exp
    'L'+128,'O','G',      					// LOG() - Log base e
    'S'+128,'I','N',      					// SIN() - sine in radians
    'S'+128,'Q','R',      					// SQR() - Square Root
    'T'+128,'A','N',      					// TAN() - Tangent in radians
    'I'+128,'N','T',      					// INT() - converts float to int
    'F'+128,'I','X', 						// FIX() = converts float to int
    'C'+128,'I','N','T',      		  		// CINT() - converts float to int
    'C'+128,'S','N','G',      		  		// CSNG() - converts numeric to float
    'L'+128,'E','N',      					// LEN() - returns length of string
    'P'+128,'E','E','K',     				// PEEK() - returns byte at address
    'R'+128,'N','D', 	 					// RND() - return random number
    'S'+128,'G','N',     					// SGN() - return BOOLEAN sign of parameter
    'U'+128,'S','R',     					// USR() - Calls asm language routine at parameter
    'V'+128,'A','L',     					// VAL() - returns numerical value of exression in string
    'A'+128,'S','C',          				// ASC() - returns ascii code of 1st char in string
    'C'+128,'H','R','$',    				// CHR$() - returns string with ascii char param in it
    'S'+128,'T','R','$',    				// STR$() - Converts numeric to string
    'T'+128,'A','B',                        // TAB() - Changes CurPosX and returns null string

    // Two Parameters
    'S'+128,'T','R','I','N','G','$',   		// STRING$() - Returns string with param chars in it
    'P'+128,'O','I','N','T', 				// POINT() - Returns BOOLEAN for graphics point
    'L'+128,'E','F','T','$',   				// LEFT$() - returns left substring
    'R'+128,'I','G','H','T','$',  			// RIGHT$() - returns right substring
    'A'+128,'T',                            // AT() - Changes Cursor location, returns null string.
    'I'+128,'N','S','T','R',                // INSTR() - Search for substring.

    // Three Parameters
    'M'+128,'I','D','$',    				// MID$() - returns mid substring
    128               // Mark end of table
};


BYTE CommandTokenTable[] =
{
    // STATEMENTS (41 TOTAL)

    // Statements with special printing go first
    'B'+128,'E','G','I','N',				// "BEGIN",
    'E'+128,'L','S','E',					// "ELSE",
    'S'+128,'T','E','P',					// "STEP",
    'T'+128,'H','E','N',               		// "THEN"
    'T'+128,'O',							// "TO",

    // Standard printing
    'A'+128,'U','T','O',					// "AUTO",


    'C'+128,'L','E','A','R',				// "CLEAR",
    'C'+128,'L','S',						// "CLS",
    'C'+128,'O','N','T',					// "CONT",

    'D'+128,'A','T','A',					// "DATA",
    'D'+128,'E','F','D','B','L',			// "DEFDBL"
    'D'+128,'E','F','I','N','T',            // "DEFINT"
    'D'+128,'E','F','S','N','G',            // "DEFSNG"
    'D'+128,'E','F','S','T','R',            // "DEFSTR"
    'D'+128,'E','L','E','T','E',			// "DELETE",
    'D'+128,'I','M',						// "DIM",

    'E'+128,'D','I','T',					// "EDIT",

//    'E'+128,'L','S','E','I','F', 			// "ELSEIF",
//    'E'+128,'N','D','I','F',				// "ENDIF",
    'L'+128,'O','A','D',                    // "LOAD",
    'S'+128,'A','V','E',                    // "SAVE",

    'E'+128,'N','D',						// "END",
    'E'+128,'R','R','O','R',				// "ERROR",

    'F'+128,'O','R',						// "FOR",

    'G'+128,'O','S','U','B',				// "GOSUB",
    'G'+128,'O','T','O',					// "GOTO",

    'I'+128,'F',							// "IF",
    'I'+128,'N','P','U','T',				// "INPUT",
//    'I'+128,'N',							// "IN",
	'F'+128,'I','L','E','S',                // "FILES",

    'L'+128,'E','T',						// "LET",
    'L'+128,'I','N','E', 					// "LINE"
    'L'+128,'I','S','T',					// "LIST",

    'N'+128,'E','W',						// "NEW",
    'N'+128,'E','X','T',					// "NEXT",

    'O'+128,'N',							// "ON",
    'O'+128,'U','T',						// "OUT",

    'P'+128,'A','U','S','E',				// "PAUSE",
    'P'+128,'L','O','T',					// "PLOT",
    'P'+128,'R','I','N','T',				// "PRINT",
    '?'+128,                           		// Also PRINT

    'R'+128,'E','A','D',					// "READ",
    'R'+128,'E','M',						// "REM",
    '\''+128,                          		// Also REM
    'R'+128,'E','N','U','M',				// "RENUM",
    'R'+128,'E','S','T','O','R','E',		// "RESTORE",
    'R'+128,'E','S','U','M','E',			// "RESUME",
    'R'+128,'E','T','U','R','N',			// "RETURN",
    'R'+128,'U','N',						// "RUN",

    'S'+128,'T','O','P',					// "STOP",


    'U'+128,'N','P','L','O','T',			// "UNPLOT",

    'E'+128,'X','I','T',                    // "EXIT"
//    'T'+128,'H','E','N','+',               		// "THENGOTO"
//    'E'+128,'L','S','E','+',					// "ELSEGOTO",
    'U'+128,'S','I','N','G',                // USING 
    'P'+128,'O','K','E',                    // POKE

    128               // marks end of table
};





char *ErrorStrings[] =
{
    "",                              // No Error
    "NEXT WITHOUT FOR",          	 // 1
    "SYNTAX",                        // 2
    "RETURN WITHOUT GOSUB",          // 3
    "OUT OF DATA",                   // 4
    "PARAMETER ERROR",               // 5
    "ARITHMETIC OVERFLOW",           // 6
    "INSUFFICIENT MEMORY",           // 7
    "INVALID LINE NUMBER",           // 8
    "SUBSCRIPT OUT OF RANGE",        // 9
    "REDIMENSIONED ARRAY",           // 10
    "DIVIDE BY ZERO",                // 11
    "ILLEGAL DIRECT",                // 12
    "TYPE CONFLICT",        		 // 13
    "OUT OF STRING SPACE",           // 14
    "STRING TOO LONG",               // 15
    "EXPRESSION TOO COMPLEX",        // 16
    "CAN'T CONTINUE",                // 17
    "NO RESUME",                     // 18
    "UNEXPECTED RESUME",             // 19
    "UNDEFINED ERROR",               // 20
    "MISSING OPERAND",               // 21

    "EXPRESSION STACK OVERFLOW",     // 22
    "EXPRESSION STACK UNDERFLOW",    // 23
    "MISSING QUOTE",                 // 24
    "UNEXPECTED END OF LINE",        // 25
    "MALFORMED NUMERIC CONSTANT",    // 26
    "FUNCTION NEEDS PARENTHESIS",    // 27
    "UNDECIFERABLE EXPRESSION",      // 28
    "LINE TOO LONG",                 // 29
    "UNRECOGNIZED SYMBOL",           // 30
    "INTERNAL ERROR",                // 31
    "VARIABLE NAME TOO LONG",        // 32
    "NOT ALLOWED HERE",              // 33
    "BAD COMMAND",                   // 34
    "UNDEFINED VARIABLE",            // 35
    "UNDEFINED FUNCTION",            // 36
    "INVALID INPUT VALUE",           // 37
    "FILE NOT FOUND", 				 // 38
    "FILE I/O",                      // 39
    "PARENTHESIS MISMATCH"           // 40
};


void PrintErrorCode(void)
{
    if (SyntaxErrorCode)
    {
        VGA_print("ERROR: ");
        if (SyntaxErrorCode < (sizeof(ErrorStrings) / sizeof(ErrorStrings[0])))
        {
            VGA_printf("%s", ErrorStrings[SyntaxErrorCode]);
        }
        else VGA_print("UNKNOWN");

        if (LineNo != 0xFFFF) VGA_printf(" in Line %d", LineNo);
        VGA_print("\n\n");

        SyntaxErrorCode = ERROR_NONE;  // reset for next error
    }

}



#ifdef __WIN32__

BYTE FontLines[14][256] =  {   // Font Line 1
{
    0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, // 0 to 15
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 16 to 31
    0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 32 to 47
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 48 to 63
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 64 to 79
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, // 80 to 95
    0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 96 to 111
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 112 to 127
    0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, // 128 to 143
    0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, // 144 to 159
    0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, // 160 to 175
    0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, // 176 to 191
    0x18, 0x18, 0x18, 0x36, 0x00, 0x00, 0x36, 0x36, 0x00, 0x36, 0x36, 0x18, 0x00, 0x18, 0x18, 0x00, // 192 to 207
    0x18, 0x00, 0x18, 0x18, 0x36, 0x36, 0x00, 0x36, 0x00, 0x36, 0x00, 0x36, 0x18, 0x36, 0x00, 0x00, // 208 to 223
    0x36, 0x18, 0x00, 0x00, 0x36, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x11, 0x55, 0xDD, 0x00, 0x00, // 224 to 239
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // 240 to 255
},

//BYTE FontLine01[256] =     // Font Line 2
{
    0xFE, 0x18, 0x38, 0x00, 0x0C, 0x40, 0x40, 0x28, 0x00, 0x0F, 0x00, 0x10, 0x00, 0x00, 0x70, 0x00, // 0 to 15
    0x00, 0x00, 0x00, 0x38, 0x00, 0x7C, 0x00, 0xD8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 16 to 31
    0x00, 0x00, 0x66, 0x00, 0x18, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 32 to 47
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 48 to 63
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 64 to 79
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, // 80 to 95
    0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 96 to 111
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 112 to 127
    0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, // 128 to 143
    0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, // 144 to 159
    0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, // 160 to 175
    0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, // 176 to 191
    0x18, 0x18, 0x18, 0x36, 0x00, 0x00, 0x36, 0x36, 0x00, 0x36, 0x36, 0x18, 0x00, 0x18, 0x18, 0x00, // 192 to 207
    0x18, 0x00, 0x18, 0x18, 0x36, 0x36, 0x00, 0x36, 0x00, 0x36, 0x00, 0x36, 0x18, 0x36, 0x00, 0x00, // 208 to 223
    0x36, 0x18, 0x00, 0x00, 0x36, 0x18, 0x18, 0x00, 0xF8, 0x00, 0x00, 0x44, 0xAA, 0x77, 0x00, 0x00, // 224 to 239
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // 240 to 255
},

//BYTE FontLine02[256] =     // Font Line 3
{
    0xFE, 0x18, 0x6C, 0x66, 0x12, 0x40, 0x40, 0xFC, 0x1C, 0x0C, 0x0C, 0x10, 0x1E, 0x3C, 0xD8, 0x10, // 0 to 15
    0x80, 0x02, 0x18, 0x6C, 0x7E, 0xC6, 0x00, 0x6C, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 16 to 31
    0x00, 0x18, 0x66, 0x6C, 0x7C, 0x00, 0x38, 0x30, 0x0C, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, // 32 to 47
    0x7C, 0x18, 0x7C, 0x7C, 0x0C, 0xFE, 0x38, 0xFE, 0x7C, 0x7C, 0x00, 0x00, 0x06, 0x00, 0x60, 0x7C, // 48 to 63
    0x7C, 0x10, 0xFC, 0x3C, 0xF8, 0xFE, 0xFE, 0x3C, 0xC6, 0x3C, 0x1E, 0xE6, 0xF0, 0xC6, 0xC6, 0x38, // 64 to 79
    0xFC, 0x7C, 0xFC, 0x7C, 0x7E, 0xC6, 0xC6, 0xC6, 0xC6, 0x66, 0xFE, 0x3C, 0x80, 0x3C, 0x6C, 0x00, // 80 to 95
    0x18, 0x00, 0xE0, 0x00, 0x1C, 0x00, 0x38, 0x00, 0xE0, 0x18, 0x06, 0xE0, 0x38, 0x00, 0x00, 0x00, // 96 to 111
    0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x18, 0x70, 0x76, 0x00, // 112 to 127
    0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, // 128 to 143
    0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, // 144 to 159
    0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, // 160 to 175
    0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, // 176 to 191
    0x18, 0x18, 0x18, 0x36, 0x00, 0x00, 0x36, 0x36, 0x00, 0x36, 0x36, 0x18, 0x00, 0x18, 0x18, 0x00, // 192 to 207
    0x18, 0x00, 0x18, 0x18, 0x36, 0x36, 0x00, 0x36, 0x00, 0x36, 0x00, 0x36, 0x18, 0x36, 0x00, 0x00, // 208 to 223
    0x36, 0x18, 0x00, 0x00, 0x36, 0x18, 0x18, 0x00, 0xCC, 0x00, 0x00, 0x11, 0x55, 0xDD, 0x00, 0x00, // 224 to 239
    0x30, 0x0C, 0x00, 0x00, 0x00, 0xFE, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x7E, 0x38, 0x38, 0x1E, 0x00  // 240 to 255
},

//BYTE FontLine03[256] =     // Font Line 4
{
    0xFE, 0x3C, 0x64, 0x66, 0x10, 0x46, 0x46, 0x66, 0x62, 0x0C, 0x12, 0x10, 0x0E, 0x66, 0x30, 0x92, // 0 to 15
    0xC0, 0x06, 0x3C, 0x6C, 0xDA, 0x60, 0x00, 0x6C, 0x3C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x10, 0xFE, // 16 to 31
    0x00, 0x3C, 0x66, 0x6C, 0xC6, 0x00, 0x6C, 0x30, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, // 32 to 47
    0xC6, 0x38, 0xC6, 0xC6, 0x1C, 0xC0, 0x60, 0xC6, 0xC6, 0xC6, 0x18, 0x18, 0x0C, 0x00, 0x30, 0xC6, // 48 to 63
    0xC6, 0x38, 0x66, 0x66, 0x6C, 0x66, 0x66, 0x66, 0xC6, 0x18, 0x0C, 0x66, 0x60, 0xEE, 0xE6, 0x6C, // 64 to 79
    0x66, 0xC6, 0x66, 0xC6, 0x7E, 0xC6, 0xC6, 0xC6, 0xC6, 0x66, 0xC6, 0x30, 0xC0, 0x0C, 0xC6, 0x00, // 80 to 95
    0x00, 0x00, 0x60, 0x00, 0x0C, 0x00, 0x6C, 0x00, 0x60, 0x18, 0x06, 0x60, 0x18, 0x00, 0x00, 0x00, // 96 to 111
    0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0xDC, 0x00, // 112 to 127
    0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, // 128 to 143
    0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, // 144 to 159
    0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, // 160 to 175
    0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, 0x00, 0xF0, 0x0F, 0xFF, // 176 to 191
    0x18, 0x18, 0x18, 0x36, 0x00, 0x00, 0x36, 0x36, 0x00, 0x36, 0x36, 0x18, 0x00, 0x18, 0x18, 0x00, // 192 to 207
    0x18, 0x00, 0x18, 0x18, 0x36, 0x36, 0x00, 0x36, 0x00, 0x36, 0x00, 0x36, 0x18, 0x36, 0x00, 0x00, // 208 to 223
    0x36, 0x18, 0x00, 0x00, 0x36, 0x18, 0x18, 0x00, 0xCC, 0x00, 0x00, 0x44, 0xAA, 0x77, 0xFE, 0x18, // 224 to 239
    0x18, 0x18, 0x18, 0x00, 0x00, 0xC6, 0x00, 0xC6, 0x00, 0x00, 0x00, 0x18, 0x6C, 0x6C, 0x30, 0x00  // 240 to 255
},

//BYTE FontLine04[256] =     // Font Line 5
{
    0xFE, 0x66, 0x60, 0x3C, 0x10, 0x4C, 0x4C, 0x66, 0x40, 0x0C, 0x12, 0x10, 0x1A, 0x66, 0x60, 0x54, // 0 to 15
    0xE0, 0x0E, 0x7E, 0x38, 0xDA, 0x38, 0x76, 0x6C, 0x7E, 0x18, 0x18, 0x30, 0x00, 0x28, 0x38, 0xFE, // 16 to 31
    0x00, 0x3C, 0x24, 0xFE, 0xC2, 0xC2, 0x6C, 0x60, 0x30, 0x0C, 0x44, 0x18, 0x00, 0x00, 0x00, 0x0C, // 32 to 47
    0xCE, 0x78, 0x06, 0x06, 0x3C, 0xC0, 0xC0, 0x06, 0xC6, 0xC6, 0x18, 0x18, 0x18, 0x00, 0x18, 0xC6, // 48 to 63
    0xC6, 0x6C, 0x66, 0xC2, 0x66, 0x62, 0x62, 0xC2, 0xC6, 0x18, 0x0C, 0x6C, 0x60, 0xFE, 0xF6, 0xC6, // 64 to 79
    0x66, 0xC6, 0x66, 0xC6, 0x5A, 0xC6, 0xC6, 0xC6, 0x6C, 0x66, 0x8C, 0x30, 0xE0, 0x0C, 0x00, 0x00, // 80 to 95
    0x00, 0x00, 0x60, 0x00, 0x0C, 0x00, 0x64, 0x00, 0x60, 0x00, 0x00, 0x60, 0x18, 0x00, 0x00, 0x00, // 96 to 111
    0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x00, 0x10, // 112 to 127
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 128 to 143
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 144 to 159
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 160 to 175
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 176 to 191
    0x18, 0x18, 0x18, 0x36, 0x00, 0x00, 0x36, 0x36, 0x00, 0x36, 0x36, 0x18, 0x00, 0x18, 0x18, 0x00, // 192 to 207
    0x18, 0x00, 0x18, 0x18, 0x36, 0x36, 0x00, 0x36, 0x00, 0x36, 0x00, 0x36, 0x18, 0x36, 0x00, 0x00, // 208 to 223
    0x36, 0x18, 0x00, 0x00, 0x36, 0x18, 0x18, 0x00, 0xF8, 0x36, 0xD8, 0x11, 0x55, 0xDD, 0x00, 0x18, // 224 to 239
    0x0C, 0x30, 0x18, 0x00, 0x7C, 0xC6, 0xFE, 0x60, 0x00, 0x66, 0x76, 0x3C, 0xC6, 0xC6, 0x18, 0x6C  // 240 to 255
},

//BYTE FontLine05[256] =     // Font Line 6
{
    0xFE, 0x60, 0xF0, 0x18, 0x10, 0x58, 0x58, 0x66, 0xF0, 0x0C, 0x10, 0x10, 0x32, 0x66, 0xC8, 0x38, // 0 to 15
    0xF8, 0x3E, 0x18, 0x00, 0xDA, 0x6C, 0xDC, 0x6C, 0x18, 0x18, 0x0C, 0x60, 0xC0, 0x6C, 0x38, 0x7C, // 16 to 31
    0x00, 0x3C, 0x00, 0x6C, 0xC0, 0xC6, 0x38, 0x00, 0x30, 0x0C, 0x28, 0x18, 0x00, 0x00, 0x00, 0x18, // 32 to 47
    0xDE, 0x18, 0x0C, 0x06, 0x6C, 0xC0, 0xC0, 0x0C, 0xC6, 0xC6, 0x00, 0x00, 0x30, 0x7E, 0x0C, 0x0C, // 48 to 63
    0xDE, 0xC6, 0x66, 0xC0, 0x66, 0x68, 0x68, 0xC0, 0xC6, 0x18, 0x0C, 0x6C, 0x60, 0xFE, 0xFE, 0xC6, // 64 to 79
    0x66, 0xC6, 0x66, 0x60, 0x18, 0xC6, 0xC6, 0xC6, 0x38, 0x66, 0x18, 0x30, 0x70, 0x0C, 0x00, 0x00, // 80 to 95
    0x00, 0x78, 0x78, 0x7C, 0x3C, 0x7C, 0x60, 0x76, 0x6C, 0x38, 0x0E, 0x66, 0x18, 0xEC, 0xDC, 0x7C, // 96 to 111
    0xDC, 0x76, 0xDC, 0x7C, 0xFC, 0xCC, 0x66, 0xC6, 0xC6, 0xC6, 0xFE, 0x18, 0x18, 0x18, 0x00, 0x38, // 112 to 127
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 128 to 143
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 144 to 159
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 160 to 175
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 176 to 191
    0x18, 0x18, 0xF8, 0x36, 0x00, 0xF8, 0xF6, 0x36, 0xFE, 0xF6, 0x36, 0xF8, 0x00, 0x18, 0x18, 0x00, // 192 to 207
    0x18, 0x00, 0x18, 0x1F, 0x36, 0x37, 0x3F, 0xF7, 0xFF, 0x37, 0xFF, 0xF7, 0xFF, 0x36, 0xFF, 0x00, // 208 to 223
    0x36, 0x1F, 0x1F, 0x00, 0x36, 0xFF, 0x18, 0x00, 0xC4, 0x6C, 0x6C, 0x44, 0xAA, 0x77, 0x00, 0x7E, // 224 to 239
    0x06, 0x60, 0x00, 0x76, 0xC6, 0xC0, 0x6C, 0x30, 0x7E, 0x66, 0xDC, 0x66, 0xC6, 0xC6, 0x0C, 0x92  // 240 to 255
},

//BYTE FontLine06[256] =     // Font Line 7
{
    0xFE, 0x60, 0x60, 0x7E, 0x7C, 0x30, 0x32, 0x7C, 0x40, 0x0C, 0x10, 0x10, 0x78, 0x3C, 0xF8, 0xEE, // 0 to 15
    0xFE, 0xFE, 0x18, 0x00, 0x7A, 0xC6, 0x00, 0x6C, 0x18, 0x18, 0xFE, 0xFE, 0xC0, 0xFE, 0x7C, 0x7C, // 16 to 31
    0x00, 0x18, 0x00, 0x6C, 0x7C, 0x0C, 0x76, 0x00, 0x30, 0x0C, 0xFE, 0x7E, 0x00, 0xFE, 0x00, 0x30, // 32 to 47
    0xF6, 0x18, 0x18, 0x3C, 0xCC, 0xFC, 0xFC, 0x18, 0x7C, 0x7E, 0x00, 0x00, 0x60, 0x00, 0x06, 0x18, // 48 to 63
    0xDE, 0xC6, 0x7C, 0xC0, 0x66, 0x78, 0x78, 0xC0, 0xFE, 0x18, 0x0C, 0x78, 0x60, 0xD6, 0xDE, 0xC6, // 64 to 79
    0x7C, 0xC6, 0x7C, 0x38, 0x18, 0xC6, 0xC6, 0xD6, 0x38, 0x3C, 0x30, 0x30, 0x38, 0x0C, 0x00, 0x00, // 80 to 95
    0x00, 0x0C, 0x6C, 0xC6, 0x6C, 0xC6, 0xF0, 0xCC, 0x76, 0x18, 0x06, 0x6C, 0x18, 0xFE, 0x66, 0xC6, // 96 to 111
    0x66, 0xCC, 0x76, 0xC6, 0x30, 0xCC, 0x66, 0xC6, 0x6C, 0xC6, 0xCC, 0x70, 0x00, 0x0E, 0x00, 0x6C, // 112 to 127
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 128 to 143
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 144 to 159
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 160 to 175
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 176 to 191
    0x18, 0x18, 0x18, 0x36, 0x00, 0x18, 0x06, 0x36, 0x06, 0x06, 0x36, 0x18, 0x00, 0x18, 0x18, 0x00, // 192 to 207
    0x18, 0x00, 0x18, 0x18, 0x36, 0x30, 0x30, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, // 208 to 223
    0x36, 0x18, 0x18, 0x00, 0x36, 0x18, 0x18, 0x00, 0xCC, 0xD8, 0x36, 0x11, 0x55, 0xDD, 0xFE, 0x18, // 224 to 239
    0x0C, 0x30, 0x7E, 0xDC, 0xFC, 0xC0, 0x6C, 0x18, 0xD8, 0x66, 0x18, 0x66, 0xFE, 0xC6, 0x3E, 0x92  // 240 to 255
},

//BYTE FontLine07[256] =     // Font Line 8
{
    0xFE, 0x66, 0x60, 0x18, 0x10, 0x60, 0x66, 0x66, 0xF0, 0xEC, 0x10, 0x10, 0xCC, 0x18, 0x00, 0x38, // 0 to 15
    0xF8, 0x3E, 0x18, 0x00, 0x1A, 0xC6, 0x76, 0x00, 0x18, 0x18, 0x0C, 0x60, 0xC0, 0x6C, 0x7C, 0x38, // 16 to 31
    0x00, 0x18, 0x00, 0x6C, 0x06, 0x18, 0xDC, 0x00, 0x30, 0x0C, 0x28, 0x18, 0x00, 0x00, 0x00, 0x60, // 32 to 47
    0xE6, 0x18, 0x30, 0x06, 0xFE, 0x06, 0xC6, 0x30, 0xC6, 0x06, 0x00, 0x00, 0x30, 0x00, 0x0C, 0x18, // 48 to 63
    0xDE, 0xFE, 0x66, 0xC0, 0x66, 0x68, 0x68, 0xDE, 0xC6, 0x18, 0x0C, 0x6C, 0x60, 0xC6, 0xCE, 0xC6, // 64 to 79
    0x60, 0xD6, 0x6C, 0x0C, 0x18, 0xC6, 0xC6, 0xD6, 0x38, 0x18, 0x60, 0x30, 0x1C, 0x0C, 0x00, 0x00, // 80 to 95
    0x00, 0x7C, 0x66, 0xC0, 0xCC, 0xFE, 0x60, 0xCC, 0x66, 0x18, 0x06, 0x78, 0x18, 0xD6, 0x66, 0xC6, // 96 to 111
    0x66, 0xCC, 0x66, 0x70, 0x30, 0xCC, 0x66, 0xD6, 0x38, 0xC6, 0x18, 0x18, 0x18, 0x18, 0x00, 0xC6, // 112 to 127
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 128 to 143
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 144 to 159
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 160 to 175
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 176 to 191
    0x18, 0xF8, 0xF8, 0xF6, 0xFE, 0xF8, 0xF6, 0x36, 0xF6, 0xFE, 0xFE, 0xF8, 0xF8, 0x1F, 0xFF, 0xFF, // 192 to 207
    0x1F, 0xFF, 0xFF, 0x1F, 0x37, 0x3F, 0x37, 0xFF, 0xF7, 0x37, 0xFF, 0xF7, 0xFF, 0xFF, 0xFF, 0xFF, // 208 to 223
    0x3F, 0x1F, 0x1F, 0x3F, 0xFF, 0xFF, 0xF8, 0x1F, 0xDE, 0x6C, 0x6C, 0x44, 0xAA, 0x77, 0x00, 0x18, // 224 to 239
    0x18, 0x18, 0x00, 0xD8, 0xC6, 0xC0, 0x6C, 0x30, 0xD8, 0x66, 0x18, 0x66, 0xC6, 0x6C, 0x66, 0x6C  // 240 to 255
},

//BYTE FontLine08[256] =     // Font Line 9
{
    0xFE, 0x3C, 0x60, 0x7E, 0x10, 0xDC, 0xCA, 0x66, 0x40, 0x6C, 0x10, 0x90, 0xCC, 0x7E, 0x00, 0x54, // 0 to 15
    0xE0, 0x0E, 0x7E, 0x00, 0x1A, 0x6C, 0xDC, 0x00, 0x18, 0x7E, 0x18, 0x30, 0xFE, 0x28, 0xFE, 0x38, // 16 to 31
    0x00, 0x00, 0x00, 0xFE, 0x86, 0x30, 0xCC, 0x00, 0x30, 0x0C, 0x44, 0x18, 0x18, 0x00, 0x00, 0xC0, // 32 to 47
    0xC6, 0x18, 0x60, 0x06, 0x0C, 0x06, 0xC6, 0x30, 0xC6, 0x06, 0x18, 0x18, 0x18, 0x7E, 0x18, 0x00, // 48 to 63
    0xDC, 0xC6, 0x66, 0xC2, 0x66, 0x62, 0x60, 0xC6, 0xC6, 0x18, 0xCC, 0x6C, 0x62, 0xC6, 0xC6, 0xC6, // 64 to 79
    0x60, 0xDE, 0x66, 0xC6, 0x18, 0xC6, 0x6C, 0xFE, 0x6C, 0x18, 0xC2, 0x30, 0x0E, 0x0C, 0x00, 0x00, // 80 to 95
    0x00, 0xCC, 0x66, 0xC0, 0xCC, 0xC0, 0x60, 0xCC, 0x66, 0x18, 0x06, 0x6C, 0x18, 0xD6, 0x66, 0xC6, // 96 to 111
    0x66, 0xCC, 0x60, 0x1C, 0x30, 0xCC, 0x66, 0xD6, 0x38, 0xC6, 0x30, 0x18, 0x18, 0x18, 0x00, 0xC6, // 112 to 127
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 128 to 143
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 144 to 159
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 160 to 175
    0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, // 176 to 191
    0x18, 0x18, 0x18, 0x36, 0x36, 0x18, 0x36, 0x36, 0x36, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, // 192 to 207
    0x18, 0x00, 0x18, 0x18, 0x36, 0x00, 0x36, 0x00, 0x36, 0x36, 0x00, 0x36, 0x00, 0x00, 0x18, 0x36, // 208 to 223
    0x00, 0x00, 0x18, 0x36, 0x36, 0x18, 0x00, 0x18, 0xCC, 0x36, 0xD8, 0x11, 0x55, 0xDD, 0x00, 0x00, // 224 to 239
    0x30, 0x0C, 0x18, 0xD8, 0xC6, 0xC0, 0x6C, 0x60, 0xD8, 0x7C, 0x18, 0x3C, 0xC6, 0x6C, 0x66, 0x00  // 240 to 255
},

//BYTE FontLine09[256] =     // Font Line 10
{
    0xFE, 0x18, 0xE6, 0x18, 0x10, 0xA2, 0x92, 0x66, 0x22, 0x3C, 0x10, 0x90, 0xCC, 0x18, 0x00, 0x92, // 0 to 15
    0xC0, 0x06, 0x3C, 0x00, 0x1A, 0x38, 0x00, 0x00, 0x18, 0x3C, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x10, // 16 to 31
    0x00, 0x18, 0x00, 0x6C, 0xC6, 0x66, 0xCC, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x00, 0x18, 0x80, // 32 to 47
    0xC6, 0x18, 0xC6, 0xC6, 0x0C, 0xC6, 0xC6, 0x30, 0xC6, 0x0C, 0x18, 0x18, 0x0C, 0x00, 0x30, 0x18, // 48 to 63
    0xC0, 0xC6, 0x66, 0x66, 0x6C, 0x66, 0x60, 0x66, 0xC6, 0x18, 0xCC, 0x66, 0x66, 0xC6, 0xC6, 0x6C, // 64 to 79
    0x60, 0x7C, 0x66, 0xC6, 0x18, 0xC6, 0x38, 0x7C, 0xC6, 0x18, 0xC6, 0x30, 0x06, 0x0C, 0x00, 0x00, // 80 to 95
    0x00, 0xCC, 0x66, 0xC6, 0xCC, 0xC6, 0x60, 0x7C, 0x66, 0x18, 0x06, 0x66, 0x18, 0xD6, 0x66, 0xC6, // 96 to 111
    0x7C, 0x7C, 0x60, 0xC6, 0x36, 0xCC, 0x3C, 0xFE, 0x6C, 0x7E, 0x66, 0x18, 0x18, 0x18, 0x00, 0xFE, // 112 to 127
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 128 to 143
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, // 144 to 159
    0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, // 160 to 175
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 176 to 191
    0x18, 0x18, 0x18, 0x36, 0x36, 0x18, 0x36, 0x36, 0x36, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, // 192 to 207
    0x18, 0x00, 0x18, 0x18, 0x36, 0x00, 0x36, 0x00, 0x36, 0x36, 0x00, 0x36, 0x00, 0x00, 0x18, 0x36, // 208 to 223
    0x00, 0x00, 0x18, 0x36, 0x36, 0x18, 0x00, 0x18, 0xCC, 0x00, 0x00, 0x44, 0xAA, 0x77, 0xFE, 0x00, // 224 to 239
    0x00, 0x00, 0x18, 0xDC, 0xFC, 0xC0, 0x6C, 0xC6, 0xD8, 0x60, 0x18, 0x18, 0x6C, 0x6C, 0x66, 0x00  // 240 to 255
},

//BYTE FontLine10[256] =     // Font Line 11
{
    0xFE, 0x18, 0xFC, 0x18, 0x10, 0x04, 0x3E, 0xFC, 0x1C, 0x1C, 0x10, 0x60, 0x78, 0x18, 0x00, 0x10, // 0 to 15
    0x80, 0x02, 0x18, 0x00, 0x1A, 0x0C, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 16 to 31
    0x00, 0x18, 0x00, 0x6C, 0x7C, 0xC6, 0x76, 0x00, 0x0C, 0x30, 0x00, 0x00, 0x18, 0x00, 0x18, 0x00, // 32 to 47
    0x7C, 0x7E, 0xFE, 0x7C, 0x1E, 0x7C, 0x7C, 0x30, 0x7C, 0x78, 0x00, 0x30, 0x06, 0x00, 0x60, 0x18, // 48 to 63
    0x7C, 0xC6, 0xFC, 0x3C, 0xF8, 0xFE, 0xF0, 0x3A, 0xC6, 0x3C, 0x78, 0xE6, 0xFE, 0xC6, 0xC6, 0x38, // 64 to 79
    0xF0, 0x0C, 0xE6, 0x7C, 0x3C, 0x7C, 0x10, 0x6C, 0xC6, 0x3C, 0xFE, 0x3C, 0x02, 0x3C, 0x00, 0x00, // 80 to 95
    0x00, 0x76, 0x7C, 0x7C, 0x76, 0x7C, 0xF0, 0x0C, 0xE6, 0x3C, 0x66, 0xE6, 0x3C, 0xC6, 0x66, 0x7C, // 96 to 111
    0x60, 0x0C, 0xF0, 0x7C, 0x1C, 0x76, 0x18, 0x6C, 0xC6, 0x06, 0xFE, 0x0E, 0x18, 0x70, 0x00, 0x00, // 112 to 127
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 128 to 143
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, // 144 to 159
    0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, // 160 to 175
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 176 to 191
    0x18, 0x18, 0x18, 0x36, 0x36, 0x18, 0x36, 0x36, 0x36, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, // 192 to 207
    0x18, 0x00, 0x18, 0x18, 0x36, 0x00, 0x36, 0x00, 0x36, 0x36, 0x00, 0x36, 0x00, 0x00, 0x18, 0x36, // 208 to 223
    0x00, 0x00, 0x18, 0x36, 0x36, 0x18, 0x00, 0x18, 0xC6, 0x00, 0x00, 0x11, 0x55, 0xDD, 0x00, 0x7E, // 224 to 239
    0x7E, 0x7E, 0x00, 0x76, 0xC0, 0xC0, 0x6C, 0xFE, 0x70, 0x60, 0x18, 0x7E, 0x38, 0xEE, 0x3C, 0x00  // 240 to 255
},

//BYTE FontLine11[256] =     // Font Line 12
{
    0xFE, 0x00, 0x00, 0x00, 0x90, 0x08, 0x02, 0x28, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, // 0 to 15
    0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 16 to 31
    0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, // 32 to 47
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 48 to 63
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 64 to 79
    0x00, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 80 to 95
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xCC, 0x00, 0x00, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, // 96 to 111
    0x60, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 112 to 127
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 128 to 143
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, // 144 to 159
    0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, // 160 to 175
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 176 to 191
    0x18, 0x18, 0x18, 0x36, 0x36, 0x18, 0x36, 0x36, 0x36, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, // 192 to 207
    0x18, 0x00, 0x18, 0x18, 0x36, 0x00, 0x36, 0x00, 0x36, 0x36, 0x00, 0x36, 0x00, 0x00, 0x18, 0x36, // 208 to 223
    0x00, 0x00, 0x18, 0x36, 0x36, 0x18, 0x00, 0x18, 0x00, 0x00, 0x00, 0x44, 0xAA, 0x77, 0x00, 0x00, // 224 to 239
    0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // 240 to 255
},

//BYTE FontLine12[256] =     // Font Line 13
{
    0xFE, 0x00, 0x00, 0x00, 0x60, 0x3E, 0x02, 0x28, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, // 0 to 15
    0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 16 to 31
    0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 32 to 47
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 48 to 63
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 64 to 79
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, // 80 to 95
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, // 96 to 111
    0xF0, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 112 to 127
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 128 to 143
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, // 144 to 159
    0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, // 160 to 175
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 176 to 191
    0x18, 0x18, 0x18, 0x36, 0x36, 0x18, 0x36, 0x36, 0x36, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, // 192 to 207
    0x18, 0x00, 0x18, 0x18, 0x36, 0x00, 0x36, 0x00, 0x36, 0x36, 0x00, 0x36, 0x00, 0x00, 0x18, 0x36, // 208 to 223
    0x00, 0x00, 0x18, 0x36, 0x36, 0x18, 0x00, 0x18, 0x00, 0x00, 0x00, 0x11, 0x55, 0xDD, 0x00, 0x00, // 224 to 239
    0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // 240 to 255
},

//BYTE FontLine13[256] =     // Font Line 14
{
    0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, // 0 to 15
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 16 to 31
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 32 to 47
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 48 to 63
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 64 to 79
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 80 to 95
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 96 to 111
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 112 to 127
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 128 to 143
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, // 144 to 159
    0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, // 160 to 175
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 176 to 191
    0x18, 0x18, 0x18, 0x36, 0x36, 0x18, 0x36, 0x36, 0x36, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, // 192 to 207
    0x18, 0x00, 0x18, 0x18, 0x36, 0x00, 0x36, 0x00, 0x36, 0x36, 0x00, 0x36, 0x00, 0x00, 0x18, 0x36, // 208 to 223
    0x00, 0x00, 0x18, 0x36, 0x36, 0x18, 0x00, 0x18, 0x00, 0x00, 0x00, 0x44, 0xAA, 0x77, 0x00, 0x00, // 224 to 239
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // 240 to 255
} };

BYTE VIDEO_MEMORY[2048];


#endif   // WIN32



