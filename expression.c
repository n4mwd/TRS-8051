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
EXPRESSION.C - This is the expression evaluator.  Significant effort was made
to avoid the use of recursive functions for the 8051's benefit.
*/

#include "bas51.h"




// Calculate precedence
// Return Precedence or 0 if not valid

BYTE Precedence(BYTE Token)
{
    BYTE p;

    p = 0;
    if (Token >= OPERATOR_TOKEN_START && Token < LAST_OPERATOR_TOKEN)
    {
		p = (BYTE)(Token - OPERATOR_TOKEN_START + 1);
        if (p > 6 && p <= 11) p = 6;       // Special case for =,<=,>=,<>,>,<
        if (p == 13) p = 12;  	           // Special case for + (add),-(minus)
        if (p == 15 || p == 16) p = 14;    // Special case for *, /, MOD
    }

    return(p);
}

// Calculate Association
// Return TRUE if token is R2L association.
// All else returns FALSE.

BIT IsR2LAssoc(BYTE Token)
{
	BIT R;

    R = FALSE;
    if (Token == TOKEN_NOT) R = TRUE;
    if (Token == TOKEN_UNARY_MINUS) R = TRUE;
    if (Token == TOKEN_POWER) R = TRUE;

    return(R);
}



// Classify the token as one of the above token classes

BYTE ClassifyToken(BYTE Token)
{
    BYTE R;

    R = INVALID_CLASS;
    if (Token >= TOKEN_OR && Token <= TOKEN_POWER) R = OPERATOR_CLASS;

    if (Token >= FUNCTION_TOKEN_START && Token < LAST_FUNCTION_TOKEN)
    	R = FUNCTION_CLASS;

    if (Token >= TOKEN_INTL_CONST && Token <= TOKEN_STRING_CONST)
    	R = CONSTANT_CLASS;

    if (Token >= TOKEN_NOTYPE_VAR && Token <= TOKEN_STRING_VAR)
    	R = VARIABLE_CLASS;

    if (Token >= TOKEN_NOTYPE_ARRAY && Token <= TOKEN_STRING_ARRAY)
    	R = ARRAY_CLASS;

    if (Token >= COMMAND_TOKEN_START && Token <= LAST_COMMAND_TOKEN)
        R = COMMAND_CLASS;

    return(R);
}





// Parse an expression in memory
// Uses ReadStream51() to get the next char to evaluate.
// Assignment is TRUE if this is part of a LET statement.
// returns the token that stopped the scan.
// Allowed Flags: EXPRESSION_ASSIGNMENT_FLAG, EXPRESSION_DIM_FLAG

BYTE Expression(BYTE Flags)
{
    BYTE Token, Class, LastToken, Parenthesis;
//    Len, b, A,

    Parenthesis = 0;
    InitStacks();

    Token = ReadStream51();

    while (Token != '\r' && Token != TOKEN_COLON && SyntaxErrorCode == NULL)
	{
        Class = ClassifyToken(Token);

        // Number and vaiables get sent straight to the Calc Stack
		if (Class == CONSTANT_CLASS)
		{
            BYTE B, Len;

            // Read into uData and push
            Len = Read2uData2(Token, GetStreamAddr());
            for (B = 0; B != Len; B++) ReadStream51();  // keep in sync
			PushStk(CALC_STACK, Token);

            // Check if this value is a function argument
    		if (!StackEmpty(PARM_STACK))
			{
            	// This is a function argument.
                // Replace value at top of Parm_stack with TRUE to
                // indicate that an argument has been found.
				PopStk(PARM_STACK);
				PushStk(PARM_STACK, TRUE);
			}
		}

        // Variables get looked up then pushed
        else if (Class == VARIABLE_CLASS)
		{
            if ((Flags & EXPRESSION_ASSIGNMENT_FLAG) && Parenthesis == 0)
            {
                // ASSIGN a simple variable
            	// Get address of variable and push it to calc stack
                uData.sVal.sPtr = GetProgVarPtr(VARPTR_ASSIGN_SIMPLE);

                // Correct for untyped variables
                if (Token == TOKEN_NOTYPE_VAR)
                    Token += (BYTE) uHash.str.TypeFlag;

			    // Push to calc stack
				PushStk(CALC_STACK, Token);

                // only one time
                Flags &= ~EXPRESSION_ASSIGNMENT_FLAG;

            }
            else if ((Flags & EXPRESSION_DIM_FLAG) && Parenthesis == 0)
            {
                // DIM STATEMENT definition of SIMPLE variable
                // Get the hash from the program file, then create variable
                // and assign it to zero.
                GetProgVarPtr(VARPTR_ASSIGN_SIMPLE);
            }
            else  // Read Var and Convert into constant
            {
            	// Read into uData and push
            	if (GetSimpleVar())    // Read variable into uData
            	{
					SyntaxErrorCode = ERROR_UNDEFINED_VARIABLE;
					return(Token);
            	}

                // Correct for untyped variables
                if (Token == TOKEN_NOTYPE_VAR)
                    Token += (BYTE) uHash.str.TypeFlag;

			    // Push to calc stack
				PushStk(CALC_STACK, (BYTE)(Token - 4));  // convert to constant
            }


    		if (StackEmpty(PARM_STACK) == FALSE)
			{
                // replace value at top of Parm_stack with TRUE.
				PopStk(PARM_STACK);
				PushStk(PARM_STACK, TRUE);
			}
		}

        // Arrays are treated like functions
        else if (Class == ARRAY_CLASS)
		{
            if (Flags & EXPRESSION_DIM_FLAG)  // declaring array
            {
                BYTE c;

                // Read hash into uHash and return existing variable address
    	        uData.sVal.sPtr = GetProgVarPtr(VARPTR_ARRAY);
                if (uData.sVal.sPtr) DeleteArray();  // Delete pre-exisiting array

                // special case: push array hash to operator stack
                for (c = 0; c != 4; c++)
                    PushStk(OPERATOR_STACK, uHash.b[c]);
            }
            else // not declaring array
            {
	            // Get the base address of array and push it to operator stack
    	        uData.sVal.sPtr = GetProgVarPtr(VARPTR_ARRAY);
        	    if (!uData.sVal.sPtr)
            	{
					SyntaxErrorCode = ERROR_UNDEFINED_VARIABLE;
					return(Token);
				}

				// Push array address and array token to operator stack
            	// Special case since most operators are only one byte.
				PushStk(OPERATOR_STACK, uData.bVal[0]);
				PushStk(OPERATOR_STACK, uData.bVal[1]);
            }

            // Correct for untyped variables
            if (Token == TOKEN_NOTYPE_ARRAY)
                Token += (BYTE) uHash.str.TypeFlag;

            // Push array token to operator stack
			PushStk(OPERATOR_STACK, Token);

    		if (!StackEmpty(PARM_STACK))
			{
                // replace value at top of Parm_stack with TRUE.
               	// This function is a parameter to another function.
				PopStk(PARM_STACK);
				PushStk(PARM_STACK, TRUE);
			}

            // reset arg counter for new function
            PushStk(ARG_STACK, 0);       	// no args yet
		   	PushStk(PARM_STACK, FALSE);
		}


		else if (Class == FUNCTION_CLASS)
		{
            // Check for parameterless functions
            if (Token < TOKEN_ABS)
            {
                // There are no parameters or parenthesis so evaluate now
                // Convert to a constant
                EvaluateFunction(Token, 0);
            }
            else   // this is a regular function
            {
				PushStk(OPERATOR_STACK, Token);   // push function token

            	// Check to see if this function is an argument to a prior function.
				if (!StackEmpty(PARM_STACK))
				{
                	// This function is a parameter to another function.
                	// Replace value at top of Parm_stack with TRUE to
                	// indicate that the prior function now has arguments.
					PopStk(PARM_STACK);
					PushStk(PARM_STACK, TRUE);
				}
            	// reset arg counter for new function
				PushStk(ARG_STACK, 0);       	// no args yet
				PushStk(PARM_STACK, FALSE);
            }
		}

        else if (Token == TOKEN_COMMA)
		{
            BYTE Op;

            // Check for comma outside of parenthesis
            // Not an error, but stops the scan
            if (Parenthesis == 0) break;

            // evaluate all operators for this parameter
			while ((Op = PopStk(OPERATOR_STACK)) != TOKEN_LEFT_PAREN &&
            		SyntaxErrorCode == ERROR_NONE)
			{
                if (Op == TOKEN_STACK_ERROR)
				{
					SyntaxErrorCode = ERROR_PARAMETER;
					return(Token);
				}
				Evaluate(Op);
			}  // end while()


            // Check to see if this was the opening paren for a function
            // OR Array.

			if (ClassifyToken(PeekStk(OPERATOR_STACK)) != FUNCTION_CLASS &&
            	ClassifyToken(PeekStk(OPERATOR_STACK)) != ARRAY_CLASS)
			{
                // The only time commas are allowed inside of parens are
                // when they are in a function call
				SyntaxErrorCode = ERROR_PARAMETER;
				return(Token);
            }

            // push fake left paren back on stack
            PushStk(OPERATOR_STACK, TOKEN_LEFT_PAREN);

            // Check if comma preceded by an actual argumment
			if (PopStk(PARM_STACK) == TRUE)
			{
                // Argument present so increment arg counter
				PushStk(ARG_STACK, (BYTE)(PopStk(ARG_STACK) + 1));
			}
            else   // empty comma argument
            {
				SyntaxErrorCode = ERROR_PARAMETER;
				return(Token);
            }

			PushStk(PARM_STACK, FALSE);    // Reset parameter_valid flag

		}  // end if token is a comma


		else if (Class == OPERATOR_CLASS)
		{
			LastToken = PeekStk(OPERATOR_STACK);

			while (ClassifyToken(LastToken) == OPERATOR_CLASS &&
            		SyntaxErrorCode == ERROR_NONE)
			{
				if (IsR2LAssoc(Token))   // right association
				{
					if (Precedence(Token) >= Precedence(LastToken)) break;
				}
				else  // LEFT_ASSOCIATION
				{
					if (Precedence(Token) > Precedence(LastToken)) break;
				}

                Evaluate(PopStk(OPERATOR_STACK));  // Evaluate based on operator

				LastToken = PeekStk(OPERATOR_STACK);
			}

			PushStk(OPERATOR_STACK, Token);

		}  // if token is an operator


		else if (Token == TOKEN_LEFT_PAREN)
		{
			PushStk(OPERATOR_STACK, Token);
            Parenthesis++;   // Track parenthesis depth
		}

	 	else if (Token == TOKEN_RIGHT_PAREN)
		{
            BYTE Op;

            Parenthesis--;   // Track parenthesis depth

            // Evaluate calc_stack until left paren or stack empty
			while ((Op = PopStk(OPERATOR_STACK)) != TOKEN_LEFT_PAREN  &&
            	SyntaxErrorCode == ERROR_NONE)
			{
				if (Op == TOKEN_STACK_ERROR)  // we didn't find the Left Parenthesis
				{
					SyntaxErrorCode = ERROR_PARENTHESIS_MISMATCH;
					return(Token);
				}

                Evaluate(Op);

			}  // end while()


			// NOTE: We don't replace the left parenthesis for Right paren.

            // Check to see if this was the closing paren for a function
			if (ClassifyToken(PeekStk(OPERATOR_STACK)) == FUNCTION_CLASS)
			{
                BYTE ArgCnt;

                // evaluate function
				ArgCnt = PopStk(ARG_STACK);
				if (PopStk(PARM_STACK))
                {
                    // We had a valid parameter before this right paren
                    ArgCnt++;
                }
                else   // empty function argument
            	{
					SyntaxErrorCode = ERROR_PARAMETER;
					return(Token);
            	}
                EvaluateFunction(PopStk(OPERATOR_STACK), ArgCnt);
			}

            // Check to see if this was the closing paren for an array
            // If so, get the value of the array and push it on the calc stack
			else if (ClassifyToken(PeekStk(OPERATOR_STACK)) == ARRAY_CLASS)
			{
                BYTE ArgCnt, ArrayType;

                // evaluate array
				ArgCnt = PopStk(ARG_STACK);
				if (PopStk(PARM_STACK))
                {
                    // We had a valid parameter before this right paren
                    ArgCnt++;
                }
                else   // empty function argument
            	{
					SyntaxErrorCode = ERROR_PARAMETER;
					return(Token);
            	}


                // Pop array type (original TOKEN)
                ArrayType = PopStk(OPERATOR_STACK);

                if (Flags & EXPRESSION_DIM_FLAG)  // declaring array
            	{
                    BYTE c;

                    // Pop hash off operator stack
                    for (c = 4; c != 0; c--)
                    {
                        uHash.b[c - 1] = PopStk(OPERATOR_STACK);
                    }

                    if (MakeArray(ArgCnt))     // uHash is set before calling
                        return(Token);
                }
                else  // Reading array
                {
	                // pop array base pointer
    	            uData.bVal[1] = PopStk(OPERATOR_STACK);
        	        uData.bVal[0] = PopStk(OPERATOR_STACK);

            	    // uData.sVal.sPtr now has pointer to array base
                	if (Flags & EXPRESSION_ASSIGNMENT_FLAG)
	                {
    	                // Get pointer to element and push that to calc stack
        	            PushElementPtr(ArrayType, ArgCnt);
            	        Flags &= ~EXPRESSION_ASSIGNMENT_FLAG;
                	}
		            else
        	        {
            	        // Get array value and push on calc stack
                    	EvaluateArray(ArrayType, ArgCnt);
                	}
                } // else not dim
			} // if array

		}  // if token is right parenthesis

		else if (Token == TOKEN_ASSIGN)
        {
        	// If the last token was an array, we need to evaluate it here
            // and condense it into a pointer to a specific element and
            // convert the token into a simple variable.

            // Push it on the operator stack
			PushStk(OPERATOR_STACK, Token);
        }

        // Catch tokens that are not part of expressions
        else break;

	    Token = ReadStream51();

	} // end while (Token != EOF)

	// When there are no more tokens to read:
	while (StackEmpty(OPERATOR_STACK) == FALSE  &&
    	SyntaxErrorCode == ERROR_NONE)
	{
        BYTE Op;

	    Op = PopStk(OPERATOR_STACK);
	    if (Op == TOKEN_LEFT_PAREN)
	    {
	        SyntaxErrorCode = ERROR_PARENTHESIS_MISMATCH;
	        return(Token);
	    }
		Evaluate(Op);
	}

    /*
    if (!StackEmpty(CALC_STACK))   // Calc stack
    {
	    SyntaxErrorCode = ERROR_SYNTAX;
	    return(Token);
	}
    */

    return(Token);
}





/*
Extension to the ‘Shunting Yard’ algorithm to allow variable arguments to functions
By Robin | February 18, 2008

token = NextToken().
While (token != EOF_TOKEN)
{
	Read a token.

	If (Classify(token) == NUMBER_TOKEN)
	{
		Push('Output Queue', token).
		If (StackEmpty('were values') == FALSE)
		{
			Pop ('were values').
			Push ('were values' ,TRUE).
		}
	}

	If (Classify(token) == FUNCTION_TOKEN)
	{
		Push('operators', token).
		Push('arg count', 0).
		If (empty('were values') == FALSE)
		{
			pop ('were values').
			push ('were values' ,TRUE).
		}
		Push('were values', FALSE).
	}

	If (classify(token) == FUNCTION_SEPARATOR_TOKEN)
	{
		while (StackEmpty('Operators') == FALSE &&
			StackTop('Operators') != LEFT_PARENTHESIS_TOKEN)
		{
			Push('Output Queue', Pop('Operators')).
		}  // end while()

		If (StackEmpty('Operators'))
		{
			ErrorStr = “Error: Either the separator was misplaced or
					parentheses were mismatched.“
			Exit.
		}

		W = Pop('were values').

		If (W == TRUE)
		{
			A = pop('arg count').
			A = A + 1.
			push('arg count', A).
		}

		Push('were values', FALSE).

	}  // end if token is a function separator


	If (classify(token) == OPERATOR_TOKEN)
	{
		OP1 = token.
		OP2 = StackTop('Operators').
		While (classify(OP2) == OPERATOR_TOKEN)
		{
			If (Association(OP1) == LEFT_ASSOCIATION)
			{
				if (Precedence(OP1) > Precedence(OP2))  BREAK.
			}
			else  // RIGHT_ASSOCIATION
			{
				If (Precedence(OP1) >= Precedence(OP2)) BREAK.
			}

			OP2 = Pop('Operators').
			Push('Output Queue', OP2).

			OP2 = StackTop('Operators').
		}

		push('Operators', OP1).

	}  // if token is an operator


	If (token == LEFT_PARENTHESIS_TOKEN)
	{
		Push('Operators', token).
	}

 	If (token == RIGHT_PARENTHESIS_TOKEN)
	{
		while (StackEmpty('Operators') == FALSE &&
			StackTop('Operators') != LEFT_PARENTHESIS_TOKEN)
		{
			Push('Output Queue', Pop('Operators')).
		}  // end while()

		If (StackEmpty('Operators') == TRUE)  // we didn't find Left Parenthesis
		{
			ErrorStr = “Error: Parentheses were mismatched.“
			Exit.
		}

		Pop('Operators').   // Pop parenthesis, but don't save.

		If (classify(StackTop('Operators')) == FUNCTION_TOKEN)
		{
			F.func = Pop('Operators').
			A = Pop('arg count').
			W = Pop('were values').
			If (W == TRUE) A = A + 1.
			F.arg_count = A.
			Push('Output queue', F).
		}

	}  / if token is right parenthesis

	token = NextToken().

} // end while (Token != EOF)

// When there are no more tokens to read:
While there are still operator tokens in the stack:
{
	If the operator token on the top of the stack is a parenthesis, then
		there are mismatched parenthesis.
	Pop the operator onto the output queue.
}

Exit.

*/

