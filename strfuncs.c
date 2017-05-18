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
STRFUNCS.C - Various internal BASIC string manipulation functions.
*/




#include "Bas51.h"



// Return the string length.
// Strings can be 0 to 255 chars.

BYTE StringLen(BYTE ArgNum)
{
    // return length
	if (uValArg[ArgNum].sVal.sPtr) return(uValArg[ArgNum].sVal.sLen);
    else return(0);
}


// Test and/or correct string bounds of argnum.
// Return TRUE if uncorrectable.

BIT StringBounds(BYTE ArgNum)
{
    if (uValArg[ArgNum].LVal < 0) uValArg[ArgNum].LVal = 0;
    if (uValArg[ArgNum].LVal > 255)  // uncorrectable error
    {
        SyntaxErrorCode = ERROR_STRING_LENGTH;
        return(TRUE);
    }
    return(FALSE);
}


// Compare two strings
// Return 0 if equal.
// Return -1 if arg0 is less than arg1.
// Return 1 if arg0 is greater than arg1.
// Shorter strings are less than the same but longer string ("we" < "weed").
// Compare is not case sensitive.

signed char StringCmp(void)
{
    BYTE Len1, Len2, B1, B2;

    // get string lengths
    Len1 = StringLen(0);
    Len2 = StringLen(1);

    // special case for null strings
    if (Len1 == 0 && Len2 == 0) return(0);  // two null strings are equal
    if (Len1 == 0) return(-1);              // 1st arg null
    if (Len2 == 0) return(1);               // 2nd arg null

    // Niether string is null iof here
    while (Len1 && Len2)
    {
        B1 = (BYTE) toupper(ReadRandom51(uValArg[0].sVal.sPtr++));
        B2 = (BYTE) toupper(ReadRandom51(uValArg[1].sVal.sPtr++));

        if (B1 < B2) return(-1);
        if (B1 > B2) return(1);

        Len1--;
        Len2--;
    }

    // Check for equal
    if (Len1 == Len2) return(0);  // both are the same

    // If here, one string is the same but shorter.
    return((signed char)((Len1 < Len2) ? -1 : 1));

}



// Concat Arg[0] and Arg[1]
// One of the args might not be a string.

void ConcatStr(void)
{
    BYTE Len1, Len2;
    WORD LenT, StrPtr;

    // Calculate lengths of strings
    Len1 = StringLen(0);
    Len2 = StringLen(1);
    LenT = (WORD)((WORD) Len1 + Len2);  // get total length
    if (LenT > 255)
    {
        SyntaxErrorCode = ERROR_STRING_LENGTH;
        return;
    }

    // initialize combined string
    uData.LVal = 0;
    if (LenT == 0) return;

    // allocate new space
    StrPtr = TempAlloc((BYTE)LenT);
    if (!StrPtr) return;   // Not enough memory - error code already set
    uData.sVal.sLen = LenT;
    uData.sVal.sPtr = StrPtr;

    // copy in the strings
    MemMove51(StrPtr, uValArg[0].sVal.sPtr, Len1);
    MemMove51((WORD)(StrPtr + Len1), uValArg[1].sVal.sPtr, Len2);
}



// INSTR(BigStr$, LittleStr$) - Searches for the first occurence of LittleStr$
// in BigStr$ and returns the position where the match was found.
// Returns 0 if not found.  Returns 1 if LittleStr$ is empty.
// Arg[0] = BigStr$, Arg[1] = LittleStr.

BYTE InstrFunction(void)
{
    BYTE LenBig, LenSub;
    BYTE BigIdx, SubIdx;
    WORD BigStr, SubStr;

    LenBig = StringLen(0);
    LenSub = StringLen(1);

    if (LenBig == 0 || LenBig < LenSub) return(0);   // Can't be found
    if (LenSub == 0) return(1);   // null small string

    BigStr = (WORD)uValArg[0].sVal.sPtr;
    SubStr = (WORD)uValArg[1].sVal.sPtr;

    for (BigIdx = 0; BigIdx < LenBig; BigIdx++)
    {
        for (SubIdx = 0; SubIdx < LenSub; SubIdx++)
        {
            BYTE B, S;

            B = ReadRandom51((WORD)(BigStr + BigIdx + SubIdx));
            S = ReadRandom51((WORD)(SubStr + SubIdx));
            if (toupper(B) != toupper(S)) break;
        }

        if (SubIdx == LenSub)    // Did we find it?
        	return((BYTE)(BigIdx + 1));        // YES.
  	}

    return(0);   // Not found
}

// Returns a Float converted from a string in Arg[0].

void ValFunction(void)
{
    BYTE Len;

    uData.fVal = 0;
    Len = StringLen(0);
    if (Len > 0)
    {
	    // Copy string from main memory to XDATA memory
    	ReadBlock51(TokBuf, (WORD)uValArg[0].sVal.sPtr, Len);
	    TokBuf[Len] = 0;   // convert to C-string
    	uData.fVal = strtod((char *) TokBuf, NULL);
    }
}

// Mid$ function
// Also called by Left$ and Right$

void MidsFunction(void)
{
    BYTE LenBig;
    BYTE Start, Len;

    uData.LVal = 0;       // initialize return string
    if (StringBounds(1) || StringBounds(2)) return;

    Start = (BYTE) uValArg[1].LVal;
    Len = (BYTE) uValArg[2].LVal;

    LenBig = StringLen(0);

    // Null string or Starts after it ends
    if (Len == 0 || LenBig == 0 || Start == 0 || Start > LenBig) return;

    // Normalize start
    Start--;

    // Correct length if they ask for more than we have
    if (Start + Len > LenBig) Len = (BYTE)(LenBig - Start);

    // allocate new space
    uData.sVal.sPtr = TempAlloc(Len);
    if (!uData.sVal.sPtr) return;   // Not enough memory - error code already set
    uData.sVal.sLen = Len;

    // Copy substring over
    MemMove51(uData.sVal.sPtr, (WORD)(uValArg[0].sVal.sPtr + Start), Len);
}


void LeftFunction(void)
{

    uValArg[2].LVal = uValArg[1].LVal;  // Length
    uValArg[1].LVal = 1;                // Start
	MidsFunction();  // Start at 1 to length
}


void RightFunction(void)
{
	BYTE LenStr;
    BYTE ReqLen;

    uData.LVal = 0;       // initialize return string
    if (StringBounds(1)) return;

    uValArg[2].LVal = ReqLen = (BYTE) uValArg[1].LVal;  // length

	LenStr = StringLen(0); 		// actual length of input string
    if (ReqLen > LenStr) ReqLen = LenStr;

    uValArg[1].LVal = LenStr - ReqLen + 1;

	MidsFunction();  // Start at pos to end
}


// Do ASC BASIC function
// Return ASCII code of first char of string in Arg[0]
// Return value in uData

void AscFunction(void)
{
    if (StringLen(0))
        uData.LVal = ReadRandom51((WORD)(uValArg[0].sVal.sPtr));
    else uData.LVal = 0;
}


// Returns a 1-char string defined by code.

void ChrFunction(void)
{
	uData.LVal = 0;   // null string
	uData.sVal.sPtr = TempAlloc(1);   // Alloc  temp string space
    if (uData.sVal.sPtr == NULL) return;

    WriteRandom51(uData.sVal.sPtr, (BYTE)uValArg[0].LVal);
    uData.sVal.sLen = 1;
}


//  Converts Float into string.

void StrFunction(void)
{
	BYTE Len;

    uData.LVal = 0;   // null string
	Len = (BYTE) sprintf((char *) TokBuf, "%G", uValArg[0].fVal);
	uData.sVal.sPtr = TempAlloc(Len);
	if (uData.sVal.sPtr == NULL) return;
	uData.sVal.sLen = Len;
	WriteBlock51(uData.sVal.sPtr, TokBuf, Len);
}


// Returns a string of Arg1[0] characters with a length of Arg0.
// Arg0 = Length, Arg1 = String, Return Temp String in uData

void StringFunction(void)
{
	BYTE Len, FirstChar;

	uData.LVal = 0;         // initialize to null string
    if (StringBounds(0)) return;

	Len = (BYTE)uValArg[0].LVal;
	if (Len != 0 && StringLen(1) != 0)
	{
		FirstChar = ReadRandom51(uValArg[1].sVal.sPtr);  // Get 1st char
		uData.sVal.sPtr = TempAlloc(Len);
		if (uData.sVal.sPtr == NULL) return;
		uData.sVal.sLen = Len;
		while (Len--)
		{
			WriteRandom51((WORD)(uData.sVal.sPtr + Len), FirstChar);
		}
	}
}


// Return last char in keyboard buffer or a null string if not there.

void InkeyFunction(void)
{
	uData.LVal = 0;      // initialize to NULL string
	if (KeyAvailable())
	{
		WORD key;
        BYTE B;

		key = GetKeyWord();
		uData.sVal.sPtr = TempAlloc(2);   // Get 2 temp bytes
		if (uData.sVal.sPtr == NULL) return;

		B = (BYTE)(key & 0xFF);
		if (B != 0)
		{
			WriteRandom51(uData.sVal.sPtr, B);
			uData.sVal.sLen = 1;
		}
		else   // extended char
		{
			WriteRandom51(uData.sVal.sPtr, 'X');
			B = (BYTE)(key >> 8);  // upper byte
			WriteRandom51((WORD)(uData.sVal.sPtr + 1), B);
			uData.sVal.sLen = 2;
		}
	}


}


