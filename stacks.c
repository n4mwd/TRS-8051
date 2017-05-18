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
STACKS.C - Stack management for the interpreter.
*/

#include "bas51.h"


//#define CalcStack InBuf     // share with InBuf[]
BYTE CalcStack[25 * 5];
BYTE OperatorStack[48];
BYTE ArgStack[16];
BYTE ParmStack[16];
signed char StackTops[4];
CODE BYTE StackSizes[4] =    // table saves a bunch of code
{
	sizeof(CalcStack) / 5,
    sizeof(OperatorStack),
    sizeof(ArgStack),
    sizeof(ParmStack)
};
CODE BYTE *StackPtrs[4] = { CalcStack, OperatorStack, ArgStack, ParmStack };


// Initialize All Stacks

void InitStacks(void)
{
    memset(StackTops, -1, sizeof(StackTops));
}


// Test to see if specified stack is empty.
// Return TRUE if it is, else FALSE.
// No bounds checking on 'Stack' parameter is done.

BIT StackEmpty(BYTE Stack)
{
    return((BIT)(StackTops[Stack] == -1));
}


// Push  a token to the specified stack.
// No bounds checking is done on the Stack parameter.
// Special Case for CALC_STACK.

void PushStk(BYTE Stack, BYTE Token)
{
    BYTE Top;

    // check for possible overflow
    if (StackTops[Stack] >= StackSizes[Stack] - 1)
    {
        SyntaxErrorCode = ERROR_EXPRESSION_STACK_OVERFLOW;
        return;
    }

    StackTops[Stack]++;

    // special case for calculator stack
    if (Stack == CALC_STACK)
    {
        // Note that StackTop for the calculator stack is divided by 5
        // This means that the max number of values pushed is 256/5=51

        Top = (BYTE)(StackTops[Stack] * 5);  // calc real stackTop

        // 4 bytes of Data to be pushed is in uData global
        memcpy(&CalcStack[Top], &uData, sizeof(UVAL_DATA));

        // Push Token Last
    	CalcStack[Top + 4] = Token;

    }
    else  // all other stacks
    {
        // Just push it
        StackPtrs[Stack][StackTops[Stack]] = Token;
    }
    return;
}


// Pop a token from the specified stack.
// No bounds checking is done on the Stack parameter.
// Special Case for CALC_STACK.
// Returns TOKEN_STACK_ERROR, on stack underflow.

BYTE PopStk(BYTE Stack)
{
    BYTE Top, Token;

    // Check for empty stack
    if (StackTops[Stack] == (signed char) -1)
    {
        SyntaxErrorCode = ERROR_EXPRESSION_STACK_UNDERFLOW;
        return(TOKEN_STACK_ERROR);
    }

    if (Stack == CALC_STACK)   // Special case
    {
        // Note that StackTop for the calculator stack is divided by 5
        // This means that the max number of values pushed is 256/5=51

        Top = (BYTE)(StackTops[Stack] * 5);  // calc real stackTop

        // Copy 1st 4 bytes into uVal global
        memcpy(&uData, &CalcStack[Top], sizeof(UVAL_DATA));

        // Copy Token
    	Token = CalcStack[Top + 4];
    }
    else
    {
        Token = StackPtrs[Stack][StackTops[Stack]];
        StackPtrs[Stack][StackTops[Stack]] = 0;   // just for debugging
    }
    StackTops[Stack]--;

    return(Token);
}


// Return a token from the top of the specified stack.
// No bounds checking is done on the Stack parameter.
// Special Case for CALC_STACK.
// Returns 0 if stack empty.

BYTE PeekStk(BYTE Stack)
{
    BYTE Top;

    // Check for empty stack
    if (StackEmpty(Stack)) return(TOKEN_STACK_ERROR);

    Top = StackTops[Stack];
    if (Stack == CALC_STACK) return(CalcStack[Top * 5 + 4]);  // Special case

    return(StackPtrs[Stack][Top]);
}





// =========================================
// FOR_GOSUB Stack
// The FOR_GOSUB stack holds return addresses for the GOSUB statements and
// the FOR_LOOP structures.
// =========================================

// Search the FOR_GOSUB stack for the FOR struct that uses value in uHash.
// Return NULL if stack searched all the way to the bottom and not found.
// Return pointer to FOR token and gFor set if found
// or pointer to GOSUB token if not found.
// SPECIAL CASE: If uHash = 0, Match first FOR struct found.
// If not NULL, returns with TmpChr set to token type.

WORD SearchFor(void)
{
    WORD ptr;
    UVAL_HASH tHash;


    ptr = (WORD)(BasicVars.GosubStackTop - 1);  // start at last token
    while (ptr >= BasicVars.GosubStackBot)
    {
        TmpChar = ReadRandom51(ptr);   // Read Token Type
        if (TmpChar == TOKEN_GOSUB || uHash.d == 0) return(ptr);

        // must be a FOR
        ptr -= (WORD) sizeof(FOR_DESCRIPTOR);   // point to variable pointer

        // Read it into tHash (Hash is the first part of FOR struct)
        ReadBlock51((BYTE *) &tHash, ptr, sizeof(UVAL_HASH));

        if (tHash.d == uHash.d)   // its a match
            return((WORD)(ptr + sizeof(FOR_DESCRIPTOR)));

        // not found yet
        ptr--;   // point to next token
    }
    return(NULL);  // not in stack
}


// Push a structure on the FOR_GOSUB stack.
// This is called by GOSUB and FOR statements.
// The FOR structure is contained in gFor and the GOSUB structure is in gReturn.
//
// When pushing a GOSUB structure, the return address is the actual address
// of the next command token.  If the GOSUB command is being executed directly
// from the command line, and there are no following commands, then the return
// address pushed should be zero.
//
// When pushing a FOR Structure, the FOR_GOSUB stack is non-destructively
// searched for a FOR loop using the same variable.  The search stops at the
// bottom of the stack or at a GOSUB structure.  If a FOR structure is found
// that uses the same variable, it is replaced with the new FOR structure and
// the FOR_GOSUB stack is chopped following that structure.
//
// If the FOR structure is not already there that uses the same variable, a
// new FOR_STRUCTURE is pushed followed by a TOKEN_FOR.
//
// Return TRUE if not enough memory.  Clears the Temp string Stack.

BIT PushStruct(BYTE Token)
{
    WORD TmpPtr;
    BYTE Len;
    BYTE *StructPtr;

    // Defaults for Gosub
    Len = sizeof(GOSUB_RETURN);
    StructPtr = (BYTE *) &gReturn;

    if (Token == TOKEN_FOR)   // special code for FOR structures
    {
	    WORD ptr;

        // set to correct size
        Len = sizeof(FOR_DESCRIPTOR);
	    StructPtr = (BYTE *) &gFor;

        // Search for pre-existing variable
        uHash = gFor.Hash;
        ptr = SearchFor();
        if (ptr != NULL && TmpChar == TOKEN_FOR)  // is it already there?
        {
            WORD clen;

            // The for lop structure was already there
            // delete it and compress stack
            clen = (WORD)(BasicVars.GosubStackTop - ptr);
            MemMove51((WORD)(ptr - sizeof(FOR_DESCRIPTOR)), (WORD)(ptr + 1), clen);
            BasicVars.GosubStackTop -= (WORD)(sizeof(FOR_DESCRIPTOR) + 1);  // chop stack
//            goto CopyStruct;
        }

    }

    TmpPtr = BasicVars.GosubStackTop;    // Save original stack top

    // Reserve space for structure + Token
    BasicVars.GosubStackTop += (WORD)(Len + 1);

    // Is there enough memory?
    if (BasicVars.GosubStackTop >= BasicVars.StringBot)
    {
        SyntaxErrorCode = ERROR_INSUFFICIENT_MEMORY;
        BasicVars.GosubStackTop = TmpPtr;
        return(TRUE);
    }

    // copy data
    WriteRandom51((WORD)(TmpPtr + Len), Token);
//CopyStruct:
    WriteBlock51(TmpPtr, StructPtr, Len);
	FreeTempAlloc();   // reset the temp strings

    return(FALSE);
}

// Pop a return address from the Gosub stack.
// If the stack was empty, return NULL, else return RETURN address.
// The return address should point to the token immediately after the GOSUB
// statement - ':' or '\r'.
// Any FOR structures in the way are removed along with the GOSUB structure
// itself.

WORD PopGosub(void)
{
    WORD ptr;

    // searching for a nonexistant FOR variable returns the last GOSUB
    uHash.d = 0xFFFF;
    ptr = SearchFor();
    if (ptr == NULL)  // not found
    {
        // Stack empty
        SyntaxErrorCode = ERROR_MISPLACED_RETURN;
        return(NULL);
    }

    // chop stack
    ptr -= (WORD) sizeof(GOSUB_RETURN);    // point to address
    BasicVars.GosubStackTop = ptr;

    // Read Structure
    ReadBlock51((BYTE *) &gReturn, ptr, sizeof(GOSUB_RETURN));
    
    return(gReturn.ReturnPtr);   // Return Line pointer
}



// Get FOR DESCRIPTOR for specified variable into gFor.
// Searches for matching variable uHash.
// If uHash is zero, return first FOR found.
// Returns TRUE if not found.
// Chops stack so that requested FOR is the last item on stack.

BIT GetFor(void)
{
    WORD addr;

    addr = SearchFor();   // Search stack using uHash
    if (addr == NULL || TmpChar == TOKEN_GOSUB)
    {
        // Not found or found gosub instead
	    SyntaxErrorCode = ERROR_NEXT;
    	return(TRUE);
    }

    // We found the FOR struct we were looking for if here
    BasicVars.GosubStackTop = (WORD)(addr + 1);  // chop stack

    if (gFor.CtrlVarPtr == 0 || gFor.Hash.d != uHash.d)
    {
        // Its different from the one we have so read new one in
        addr -= (WORD) sizeof(FOR_DESCRIPTOR);  // point to base
        ReadBlock51((BYTE *) &gFor, addr, sizeof(FOR_DESCRIPTOR));
    }

    return(FALSE);
}




