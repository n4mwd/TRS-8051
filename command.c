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
COMMAND.C - This is where most of the command line is processed.
*/


#include "bas51.h"

//XDATA WORD *Bytes = (BYTE *) TokBuf;         // Array of 16 BYTEs
//XDATA WORD *Word = (WORD *) (TokBuf + 16);  // Array of WORDs
//long L1, L2;


// Tests CurChar to see if it is a Terminal Char.
// Returns TRUE if CurChar is a terminal Char.

BIT TerminalChar(void)
{
    if (CurChar == '\r' || CurChar == TOKEN_COLON) return(TRUE);
    if (CurChar == TOKEN_ELSE)
    {
        SetStream51((WORD)(GetStreamAddr() + ReadStream51()));
        return(TRUE);
    }
    return(FALSE);
}


// Read next token into CurChar, return TRUE if terminal token, else FALSE.

BIT GetTerminalToken(void)
{
    if (!TerminalChar())   // Don't read if already terminal
    {
        CurChar = ReadStream51();   // Should be terminal Token
        return(TerminalChar());
    }
    return(TRUE);
}


// Evaluates a simple expression.
// Curchar is updated to the char the stopped the expression.
// On error or stack empty, return 0.
// Otherwise return the token resultant from the expression AND
// the value in uData.

BYTE GetSimpleExpr(void)
{
    BYTE Token;

    // process expression
    Token = 0;
    CurChar = Expression(FALSE);
    if (!StackEmpty(CALC_STACK))
	{
    	// Pop calc stack to uData
    	Token = PopStk(CALC_STACK);

        // stack should be empty now
        if (!StackEmpty(CALC_STACK) && SyntaxErrorCode == 0)
	        SyntaxErrorCode = ERROR_UNDECIFERABLE_EXPRESSION;
    }

    return(Token);
}

// Get a WORD parameter expression.
// Return 0 with SyntaxErrorCode set on error.

WORD GetWordParameter(void)
{
    BYTE Token;

    Token = GetSimpleExpr();
    if (Token != TOKEN_INTL_CONST && Token != TOKEN_FLOAT_CONST)
        SyntaxErrorCode = ERROR_TYPE_CONFLICT;
    if (SyntaxErrorCode) return(0);

    if (Token == TOKEN_INTL_CONST) return((WORD) uData.LVal);
    else return((WORD) uData.fVal);  // convert float to INTL
}


// Test uData of type Token for plausibility of being a valid line number.
// if Token is a FLOAT, convert to INTL.
// Return TRUE if uData is an invalid line number.

BIT ForceLineNumber(BYTE Token)
{
    if (Token == TOKEN_FLOAT_CONST)
    {
        uData.LVal = uData.fVal;
        Token = TOKEN_INTL_CONST;
    }

    // Check for proper type
    if (Token != TOKEN_INTL_CONST) goto Error;

    // If its within bounds then we are good
    if (uData.LVal > 0 && uData.LVal <= MAX_LINE_NUMBER) return(FALSE);

Error:
    SyntaxErrorCode = ERROR_INVALID_LINE_NUMBER;    
    return(TRUE);
}



// Read a sequence of line numbers into the wArg[] array.
// Line numbers must be separated by a Separator token.
// Each line number is stored in order in the wArg[] array starting at wArg[0].
// Missing Line numbers are stored as 0xFFFF in the wArg[] array.
// Returns the number of valid line numbers found minus 1.
// On return, CurChar will be a terminal char if successful.
// On entry, the stream points to the first line number.
// Variables and expressions are not allowed.
// If Token found is unary minus, it is promoted to plain minus.
// Returns -1 is unrecognized parameter or separator is found.
// Max of 3 line numbers accepted.

BYTE GetLineNumberList(BYTE Separator)
{
    BYTE Count;

    // Read Line and fill in parameters until a terminal char is found
    wArg[0] = wArg[1] = wArg[2] = 0xFFFF;    // initialize
    Count = 0;
    while (1)
    {
        CurChar = GetNextToken();
        if (TerminalChar()) break;

        // Special case for unary minus
        if (CurChar == TOKEN_UNARY_MINUS) CurChar = TOKEN_MINUS;

        if (CurChar == Separator) // Is it a separator
        {
            Count++;     // time to fill in the next word
            if (Count == 3) return(0xFF);  // too many args
        }
        else  // not a separator, should be a line number
        {
            if (ForceLineNumber(CurChar))   // Error if not a line number
            {
			    SyntaxErrorCode = ERROR_INVALID_LINE_NUMBER;
                return(0xFF);
            }
            wArg[Count] = (WORD) uData.LVal;
        }
    }  // end while()

    return(Count);

}


// Get a line range from the command line
// Used by LIST and DELETE and uses the following syntax rules:
// <Line Range>       ::= <Line-Num>
//                     | <Line-Num> '-'
//                     | '-' <Line-Num>
//                     | <Line-Num> '-' <Line-Num>
//
// First Line Num returned in Word[0] and second in Word[1].

void GetLineRange(void)
{
    BYTE Count;

    Count =  GetLineNumberList(TOKEN_MINUS);
    if (Count > 1) goto Error;    // too many

    // correct defaults
    if (wArg[0] == 0xFFFF)
    {
    	wArg[0] = 0;
        Count++;      // force dash
    }

    if (wArg[1] == 0xFFFF)
    {
        if (Count)     // found dash
         	wArg[1] = MAX_LINE_NUMBER;
        else
         	wArg[1] = wArg[0];   // no dash
    }
    return;

Error:
    if (SyntaxErrorCode == 0) SyntaxErrorCode = ERROR_NOT_ALLOWED_HERE;
    return;
}






// Process DIM statement
// Return TRUE of error.

BIT DoDimCmd(void)
{
    while (!TerminalChar())
    {
        CurChar = Expression(EXPRESSION_DIM_FLAG); // evaluate the DIM expression
    }

    // The calculator stack should be empty after a DIM statement.
    if (!StackEmpty(CALC_STACK)) return(TRUE);
    return(FALSE);
}






void DoPauseCmd(void)
{
    WORD Ms;

    Ms = GetWordParameter();
    if (!TerminalChar()) goto Error;

    SysWait(Ms);

    return;

Error:
	SyntaxErrorCode = ERROR_SYNTAX;
    return;

}


BIT DoListCmd(void)
{
    GetLineRange();
    if (SyntaxErrorCode) return(TRUE);  // return if error
    wArg[2] = GetStreamAddr();  // save stream pointer b/c List messes it up
    ListProgram(wArg[0], wArg[1]);
    SetStream51(wArg[2]);   // retore command pointer
    VGA_print("\n");

    return(FALSE);
}


BIT DoAutoCmd(void)
{
    BYTE Count;

    // AUTO {<Line-Num> {,uint}}
    AutoLine = 10;
    AutoIncrement = 10;

    wArg[0] = wArg[1] = 0xFFFF;   // defaults
    Count = GetLineNumberList(TOKEN_COMMA);
    if (Count >= 2) return(TRUE);
    if (wArg[0] != 0xFFFF) AutoLine = wArg[0];
    if (wArg[1] != 0xFFFF) AutoIncrement = wArg[1];

    return(FALSE);
}


// EDIT <Line-Num>

BIT DoEditCmd(void)
{
    Running = FALSE;

    CurChar = GetNextToken();
    if (ForceLineNumber(CurChar))    // Line number mandatory
    {
Error:    
		SyntaxErrorCode = ERROR_INVALID_LINE_NUMBER;
        return(TRUE);     // bad line number
    }

    EditLinePtr = FindLinePtr((WORD) uData.LVal, TRUE);
    if (EditLinePtr == 0) goto Error;

    // get terminal token and return
    if (GetTerminalToken()) return(FALSE);
    else return(TRUE);   // extra junk on line

}


BIT DoRunCmd(void)
{
    CurChar = GetNextToken();
    if (TerminalChar()) wArg[0] = BasicVars.ProgStart;
    else if (ForceLineNumber(CurChar))
    {
Error:
		SyntaxErrorCode = ERROR_INVALID_LINE_NUMBER;
        return(TRUE);     // bad line number
    }
    else
    {
        // Get pointer to first line that is >= line number.
        wArg[0] = FindLinePtr((WORD) uData.LVal, FALSE);
        if (wArg[0] == NULL) goto Error;     // line number not found
    }

    if (!GetTerminalToken()) return(TRUE);   // Extra junk on line

    // wArg[0] has line address if here
    SetStream51(wArg[0]);
    ClearVariables();
	Running=TRUE;

    return(FALSE);
}

BIT DoEndCmd(void)
{
    if (!GetTerminalToken()) return(TRUE);  // no parameters allowed here
    SetStream51(0xFFFF);
	Running=FALSE;
    return(FALSE);
}


BIT DoStopCmd(void)
{
    if (!GetTerminalToken()) return(TRUE);  // no parameters allowed here
    StopProg();

    return(FALSE);
}


BIT DoContCmd(void)
{
    if (!GetTerminalToken()) return(TRUE);  // no parameters allowed here
	Running=TRUE;
    SetStream51(StopLinePtr);   // Restore
    CurChar = TOKEN_COLON;

    return(FALSE);
}

BIT DoDeleteCmd(void)
{
    GetLineRange();
    if (SyntaxErrorCode) return(TRUE);  // return if error
    DeleteLineRange(wArg[0], wArg[1]);
    VGA_print("\n");

    return(FALSE);
}

BIT DoErrorCmd(void)
{
    BYTE Tok;

	Tok = GetSimpleExpr();
    if (Tok == TOKEN_FLOAT_CONST)
    {
        uData.LVal = uData.fVal;
        Tok = TOKEN_INTL_CONST;
    }
	if (Tok == TOKEN_INTL_CONST) SyntaxErrorCode = (BYTE) uData.LVal;
    else SyntaxErrorCode = ERROR_TYPE_CONFLICT;

//    GetTerminalToken();
    return((BIT)(SyntaxErrorCode != ERROR_NONE));
}


// Set the default type for variables that begin with certain letters.
// Actual token is in CurChar
// DEFTYPE <Letter>{-Letter}{,Letter {-Letter}}*

BIT DoDefTypesCmd(void)
{
    BYTE LetIdx, Letter[2];
    BYTE DefType;

    // Save the requested type
    if (CurChar == TOKEN_DEFDBL) CurChar = TOKEN_DEFSNG;  // we don't do DBL's
    DefType = (BYTE)(CurChar - TOKEN_DEFDBL);

    LetIdx = 0;
    Letter[0] = Letter[1] = 99;

    while (1)
    {
        CurChar = GetNextToken();

        if (CurChar == TOKEN_NOTYPE_VAR)  // Letters are encoded as variables
        {
            // Save the letter
            if (Letter[LetIdx] == 99) Letter[LetIdx] = (BYTE) uHash.str.First;
            else goto Error;   // Multiple assignments
        }
        else if (CurChar == TOKEN_MINUS)
        {
            // Advance index variable
            if (LetIdx == 0) LetIdx = 1;
            else goto Error;
        }
        else if (CurChar == TOKEN_COMMA || TerminalChar())
        {
            // Check for errors
            if (Letter[1] == 99) Letter[1] = Letter[0];
            if (Letter[0] == 99 || Letter[0] > Letter[1]) goto Error;

            // Assign variable letters to DefTypes table
            while (Letter[0] <= Letter[1])
            {
                DefTypes[Letter[0]++] = DefType;
            }

            // exit if terminal char
            if (TerminalChar()) break;

		    LetIdx = 0;
            Letter[0] = Letter[1] = 99;
        }
        else
        {
Error:
            SyntaxErrorCode = ERROR_SYNTAX;
            return(TRUE);
        }


    }


    return(FALSE);
}




/*
// Enums for Operator Tokens (starts at 128)
enum {TOKEN_AND=OPERATOR_TOKEN_START, TOKEN_NOT, TOKEN_MOD, TOKEN_OR,
      TOKEN_LESS_THAN_OR_EQUAL, TOKEN_GREATER_THAN_OR_EQUAL, TOKEN_NOT_EQUAL,
      TOKEN_EQUALS, TOKEN_GREATER_THAN, TOKEN_LESS_THAN, TOKEN_PLUS,
      TOKEN_CONCAT, TOKEN_MINUS, TOKEN_UNARY_MINUS, TOKEN_MULTIPLY,
      TOKEN_DIVIDE, TOKEN_POWER, TOKEN_COLON, TOKEN_COMMA, TOKEN_SEMICOLON,
      TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN, LAST_OPERATOR_TOKEN };


      // Enums for function tokens (starts at 160)

enum {TOKEN_ABS=FUNCTION_TOKEN_START, TOKEN_ATN, TOKEN_COS, TOKEN_EXP,
      TOKEN_LOG, TOKEN_SIN, TOKEN_SQR, TOKEN_TAN, TOKEN_INT, TOKEN_LEN,
      TOKEN_PEEK, TOKEN_POINT, TOKEN_RND, TOKEN_SGN, TOKEN_USR, TOKEN_VAL,
      TOKEN_ASC, TOKEN_CHR, TOKEN_LEFT, TOKEN_RIGHT, TOKEN_MID, TOKEN_STR,
      TOKEN_STRING, LAST_FUNCTION_TOKEN };


// Enums for command tokens (starts at 192)

enum {TOKEN_BEGIN=COMMAND_TOKEN_START, TOKEN_ELSE, TOKEN_STEP, TOKEN_THEN,
      TOKEN_TO, TOKEN_AUTO, TOKEN_CLEAR, TOKEN_CLS, TOKEN_CONT, TOKEN_DATA,
      TOKEN_DELETE, TOKEN_DIM, TOKEN_EDIT, TOKEN_ELSEIF, TOKEN_ENDIF, TOKEN_END,
      TOKEN_ERROR, TOKEN_FOR, TOKEN_GOSUB, TOKEN_GOTO, TOKEN_IF, TOKEN_INPUT,
      TOKEN_IN, TOKEN_LET, TOKEN_LIST, TOKEN_NEW, TOKEN_NEXT, TOKEN_ON,
      TOKEN_OUT, TOKEN_PAUSE, TOKEN_PLOT, TOKEN_PRINT, TOKEN_PRINT2, TOKEN_READ,
      TOKEN_REM, TOKEN_REM2, TOKEN_RENUM, TOKEN_RESTORE, TOKEN_RESUME,
      TOKEN_RETURN, TOKEN_RUN, TOKEN_STOP, TOKEN_UNPLOT, LAST_COMMAND_TOKEN };

// Variables and constants
// We Only support int and float variables and arrays and only float constants.

enum {TOKEN_UINT_CONST=LAST_COMMAND_TOKEN, TOKEN_INT_CONST, TOKEN_FLOAT_CONST,
      TOKEN_STRING_CONST, TOKEN_UINT_VAR, TOKEN_INT_VAR, TOKEN_FLOAT_VAR,
      TOKEN_STRING_VAR, TOKEN_UINT_ARRAY, TOKEN_INT_ARRAY, TOKEN_FLOAT_ARRAY,
      TOKEN_STRING_ARRAY };



*/

// Execute next command token in stream.
// The stream points to the actual command token and not the start of the line.
// Returns with CurChar set to terminal token that ended the command.
// This is '\r' for the end of line or ':' for a subcommand or 0 for an error.

void ExecuteCommand(void)
{
    CurChar = ReadStream51();
    switch (CurChar)
    {
        // Terminal Tokens
        case TOKEN_COLON:     // command separator
        case '\r':    // empty line
            break;    // Curchar has terminal token

        case TOKEN_LIST:
        	if (DoListCmd()) goto Error;
            break;

        case TOKEN_CLS:
            VGA_ClrScrn();
            // Make sure there are no parameters
            if (!GetTerminalToken()) goto Error;
            break;

        case TOKEN_AUTO:
            if (DoAutoCmd()) goto Error;
            break;

        case TOKEN_EDIT:
            if (DoEditCmd()) goto Error;
            break;

        case TOKEN_PRINT:
        case TOKEN_PRINT2:
            if (DoPrintCmd()) goto Error;
            break;

#ifdef __WIN32__
        case TOKEN_EXIT:
            VGA_print("\nAre you sure you want to exit? (Y/N)\n");
            if (toupper(GetKeyWord()) != 'Y') break;
            exit(0);
#endif

        case TOKEN_LET:
            CurChar = Expression(EXPRESSION_ASSIGNMENT_FLAG);    // assignment expression
            if (!StackEmpty(CALC_STACK)) goto Error;
            if (!GetTerminalToken()) goto Error;
            break;

        case TOKEN_FOR:
            if (DoForCmd()) goto Error;
            break;

        case TOKEN_NEXT:
            if (DoNextCmd()) goto Error;
            break;

        case TOKEN_RUN:
            if (DoRunCmd()) goto Error;
            break;

        case TOKEN_CONT:
            if(DoContCmd()) goto Error;
            break;

        case TOKEN_END:
            if (DoEndCmd()) goto Error;
            break;

        case TOKEN_STOP:
            if (DoStopCmd()) goto Error;
            break;

        case TOKEN_NEW:
            Running = FALSE;
            ClearEverything();
            break;

        case TOKEN_GOTO:
            // jump to new line number
            if (DoGotoCmd(FALSE)) goto Error;  
            break;

        case TOKEN_GOSUB:
            // Jump to a new line and save return address on stack
            if (DoGotoCmd(TRUE)) goto Error;
            break;

        case TOKEN_RETURN:
            uData.wVal[0] = PopGosub();
            uData.wVal[1] = gReturn.Line;
            GotoHelper(FALSE);
            break;

        case TOKEN_REM:
        case TOKEN_REM2:
            // ignore and skip to end of line
            while ((CurChar = ReadStream51()) != TOKEN_CR);
            break;

        case TOKEN_CLEAR:
            if (!GetTerminalToken()) goto Error;
            ClearVariables();
            break;

        case TOKEN_DELETE:     // DELETE <line>-<Line>
            if (DoDeleteCmd()) goto Error;
            break;

        // IF <expression> THEN <clause> [ELSE <clause>]
        // IF <expression> GOTO <line> [ELSE <clause>]
        // IF_TOKEN <offset to ELSE> ... ELSE_TOKEN <offset to EOL>
        case TOKEN_IF:
            if (DoIfCmd()) goto Error;
            break;


        // RENUM [OldNum] [,[NewNum] [,Increment]
        case TOKEN_RENUM:
            DoRenumCmd();
            break;

        case TOKEN_DIM:
            if (DoDimCmd()) goto Error;
            break;

        case TOKEN_INPUT:
            if (DoInputCmd(0)) goto Error;
            break;

        case TOKEN_LINE:   // for line input command
            if (DoLineInputCmd()) goto Error;
            break;    

        case TOKEN_READ_DATA:
            if (InputHelper(2)) goto Error;
            break;

        // These commands are ignored
        case TOKEN_OUT:
        case TOKEN_POKE:
        case TOKEN_DATA:
            while (1)
            {
                // Scan to terminal token
                CurChar = GetNextToken();
                if (TerminalChar()) break;
            }
            break;

        case TOKEN_RESTORE:
            if (DoRestoreCmd()) goto Error;
            break;

        case TOKEN_PLOT:
            DoPlotCmd(TRUE);
            break;

        case TOKEN_UNPLOT:
            DoPlotCmd(FALSE);
            break;

        case TOKEN_PAUSE:    // PAUSE x - delay x milliseconds
            DoPauseCmd();
            break;

        case TOKEN_LISTFILES:
            DoListFilesCmd();
            break;

        case TOKEN_LOADFILE:
            if (DoLoadFileCmd()) goto Error;
            break;

        case TOKEN_SAVEFILE:
            if (DoSaveFileCmd()) goto Error;
            break;

        case TOKEN_ON:
            if (DoOnCmd()) goto Error;
            break;

        case TOKEN_RESUME:
        	if (DoResumeCmd()) goto Error;
            break;

        case TOKEN_ERROR:
        	if (DoErrorCmd()) goto Error;
            break;

        // Def Types
        case TOKEN_DEFDBL:
        case TOKEN_DEFINT:
        case TOKEN_DEFSNG:
        case TOKEN_DEFSTR:
            if (DoDefTypesCmd()) goto Error;   // token is in CurChar
            break;


        default:
            SyntaxErrorCode = ERROR_BAD_BAS51_COMMAND;
            break;

// Sub-commmands
//        case TOKEN_BEGIN:     	// IF - BEGIN sub-command
//        case TOKEN_THEN:			// IF - THEN - ELSE sub-command
//        case TOKEN_ELSE:     	// IF - THEN - ELSE sub-command
//        case TOKEN_TO: 			// FOR - TO - STEP sub-command
//        case TOKEN_STEP:			// FOR - TO - STEP sub-command
    }

     return;

Error:
     if (SyntaxErrorCode == 0) SyntaxErrorCode = ERROR_SYNTAX;
     return;
}


// Execute the line in memory pointed to by LinePtr
// Return LinePtr to next line to be executed.
// If LinePtr == 0xFFFF, then execute single line from InBuf[]
// LinePtr cannot be NULL.
// Returns zero on syntax error

void ExecuteLine(void)
{
    //SetStream51(ExecLinePtr);

    if (CurChar == '\r')
    {
        // Get line number
        LineNo = ReadStreamWord();

	    // Read over length
    	ReadStream51();
    }


    FreeTempAlloc();  // clear the temporary
    BasicVars.CurCmdAddr = GetStreamAddr();
    ExecuteCommand();

    if (SyntaxErrorCode != ERROR_NONE)
    {
        BasicVars.LastErrorCode = SyntaxErrorCode;
        BasicVars.LastErrorLine = LineNo;

        if (BasicVars.OnErrorLine &&  // error traping enabled
        	!BasicVars.ResumeAddr)    // Not already in an error routine
        {
        	// Recursive errors not allowed
            SyntaxErrorCode = ERROR_NONE;
        	BasicVars.ResumeAddr = BasicVars.CurCmdAddr;
            // Goto Error handler
            uData.wVal[1] = BasicVars.OnErrorLine;
            uData.wVal[0] = (WORD)(FindLinePtr(uData.wVal[1], FALSE) + 3);  // Get Line address
            GotoHelper(FALSE);
        }
        else
        {
            PrintErrorCode();
            Running = FALSE;
        }
    }

    if (BreakFlag || CheckBreak()) StopProg();

    // Check to see if that was the last line in the program
    if (CurChar == '\r' && GetStreamAddr() >= BasicVars.VarStart - 1)
        Running = FALSE;
}




