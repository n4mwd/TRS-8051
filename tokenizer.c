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
TOKENIZER.C - Routines to tokenize and de-tokenize BASIC source lines.
*/




#include "Bas51.h"


/*
The purpose of this module is to take an input line, as typed, and convert
keywords, variables, function, operators and constants into tokens that are
more easily read by the cpu.  In addition, the conversion has to be reversible
such that the LIST command will display a reasonably true copy of what was
actually typed.

Lines entered without line numbers are given a temporary line number of 65535
and then executed as soon as tokenization is complete.  Lines are stored in
the following format:

| Line number | Line Length |  Tokenized line | CR     |
| 2 bytes     | 1 byte      |                 | 1 byte |

A line can contain multiple statements separated by ':'.

Keywords are replaced by tokens which start with single bytes with bit 7 set.
A token identifier byte may have a number of additional bytes that follow.
The exact number is usually determined by the type of token.  String tokens,
for example, may be quite large.
*/




// Search specified table for token pointed to by InBufPtr
// Assumes leading blanks already skipped.
// Note: Had to split tables because combined they were bigger than 256
// Returns index of token found, or 0xFF if not found.
// If match is found, advance InBufPtr to the char after the current token.

BYTE MatchTokenTable(BYTE *Table)
{
    BYTE b;      // index
    BYTE sch;    // search char
    BYTE c;      // count


    c = 0;
    sch = (BYTE)(toupper(InBufPtr[0]) + 0x80);   // get modified first char
    while (*Table != 0x80)           // 0x80 marks the end of the table
    {
        if (*Table & 0x80) c++;      // increment counter

        if (*Table == sch)           // found start of match
        {
            // start on second character
            b = 1;
            Table++;

            // Compare and stop on first non-matching char
            while (toupper(InBufPtr[b++]) == *Table) Table++;

            // See if match found.
            // If its a total match, Table will point to next token
            if (*Table & 0x80)
            {
                // advance InBufPtr
                InBufPtr += (b - 1);
                return((BYTE)(c - 1));  // return token number
            }

            // if not, then just keep going until the end
        }
        else Table++;
    }

    // we didn't find it
    return(0xFF);


}


// Search all token tables for token pointed to by InBufPtr.
// Assumes leading blanks already skipped.
// Returns actual Token number, or 0 if not found.
// If match is found, advance InBufPtr to the char after the current token.

BYTE MatchToken(void)
{
    BYTE r;

    r = MatchTokenTable(OperatorTokenTable);
    if (r != 0xFF) return((BYTE)(OPERATOR_TOKEN_START + r));

    r = MatchTokenTable(CommandTokenTable);
    if (r != 0xFF) return((BYTE)(COMMAND_TOKEN_START + r));

    r = MatchTokenTable(FunctionTokenTable);
    if (r != 0xFF) return((BYTE)(FUNCTION_TOKEN_START + r));

    return(0xFF);   // not a known token

}



// Move InBufPtr ahead to the next non-blank
// Returns with Curchar set to the first non-blank found

BYTE SkipBlanks(void)
{
    BYTE tChar;

    // skip blanks
    while ((tChar = *InBufPtr) != 0)
    {
        if (!isspace(tChar)) break;
        InBufPtr++;
    }
    
    return(tChar);
}


// Output a float in BIG ENDIAN to TokBuf
void OutUval(void)
{
    BYTE b;

    for (b = 0; b != 4; b++) *TokBufPtr++ = uData.bVal[b];
}

// Return TRUE if the last token was a pre-unary token

BIT PreUnary(BYTE Token)
{

    if (Token < LAST_OPERATOR_TOKEN && Token != TOKEN_RIGHT_PAREN)
        return(TRUE);   // all operators except ')' are pre-unary

    // Functions are not pre-unary

    if (Token >= COMMAND_TOKEN_START && Token < LAST_COMMAND_TOKEN)
        return(TRUE);   // Commands are considered pre-unary

    // everything else is not pre-unary
    return(FALSE);
}


// Convert entire line located in InBuf[] into a tokenized version in TokBuf[].
// If there are any input characters that are not understood, then its a
// syntax error.  Other than that, there isn't much syntax checking done here.
// It is possible that the tokenized version may be longer than the original.
// If so, and the the tokenized version exceeds the buffer size, an
// ERROR_LINE_TOO_LONG error will be thrown.
// The real purpose of tokenizing is not to save space, but to make it easier
// for the cpu to process the line at run time.
//
// If successful, replace InBuf with tokenized version.
// Return 0 on success, or position in input line (+1) where syntax error occurred.


BYTE TokenizeLine(BYTE *Buffer, BYTE BufLen)
{
    BYTE LastToken;
    BYTE r;
    BYTE b;
    BIT StartOfStatement, Assignment;
    BYTE LineLen;
    BYTE Parenthesis;
    BYTE IfPtr;
    BYTE ret;

    ret = 0;
    InBufPtr = Buffer;
//    InBufPtr = InBuf;
    Buffer[BufLen] = 0;    // temporary null char so skipblanks() will work
    memset(TokBuf, 0, sizeof(TokBuf));
    TokBufPtr = TokBuf;
    LastToken = 0xFF;
    IfPtr = 0;

    // Check for leading line number
    CurChar = SkipBlanks();   // skip blanks and set CurChar
    LineNo = 0xFFFF;     // default line number that gets executed immediately
    if (isdigit(CurChar))    // we have a leading digit
    {
	    DWORD dw;

        dw = strtoul((char *) InBufPtr, (char **) &InBufPtr, 10);
        if (dw > 65534)   // syntax error
        {
            SyntaxErrorCode = ERROR_INVALID_LINE_NUMBER;
            ret = 1;
            goto error;
        }
        LineNo = (WORD) dw;
    }
    // Set the line number
    TokBuf[0] = (BYTE)(LineNo >> 8);    // High Byte
    TokBuf[1] = (BYTE)(LineNo & 0xFF);  // Low Byte
    TokBufPtr = TokBuf + 3;   // leave space for line length
    LineLen = 0;    // the official length does not include the 3 byte header

    // Main Tokenizing Loop
	Parenthesis = 0;
    Assignment = FALSE;
    StartOfStatement = TRUE;
    while (TRUE)
    {
        CurChar = SkipBlanks();  // skip blanks and set CurChar
        if (InBufPtr - Buffer == BufLen) break;
//        if (*InBufPtr == 0) break;

        // Figure out what it is
        r = MatchToken();     // is it a known token
        if (r != 0xFF)  // we found it
        {
            // handle abbreviations
            if (r == TOKEN_PRINT2) r = TOKEN_PRINT;
//            if (r == TOKEN_REM2) r = TOKEN_REM;
            if (r == TOKEN_LEFT_PAREN) Parenthesis++;
            if (r == TOKEN_RIGHT_PAREN) Parenthesis--;
            if (r == TOKEN_LET) Assignment = TRUE;

            if (r == TOKEN_ASSIGN)  // EQUALS defaults to ASSIGN
            {
                if (Assignment && Parenthesis == 0)
                {
                    Assignment = FALSE;
                }
                else r = TOKEN_EQUALS;  // change default
            }

            // Check for unary operators
            if (r == TOKEN_PLUS || r == TOKEN_MINUS)
            {
                // Is the last token pre-unary?
                if (PreUnary(LastToken))
                {
                    // Ignore unary plus because they don't do anything
                    if (r == TOKEN_PLUS) continue;

                    // Must be a unary minus if here
                    r = TOKEN_UNARY_MINUS;

                    // check for two in a row
                    if (LastToken == TOKEN_UNARY_MINUS)
                    {
                        // erase preivous token
                        TokBufPtr--;
                        *TokBufPtr = 0;
                        LineLen--;
                        LastToken = TOKEN_PLUS; // not exact, but still pre-unary
                        continue;
                    }

                }
            }

            *TokBufPtr++ = r;
            LineLen++;

            StartOfStatement = (BIT)((r == TOKEN_COLON ||
            		r == TOKEN_THEN || r == TOKEN_ELSE) ? TRUE : FALSE);
            LastToken = r;

            // Special case for REM
            if (r == TOKEN_REM || r == TOKEN_REM2)
            {
                // Skip first space if there
                if (*InBufPtr == ' ') InBufPtr++;

                // copy into TokBuf to the end of line
                while (*InBufPtr != 0)
                {
                    *TokBufPtr++ = *InBufPtr++;
                    LineLen++;
                }
            }


            // special case for IF-THEN-ELSE
            // IF <expression> THEN <clause> [ELSE <clause>]
            // IF <expression> GOTO <line> [ELSE <clause>]
            // IF_TOKEN <offset to ELSE> ... ELSE_TOKEN <offset to EOL>
            // Create Linked list of IF-THEN tokens.

            if (r == TOKEN_IF || r == TOKEN_ELSE)
            {
                *TokBufPtr++ = IfPtr;   // reserve space for jump size
                IfPtr = (BYTE)(LineLen + 3);
                LineLen++;
			}

        }

//        // SPECIAL TERMINAL CHAR COLON
//        else if (CurChar == ':')
//       {
//            *TokBufPtr++ = LastToken = TOKEN_COLON;
//            LineLen++;
//            InBufPtr++;
//
//            StartOfStatement = TRUE;
//        }

        // Check to see if its a number
        // must work for 3, 3.0, .3 and 3.
        else if (isdigit(CurChar) || CurChar == '.')    // we have a leading digit
        {
		    BYTE *Ptr;

            // type modifiers are not allowed, constants stored as a long int
            // if it doesn't have a decimal point.

            b =  TOKEN_INTL_CONST;
            uData.LVal = strtol((char *) InBufPtr, (char **) &Ptr, 10);
            if (*Ptr == '.' || toupper(*Ptr) == 'E')   // oops, it was a float
            {
                b = TOKEN_FLOAT_CONST;
				uData.fVal = strtod((char *) InBufPtr, (char **) &Ptr);
            }
            if (Ptr == InBufPtr)
            {
                SyntaxErrorCode = ERROR_SYNTAX;
                ret = (BYTE)(InBufPtr - Buffer);
                goto error;
            }
            InBufPtr = Ptr;

            /*
            // check to see if then-goto or else-goto tokens need to be
            // substituted

            if (b == TOKEN_INTL_CONST)
            {
                if (LastToken == TOKEN_ELSE) *(TokBufPtr - 2) = TOKEN_ELSEGOTO;
                if (LastToken == TOKEN_THEN) *(TokBufPtr - 1) = TOKEN_THENGOTO;
            }
            */

            *TokBufPtr++ = LastToken = b;
            OutUval();  // Output uData
            LineLen += (BYTE) 5;

            StartOfStatement = FALSE;
        }

        // Check if string constant
        else if (CurChar == '"')    // start of string
        {
            BYTE *ptr;

            // Output string token structure
            *TokBufPtr++ = LastToken = TOKEN_STRING_CONST;
            ptr = TokBufPtr++;     // save pointer for length
            LineLen++;
            LineLen++;         // increment LineLen for the two header bytes

            InBufPtr++;     // skip leading quote
            b = 0;
            // copy directly
            while ((CurChar = *InBufPtr) != 0 && LineLen < 240)
            {
                InBufPtr++;           // point to next char
                if (CurChar == '"')   // End of string
                {
                    *ptr = b;         // save length
                    break;
                }
                else                  // Still in string
                {
                     *TokBufPtr++ = CurChar;
                     LineLen++;
                     b++;
                }
            }
            // When here, string copy is complete
            if (CurChar != '"')   // missing end quote
            {
                SyntaxErrorCode = ERROR_MISSING_QUOTE;
                ret = (BYTE)(InBufPtr - Buffer);
                goto error;
            }

        }

        // Check for variable
        else if (isalpha(CurChar))    // variables always start with alpha
        {
            if (StartOfStatement)
            {
                // encode implicit "LET"
                Assignment = TRUE;
                *TokBufPtr++ = TOKEN_LET;
                LineLen++;
            }

            // Encode variable name and return length including type suffix.
            // EncodeVariableName does not modify the TokBufPtr pointer.

            b = EncodeVariableName();
            if (b >= 38) // variable name too long (tag + len + name + hash)
            {
                SyntaxErrorCode = ERROR_VARIABLE_NAME_TOO_LONG;
                ret = (BYTE)(InBufPtr - Buffer);
                goto error;
            }
            r = *TokBufPtr;   // Token of this var

            LastToken = r;
            StartOfStatement = FALSE;
            LineLen += (BYTE) b;
            TokBufPtr += (BYTE) b;

        }  // variable

        // special check for labels
        else if (CurChar == '_')    // labels always start with underscore
        {
            EncodeLabel();
            LineLen += (BYTE) 5;
            TokBufPtr += 5;

            if (LineLen == 5)   // Label definition
            {
                // Treat rest of line as a comment
                // copy into TokBuf to the end of line
                while (*InBufPtr != 0)
                {
                    *TokBufPtr++ = *InBufPtr++;
                    LineLen++;
                }
            }
        }

        // Token not recognized
        else
        {
            SyntaxErrorCode = ERROR_UNRECOGNIZED_SYMBOL;
            ret = (BYTE)(InBufPtr - Buffer);
            goto error;
        }

        // Check for excessive line length
        if (LineLen >= 240)
        {
            SyntaxErrorCode = ERROR_LINE_TOO_LONG;
            ret = (BYTE)(InBufPtr - Buffer);
            goto error;
        }
    }  // End main while()

    if (Parenthesis != 0)
    {
        SyntaxErrorCode = ERROR_PARENTHESIS_MISMATCH;
        ret = (BYTE)(InBufPtr - Buffer);
        goto error;
    }


    *TokBufPtr = '\r';    // terminate line with CR
    LineLen++;
    TokBuf[2] = (BYTE)(LineLen);

    // Correct LineLen to actual value
    LineLen += (BYTE) 3;


    // Check to see if we need to do any IF-ELSE fixups.
    if (IfPtr)
    {

        // Since we are done with inbuf[] we can use the stacks
        InitStacks();

        // Traverse List of IF-ELSE's until no more.
        while (IfPtr)
        {
            BYTE Next;
//TokBufPtr = TokBuf;
            Next = TokBuf[IfPtr];    // save next location

            // Figure out which one
            if (TokBuf[IfPtr - 1] == TOKEN_IF)
            {
                // check to see if the stack has any ELSE's
                if (StackEmpty(OPERATOR_STACK))
                {
                    // No ELSE found on the stack so a False-IF jumps
                    // to the end of the line.
                    TokBuf[IfPtr] = (BYTE)(LineLen - IfPtr - 2);
                }
                else   // There are ELSE's
                {
                    // Pop the ELSE and assign that to the IF
                    TokBuf[IfPtr] = (BYTE)(PopStk(OPERATOR_STACK) - IfPtr - 1);
                }
            }
            else   // must be an ELSE or ELSEGOTO
            {
                // Else token - push location on stack
                // This is the location a False-IF will jump to
                PushStk(OPERATOR_STACK, (BYTE)(IfPtr + 1));

                // If the ELSE is executed at the end of a True-IF,  it
                // will jump to the end of the line
                TokBuf[IfPtr] = (BYTE)(LineLen - IfPtr - 2);
            }

            IfPtr = Next;
        }



    }

 //   memcpy(InBuf, TokBuf, LineLen);
    SyntaxErrorCode = ERROR_NONE;
error:
    Buffer[BufLen] = ' ';    // change temporary null char back to space
    return(ret);
}



// Return the next token in the program line pointed to by the stream.
// As appropriate, sets uData and uHash.

BYTE GetNextToken(void)
{
    BYTE Token;

    Token = ReadStream51();

    // process token
    switch(Token)
    {
        case TOKEN_STRING_CONST:
            uData.sVal.sLen = ReadStream51(); // Length
            uData.sVal.sPtr = GetStreamAddr();  // pointer to string
            StreamSkip(uData.sVal.sLen);    // skip string
            break;

        case TOKEN_INTL_CONST:   // possible line number
            uHash.ptr = GetStreamAddr();   // save address in case its a line number
            uData.LVal = ReadStreamLong();
            break;

        case TOKEN_FLOAT_CONST:
            uData.fVal = ReadStreamFloat();
                break;

        case TOKEN_LABEL:
            uHash.d = ReadStreamLong();
//            SetStream51((WORD)(GetStreamAddr() - 5));  // Point back to Token
            break;

        case TOKEN_NOTYPE_VAR:
        case TOKEN_INTL_VAR:
        case TOKEN_FLOAT_VAR:
        case TOKEN_STRING_VAR:
        case TOKEN_NOTYPE_ARRAY:
        case TOKEN_INTL_ARRAY:
        case TOKEN_FLOAT_ARRAY:
        case TOKEN_STRING_ARRAY:
            // get variable name string in uData
            uData.sVal.sLen = ReadStream51(); // Length
            uData.sVal.sPtr = GetStreamAddr();  // pointer to array name
            StreamSkip(uData.sVal.sLen);   // read over  variable name
            // Get variable Hash in uHash
            uHash.d = ReadStreamLong();
            break;

    	//  IF and ELSE tokens are followed by an offset byte
        case TOKEN_IF:
        case TOKEN_ELSE:
            uData.bVal[0] = ReadStream51();  // offset
            break;

        default:
            break;
    }

    return(Token);
}





// Decide if there needs to be a space printed before the operator
BIT PrintPreSpace(BYTE Token)
{
    if (Token == TOKEN_LEFT_PAREN || Token == TOKEN_COMMA) return(FALSE);
   // if (Token == TOKEN_GOTO || Token == TOKEN_GOSUB) return(TRUE);
    if (Token < TOKEN_SEMICOLON && Token != TOKEN_UNARY_MINUS) return(TRUE);
    if (Token >= COMMAND_TOKEN_START && Token < LAST_COMMAND_TOKEN) return(TRUE);
    return(FALSE);
}

// Decide if there needs to be a space printed after the operator
BIT PrintPostSpace(BYTE Token)
{
    if (Token == TOKEN_LEFT_PAREN) return(FALSE);
    if (Token <= TOKEN_SEMICOLON && Token != TOKEN_UNARY_MINUS) return(TRUE);
    if (Token >= COMMAND_TOKEN_START && Token < LAST_COMMAND_TOKEN) return(TRUE);
    return(FALSE);
}



// Return a pointer, or NULL if not found, to the token string.
// Token is zero based, not TOKEN number based.

BYTE *GetTokenName(BYTE *TokenTable, BYTE Token)
{
    while (*TokenTable != 128)   // 128 marks the end of table
    {
        if (*TokenTable & 0x80)    // start of new token string
        {
            if (Token == 0)    // Found it
            {
                // Return pointer to table entry
                return(TokenTable);
            }
            Token--;
        }
        TokenTable++;
    }

    return(NULL);   // we didn't find it
}


// Given the token number, return pointer to token string.
// ASSUMES: Token is less than LAST_COMMAND_TOKEN.
// Return NULL if not found.

BYTE *GetTokenFromTable(BYTE Token)
{
    if (Token >= COMMAND_TOKEN_START)
        return(GetTokenName(CommandTokenTable, (BYTE)(Token - COMMAND_TOKEN_START)));

    if (Token >= FUNCTION_TOKEN_START)
        return(GetTokenName(FunctionTokenTable, (BYTE)(Token - FUNCTION_TOKEN_START)));

    if (Token >= OPERATOR_TOKEN_START)
        return(GetTokenName(OperatorTokenTable, (BYTE)(Token - OPERATOR_TOKEN_START)));

    // if we are here, then something went wrong
    return(NULL);
}



// Print Token at cursor location
// Returns SyntaxErrorCode set on error.

void PrintTokenFromTable(BYTE Token)
{
    BYTE *bp;

    SyntaxErrorCode = ERROR_NONE;
    bp = GetTokenFromTable(Token);    // look up token
    if (!bp)   // not a valid token
    {
        SyntaxErrorCode = ERROR_INTERNAL_BAS51_ERROR;
    }
    else
    {
    	VGA_putchar((BYTE)(*bp & 0x7F));
    	bp++;
    	while (!(*bp & 0x80))
    	{
        	VGA_putchar((BYTE)(*bp++));
    	}
    }

    return;
}




// Converts tokenized Line into ascii and prints at cursor location

BYTE PrintTokenizedLine(WORD LinePtr)
{
    BYTE CurToken;
    BIT LastCharWasSpace;

    SetStream51(LinePtr);    // must use memory file functions
    FieldStart = (WORD)(CurPosY * VID_COLS + CurPosX);
    FieldLen = 0;     // will be incremented by VGA_putchar()

    // Do Line number
    {       // <<-- to keep variables private, must be in brackets
    	WORD w;

    	w = (WORD)(ReadStream51() << 8);
    	w |= ReadStream51();
    	VGA_printf("%5u ", w);
        LastCharWasSpace = TRUE;
    	ReadStream51();   // Skip Length
    }

    while((CurToken = ReadStream51()) != '\r')
    {
        // numerical float constants
        if (CurToken == TOKEN_FLOAT_CONST)
        {
            VGA_printf("%G", ReadStreamFloat());
            LastCharWasSpace = FALSE;
        }

        // Numerical UINT constants
        else if (CurToken == TOKEN_INTL_CONST)
        {
            VGA_printf("%d", ReadStreamLong());
            LastCharWasSpace = FALSE;
        }

        // String constants
        else if (CurToken == TOKEN_STRING_CONST)
        {
            BYTE StringLen;

            VGA_putchar('"');
            StringLen = ReadStream51();
            for (; StringLen; StringLen--)
                VGA_putchar(ReadStream51());
            VGA_putchar('"');
            LastCharWasSpace = FALSE;
        }

        else if (CurToken == TOKEN_LABEL)
        {
            VGA_print("UNKNOWN-LABEL");
            StreamSkip(4);
        }

        else if (CurToken >= TOKEN_NOTYPE_VAR)
        {
            BYTE VarLen;

            VarLen = ReadStream51();       // Variable Length

            // copy variable name
            for (; VarLen; VarLen--)
                VGA_putchar(ReadStream51());

            // skip past hash
            for (VarLen = 4; VarLen; VarLen--)
                ReadStream51();

            // Add type modifier char
            if (CurToken == TOKEN_INTL_VAR || CurToken == TOKEN_INTL_ARRAY)
            {
                VGA_putchar('%');
            }

            if (CurToken == TOKEN_FLOAT_VAR || CurToken == TOKEN_FLOAT_ARRAY)
            {
                VGA_putchar('!');
            }

            if (CurToken == TOKEN_STRING_VAR || CurToken == TOKEN_STRING_ARRAY)
            {
                VGA_putchar('$');
            }

            LastCharWasSpace = FALSE;
        }

        else if (CurToken >= OPERATOR_TOKEN_START && CurToken < LAST_COMMAND_TOKEN)
        {
            if (!LastCharWasSpace && PrintPreSpace(CurToken)) VGA_putchar(' ');
            PrintTokenFromTable(CurToken);
            if (SyntaxErrorCode) goto error;
            if (PrintPostSpace(CurToken))
            {
                VGA_putchar(' ');
                LastCharWasSpace = TRUE;
            }


            // Special case for REM
            if (CurToken == TOKEN_REM || CurToken == TOKEN_REM2)
            {
                while ((CurToken = ReadStream51()) != '\r')
                    VGA_putchar(CurToken);
                break;   // we already found the \r
            }

            // special case for IF-ELSE
            if (CurToken == TOKEN_IF || CurToken == TOKEN_ELSE)
                    ReadStream51();   // sync back up
        }


//        // Special case for Terminal Char COLON
//        else if (CurToken == TOKEN_COLON)
//        {
//                VGA_print(" : ");
//                LastCharWasSpace = TRUE;
//        }

        // should be nothing left
        else
        {
error:
            // Unrecognized token
            SyntaxErrorCode = ERROR_INTERNAL_BAS51_ERROR;
            return(ERROR_INTERNAL_BAS51_ERROR);
        }
    }

    return(ERROR_NONE);

}





// List program from start line to end line.
// If StartLine == 0, lists from beginning.
// If StartLine > 0xFF00) Lists to end.
// EndLine cannot be 0xFFFF.
// If StartLine == 0xFFFF, entire file is being saved.

void ListProgram(WORD StartLine, WORD EndLine)
{
    BYTE r;
    BIT SaveFlag;

    // Get pointer to starting line
    SaveFlag = FALSE;
    if (StartLine == 0)
    {
        StartLine = sizeof(BASIC_VARS_TYPE);
    }
    else if (StartLine == 0xFFFF)   // Special case for saving files
    {
        SaveFlag = TRUE;
        StartLine = sizeof(BASIC_VARS_TYPE);
    }
    else  // Search For It
    {
        StartLine = FindLinePtr(StartLine, FALSE);
    }

    // Get pointer to last line
    EndLine = FindLinePtr((WORD)(EndLine + 1), FALSE);
    if (EndLine == 0) EndLine = BasicVars.VarStart;


    while (StartLine < EndLine)
    {
        FieldStart = (WORD) (CurPosY * 64 + CurPosX);  // Set FieldStart

        r = PrintTokenizedLine(StartLine);
        if (r != ERROR_NONE) break;

        // calculate FieldLen
        FieldLen = (BYTE)((CurPosY * 64 + CurPosX) - FieldStart);

        // Don't print newline until length calculated.
        VGA_putchar('\n');

        // Special code to save file
        if (SaveFlag)
        {
            while (FieldLen != 0)
            {
                BasWriteByte(VIDEO_MEMORY[FieldStart++]);
                FieldLen--;
            }
            BasWriteByte('\n');
        }

        StartLine = GetStreamAddr();   // next line
    }

}




