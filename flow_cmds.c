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
FLOW_CMDS.C - Various routines to handle the flow between BASIC statements.
*/

#include "bas51.h"


// Get the return address for the next statement.
// Curchar must be a terminal char.  Stream address points to character after
// terminal char.  If terminal char is a '\r', the return address is advanced
// by 3 bytes to account for the line number and length bytes.  If the
// resulting address is not in the program file, then the function returns
// NULL, otherwise it returns the proper return address.
// Returns the RETURN line number in Word[2].
// If a program jumps to address 0, it should quietly return to the command line.

WORD GetReturnAddress(void)
{
    WORD addr;

    addr = GetStreamAddr();

    if (CurChar == '\r')
    {
        gReturn.Line = ReadRandomWord(addr);  // Read line number
        addr += (WORD) 3;   // skip line number and length

        // Check to make sure it isn't the return off the cmdline
        if (addr > BasicVars.CmdLine) addr = 0;
    }
    else gReturn.Line = LineNo;

    // Check for bounds
    if (addr >= BasicVars.VarStart &&
        addr < BasicVars.CmdLine) addr = 0;

    gReturn.ReturnPtr = addr;

    return(addr);
}


// Jump to a new line by address
// Target address in uData.wVal[0], New Line # in uData.wVal[1]

void GotoHelper(BIT GosubFlag)
{
    if (GosubFlag)   // we need to push the return address
    {
        // Curchar must be on a terminal token
        GetReturnAddress();  // Fill out Gosub Structure w/return Addr&Line
        if (PushStruct(TOKEN_GOSUB))
        {
            SyntaxErrorCode = ERROR_SYNTAX;
            return;
        }
    }

    if (uData.wVal[0])
    {
        SetStream51(uData.wVal[0]);
        CurChar = TOKEN_COLON;
        LineNo = uData.wVal[1];
    }
    else  // null pointer
    {
        Running = FALSE;
    }
}


// Read Goto Line number and jump to new line
// If GosubFlag is TRUE, then the return address is also
// pushed to the FOR_GOSUB stack.
// Return TRUE on Error.

BIT DoGotoCmd(BIT GosubFlag)
{
    BYTE Token;

    Token = GetSimpleExpr();    // Get the line number
    // Line number now in uData

    // Cannot work with no parameters or strings
    if (Token == 0 || !TerminalChar())
        return(TRUE);

    // Check for proper line number
    if (ForceLineNumber(Token)) return(TRUE);

    // Do Jump
    uData.wVal[1] = (WORD) uData.LVal;   // Line number
    uData.wVal[0] = (WORD)(FindLinePtr(uData.wVal[1], FALSE) + 3);  // Get Line address
    GotoHelper(GosubFlag);

    return(FALSE);
}


// FOR_HELPER function
// Called by the For statement function.
// Returns TRUE if a syntax error occurs, otherwise uData has the next
// value in it.

BIT ForHelper(BYTE ExpectedToken)
{
    BYTE Token;

	// Make sure next token is expected Token
    if (CurChar != ExpectedToken) return(TRUE);

	// Convert expression into value
    Token = GetSimpleExpr();
    if (!Token) return(TRUE);
//    CurChar = Expression(FALSE);

    // Pop calc stack to uData
  //  Token = PopStk(CALC_STACK);

    // If not same type as VarType, convert it or generate error
    if (Token + 4 == gFor.VarType) return(FALSE);  // perfect match
    if (Token != TOKEN_FLOAT_CONST && Token != TOKEN_INTL_CONST)
    	return(TRUE);  // incompatable

    // We know they are different, but legal if here
    if (Token == TOKEN_FLOAT_CONST) uData.LVal = uData.fVal;
    else uData.fVal = uData.LVal;

    return(FALSE);

}

// FOR Statement
// Create FOR_DESCRIPTOR global and push to FOR_GOSUB stack

BIT DoForCmd(void)
{
    // Get next token and make sure its a simple FLOAT or INTL
    CurChar = ReadStream51();
    if (CurChar < TOKEN_NOTYPE_VAR || CurChar > TOKEN_FLOAT_VAR) return(TRUE);

    // Get pointer to variable and save in CtrlVarAddr
    gFor.CtrlVarPtr = GetProgVarPtr(VARPTR_ASSIGN_SIMPLE);

    // Get corrected type and hash
    if (uHash.str.TypeFlag == 0)     // untyped, use default
        uHash.str.TypeFlag = DefTypes[uHash.str.First];
    gFor.VarType = (BYTE)(TOKEN_NOTYPE_VAR + uHash.str.TypeFlag);
    if (gFor.VarType == TOKEN_STRING_VAR) return(TRUE);

    gFor.Hash = uHash;   // save hash for searching

    // Make sure next token is TOKEN_EQUALS and get value in uData
    CurChar = ReadStream51();
    if(ForHelper(TOKEN_EQUALS)) return(TRUE); // Exit on error
    WriteVar(gFor.VarType, gFor.CtrlVarPtr);   // Write uData to CtrlVarPtr

    // Make sure next token is TOKEN_TO and get value in uData
    if(ForHelper(TOKEN_TO)) return(TRUE); // Exit on error
    gFor.ToField = uData;   // Copy uData to ToField

    // Set uData to 1
    if (gFor.VarType == TOKEN_FLOAT_VAR) uData.fVal = 1.0;
    else uData.LVal = 1;

    if (!TerminalChar())
    {
	    // Make sure next token is TOKEN_STEP and get value in uData
    	if(ForHelper(TOKEN_STEP)) return(TRUE); // Exit on error
    }

    gFor.StepField = uData;   // Copy uData to StepField

    if (!TerminalChar()) return(TRUE);   // illegal junk on line

    // Save current address in LoopCmdAddr
    GetReturnAddress();
    gFor.LoopCmdAddr = gReturn.ReturnPtr;
    gFor.Line = gReturn.Line;

    return(PushStruct(TOKEN_FOR)); // Push to FOR_GOSUB stack
}

// Next Helper function
// This function requires that gFor be valid for the current NEXT statement.
// Returns TRUE if jump taken, else FALSE.

BIT NextHelper(void)
{
    BIT Jump;

    // read variable
  	ReadBlock51((BYTE *) &uData, gFor.CtrlVarPtr, sizeof(UVAL_DATA));

    Jump = FALSE;
    if (gFor.VarType == TOKEN_FLOAT_VAR)
    {
        uData.fVal += gFor.StepField.fVal;   // increment control variable

        if (gFor.StepField.fVal > 0)  // positive test - 1 TO 100
		{
         	if (uData.fVal <= gFor.ToField.fVal) Jump = TRUE;
        }
        else   // negative test - 100 TO 1 STEP -1
        {
         	if (uData.fVal >= gFor.ToField.fVal) Jump = TRUE;
        }
    }
    else    // INTL version
    {
        uData.LVal += gFor.StepField.LVal;   // increment control variable

        if (gFor.StepField.LVal > 0)  // positive test - 1 TO 100
		{
         	if (uData.LVal <= gFor.ToField.LVal) Jump = TRUE;
        }
        else   // negative test - 100 TO 1 STEP -1
        {
         	if (uData.LVal >= gFor.ToField.LVal) Jump = TRUE;
        }
    }

    // Write updated variable
  	WriteBlock51(gFor.CtrlVarPtr, (BYTE *) &uData, sizeof(UVAL_DATA));

    // Jump back
    if (Jump)
    {
    	uData.wVal[0] = gFor.LoopCmdAddr;
        uData.wVal[1] = gFor.Line;
        GotoHelper(FALSE);
    }
    else
    {
        gFor.CtrlVarPtr = 0;   // mark as invalid, jump not taken
        // chop stack
        BasicVars.GosubStackTop -= (WORD)(sizeof(FOR_DESCRIPTOR) + 1);
    }

    return(Jump);
}

// NEXT STATEMENT

BIT DoNextCmd(void)
{
    if (GetTerminalToken())   // no parameters
    {
        uHash.d = 0;     // find first available FOR structure
        if (GetFor()) return(TRUE);
        NextHelper();
        return(FALSE);
    }

    // We have parameters if here
    while (1)
    {
        // must be INTL or FLOAT var
        if (CurChar < TOKEN_NOTYPE_VAR && CurChar > TOKEN_FLOAT_VAR) return(TRUE);

        ReadProgVarHash();  // Get hash into uHash
        if (uHash.str.TypeFlag == 3) return(TRUE);  // stringa not allowed

        // Get FOR structure
        if (GetFor()) return(TRUE);   // Error if FOR variable not found

        // process FOR-NEXT
        if (NextHelper()) break;   // Jump taken, process no more variables

        // get next token after variable hash, and exit loop if terminal
        if (GetTerminalToken()) break;

        // look for comma - error if not found
        if (CurChar != TOKEN_COMMA) return(TRUE);

        // get next token after comma
        if (GetTerminalToken()) return(TRUE);  // error if terminal now
    }

    return(FALSE);

}


void StopProg(void)
{
    Running = FALSE;
    StopLinePtr = GetStreamAddr();   // Save current line pointer
    if (CurChar == '\r') StopLinePtr += (WORD) 3;
    // Force line execution to start exactly on StopLinePtr;
    CurChar = TOKEN_COLON;
    if (LineNo == 0xFFFF) VGA_print("Break\n");
    else VGA_printf("Break in Line %d\n", LineNo);
    BreakFlag = FALSE;
}

BIT DoOnCmd(void)
{
    // Peek to see if the next token is TOKEN_ERROR
    CurChar = PeekStream51();
    if (CurChar == TOKEN_ERROR)
    {
        ReadStream51();   // read ERROR token for real
        if (ReadStream51() != TOKEN_GOTO) return(TRUE);  // must be GOTO
        CurChar = GetNextToken();
        if (CurChar != TOKEN_INTL_CONST) return(TRUE);   // must be INTL line#
        BasicVars.OnErrorLine = (WORD) uData.LVal;
    }
    else   // either ON <exp> GOTO or GOSUB
    {
        BYTE Token;
        BYTE JumpPos;
        BIT GosubFlag;

        // Get expression value
        Token = GetSimpleExpr();
        if (Token == TOKEN_FLOAT_CONST)
        {
            // convert to Int
            Token = TOKEN_INTL_CONST;
            uData.LVal = uData.fVal;
        }
        if (Token != TOKEN_INTL_CONST) return(TRUE); // expr must be numeric

        // CurChar must be either GOTO or GOSUB
        if (CurChar != TOKEN_GOTO && CurChar != TOKEN_GOSUB) return(TRUE);
        GosubFlag = (BIT)(CurChar == TOKEN_GOSUB);

        JumpPos = 0;
        if (uData.LVal > 0 || uData.LVal < 256)
            JumpPos = (BYTE) uData.LVal;   // Save jump position

        gReturn.ReturnPtr = 0;
        while (1)
        {
            CurChar = GetNextToken();
            if (CurChar != TOKEN_INTL_CONST) return(TRUE);  // not a line number

            JumpPos--;
            if (JumpPos == 0)
            {
                // Store line data in gReturn because we can't jump right now
                gReturn.Line = (WORD) uData.LVal;
                gReturn.ReturnPtr = (WORD)(FindLinePtr(gReturn.Line, FALSE) + 3);  // Get Line address
            }

            CurChar = ReadStream51();
            if (TerminalChar()) break;
            else if (CurChar != TOKEN_COMMA) return(TRUE);  // must have comma separator
        }

        if (gReturn.ReturnPtr)   // we are going somewhere
        {
            uData.wVal[0] = gReturn.ReturnPtr;
            uData.wVal[1] = gReturn.Line;
            GotoHelper(GosubFlag);
        }
    }


    return(FALSE);


}


BIT DoResumeCmd(void)
{
    if (BasicVars.ResumeAddr == 0)
    {
        SyntaxErrorCode = ERROR_RESUME_NO_ERROR;
        return(TRUE);
    }

	CurChar = GetNextToken();
    if (TerminalChar())    // No parameters - Resume to cmd with error
    {
    	uData.wVal[1] = BasicVars.LastErrorLine;
    	uData.wVal[0] = BasicVars.ResumeAddr;
    }
    else if (CurChar == TOKEN_NEXT)   // NEXT - Resume to cmd after error
    {
    	SetStream51(BasicVars.ResumeAddr);  // Set to current cmd
        while (!TerminalChar()) CurChar = GetNextToken();  // loop until next cmd
        if (CurChar == '\r')
        {
            BasicVars.LastErrorLine = ReadStreamWord();   // Get line number
        	ReadStream51();    // Read over length
        }
    	uData.wVal[1] = BasicVars.LastErrorLine;
    	uData.wVal[0] = GetStreamAddr();
    }
    else if (CurChar == TOKEN_INTL_CONST)  // Line number - resume to line#
    {
        uData.wVal[1] = (WORD) uData.LVal;
	    uData.wVal[0] = (WORD)(FindLinePtr(uData.wVal[1], FALSE) + 3);  // Get Line address
    }
    else
    {
        SyntaxErrorCode = ERROR_SYNTAX;
    	return(TRUE);
    }

    // Resume from Error handler
	BasicVars.ResumeAddr = 0;
    GotoHelper(FALSE);

	return(FALSE);
}



BIT DoIfCmd(void)
{
    // save false address
    wArg[2] = (WORD)(GetStreamAddr() + ReadStream51());

    GetSimpleExpr();  // CurChar is token that stopped the scan
    if (uData.LVal == 0)   // Make FALSE jump to ELSE or end of line
    {
        SetStream51(wArg[2]);
        CurChar = TOKEN_COLON;
        if (ReadRandom51(wArg[2]) == TOKEN_INTL_CONST)  // implied GOTO
        {
            if (DoGotoCmd(FALSE)) return(TRUE);
        }
    }
    else  // look for THEN or GOTO
    {
        if (CurChar == TOKEN_GOTO)
        {
            if (DoGotoCmd(FALSE)) return(TRUE);
        }
        else if (CurChar == TOKEN_THEN)
        {
            // peek at token
            if (ReadRandom51(GetStreamAddr()) == TOKEN_INTL_CONST)
            {
                if (DoGotoCmd(FALSE)) return(TRUE);
            }
            else CurChar = TOKEN_COLON;

        }
        else return(TRUE);  // must be THEN or GOTO
    }

    return(FALSE);
}


