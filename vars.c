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
VARS.C - Variable management.
*/


#include "bas51.h"






//XDATA BYTE VarTmpTag[34];




// Clear the variables but leave the program space alone

void ClearVariables(void)
{
    BYTE x;

    // Initialize BasicVars Structure - Zero up
    BasicVars.DimStart = BasicVars.VarStart;
    BasicVars.GosubStackBot = BasicVars.VarStart;
    BasicVars.GosubStackTop = BasicVars.VarStart;
    BasicVars.TempStringBot = BasicVars.VarStart;
    BasicVars.TempStringTop = BasicVars.VarStart;
    BasicVars.StringBot = (WORD)BasicVars.CmdLine; // empty
    BasicVars.OnErrorLine = 0;
    BasicVars.ResumeAddr = 0;
    DATAptr = NULL;
    for (x = 0; x != 26; x++)
        DefTypes[x] = (BYTE)(TOKEN_FLOAT_VAR - TOKEN_NOTYPE_VAR);
    for (x = 0; x != 16; x++) BasicVars.FreeList[x] = 0;    

}


// Clear the program and all variables

void ClearEverything(void)
{
    // Initialize BasicVars Structure - Zero up
    BasicVars.ProgStart = sizeof(BasicVars);
    BasicVars.VarStart = sizeof(BasicVars);
    ClearVariables();
}



// This code is the equivalent CRC16 code that the Laser Bee does in hardware.
// It is nearly verbatim from the Laser Bee Manual.

#define POLY 0x1021
WORD UpdateCRC (WORD CRC_acc, BYTE CRC_input)
{
#ifdef __BORLANDC__
    BYTE i; // loop counter

    // Create the CRC "dividend" for polynomial arithmetic (binary arithmetic
    // with no carries)
    CRC_acc = (WORD)(CRC_acc ^ (CRC_input << 8));

    // "Divide" the poly into the dividend using CRC XOR subtraction
    // CRC_acc holds the "remainder" of each divide
	 //
    // Only complete this division for 8 bits since input is 1 byte
    for (i = 0; i < 8; i++)
    {
        // Check if the MSB is set (if MSB is 1, then the POLY can "divide"
        // into the "dividend")
        if ((CRC_acc & 0x8000) == 0x8000)
        {
            // if so, shift the CRC value, and XOR "subtract" the poly
            CRC_acc <<= 1;    // = (WORD)(CRC_acc << 1);
            CRC_acc ^= POLY;
        }
        else
        {
            // if not, just shift the CRC value
            CRC_acc <<= 1; // CRC_acc << 1;
        }
    }

#endif
    // Return the final remainder (CRC value)
    return CRC_acc;
}




// Create a 4 byte Hash out of a variable name.
// VarTokPtr points to the Type Token followed by a pascal style string.
// If VarTokPtr points to a LABEL Token, it replaces the label string
// with the Label HASH.
// Hash seems to have no collisions with less than 6 chars.
// Is unique for variables with different 1st and last chars, and lengths.
// Hash is copied directly after variable name in TokBuf.


BYTE *CalcHash(BYTE *VarTokPtr)
{
    WORD Crc16;
    BYTE Len, TokenType;
    HASH_TYPE var;    // special bitfield structure for hash

    // Save type token
    TokenType = *VarTokPtr++;
    var.TypeFlag = (TokenType - TOKEN_NOTYPE_VAR) & 0x3;

    // Get Length from pascal style string
    Len = *VarTokPtr++;
    Len--;   			// one less than actual length

    // At this point, VarTokPtr points to first char in name string.

    // Calc CRC only on chars between 1st and last char
    Crc16 = 0xFFFF;
    if (Len > 1)
    {
        BYTE Count, Ch;

        // Only do CRC on part that is not directly stored
        for (Count = 1; Count != Len; Count++)
        {
            Ch = (BYTE) VarTokPtr[Count];
            Crc16 = (WORD) UpdateCRC(Crc16, Ch);
        }
    }

    // Assign parameters to bitfield structure
    var.First = VarTokPtr[0] - 'A';
    var.Last = VarTokPtr[Len] - '0';
    var.Len = Len;    // one less than actual length
    var.Crc = Crc16;

    // copy to TokBuf
    if (TokenType == TOKEN_LABEL)   // Label
    {
    	// For labels, put the hash right after the Token.
    	VarTokPtr--;
    }
    else   // Regular variable
    {
    	Len++;           			// Len back to actual length
    	VarTokPtr += Len;  			// point to 1st char after name
    }
    memcpy(VarTokPtr, &var, 4);    // copy hash

    return(VarTokPtr);
}


// Encode LABEL name pointed to by InBufPtr and store hash at TokBufPtr.
// TokBufPtr is not modified so calling function must increment it.
// InBufPtr is advanced to the char that is not part of the LABEL.
//
// ASSUMES: InBufPtr points to the leading alpha char.
// ASSUMES: There are sufficient bytes available in TokBuf[].

void EncodeLabel(void)
{
    BYTE Len;
    BYTE *vptr;

    // copy variable name and calculate length
    InBufPtr++;   // point to the first char in label name
    vptr = (BYTE *)(TokBufPtr + 2);  // leave room for TOKEN and length
    for (Len = 0; isalnum(CurChar = (BYTE) *InBufPtr); Len++)
    {
        *vptr++ = (BYTE) toupper(CurChar);
        InBufPtr++;
    }
    TokBufPtr[1] = Len;         // No check here for excessive var length
    TokBufPtr[0] = TOKEN_LABEL;

	CalcHash(TokBufPtr);         // place 4 byte hash after variable name

}




// Encode variable name pointed to by InBufPtr and store encoded variable
// name at TokBufPtr.  Variable can be up to 32 chars long.
// Returns total Length of variable encoding.
// TokBufPtr is not modified so calling function must increment it.
// InBufPtr is advanced to the char that is not part of the variable.
//
// ASSUMES: InBufPtr points to the leading alpha char.
// ASSUMES: There are sufficient bytes available in TokBuf[].
// VARIABLE TYPE SUFFIXES:
//   % - INTEGER
//   ! - FLOAT
//   # - DOUBLE FLOAT
//   $ - STRING

BYTE EncodeVariableName(void)
{
    BYTE Len;
    BYTE *vptr;
    BYTE VarType;

    // copy variable name and calculate length
    vptr = (BYTE *)(TokBufPtr + 2);  // leave room for TOKEN and length
    for (Len = 0; isalnum(CurChar = (BYTE) *InBufPtr); Len++)
    {
        *vptr++ = (BYTE) toupper(CurChar);
        InBufPtr++;
    }
    TokBufPtr[1] = Len;         // No check here for excessive var length

    // InBufPtr now points to the char that stopped the scan
    // If there is a type symbol, it MUST be immediately after the variable name
    // with no spaces.  CurChar now has that byte.
    if (CurChar =='$')
    {
        VarType = TOKEN_STRING_VAR;
    }
    else if (CurChar == '%')
    {
        VarType = TOKEN_INTL_VAR;
    }
    else if (CurChar == '!' || CurChar == '#')  // must be float
    {
        VarType = TOKEN_FLOAT_VAR;  // default type
    }
    else VarType = TOKEN_NOTYPE_VAR;   // untyped

    // Bump inbufPtr past type char
    if (VarType != TOKEN_NOTYPE_VAR) InBufPtr++;

    SkipBlanks();   // skip leading blanks

    // Now look for left parenthesis - indicates array
    if (*InBufPtr == '(')   // this is an array
    {
        VarType += (BYTE) 4;   // bump to TOKEN_<TYPE>_ARRAY
    }
    TokBufPtr[0] = VarType;

	CalcHash(TokBufPtr);         // place 4 byte hash after variable name

    return((BYTE)(Len + 6));
}



// Allocates storage for Count simple variables.
// Up to 255 variables can be reserved at once.
// This function adds space to the simple variable file and sets it to zero.
// Returns a pointer to the first initialized spot, or NULL if there was
// insufficient memory.
// NOTE: There is no check to see if the space is actually needed.
// NOTE: Temp string space is reset.  Everything else persists.
// NOTE: The returned pointer is to the new space and not to pre-existing
//       free space that was allocated earlier.


WORD AllocSimpleVar(BYTE Count)
{
    WORD Size;

    Size = (WORD)(Count * (sizeof(uData) + sizeof(uHash)));

    // is there enough room?
    if (BytesFree() < Size)
    {
        SyntaxErrorCode = ERROR_INSUFFICIENT_MEMORY;
        return(NULL);
    }

    // move everything up
    MemMove51((WORD)(BasicVars.DimStart + Size),
              BasicVars.DimStart,
        (WORD)(BasicVars.GosubStackTop - BasicVars.DimStart));

    // Clear the new space
    MemClear51(BasicVars.DimStart, Size);

    // adjust all the pointers
    BasicVars.DimStart += Size;
    BasicVars.GosubStackBot += Size;
    BasicVars.GosubStackTop += Size;
    FreeTempAlloc();

    // Return pointer to start of NEW space.
    return((WORD)(BasicVars.DimStart - Size));

}



// Given the global Variable uHash, and a TOKEN representing the variable
// space, return a pointer to the variable.
// Return NULL if not found.
// Note that because sometimes variable space will be allocated without
// being assigned, when we hit a zero hash, there is no reason to continue
// the scan.
// Flags: Bit0 - Set for Arrays, Clear for simple variables.
//        Bit1 - Set for assigning variables, clear for reading variables.


WORD GetVarPtr(BYTE Flags)
{
    WORD StartPtr, StopPtr, Len;
    UVAL_HASH hash;

    StartPtr = BasicVars.VarStart;     // default simple space
    StopPtr = BasicVars.DimStart;

    if (Flags & VARPTR_ARRAY)   // array space
    {
	    StartPtr = BasicVars.DimStart;
    	StopPtr = BasicVars.GosubStackBot;
    }

    while (StartPtr < StopPtr)
    {
        // read hash
    	ReadBlock51((BYTE *) &hash, StartPtr, sizeof(UVAL_HASH));

        // Check for an empty memory location
        if (hash.d == 0)    // found clear variable
        {
            if (Flags & VARPTR_ASSIGN) goto Assign;    // assigning variables
            return(NULL);      // Variable not found for reading
        }

        // Is this the one?
        StartPtr += (WORD) sizeof(UVAL_HASH);   // point to actual variable
        if (hash.d == uHash.d) return(StartPtr);  // matched, return w/pointer

        // figure next address
        Len = sizeof(UVAL_DATA);
        if (Flags & VARPTR_ARRAY)  // array
            Len = ReadRandomWord(StartPtr);  // Get Array Length

        StartPtr += Len;
    }

    // variable space was completely searched and not found if here.
    if (!(Flags & VARPTR_ASSIGN) || (Flags & VARPTR_ARRAY))
        return(NULL);  // reading variables  or Arrays

    // if we are here, we are assigning simple variables

    // allocate new space and get pointer to it
    StartPtr = AllocSimpleVar(MIN_VAR_ALLOC);
    if (StartPtr == NULL) return(NULL);  // no memory

Assign:
    // we are making an assignment so write hash
   	WriteBlock51(StartPtr, (BYTE *) &uHash, sizeof(UVAL_HASH));

    // return pointing to actual variable
    return((WORD)(StartPtr + sizeof(UVAL_HASH))); // return pointer to new variable
}


// Read variable or constant into uData global
// Given Token and Pointer to variable data
// Return Variable Length to skip

BYTE Read2uData2(BYTE Token, WORD VarPtr)
{
    BYTE Len, b;

    uData.LVal = 0;
    if (Token == TOKEN_STRING_CONST)
    {
        // THIS IS ONLY FOR STRING CONSTANTS IN PROGRAM FILE OR CMD LINE
        // uData is just a pointer for strings
        Len = ReadRandom51(VarPtr++);   // String get length
        if (Len) uData.sVal.sPtr = VarPtr;     // point to string data
        uData.sVal.sLen = Len;        // String Length
        Len++;
    }
    else
    {
        Len = 4;
        for (b = 0; b != Len; b++) uData.bVal[b] = ReadRandom51(VarPtr++);
    }

    return(Len);
}


// Read variable name and hash from program file and store in uHash.
// Stream should point to the first byte after the token.

void ReadProgVarHash(void)
{
    BYTE Len;

    Len = ReadStream51();   			// Get length of variable name
    StreamSkip(Len);                    // skip over name
    uHash.d = (DWORD) ReadStreamLong(); // get hash
    if (uHash.str.TypeFlag == 0)   		// untyped
    {
        uHash.str.TypeFlag = DefTypes[uHash.str.First];
    }

}

// Read variable name and hash from program file and store in uHash.
// Stream should point to the first byte after the token.
// Then return a pointer to the variable.

WORD GetProgVarPtr(BYTE Flags)
{
    ReadProgVarHash();

    // Get pointer to simple variable for reading
    return(GetVarPtr(Flags));
}


// Read the stream to get the hash, then access the variable file to get the
// actual variable into uData.  Enter with the stream pointing to the first
// byte after the token.
// Return TRUE if there variable does not exist.

BIT GetSimpleVar(void)
{
    WORD VarPtr;

    // Get pointer to simple variable for reading
    VarPtr = GetProgVarPtr(VARPTR_READ_SIMPLE);   // Get Variable hash & pointer
    if (VarPtr == NULL) return(TRUE);

    Read2uData2(0, VarPtr);    // read actual variable into uData

    return(FALSE);
}


// Write the variable in uData to the address at VarPtr
// If Token is a string, check uData to see if the string has already been
// allocated.  If not, allocate it.  It it has, check to make sure its big
// enough for the new string.  If not, enlarge space.

void WriteVar(BYTE Token, WORD VarPtr)
{
    //BYTE B;

    if (!VarPtr) return;

    // special case for strings
    if (Token == TOKEN_STRING_VAR)
    {
        BYTE NewLen;
        UVAL_DATA tData;

        // Get New string Length
        NewLen = StringLen(UDATA);

        // Read existing variable
       	ReadBlock51((BYTE *) &tData, VarPtr, sizeof(UVAL_DATA));

		// Check to see if we are assigning a constant
        if (StringPtrIsConst(uData.sVal.sPtr))
        {
            // We are assigning a constant so we don't allocate space
            // but we still need to free any space previously used by
            // the variable.

            FreeStringAlloc(tData.sVal.sPtr);
        }
        else  // Not assigning a constant
        {
            // Realloc will return a Null pointer if the input was null and
            // the length is zero.

            tData.sVal.sPtr = ReStringAlloc(tData.sVal.sPtr, NewLen);
    	    if (SyntaxErrorCode) return;  // Insufficient memory

	        // We know that tData.sVal.sPtr has enough space if here
    	    // Copy string data from uData to durable string space

	        if (tData.sVal.sPtr)  // But only if pointer is not NULL
	        {
	        	// Copy new string to durable storage
	           	MemMove51(tData.sVal.sPtr, uData.sVal.sPtr, NewLen);
	        }

	        // Correct uData for copy below
	        uData.sVal.sPtr = tData.sVal.sPtr;
	        uData.sVal.sLen = NewLen;
        }

    } // End if STRING_VAR

    // copy root variable
   	WriteBlock51(VarPtr, (BYTE *) &uData, sizeof(UVAL_DATA));

}



// Calulate the elemental address and return it.
// BaseAddr -> Array base (Array Size Word)
// ArgCnt = Number of array subscripts actually present
// Return NULL on error.

WORD GetElementalAddr(WORD BaseAddr, BYTE ArgCnt)
{
    BYTE NumDims;
    WORD Offset, PrevDim;

    BaseAddr += (WORD) 2;  // Skip array size
    NumDims = ReadRandom51(BaseAddr);   // get number of dimensions
    if (ArgCnt != NumDims) return(NULL);  // subscript count mismatch

    // Get args and validate and calculate address
    // String args are errors, float args are converted to INTL.

    BaseAddr++;  // point to first DIM

    Offset = 0;  // Initialize offset
    PrevDim = 1;  // Previous Dimension

    while (NumDims--)   // prior code must prevent ArgCnt == 0
    {
        WORD DimSize;
        BYTE Token;

        // Note allowed subscripts are 0..<size>
        DimSize = ReadRandomWord(BaseAddr);  // Get official dimension size
        BaseAddr += (WORD) 2;    // point to next DIM or First element

	    // First arg popped will be rightmost arg.
        Token = PopStk(CALC_STACK);  // uData <- value

        // Validate token type
        if (Token != TOKEN_INTL_CONST)
        {
            if (Token == TOKEN_FLOAT_CONST) uData.LVal = uData.fVal;
            else goto Error; // error if anything else
        }

        // Validate bounds
        if (uData.LVal < 0 || uData.LVal > DimSize) goto Error;

        // Calulate offset
        Offset += (WORD)(PrevDim * uData.LVal);
        PrevDim *= (WORD)(DimSize + 1);  // count zero in allowed dimensions

    }

    // When here, offset is set
    Offset *= (WORD) 4;    // Adjust for size of uData variables

    return((WORD)(BaseAddr + Offset));   // return poimnter to actual uData element


Error:
    SyntaxErrorCode = ERROR_SUBSCRIPT;
    return(NULL);

}


// Calulate the elemental address and return the value contained in it.
// uData.sVal.sPtr -> array base
// ArgCnt = Number of array subscripts

void EvaluateArray(BYTE ArrayType, BYTE ArgCnt)
{
    WORD VarPtr;

    VarPtr = GetElementalAddr(uData.sVal.sPtr, ArgCnt);
    if (VarPtr)
    {
        // read element into uData
        ReadBlock51((BYTE *) &uData, VarPtr, sizeof(UVAL_DATA));

        // Transform array into const and push
        PushStk(CALC_STACK, (BYTE)(ArrayType - 8));
    }

}


// Calulate the elemental address and push it to the calc stack.
// uData.sVal.sPtr -> array base
// ArgCnt = Number of array subscripts

void PushElementPtr(BYTE ArrayType, BYTE ArgCnt)
{
    WORD VarPtr;

    VarPtr = GetElementalAddr(uData.sVal.sPtr, ArgCnt);
    if (VarPtr)
    {
        // copy to uData
        uData.sVal.sPtr = VarPtr;

        // Transform array into simple var and push
        PushStk(CALC_STACK, (BYTE)(ArrayType - 4));
    }

}





// Create an array.  Return TRUE on error.
// All elements are initialized to zero or null strings.
// On entry, uHash = array name hash.
// ArgCnt = Number of array subscripts actually present
// Caller must guarantee that ArgCnt > 0

BIT MakeArray(BYTE ArgCnt)
{
    BYTE NumDims, HeadIdx;
    WORD DimSizeW;
    DWORD DimSize;

    // Stage 1 - Create array header in TokBuf[]
    // This way, we allocate the array header and elements later all in one op.

    memcpy(TokBuf, &uHash, sizeof(uHash));     // copy hash up front
    HeadIdx = sizeof(uHash) + 2;  // skip array size word for now
    TokBuf[HeadIdx++] = ArgCnt;   // Number of DIMs

    DimSize = 1;
    NumDims = ArgCnt;

    while (NumDims--)   // prior code must prevent ArgCnt == 0
    {
        BYTE Token;

        // Note allowed subscripts are 0..<size>
	    // First arg popped will be rightmost arg.
        Token = PopStk(CALC_STACK);  // uData <- value

        // Get args and validate
        // Validate token type
        // String args are errors, float args are converted to INTL.
        if (Token != TOKEN_INTL_CONST)
        {
            if (Token == TOKEN_FLOAT_CONST) uData.LVal = uData.fVal;
            else goto Error;  // error if anything else
        }

        // Validate subscript bounds
        if (uData.LVal < 0 || uData.LVal > MAX_ARRAY_SUBSCRIPT) goto Error;

        // Store subscript
        TokBuf[HeadIdx++] = uData.bVal[1];
        TokBuf[HeadIdx++] = uData.bVal[0];

        DimSize *= (uData.LVal + 1);   // include zero

    }

    // Calulate actual storage required
    // DimSize is currently the number of elements
    DimSize *= 4;   // four bytes for each element
    DimSize += (ArgCnt * 2 + 3 + 4);  // add in header and hash


    // Is there enough room?
    if ((DWORD) BytesFree() < DimSize)
    {
        SyntaxErrorCode = ERROR_INSUFFICIENT_MEMORY;
        return(TRUE);
    }

    // Save DimSize
    DimSizeW = (WORD) DimSize;
    DimSizeW -= (WORD) 4;   // Hash not included in length
    TokBuf[sizeof(uHash)] = 	(BYTE)(DimSizeW >> 8);      // MSB
    TokBuf[sizeof(uHash) + 1] = (BYTE)(DimSizeW & 0xFF); 	// LSB
    DimSizeW += (WORD) 4;   // Hash included in everything else

    // When here, TokBuf has header and DimSize is the total size or alloc

    // move everything up
    MemMove51((WORD)(BasicVars.GosubStackBot + DimSizeW),
              BasicVars.GosubStackBot,
        (WORD)(BasicVars.GosubStackTop - BasicVars.GosubStackBot));

    // Clear the new space
    MemClear51(BasicVars.GosubStackBot, DimSizeW);

    // Copy the new array header in
    WriteBlock51(BasicVars.GosubStackBot, TokBuf, HeadIdx);

    // adjust all the pointers
    BasicVars.GosubStackBot += DimSizeW;
    BasicVars.GosubStackTop += DimSizeW;
    FreeTempAlloc();

    return(FALSE);


Error:
	SyntaxErrorCode = ERROR_PARAMETER;
    return(TRUE);

}


// Delete a pre-exisitng array
// uData.sVal.sPtr -> array variable

void DeleteArray(void)
{
    WORD DimSize, Src;

    DimSize = (WORD)(ReadRandomWord(uData.sVal.sPtr) + sizeof(UVAL_HASH));
    uData.sVal.sPtr -= (WORD) 4;   // include hash

    // move everything back down
    Src = (WORD)(uData.sVal.sPtr + DimSize);
    MemMove51(uData.sVal.sPtr, Src, (WORD)(BasicVars.GosubStackTop - Src));

    // adjust all the pointers
    BasicVars.GosubStackBot -= DimSize;
    BasicVars.GosubStackTop -= DimSize;
    FreeTempAlloc();
}













