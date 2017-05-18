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
IO_CMDS.C - Routines that pertain to BASIC input ansd output.
*/


#include "bas51.h"

// Get an expression parameter from the stream of a specific type.
// Automatically convert INTL and FLOAT types.
// Return TRUE if terminal token found instead.  Return TRUE on error with
// SyntaxErrorCode set.  Else return FALSE if good paramter found.

BIT GetParameter(BYTE ReqTok)
{
    BYTE Tok;

    uData.LVal = 0;

    CurChar = PeekStream51();   // see whats there first
    if (CurChar == TOKEN_COMMA || CurChar == TOKEN_SEMICOLON)
        ReadStream51();    // preceeding commas and semicolons allowed
    if (TerminalChar()) return(TRUE);  // unexpected end of parameters

    Tok = GetSimpleExpr();
    if (Tok == ReqTok) return(FALSE);
    if (Tok == TOKEN_FLOAT_CONST && ReqTok == TOKEN_INTL_CONST)
    {
        uData.LVal = uData.fVal;
        return(FALSE);
    }
    if (ReqTok == TOKEN_FLOAT_CONST && Tok == TOKEN_INTL_CONST)
    {
        uData.fVal = uData.LVal;
        return(FALSE);
    }

    SyntaxErrorCode = ERROR_TYPE_CONFLICT;
    return(TRUE);  // incompatable
}

// The PRINT USING statement uses the following format:
//    PRINT USING string; value { (, | ;} value }*
// String and value may be expressed as variables or constants. This statement
// will print the expression contained in the string, inserting the numeric value
// shown to the right of the semicolon as specified by the field specifiers.
//
// The following field specifiers may be used in the string:
//
// NUMERICAL TYPES
//   # This sign specifies the position of each digit located in the
// numeric value. The number of # signs you use establishes the
// numeric field. If the numeric field is greater than the number of
// digits in the numeric value, then the unused field positions to
// the left of the number will be displayed as spaces and those to
// the right of the decimal point will be displayed as zeros.
// The decimal point can be placed anywhere in the numeric field
// established by the # sign. Rounding-off will take place when
// digits to the right of the decimal point are suppressed.
// The comma – when placed in any position between the first
// digit and the decimal point - will display a comma to the left of
// every third digit as required. The comma establishes an
// additional position in the field.
//   ** Two asterisks placed at the beginning of the field will cause all
// unused positions to the left of the decimal to be filled with
// asterisks. The two asterisks will establish two more positions in
// the field.
//   $$ Two dollar signs placed at the beginning of the field will act as a
// floating dollar sign. That is, it will occupy the first position
// preceding the number.
//   **$ If these three signs are used at the beginning of the field, then
// the vacant positions to the left of the number will be filled by
// the * sign and the $ sign will again position itself in the first
// position preceding the number.
//   ^^^^ Causes the number to be printed in exponential (E or D) format
// when it follows the regular digit format characters.
//   + When a + sign is placed at the beginning or end of the field, it
// will be printed as specified as a + for positive numbers or as a -
// for negative numbers.
//   – When a – sign is placed at the end of the field, it will cause a
// negative sign to appear after all negative numbers and will
// appear as a space for positive numbers.
//
// CHARACTER TYPES
//   % spaces % To specify a string field of more than one character, % spaces %
// is used. The length of the string field will be 2 plus the number
// of spaces between the percent signs.
//   ! Causes the Computer to use the first string character of the
// current value.
//   & To specify a string of any length.
//
// LITERAL TYPES
// Any character not defined above is printed as a literal.  An underscore "_"
// preceeding a predefined character causes it to be printed as a literal.
//
// On entry, CurChar = TOKEN_USING, stream points to first char in format
// expression.
// Return TRUE if there was an error.
// FC = Format string null
// TM = Type mismatch between format specifier and parameter.
// MO = A format specifier required a parameter that wasn't there.  Note that
//      this error does not occur if there was at least one parameter.
// FC = No Format specifiers regardless of number of parameters.
// FC = The number of numerical format specifier characters exceeds 24.


BIT PrintUsing(void)
{
	WORD FmtStr;
    BYTE FmtLen, FmtIdx, FmtChar, FmtChar1; //, FmtChar2;
    WORD Flags;
    BIT FoundFmtField;
    BYTE PreDec, PostDec;
    BYTE FieldSize, i;

    if (GetParameter(TOKEN_STRING_CONST) ||   // must be a string
    	CurChar != TOKEN_SEMICOLON ||  // next char must be a semicolon
        StringLen(UDATA) == 0)         // Format string length must not be null
            goto ParameterError;

    FmtStr = uData.sVal.sPtr;    // save format string
    FmtLen = StringLen(UDATA);
    FoundFmtField = FALSE;

    // Make sure there is at least one parameter regardless
    CurChar = PeekStream51();
    if (TerminalChar()) goto ParameterError;


    while (TRUE)
    {
	    Flags = 0;
	    PreDec = 0;
	    PostDec = 0;

	    // scan the format string
	    for (FmtIdx = 0; FmtIdx < FmtLen; FmtIdx++)
		{
	        FmtChar = ReadRandom51((WORD)(FmtStr + FmtIdx));
            FmtChar1 = ReadRandom51((WORD)(FmtStr + FmtIdx + 1));
//            FmtChar2 = ReadRandom51((WORD)(FmtStr + FmtIdx + 2));

	        switch(FmtChar)
	        {
	            // String fields
	            case '!':   // Output single character.
	            	// If the input parameter string is NULL then output a space.
	                FieldSize = 1;
	                goto PrintStringField;

	            case '%':   // Output two or more characters
	                // Strings are left justified.  Shorter strings are padded to
	                // the right.  Longer strings are truncated.
	                // If EOS or any non blank chars found before the second '%'
	                // then the first '%' is treated as a literal.

	                // scan for ending '%'
	                FieldSize = 2;
	                for (i = 1; FmtIdx + i < FmtLen; i++)
	                {
				        FmtChar = ReadRandom51((WORD)(FmtStr + FmtIdx + i));
	                    if (FmtChar == '%')
                        {
                            FmtIdx += i;
                            goto PrintStringField;
                        }
	                    else if (FmtChar != ' ') break;
	                    FieldSize++;
	                }
	                // print as literal if here
	                FmtChar = '%';
	                goto PrintLiteral;

	            case '&':   // Output a variable length string
	                FieldSize = 0;
PrintStringField:
					FoundFmtField = TRUE;
	                if (GetParameter(TOKEN_STRING_CONST)) goto MissingOpError;
	                VGA_PrintString(FieldSize);
	                break;



	            // Numeric fields
		        // Allowed numerical format:
	            //		{+}{** or $$ or **$ or #}{#}*{,}{.{#}*}{^^^^}{+ or -}

               	// A '+' can appear at the beginning or end of a field.  If it
               	// is in both locations, only the first is valid and the
               	// second is literal.  If it doesn't appear immediately
                // before a "#", '**', or '$$', its a literal.

				case '+':   // Possible forced sign
	                FmtChar = ReadRandom51((WORD)(FmtStr + FmtIdx + 2));
    	            if (FmtChar1 == '#' ||
        	        	(FmtChar1 == '*' && FmtChar == '*') ||
            	        (FmtChar1 == '$' && FmtChar == '$') )
                	{
	                	PreDec++;
	                    Flags = FMTUSING_PLUS;
	                }
	                else
                    {
                        FmtChar = '+';
                        goto PrintLiteral;  // its just a literal
                    }
					break;


               	// A '$' by itself is a literal.  A '$$' starts a numeric field
               	// and is worth a digit plus a '$'.
            	case '$':
                    if (FmtChar1 == '$')  // We have "$$"
                    {
                        Flags |= FMTUSING_DOLLAR;
                        PreDec += (BYTE) 2;
                        FmtIdx += (BYTE) 2;   // skip over second '$'
                        goto EndPrint;              // print if last char
                    }
                    else goto PrintLiteral;  // its just a literal


           	    // A single '*' is considered a literal.  A '**' starts a field
				// and is worth 2 digits.  A '**$' starts a field and is worth
				// 2 digits plus the leading '$'.  When used with exponent
               	// fields, it is ignored except to print a zero.

				case '*':   // Leading '*' character
                    // If here, there were no previous '$' or '#'.
                    if (FmtChar1 == '*')  // We have "**"
                    {
                        Flags |= FMTUSING_STAR;
                        FmtIdx += (BYTE) 2;   // skip over second '*'
                        PreDec += (BYTE) 2;
//                        NumDefined = TRUE;
						// See if we actually have "**$"
                        if (ReadRandom51((WORD)(FmtStr + FmtIdx)) == '$')
                        {
	                        FmtIdx++;   // skip over '$'
                            PreDec++;
	                        Flags |= FMTUSING_DOLLAR;
                        }
                        goto EndPrint;              // print if last char
                    }
                    else goto PrintLiteral;  // its just a literal


                case '.':   // possible decimal and start of field
                    if (FmtChar1 == '#')
                    {
                        Flags |= FMTUSING_HASDPT;
                        FmtIdx++;  // point to first char after '.'
                        goto EndPrint;
                    }
                    else goto PrintLiteral;  // its just a literal



	            case '#':   // print a digit
                    // Get any remaining '#'s.

EndPrint:
					// Scan the rest of the number field
                    while (FmtIdx < FmtLen)
	                {
				        FmtChar = ReadRandom51((WORD)(FmtStr + FmtIdx));

	                    if (FmtChar == '#' && !(Flags & FMTUSING_EXPONENT))
                        {
                            if (Flags & FMTUSING_HASDPT) PostDec++;
                        	else PreDec++;
                        }
                        else if (FmtChar == ',' && !(Flags & (FMTUSING_COMMA |
                        		FMTUSING_HASDPT | FMTUSING_EXPONENT)))
                        {
                            Flags |= FMTUSING_COMMA;
                            PreDec++;    // comma counts as digit
                        }
                        else if (FmtChar == '.' &&
                        	!(Flags & (FMTUSING_HASDPT | FMTUSING_EXPONENT)))
                        {
                            Flags |= FMTUSING_HASDPT;
                        }
                        else if (FmtChar == '+' &&
                        	!(Flags & (FMTUSING_PLUS | FMTUSING_POSTSIGN)))
                        {
                            Flags |= (FMTUSING_PLUS | FMTUSING_POSTSIGN);
                            FmtIdx++;   // point to char that stopped scan
                            break;
                        }
                        else if (FmtChar == '-' && !(Flags & FMTUSING_PLUS))
                        {
                            Flags |= FMTUSING_POSTSIGN;
                            FmtIdx++;   // point to char that stopped scan
                            break;
                        }
                        else if (FmtChar == '^' &&
                            '^' == ReadRandom51((WORD)(FmtStr + FmtIdx + 1)) &&
                            '^' == ReadRandom51((WORD)(FmtStr + FmtIdx + 2)) &&
                            '^' == ReadRandom51((WORD)(FmtStr + FmtIdx + 3)))
                        {
                            Flags |= FMTUSING_EXPONENT;
                            FmtIdx += (BYTE) 4;   // point to char that stopped scan
                        }
                        else break;   // FmtIdx points to char that stopped scan

                        FmtIdx++;
                    }

                    // Print Now
					FoundFmtField = TRUE;
                    FmtIdx--;      // will be re-incremented later
                    if (PreDec > 15 || PostDec > 15 || PreDec + PostDec == 0)
                    	goto ParameterError;
                    Flags |= (WORD)(((PreDec & 0x0F) << 4) | (PostDec & 0x0F));
	                if (GetParameter(TOKEN_FLOAT_CONST)) goto MissingOpError;
					format_using(uData.fVal, Flags);
                    Flags = 0;
                    PreDec = 0;
                    PostDec = 0;
                    break;



	            // literals

	            case '_':    // literal override
	                // This character causes the character that immediately follows
	                // to be treated as a literal.  This includes the '_' also.
	                if (FmtIdx + 1 != FmtLen)   // make sure its not last char
	                {
	                    FmtIdx++;
				        FmtChar = ReadRandom51((WORD)(FmtStr + FmtIdx));
	                }
	                // Fall through

	            default:     // everything else is a literal
PrintLiteral:
					VGA_putchar(FmtChar);    // just print literal
                    Flags = 0;
                    PreDec = 0;
                    PostDec = 0;
                    break;
	        } // end switch()


	    }

	    if (!FoundFmtField)   // error - no format specifiers
    	{
        	SyntaxErrorCode = ERROR_PARAMETER;
	        return(TRUE);
		}

        if (TerminalChar()) break;
    }  // end while (TRUE)


    return(FALSE);

ParameterError:
    SyntaxErrorCode = ERROR_PARAMETER;
    return(TRUE);

MissingOpError:
    if (SyntaxErrorCode == ERROR_NONE)
        SyntaxErrorCode = ERROR_MISSING_OPERAND;

    return(TRUE);




}


// Process print command

BIT PrintNormal(void)
{
    BYTE B;
    BIT  PrintCR;

    PrintCR = TRUE;       // print a CR when terminal token found
    while (1)
    {
        B = GetSimpleExpr();
        if (B)
        {
            PrintCR = TRUE;         // True until proven otherwise
	       	if (B == TOKEN_INTL_CONST)
            	VGA_printf("%ld", uData.LVal);
	       	else if (B == TOKEN_FLOAT_CONST)
            {
                format_using(uData.fVal, FMTUSING_PLAIN | FMTUSING_HASDPT);
            }
	       	else if (B == TOKEN_STRING_CONST)// string
	       	{
	           	for (B = 0; B < uData.sVal.sLen; B++)
	   				VGA_putchar(ReadRandom51(uData.sVal.sPtr++));
	       	}
        }

	    if (CurChar == TOKEN_COMMA)
        {
        	VGA_print("\t");
            PrintCR = FALSE;
        }
    	else if (CurChar == TOKEN_SEMICOLON)
        {
            // Mark that last token was a semicolon
            PrintCR = FALSE;
        }
        else // Terminal char or error
        {
        	if (PrintCR) VGA_print("\n");
            break;
        }

    }

    if (TerminalChar()) return(FALSE);
    else return(TRUE);

}


// Process print command.
//

BIT DoPrintCmd(void)
{
    // check for special stuff
	CurChar = PeekStream51();

    // Is there an @ sign
    if (CurChar == TOKEN_AMP)     // set cursor to screen address
    {
        BYTE B;
        WORD off;

        ReadStream51();
        B = GetSimpleExpr();
        if (B == TOKEN_FLOAT_CONST)
        {
            B = TOKEN_INTL_CONST;
            uData.LVal = uData.fVal;
        }

        // Check syntax
        if (CurChar != TOKEN_COMMA || B != TOKEN_INTL_CONST) return(TRUE);
        off = (WORD) uData.LVal;

        GotoXY((BYTE)(off % VID_COLS), (BYTE)(off / VID_COLS));

    	CurChar = PeekStream51();

    }

    // Is there a USING token?
    if (CurChar == TOKEN_USING)    // Formatted printing
    {
        ReadStream51();
        return(PrintUsing());
    }

	return(PrintNormal());
}

// Advance DATAptr to the next DATA element.
// On entry, DATAptr must be NULL or pointing to a comma or '\r'.
// Return TRUE on error including EOF.

BIT AdvanceDATAptr(void)
{
    if (DATAptr == NULL)
    {
        // Point to first line number
        DATAptr = BasicVars.ProgStart;
    }
    else
    {
        BYTE ch;

        ch = ReadRandom51(DATAptr++);
        if (ch == TOKEN_COMMA) return(FALSE);
        else if (ch != '\r')   // illegal token
        {
            SyntaxErrorCode = ERROR_SYNTAX;
            return(TRUE);
        }
        // DATAptr now points to next Line number if here.
    }

    // If here, DATAptr points to a line number and needs to be advanced to
    // the next DATA statement.

    while (DATAptr < BasicVars.VarStart)
    {
        BYTE Len;

        DATAptr += (WORD) 2;   // advance over line number
        Len = (BYTE) ReadRandom51(DATAptr++);
        if (ReadRandom51(DATAptr) == TOKEN_DATA)  // found one
        {
            DATAptr++;   // advance to data element
            return(FALSE);
        }
        DATAptr += Len;
    }

    // if here, then we went EOF.
    SyntaxErrorCode = ERROR_DATA_EOF;

    return(TRUE);
}



// Find next data Item and read it
// Special Rules:
// A single DATA statement must appear on its own line with no other commands.
// DATA statements in any other location are ignored, but are not errors.
// Multiple DATA items can occur on a DATA line separated by commas.
// All string data elements must be quoted.  Numerical data is not quoted.
// Data elements must be read into a variable of the same type or an error occurs.
// The DATAptr variable is NULL indicating no reads have occured or
// points to a comma or '\r' which occur after a data element.
// If any token besides string and numerical constants and commas are on the
// line, then a syntax error occurs.
// If there are no errors, data element is returned in uData.  If the element
// is a string, it will point to the data element in the program.


BIT ReadData(BYTE VarType)
{
    if (VarType < TOKEN_INTL_CONST && VarType > TOKEN_STRING_CONST)
        goto TypeError;

    // Advance DATAptr to next DATA element, return on error.
    if (AdvanceDATAptr()) return(TRUE);
    else
    {
        BYTE Tok;

        Tok = ReadRandom51(DATAptr++);
        // DATAptr now points to the actual data.

        // check for incompatability
    	if (VarType != Tok)
        {
            if (VarType == TOKEN_STRING_CONST ||
                Tok == TOKEN_STRING_CONST ||
                Tok == TOKEN_FLOAT_CONST)
        	{
TypeError:
			    SyntaxErrorCode = ERROR_TYPE_CONFLICT;
			    return(TRUE);
            }
        }

        // DATA element is compatable if here
        if (VarType != TOKEN_STRING_CONST)
        {
            // read in case they are the same numerical type
            uData.LVal = ReadRandomLong(DATAptr);
            DATAptr += (WORD) 4;

            if (VarType != Tok)
            {
                // This means we are reading an int constant
                // into a float variable
                uData.fVal = uData.LVal;
            }
        }
        else    // its a string constant
        {
            uData.sVal.sLen = ReadRandom51(DATAptr++);
            uData.sVal.sPtr = DATAptr;
            DATAptr += uData.sVal.sLen;
        }

    }

    return(FALSE);   // no errors - uData valid

}



// Input command helper
// Also used for ReadCommand.
// Mode = 0 for Input, 1 for Read command, 2 for LINE INPUT.
// Return TRUE on error with SyntaxErrorCode set.

BIT InputHelper(BYTE Mode)
{
	BYTE VarType;
    WORD VarPtr;
    WORD CodePos;


    CodePos = GetStreamAddr();  // save start in case of error

    while (1)   // repeat for each variable in input command
    {
        // Parse input command up to comma or other terminal char
        CurChar = Expression(EXPRESSION_ASSIGNMENT_FLAG);

        // The calc stack should have the address of the variable to assign.
        // If not, then its an error.

        if (SyntaxErrorCode || StackEmpty(CALC_STACK))
        {
	        SyntaxErrorCode = ERROR_SYNTAX;
	        goto Error;
	    }

        // Pop the stack and get the variable type and pointer.
        VarType = PopStk(CALC_STACK);
        VarPtr = uData.sVal.sPtr;

	    // Get input into uData
        if (Mode == 2)   // READ for DATA command
        {
        	// Read the next item from the current data command
            if (ReadData((BYTE)(VarType - 4)))
            {
                // Unexpected token
                return(TRUE);
            }
        }
        else if (Mode == 0)  // regular input command
        {
		    if (ReadInputLine((BYTE)(VarType - 4)))
	        {
	            // There was a fatal input error, make user re-enter everything
	            SetStream51(CodePos);
            	VGA_print("\nIMPROPER INPUT - REDO FROM START\n");
			    FieldLen = 0;
			    FieldStart = 9999;
        	    continue;
            }
        }
        else  // Mode == 1 - LINE INPUT
        {
            if (VarType != TOKEN_STRING_VAR)
            {
                SyntaxErrorCode = ERROR_TYPE_CONFLICT;
                goto Error;
            }
       	    FieldStart = (WORD) (CurPosY * VID_COLS + CurPosX);
		    FieldLen = 0;
		    EditScreen(0);  // Sets FieldStart and FieldLen to screen edit field

            uData.LVal = 0;
            uData.sVal.sLen = FieldLen;

            if (FieldLen != 0)
            {
		        // allocate temp storage and copy
    		    uData.sVal.sPtr = TempAlloc(FieldLen);
                if (uData.sVal.sPtr)
                {
            	    WriteBlock51((WORD)(uData.sVal.sPtr),      // copy string
            	    		 &VIDEO_MEMORY[FieldStart],
                             uData.sVal.sLen);
                }

	        }
	        FieldLen = 0;

            if (!TerminalChar()) SyntaxErrorCode = ERROR_SYNTAX;
        }

        // ReadInputLine returns SyntaxErrorCode on a syntax error.
        if (SyntaxErrorCode) goto Error;

	    // Store udata in variable
    	WriteVar(VarType, VarPtr);

        // Check for terminal char or comma
        if (TerminalChar() || CurChar != TOKEN_COMMA) break;  // no more input
    }

    return(FALSE);

Error:
    return(TRUE);
}

// Return TRUE if Error
// Mode = 0 for regular input, 1 for Line Input
// INPUT <constant string prompt ';'> variable <,variable><,variable...>

BIT DoInputCmd(BYTE Mode)
{
    CurChar = PeekStream51();    // PEEK at next token

    // check for string constant prompt
    if (CurChar == TOKEN_STRING_CONST)
    {
        GetNextToken();   // Get string descriptor in uData
        VGA_PrintString(0);

        if (ReadStream51() != TOKEN_SEMICOLON) return(TRUE);  // syntax error
    }


    {      // <-- Done this way to allow local variable
        BIT ret;

	    FieldLen = 0;       // Indicates a new edit line needs to be gotten
    	FieldStart = 9999;  // indicates multiple lines of input
	    ret = InputHelper(Mode);
    	if (FieldLen && SyntaxErrorCode == 0) VGA_print("\n?Extra Ignored!\n");
    	VGA_putchar('\n');

	    return(ret);
    }
}

BIT DoLineInputCmd(void)
{
    CurChar = ReadStream51();
    if (CurChar != TOKEN_INPUT)
    {
        SyntaxErrorCode = ERROR_TYPE_CONFLICT;
        return(TRUE);
    }

    return(DoInputCmd(1));
}


// Set Data Pointer to a specific line - Set to the begining if no line.
// Returns error if anything else on line.

BIT DoRestoreCmd(void)
{
    CurChar = GetNextToken();
    if (TerminalChar())   // retore to begining if no line num
    {
        DATAptr = 0;
	}
    else  // line number supplied and is in uData
    {
        WORD Line;

        if (ForceLineNumber(CurChar)) return(TRUE);  // bad line number
        Line = (WORD) uData.LVal;

        DATAptr = FindLinePtr(Line, FALSE);
        if (DATAptr == BasicVars.ProgStart) DATAptr = 0;
        if (DATAptr) DATAptr--;  // must point to actual data
    }

    return((BIT)!GetTerminalToken());
}


// get a listing of the files in the current basfile directory

void DoListFilesCmd(void)
{
    BYTE Count;

    Count = 0;
	FindBasFile(NULL);   // reset file find routine

    while(1)
    {
        FindBasFile(TokBuf);
        if (TokBuf[0] == 0) break;   // no more
        Count++;
        if (Count > 1) VGA_print(", ");
        if (CurPosX + strlen((char *) TokBuf) + 1 >= VID_COLS) VGA_putchar('\n');
        VGA_print((char *) TokBuf);
    }
    VGA_printf("\n\n%d files found\n", Count);
}

BIT DoLoadFileCmd(void)
{
    BYTE Token;
    BIT NewLineFlag, LabelFlag;
    WORD W, LastLine, LabelTable;
    struct
    {
        BYTE Hash[4];
        WORD Line;
    } Label;

    LastLine = 0;
    LabelTable = (WORD)(BasicVars.RamTop + 1);
    Running = FALSE;
    LabelFlag = FALSE;
    ClearEverything();

    Token = GetSimpleExpr();
    if (Token == 0)
    {
	    SyntaxErrorCode = ERROR_MISSING_OPERAND;
    	goto Error;
    }
    else if (Token != TOKEN_STRING_CONST)
    {
	    SyntaxErrorCode = ERROR_TYPE_CONFLICT;
    	goto Error;
    }

    if (BasFileOpen(TRUE))
    {
	    SyntaxErrorCode = ERROR_NOT_FOUND_FILE;
    	goto Error;   // True for reading
    }

    NewLineFlag = TRUE;
    while ((W = BasReadByte()) != 0xFFFF)
    {
        if (NewLineFlag)
        {
            // Mark the field start
            FieldStart = (WORD)(CurPosY * VID_COLS + CurPosX);
            FieldLen = 0;
            NewLineFlag = FALSE;
        }

        // Newline character starts processing.
        if (W == '\n')
        {
        	WORD Line;
            BYTE B;

            // prepare for next line
   	        NewLineFlag = TRUE;

            // Skip leading spaces
            while ((B = VIDEO_MEMORY[FieldStart]) == ' ' || B == '\t')
                FieldStart++;

            // check for secret comments
            if (VIDEO_MEMORY[FieldStart] == ';') goto SkipLine;

            // Convert to tokenized form
            if (TokenizeLine(&VIDEO_MEMORY[FieldStart], FieldLen)) goto Error2;

            // check for blank lines and don't store them
            if (TokBuf[2] > 1)  // blank line still has '\r'
            {
	            // Get the Line number
			 	Line = (WORD)(((WORD) TokBuf[0] << 8) | (WORD) TokBuf[1]);

        	    if (Line == 0xFFFF)   // no line number specified
            	{
                    // Special case for RUN command
                    if (TokBuf[3] == TOKEN_AUTO && TokBuf[4] == TOKEN_RUN)
                    {
                        Running = TRUE;
                        continue;
                    }
                    else goto SupplyNewLineNumber;
            	}

	            // Check if line already exists
				if (FindLinePtr(Line, TRUE))    // Error: Line already exists
                {
SupplyNewLineNumber:
           			// Change to last line number plus 10
                	Line = (WORD)(LastLine + 10);
	        		TokBuf[0] = (BYTE)(Line >> 8);    // High Byte
					TokBuf[1] = (BYTE)(Line & 0xFF);  // Low Byte
                }

            	// Print error code if any.
	        	if (SyntaxErrorCode != ERROR_NONE) goto Error2;

                if (TokBuf[3] == TOKEN_LABEL) // check for label definition
                {
                	// Add label to label table
                    LabelFlag = TRUE;
                    Label.Line = Line;
                    memcpy(&Label.Hash[0], &TokBuf[4], 4);  // copy hash
                    LabelTable -= (WORD) 6;  // make room
                    WriteBlock51(LabelTable, (BYTE *) &Label, 6);

                    // Make the rest of TokBuf[] a comment
                    TokBuf[3] = TOKEN_REM;
                    memset(&TokBuf[4], ' ', 4);  // change hash to blanks
                    // Store modified line as usual
                }

            	// Save line in RAM
	            StoreBasicLine(NULL);

                // Save old Line number
        	    LastLine = Line;

			}  // if not a blank line

        } // if newline char

SkipLine:
        VGA_putchar((BYTE)(W));  // increments FieldLen
    }
    BasFileClose();

    // Pass 2 for label fixups
    if (LabelFlag)
    {
        BYTE Token;
        WORD LabPtr;

        // Go through newly loaded program and change labels to real values
        // If Label isn't found in label table, then use 999999 which should
        // kick out an invalid line number error.

        NewLineFlag = FALSE;    // reused as error flag
        SetStream51(BasicVars.ProgStart);
        while (GetStreamAddr() < BasicVars.VarStart)
        {
            Token = GetNextToken();
            if (Token == '\r')
            {
                StreamSkip(3);
                continue;
            }

            if (Token == TOKEN_LABEL)   // found a label, hash is in uHash
            {
                WORD StrPtr;

                // Look up label
                LastLine = 0xFFFF;
                LabPtr = LabelTable;
                while (LabPtr != (WORD)(BasicVars.RamTop + 1))
                {
                    ReadBlock51((BYTE *)&Label, LabPtr, sizeof(Label));
                    if (memcmp(&uHash, &Label, 4) == 0)  // found it
                    {
                    	LastLine = Label.Line;
                        break;
                    }

                    LabPtr += (WORD) sizeof(Label);
                }

                // Point back to Token
                StrPtr = (WORD)(GetStreamAddr() - 5);

                // reWrite Label with valid line number if found
                if (LastLine == 0xFFFF)   // not found
                {
                    NewLineFlag = TRUE;  // error flag - didn't find label
                    StrPtr++;     // Advance pointer to line number
                }
                else   // Label Found
                {
                    // Change LABEL to Line number
                    WriteRandom51(StrPtr++, TOKEN_INTL_CONST);
                }

                // Write new line number
                WriteRandomLong(StrPtr, (DWORD) LastLine);
         	}
		}

        // Search label table for duplicates
        while (LabelTable != (WORD)(BasicVars.RamTop + 1))
        {
            ReadBlock51((BYTE *)&uHash, LabelTable, sizeof(uHash));
            LabelTable += (WORD) sizeof(Label);
            LabPtr = LabelTable;
            while (LabPtr != (WORD)(BasicVars.RamTop + 1))
            {
                ReadBlock51((BYTE *)&Label, LabPtr, sizeof(Label));
                if (memcmp(&uHash, &Label, 4) == 0)  // found duplicate
                {
                    VGA_printf("Duplicate Label found on line: %d\n", Label.Line);
                    NewLineFlag = TRUE;  // error flag
                }
	            LabPtr += (WORD) sizeof(Label);
            }
        }

        if (NewLineFlag)   // error flag - could not find label
        {
			SyntaxErrorCode = ERROR_INVALID_LINE_NUMBER;
            Running = FALSE;
        	goto Error;
        }
    }

	if (Running)
    {
        // RUN command found
        // Start running the program
        SetStream51(BasicVars.ProgStart);
		ClearVariables();
        CurChar = '\r';
        VGA_ClrScrn();
    }

    return(FALSE);


Error2:
	VGA_putchar('\n');
	BasFileClose();

Error:
	Running = FALSE;
    return(TRUE);
}


BIT DoSaveFileCmd(void)
{
    BYTE Token;

//    VGA_print("Save File\n");

    // Get filename
    Token = GetSimpleExpr();
    if (Token != TOKEN_STRING_CONST) goto Error;

    if (BasFileOpen(FALSE)) goto Error2;   // False for Writing

    ListProgram(0xFFFF, 0xFFF0);

    BasFileClose();

    return(FALSE);

Error2:
    SyntaxErrorCode = ERROR_FILE_IO;
    return(TRUE);

Error:
    SyntaxErrorCode = ERROR_TYPE_CONFLICT;
    return(TRUE);
}




