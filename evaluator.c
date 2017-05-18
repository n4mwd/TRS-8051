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
EVALUATOR.C - Routines to handle evaluation of functions and standard
BASIC operators.
*/


#include "bas51.h"
#include <math.h>






float FloatMod(float a, float b)
{
    return (a - b * floor(a / b));
}


void Evaluate(BYTE OperatorToken)
{
    BIT  UnaryFlag, FloatFlag, StringFlag;

    UnaryFlag = FALSE;
    FloatFlag = FALSE;
    StringFlag = FALSE;

    // Figure out how many parameters
    if (OperatorToken == TOKEN_NOT || OperatorToken == TOKEN_UNARY_MINUS)
        UnaryFlag = TRUE;

    // Pop the arguments
    uValTok[1] = PopStk(CALC_STACK);     // second parameter first
    uValArg[1] = uData;

    uValTok[0] = 0;
    if (!UnaryFlag)
    {
        uValTok[0] = PopStk(CALC_STACK);    // Pop first parameter if not unary
        uValArg[0] = uData;
    }

    // special case for assignment
    if (OperatorToken == TOKEN_ASSIGN)
    {
        // modify compatible types
        if (uValTok[0] == TOKEN_INTL_VAR && uValTok[1] == TOKEN_FLOAT_CONST)
        {
            // change float to int
            uValArg[1].LVal = uValArg[1].fVal;
            uValTok[1] = TOKEN_INTL_CONST;
        }
        else if (uValTok[0] == TOKEN_FLOAT_VAR && uValTok[1] == TOKEN_INTL_CONST)
        {
            // change float to int
            uValArg[1].fVal = uValArg[1].LVal;
            uValTok[1] = TOKEN_FLOAT_CONST;
        }

        // check for compatable assignment
        if (uValTok[0] != uValTok[1] + 4) goto Error1;

        // Make assignment
        uData = uValArg[1];
        WriteVar(uValTok[0], uValArg[0].sVal.sPtr);

        return;
    }

    // Check for any strings
    if (uValTok[0] == TOKEN_STRING_CONST || uValTok[1] == TOKEN_STRING_CONST)
    {
        // only certain operators are allowed for string parameters
        if (OperatorToken >= TOKEN_EQUALS && OperatorToken <= TOKEN_PLUS)
        {
        	StringFlag = TRUE;
        }
        else goto Error1;

        if (uValTok[0] != uValTok[1]) goto Error1;   // The both must be strings
    }

    // Check for float arguments
    if (uValTok[0] == TOKEN_FLOAT_CONST || uValTok[1] == TOKEN_FLOAT_CONST)
    {
        // advance ints to floats
        if (uValTok[0] == TOKEN_INTL_CONST)
            uValArg[0].fVal = uValArg[0].LVal;
        if (uValTok[1] == TOKEN_INTL_CONST)
            uValArg[1].fVal = uValArg[1].LVal;

        FloatFlag = TRUE;
    }

    uData.fVal = 0;
    if (FloatFlag)
    {
        switch(OperatorToken)
        {
            case TOKEN_PLUS:
                uData.fVal = uValArg[0].fVal + uValArg[1].fVal;
                break;

            case TOKEN_MINUS:
                uData.fVal = uValArg[0].fVal - uValArg[1].fVal;
                break;

            case TOKEN_MULTIPLY:
                uData.fVal = uValArg[0].fVal * uValArg[1].fVal;
                break;

            case TOKEN_DIVIDE:
                if (uValArg[1].fVal == 0.0) goto Error2;  // divide by zero
                uData.fVal = uValArg[0].fVal / uValArg[1].fVal;
                break;

            case TOKEN_MOD:
                if (uValArg[1].fVal == 0.0) goto Error2;  // divide by zero
                uData.fVal = FloatMod(uValArg[0].fVal, uValArg[1].fVal);
                break;

            case TOKEN_UNARY_MINUS:
                uData.fVal = -(uValArg[1].fVal);
                break;

			case TOKEN_POWER:
				uData.fVal = pow(uValArg[0].fVal, uValArg[1].fVal);
                break;

			case TOKEN_NOT:
                uData.fVal = (float)~(long)uValArg[1].fVal;
                break;

			case TOKEN_OR:
                uData.fVal = (DWORD) uValArg[0].fVal | (DWORD) uValArg[1].fVal;
                break;

			case TOKEN_AND:
                uData.fVal = (DWORD) uValArg[0].fVal & (DWORD) uValArg[1].fVal;
                break;

			case TOKEN_EQUALS:
                uData.fVal = (uValArg[0].fVal == uValArg[1].fVal) ? -1 : 0;
                break;

			case TOKEN_LESS_THAN_OR_EQUAL:
                uData.fVal = (uValArg[0].fVal <= uValArg[1].fVal) ? -1 : 0;
                break;

			case TOKEN_GREATER_THAN_OR_EQUAL:
                uData.fVal = (uValArg[0].fVal >= uValArg[1].fVal) ? -1 : 0;
                break;

			case TOKEN_NOT_EQUAL:
                uData.fVal = (uValArg[0].fVal != uValArg[1].fVal) ? -1 : 0;
                break;

			case TOKEN_GREATER_THAN:
                uData.fVal = (uValArg[0].fVal > uValArg[1].fVal) ? -1 : 0;
                break;

			case TOKEN_LESS_THAN:
                uData.fVal = (uValArg[0].fVal < uValArg[1].fVal) ? -1 : 0;
                break;

        }

        PushStk(CALC_STACK, TOKEN_FLOAT_CONST);
    }
    else if (StringFlag)
    {
        uData.LVal = 0;   // FALSE
        switch (OperatorToken)
        {
            case TOKEN_PLUS:
                ConcatStr();
 				if (SyntaxErrorCode) return;
                PushStk(CALC_STACK, TOKEN_STRING_CONST);
                return;

			case TOKEN_EQUALS:
                if (StringCmp() == 0) uData.LVal = -1; // TRUE
                break;

			case TOKEN_NOT_EQUAL:
                if (StringCmp() != 0) uData.LVal = -1; // TRUE
                break;

			case TOKEN_LESS_THAN_OR_EQUAL:
                if (StringCmp() <= 0) uData.LVal = -1; // TRUE
                break;

			case TOKEN_GREATER_THAN_OR_EQUAL:
                if (StringCmp() >= 0) uData.LVal = -1; // TRUE
                break;

			case TOKEN_GREATER_THAN:
                if (StringCmp() > 0) uData.LVal = -1; // TRUE
                break;

			case TOKEN_LESS_THAN:
                if (StringCmp() < 0) uData.LVal = -1; // TRUE
                break;

            default:
                goto Error1;  // operator type conflict
        }

        PushStk(CALC_STACK, TOKEN_INTL_CONST);
    }
    else  // must be INTL
    {
        switch(OperatorToken)
        {
            case TOKEN_PLUS:
                uData.LVal = uValArg[0].LVal + uValArg[1].LVal;
                break;

            case TOKEN_MINUS:
                uData.LVal = uValArg[0].LVal - uValArg[1].LVal;
                break;

            case TOKEN_MULTIPLY:
                uData.LVal = uValArg[0].LVal * uValArg[1].LVal;
                break;

            case TOKEN_DIVIDE:
                if (uValArg[1].LVal == 0L) goto Error2;  // divide by zero
                uData.LVal = uValArg[0].LVal / uValArg[1].LVal;
                break;

            case TOKEN_MOD:
                if (uValArg[1].LVal == 0L) goto Error2;  // divide by zero
                uData.LVal = uValArg[0].LVal % uValArg[1].LVal;
                break;

            case TOKEN_UNARY_MINUS:
                uData.LVal = -(uValArg[1].LVal);
                break;

			case TOKEN_POWER:
				uData.LVal = (long) pow((float) uValArg[0].LVal, (float) uValArg[1].LVal);
                break;

			case TOKEN_NOT:
                uData.LVal = ~(uValArg[1].LVal);
                break;

			case TOKEN_OR:
                uData.LVal = uValArg[0].LVal | uValArg[1].LVal;
                break;

			case TOKEN_AND:
                uData.LVal = uValArg[0].LVal & uValArg[1].LVal;
                break;

			case TOKEN_EQUALS:
                uData.LVal = (uValArg[0].LVal == uValArg[1].LVal) ? -1 : 0;
                break;

			case TOKEN_LESS_THAN_OR_EQUAL:
                uData.LVal = (uValArg[0].LVal <= uValArg[1].LVal) ? -1 : 0;
                break;

			case TOKEN_GREATER_THAN_OR_EQUAL:
                uData.LVal = (uValArg[0].LVal >= uValArg[1].LVal) ? -1 : 0;
                break;

			case TOKEN_NOT_EQUAL:
                uData.LVal = (uValArg[0].LVal != uValArg[1].LVal) ? -1 : 0;
                break;

			case TOKEN_GREATER_THAN:
                uData.LVal = (uValArg[0].LVal > uValArg[1].LVal) ? -1 : 0;
                break;

			case TOKEN_LESS_THAN:
                uData.LVal = (uValArg[0].LVal < uValArg[1].LVal) ? -1 : 0;
                break;

        }
        PushStk(CALC_STACK, TOKEN_INTL_CONST);
    }

    return;

Error1:
    SyntaxErrorCode = ERROR_TYPE_CONFLICT;
    return;

Error2:
    SyntaxErrorCode = ERROR_DIVIDE_BY_ZERO;
    return;


}



/*
// Parameter Types
TOKEN_INTL_CONST - Must be INTL, can be converted from Float, String is error.
TOKEN_NUMERICAL_CONST - Can be either INTL or Float, but not String.
TOKEN_FLOAT_CONST - Must be Float, can be converted from INTL, String is error.
TOKEN_STRING_CONST - Must be string, Can be converted from numerical.
*/

// Return Acceptable Parameter Type for 1st parameter of functions.
// This table must be in the correct order.

CODE BYTE ParameterTable[] =
{
    // Single parameter functions
    TOKEN_NUMERICAL_CONST,  // TOKEN_ABS - Numerical (output type same as input)
    TOKEN_FLOAT_CONST, 		// TOKEN_ATN - Float in, Float out
    TOKEN_FLOAT_CONST,   	// TOKEN_COS - Float in, Float out
    TOKEN_FLOAT_CONST,   	// TOKEN_EXP - Float in, Float out
    TOKEN_FLOAT_CONST,   	// TOKEN_LOG - Float in, Float out
    TOKEN_FLOAT_CONST,   	// TOKEN_SIN - Float in, Float out
    TOKEN_FLOAT_CONST,   	// TOKEN_SQR - Float in, Float out
    TOKEN_FLOAT_CONST,   	// TOKEN_TAN - Float in, Float out
    TOKEN_FLOAT_CONST,   	// TOKEN_INT - Float in, Float out
    TOKEN_FLOAT_CONST,   	// TOKEN_FIX - Float in, Float out
    TOKEN_FLOAT_CONST,   	// TOKEN_CINT - Float in, INTL out
    TOKEN_FLOAT_CONST,      // TOKEN_CSNG - Float in, Float out
    TOKEN_STRING_CONST,   	// TOKEN_LEN - String in, INTL out
	TOKEN_INTL_CONST,   	// TOKEN_PEEK - INTL in, INTL out
    TOKEN_INTL_CONST,   	// TOKEN_RAND - INTL in, Float out
    TOKEN_NUMERICAL_CONST,	// TOKEN_SGN - Numerical in, INTL out
	TOKEN_INTL_CONST,   	// TOKEN_USR - INTL in, INTL out
    TOKEN_STRING_CONST,   	// TOKEN_VAL - String in, Float out
    TOKEN_STRING_CONST,   	// TOKEN_ASC - String in, INTL out
	TOKEN_INTL_CONST,   	// TOKEN_CHR - INTL in, String out
    TOKEN_FLOAT_CONST,   	// TOKEN_STR - Float in, String out
    TOKEN_INTL_CONST,		// TOKEN_TAB - INTL in, String out

    // Double parameter functions
	TOKEN_INTL_CONST,   	// TOKEN_STRING - INTL in, String, String out
	TOKEN_INTL_CONST,   	// TOKEN_POINT - INTL, INTL in, INTL out
    TOKEN_STRING_CONST,   	// TOKEN_LEFT -  String, INTL in, String out
    TOKEN_STRING_CONST,   	// TOKEN_RIGHT - String, INTL in, String out
    TOKEN_INTL_CONST,		// TOKEN_AT - INTL, INTL in, String out
    TOKEN_STRING_CONST,     // TOKEN_INSTR - String, String in, INTL out

    // Triple parameter function
    TOKEN_STRING_CONST,   	// TOKEN_MID - String, INTL, INTL in, String out
};


// Normalize function parameter.
// Return TRUE if there was an error
// Param is the paramter index number 0-2
// ReqType is the paramter type it is supposed to be.
// If ReqType is INTL or FLOAT, and param is not that type,
//  convert to required type.  Strings cannot be converted.

BIT NormParams(BYTE Param, BYTE ReqType)
{
    if (uValTok[Param] != ReqType)    // not exactly what we want
    {   						// but is it compatible?
        if (ReqType == TOKEN_STRING_CONST ||
            uValTok[Param] == TOKEN_STRING_CONST)
        {
            // Strings are not be converted - sorry.
            SyntaxErrorCode = ERROR_PARAMETER;
            return(TRUE);
        }

        else if (ReqType == TOKEN_INTL_CONST)   // must have an INTL
        {
            uValArg[Param].LVal = (long) uValArg[Param].fVal;    // convert to INTL
        }

        else if (ReqType == TOKEN_FLOAT_CONST)
        {
            uValArg[Param].fVal = (float) uValArg[Param].LVal;    // convert to Float
        }
        // else TOKEN_NUMERICAL_CONST is passed as is.
    }

    return(FALSE);
}


void EvaluateFunction(BYTE FunctionToken, BYTE NumParams)
{
	BYTE ReqParams;
    BYTE B;
    BYTE RetType;

    uData.LVal = 0;   // default return value

    // Calculate the number of required parameters
    if (FunctionToken <= TOKEN_TIMETICKS) ReqParams = 0;
    else if (FunctionToken <= TOKEN_TAB) ReqParams = 1;
    else if (FunctionToken <= TOKEN_INSTR) ReqParams = 2;
    else ReqParams = 3;

    // Make sure its the right number of parameters
    if (NumParams != ReqParams)    // Parameter error
    {
        SyntaxErrorCode = ERROR_PARAMETER;
        return;
    }

    // Pop the arguments
    for (B = NumParams; B != 0; B--)
    {
        uValTok[B-1] = PopStk(CALC_STACK);
        uValArg[B-1] = uData;
    }

    // Correct and normalize parameters
    if (NumParams > 0)
    {
        ReqParams = ParameterTable[FunctionToken - TOKEN_ABS];
        if (NormParams(0, ReqParams)) return;
    }

    if (NumParams > 1)
    {
        ReqParams = TOKEN_INTL_CONST;
        if (FunctionToken == TOKEN_INSTR || FunctionToken == TOKEN_STRING)
        	ReqParams = TOKEN_STRING_CONST;
        if (NormParams(1, ReqParams)) return;
    }

    if (NumParams == 3)
    {
        ReqParams = TOKEN_INTL_CONST;
        if (NormParams(2, ReqParams)) return;
    }

    // Perform the function, Return values in RetType and uData

    switch(FunctionToken)
    {
		// Parameterless functions
        case TOKEN_PI:   			// Returns 3.14159265359
            uData.fVal = (float) 3.14159265358979323846264338327950;
            RetType = TOKEN_FLOAT_CONST;
            break;

   		case TOKEN_MEM:			// Returns Unused Memory Amount
            uData.LVal = BytesFree();
            RetType = TOKEN_INTL_CONST;
        	break;

   		case TOKEN_POSX:   			// Returns cursor X position
            uData.LVal = CurPosX;
            RetType = TOKEN_INTL_CONST;
        	break;

   		case TOKEN_POSY: 			// Returns cursor Y position
            uData.LVal = CurPosY;
            RetType = TOKEN_INTL_CONST;
        	break;

   		case TOKEN_INKEY: 			// Returns keystroke in string or null string
            RetType = TOKEN_STRING_CONST;
            InkeyFunction();
        	break;

        case TOKEN_EXTKEY:
            // Returns flag with the current states of SHIFT, CTRL, ALT and WIN
            // Bit 0: LEFT  SHIFT
            // Bit 1: RIGHT SHIFT
            // Bit 2: LEFT  CONTROL
            // Bit 3: RIGHT CONTROL
            // Bit 4: LEFT  ALT
            // Bit 5: RIGHT ALT
            // Bit 6: LEFT  WINDOWS
            // Bit 7: RIGHT WINDOWS
            RetType = TOKEN_INTL_CONST;
            uData.LVal = KeyFlags;
            break;

        case TOKEN_ERR:
            uData.LVal = BasicVars.LastErrorCode;
            RetType = TOKEN_INTL_CONST;
        	break;

        case TOKEN_ERL:
            uData.LVal = BasicVars.LastErrorLine;
            RetType = TOKEN_INTL_CONST;
        	break;

        case TOKEN_TIME:
            RetType = TOKEN_STRING_CONST;
            uData.sVal.sLen = 0;
            uData.sVal.sPtr = TempAlloc(17);
            if (uData.sVal.sPtr)
            {
                BYTE Hours, Mins, Secs;
                WORD Temp;   // Works up to 18 hours

                uData.sVal.sLen = 17;
                Temp = (WORD)(GetScreenCount() / 1000);   // Get total seconds
                Secs = (BYTE)(Temp % 60);                 // get clock seconds
                Temp = (WORD)(Temp / 60);                 // Get total minutes
                Mins = (BYTE)(Temp % 60);                 // get clock minutes
                Temp = (WORD)(Temp / 60);                  // get total hours
                if (Temp > 99) Temp = 99;
                Hours = (BYTE) Temp;
                sprintf((char *) TokBuf,
                    "04/08/17 %02d:%02d:%02d", Hours, Mins, Secs);
                WriteBlock51(uData.sVal.sPtr, TokBuf, 17);
            }
            break;

        case TOKEN_TIMETICKS:
        	uData.LVal = GetScreenCount();
            RetType = TOKEN_INTL_CONST;
            break;



		// Single Parameter Functions
   		case TOKEN_ABS:  	     // 1
            // Arg can be INTL or FLOAT
            RetType = uValTok[0];
            if (uValTok[0] == TOKEN_INTL_CONST)
        	{
                uData.LVal = uValArg[0].LVal;
                if (uData.LVal < 0) uData.LVal = -uData.LVal;
            }
            else // float
            {
                uData.fVal = uValArg[0].fVal;
                if (uData.fVal < 0) uData.fVal = -uData.fVal;
            }
        	break;

   		case TOKEN_ATN:        // 1
            RetType = TOKEN_FLOAT_CONST;
            uData.fVal = atan(uValArg[0].fVal);
        	break;

   		case TOKEN_COS:        // 1
            RetType = TOKEN_FLOAT_CONST;
            uData.fVal = cos(uValArg[0].fVal);
        	break;

   		case TOKEN_EXP:        // 1
            RetType = TOKEN_FLOAT_CONST;
            uData.fVal = exp(uValArg[0].fVal);
        	break;

   		case TOKEN_LOG:        // 1
            RetType = TOKEN_FLOAT_CONST;
            uData.fVal = log(uValArg[0].fVal);
        	break;

   		case TOKEN_SIN:        // 1
            RetType = TOKEN_FLOAT_CONST;
            uData.fVal = sin(uValArg[0].fVal);
        	break;

   		case TOKEN_SQR:        // 1
            RetType = TOKEN_FLOAT_CONST;
            uData.fVal = sqrt(uValArg[0].fVal);
        	break;

   		case TOKEN_TAN:        // 1
            RetType = TOKEN_FLOAT_CONST;
            uData.fVal = tan(uValArg[0].fVal);
        	break;

   		case TOKEN_INT:        // 1
            RetType = TOKEN_FLOAT_CONST;
            uData.fVal = floor(uValArg[0].fVal);
        	break;

   		case TOKEN_FIX:        // 1
            RetType = TOKEN_FLOAT_CONST;
            uData.fVal = (float)(long)uValArg[0].fVal;
        	break;

   		case TOKEN_CINT:        // 1
            RetType = TOKEN_INTL_CONST;
            uData.LVal = floor(uValArg[0].fVal);
        	break;

   		case TOKEN_CSNG:        // 1
            RetType = TOKEN_FLOAT_CONST;
            uData.fVal = uValArg[0].fVal;
            //if (uValTok[0] == TOKEN_INTL_CONST)
            //    uData.fVal = uData.LVal;
        	break;

   		case TOKEN_LEN:        // 1
            RetType = TOKEN_INTL_CONST;
            uData.LVal = StringLen(0); // Arg[0].sVal.sEnd - Arg[0].sVal.sStart + 1;
        	break;

   		case TOKEN_PEEK:       // 1
            RetType = TOKEN_INTL_CONST;
            uData.LVal = ReadRandom51((WORD) uValArg[0].LVal);
        	break;

   		case TOKEN_RAND:       // 1  // Random number - output depends on input
            RetType = TOKEN_INTL_CONST;
            if (uValArg[0].LVal == 0)    // output number from 0.000001 and 0.999999
            {
                RetType = TOKEN_FLOAT_CONST;
                uData.fVal = 1.0 / rand();
            }
            else   // output intL from 0 to Arg Val
            {
                uData.LVal = rand() % uValArg[0].LVal + 1;
            }
        	break;

   		case TOKEN_SGN:        // 1
            RetType = TOKEN_INTL_CONST;
            uData.LVal = 0;
            if (uValTok[0] == TOKEN_INTL_CONST)
            {
                if (uValArg[0].LVal > 0) uData.LVal++;
                if (uValArg[0].LVal < 0) uData.LVal--;
            }
            else   // float
            {
                if (uValArg[0].fVal > 0) uData.LVal++;
                if (uValArg[0].fVal < 0) uData.LVal--;
            }
        	break;

   		case TOKEN_USR:        // 1
        	break;

   		case TOKEN_VAL:        // 1   Converts number in string to float.
            RetType = TOKEN_FLOAT_CONST;
            ValFunction();
        	break;

   		case TOKEN_ASC:        // 1   Returns ASCII code of 1st char in string.
            RetType = TOKEN_INTL_CONST;
            AscFunction();
        	break;

   		case TOKEN_CHR:        // 1  Returns a 1-char string defined by code.
            RetType = TOKEN_STRING_CONST;
            ChrFunction();
        	break;

   		case TOKEN_STR:        // 1  Converts Float into string.
            RetType = TOKEN_STRING_CONST;
            StrFunction();
        	break;

        case TOKEN_TAB:       // Changes CurPosX, returns null string
            RetType = TOKEN_STRING_CONST;
            if (uValArg[0].LVal < 0 || uValArg[0].LVal >= VID_COLS) uValArg[0].LVal = 0;
            uData.LVal = 0;
            if (uValArg[0].LVal < CurPosX) VGA_putchar('\n');
            GotoXY((BYTE) uValArg[0].LVal, CurPosY);
            break;





   		// Double parameter functions
   		case TOKEN_STRING:     // 2
            RetType = TOKEN_STRING_CONST;
            StringFunction();
        	break;

   		case TOKEN_POINT:      // 2
            RetType = TOKEN_INTL_CONST;
            uData.LVal = GraphPlot((WORD) uValArg[0].LVal, (WORD) uValArg[1].LVal, 0xFF);
        	break;

   		case TOKEN_LEFT:       // 2
            RetType = TOKEN_STRING_CONST;
            LeftFunction();
        	break;

   		case TOKEN_RIGHT:      // 2
            RetType = TOKEN_STRING_CONST;
            RightFunction();
        	break;

        case TOKEN_AT:          // 2 - change cursor location, return null string
            RetType = TOKEN_STRING_CONST;
//            if (uValArg[0].LVal < 0 || uValArg[0].LVal >= VID_COLS) uValArg[0].LVal = 0;
//            if (uValArg[1].LVal < 0 || uValArg[1].LVal >= VID_ROWS) uValArg[1].LVal = 0;
            uData.LVal = 0;
            GotoXY((BYTE) uValArg[0].LVal, (BYTE) uValArg[1].LVal);
            break;

        case TOKEN_INSTR:      // 2 - Search for substring, return INTL.
            RetType = TOKEN_INTL_CONST;
            uData.LVal = InstrFunction();
            break;


	   // Triple parameter function
   		case TOKEN_MID:        // 3
            RetType = TOKEN_STRING_CONST;
            MidsFunction();  // Start at Parm[1](pos) to Parm[2](length)
        	break;


        default:    // undefined function
            SyntaxErrorCode = ERROR_UNDEFINED_FUNCTION;
            break;
    }

    PushStk(CALC_STACK, RetType);


}



