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
FLOATS.C - Floating point routines.
*/


#include "Bas51.h"
#include "math.h"




float bas51_modf(float val, float *intpart)
{
    signed char exp;
    union
    {
        FLOAT_TYPE fs;
        float fb;
        BYTE b[4];
    } x;

    x.fb = val;
    exp = (signed char)(x.fs.exponent - 127);

    // check for exclusives
    if (exp <  0)   // all fraction
    {
        *intpart = 0;
        return(val);
    }

    if (exp > 22)    // all integer
    {
        *intpart = val;
        return(0.0);
    }

    // split mantissa
    *intpart = (float)((long) val);

    x.fb = val - *intpart; 

    // add bias for short fractions
    if (exp > 12  && x.fb > 0.0) x.fs.mantissa |= (1 << (exp - 2));

    // do fractional part
    return(x.fb);

}





#define NAN			0xFFFFFFFF
#define POSINF		0x7F800000
#define NEGINF		0xFF800000
#define	MAXDIGITS	12

#define F2DWORD(x)  ( *((DWORD *)&(x)) )
#define ISNAN(x)    ( F2DWORD(x) == NAN )
#define ISPOSINF(x) ( F2DWORD(x) == POSINF )
#define ISNEGINF(x) ( F2DWORD(x) == NEGINF )


// Convert float to null terminated decimal string.
// fArg is the input float.
// Buf is the return buffer and must be MAXDIGITS bytes in size.
// nDigits is the number of digits after the DP in F mode, or 0 in E mode.
// pDecimalPoint is a pointer to an integer that will recieve the decimal point.
// pSign is a pointer to an integer that receives the sign.  Zero is positive.
// Returns a pointer to the input buffer.
// Returns a text string "NaN", "+INF", or "-INF" if appropriate.
// The decimal point character is not returned in the string.
// Its possible that the length of the string will be shorter than requested.


char *
bas51_cvt(float fArg, char Buf[MAXDIGITS], BYTE nDigits,
		    signed char *pDecimalPoint, BYTE *pSign)
{
    signed char DecimalPoint;
    float fInteger, fFraction;
    BYTE DigIdx;
    BIT RoundFlag, BaseIntegerFlag;

    memset(Buf, 0, MAXDIGITS);       // initialize buffer

    DecimalPoint = 0;
    *pSign = 0;

    // Check for special numbers
    if (fArg == 0.0)
    {
        Buf[0] = '0';
	    *pDecimalPoint = 1;
		return (Buf);
    }
    else if (ISNAN(fArg))       // not a number
    {
        strcpy(Buf, "NaN ");
        goto ExitSpecial;
    }
    else if (ISPOSINF(fArg))    // Positive Infinity
    {
        strcpy(Buf, "+INF");
        goto ExitSpecial;
    }
    else if (ISNEGINF(fArg))    // Negative Infinity
    {
    	strcpy(Buf, "-INF");
ExitSpecial:
        *pDecimalPoint = 4;
        return(Buf);
    }

    if (fArg < 0.0)
    {
        // for negative numbers, just record the sign and convert to positive.
		*pSign = 1;
		fArg = -fArg;
    }

    DigIdx = 0;
    BaseIntegerFlag = (BIT)(fArg <= (float) 16777215.0);  // integer without exponent
    fArg = bas51_modf(fArg, &fInteger);   // split fArg into int and fraction parts
    // fArg now contains only the fractional part

    if (fInteger != 0.0)    // Do integer part if it exists
    {
        // This loop successively divides the integer float by 10 until
        // there are no more digits.  It takes a large buffer to do this
        // because exponent numbers can get quite large.

		while (fInteger != 0.0)
        {
            BYTE b;

            // divide the integer by 10 and save the remainder as a digit
	    	fFraction = bas51_modf(fInteger/10, &fInteger);

			// Scroll buffer to make room for new digit
            for (b = MAXDIGITS - 1; b != 0; b--)
            	Buf[b] = Buf[b - 1];

            // Store new digit in first byte
			Buf[0] = (char)((int) ((fFraction + 0.05) * 10) + '0');   // djh

            // Bump Decimal point
	    	DecimalPoint++;
            DigIdx++;
		}
    }
    else if (fArg > 0)   // No integer, but there is a fractional part
    {
        // Count the fractional leading zeros
		while ((fFraction = fArg * 10) < 1)
        {
	    	fArg = fFraction;
	    	DecimalPoint--;
		}
    }

    // Save our decimal point
    *pDecimalPoint = DecimalPoint;

    RoundFlag = FALSE;
    if (fArg != 0.0 && DigIdx <= 6)	// No fraction or large integer
    {
    	// Do the fractional part.

        // Compute the max digits to convert
        if (nDigits == 0) nDigits = 6;
        else nDigits += DigIdx;
        if (nDigits > 6) nDigits = 6;

        // nDigits is now the max digits
	    // Multiply the fractional part by 10 until the buffer is filled.
    	while (DigIdx < nDigits && fArg != 0.0)
	    {
			fArg *= 10.0;
            fArg = bas51_modf(fArg, &fInteger);
			Buf[DigIdx++] = (char)((int) fInteger + '0');
		}

        if (fArg > 0.4) RoundFlag = TRUE;   // do we need to round
    }
    else   // Integer only or large integer with too small fraction
    {
        if (!BaseIntegerFlag && DigIdx > 6) DigIdx = 6;   // 6 digits
    }
    Buf[DigIdx] = 0;


    // At this point, DigIdx points to the null terminator.

    while (DigIdx--)
    {
        if (RoundFlag) Buf[DigIdx]++;
        RoundFlag = FALSE;

        if (Buf[DigIdx] > '9')   // we need to carry to previous digit
        {
            RoundFlag = TRUE;   // carry to previous digit
			Buf[DigIdx] = 0;    // Null terminate as we round
        }
        else if (Buf[DigIdx] == '0')
        {
			Buf[DigIdx] = 0;    // Null terminate as we round
        }
        else break;

    }

    if (Buf[0] == 0)    		// we went from .99999 to 00000
    {
	   	Buf[0] = '1';   		// change to 1.00000
	    (*pDecimalPoint)++;     // Adjust returned decimal point
        return(Buf);
    }

    return (Buf);
}








//#define FMTUSING_DOLLAR     0x8000  // Prints a floating dollar
//#define FMTUSING_STAR       0x4000  // Fills int field with leading '*'s
//#define FMTUSING_COMMA      0x2000  // places comma separators every 3 digits
//#define FMTUSING_PLAIN      0x1000  // prints G style format
//#define FMTUSING_HASDPT     0x0800  // The field has a decimal point
//#define FMTUSING_EXPONENT   0x0400  // prints trailing E+00
//#define FMTUSING_PLUS       0x0200  // prints '+' for positive sign
//#define FMTUSING_POSTSIGN   0x0100  // prints sign after number, else front

#define PLAIN_FMT_SIZE     8

// The low byte of flags contains the decimal places to print.  The high
// nibble is the integer digits, the low nibble is the fractional digits.
// The max digits is 6 pre and 6 post.  The digit sizes do not include the
// sign character which is mandatory either on the right or left side of
// the field.

void format_using(float Value, WORD flags)
{
    BYTE lead_digits, post_digits;
    signed char dec;
    BYTE sign;
    char *Buf;
    BIT PrePlus, PostSign;
    char Buffer[MAXDIGITS];

    lead_digits = (BYTE)((flags >> 4) & 0x000F);
    post_digits = (BYTE)(flags & 0x000F);

    PostSign = (BIT) (flags & FMTUSING_POSTSIGN);
    PrePlus = (BIT)((flags & FMTUSING_PLUS) && !PostSign);

    // Format into string

    if (flags & (FMTUSING_EXPONENT | FMTUSING_PLAIN))
    	Buf = bas51_cvt(Value, Buffer, 0, &dec, &sign);
    else
    	Buf = Buf = bas51_cvt(Value, Buffer, post_digits, &dec, &sign);

    if (!isdigit(Buf[0]))      // Check for special numbers
    {
         VGA_print(Buf);
         return;
    }

    // Special case for PLAIN format
    if (flags & FMTUSING_PLAIN)
    {
    	float aVal;

        // Decide which is best, standard or exponent

        flags = FMTUSING_PLAIN;   // kill off other flags for now
        aVal = fabs(Value);       // Get absolute value
        lead_digits = 2; // the '%' character is suppressed in plain mode

        if (aVal == 0.0 || (aVal >= 0.000001 && aVal <= (float) 16777215.0))
        {
            // print standard format
        	if (dec < 0) post_digits = (BYTE)(-dec + strlen(Buf));
            else post_digits = (BYTE) strlen(Buf + dec);
        }
        else // print in expomential format
        {
	        BYTE Count;

            // Get rid of trailing zeros
            Count = (BYTE) strlen(Buf);
            while (Count-- && Buf[Count] == '0') Buf[Count] = 0;
            post_digits = (BYTE)(strlen(Buf) - 1);
            flags |= FMTUSING_EXPONENT;
        }
        if (post_digits != 0) flags |= FMTUSING_HASDPT;  // print decimal point

    }


    if (flags & FMTUSING_EXPONENT)    // format with exponent
    {
        // In exponmential mode, dollars, stars and commas are ignored,
        // but will still count as digit positions.
        // One digit position is used before the decimal point for a sign
        // unless a post sign is used.  This will be blank if positive and
        // a pre-plus was not specified.

        // After any sign space, all spaces before the decimal are used.
        // If there are no pre-decimal spaces and the sign is negative,
        // then a '%' overflow character is emitted.

        if (!PostSign && sign && lead_digits == 0)
            VGA_putchar('%');

        // print the sign if appropriate
        if (!PostSign)
        {
        	VGA_putchar((char)(sign ? '-' : (PrePlus ? '+' : ' ')));
            if (lead_digits) lead_digits--;
        }

        // lead_digits is now the true number of leading digits without
        // respect to the sign.

        // A special exception is for zero.  With zero, if there is more
        // than 1 lead digit with a post-sign or 2 digits with a pre-sign,
        // all but the sign get padded with spaces.

        if (Value == 0.0)
        {
            if (lead_digits)
            {
                lead_digits--;   // take off one for the zero
                while (lead_digits-- != 0) VGA_putchar(' ');
                VGA_putchar('0');
            }
            if (flags & FMTUSING_HASDPT) VGA_putchar('.');
            while (post_digits-- != 0) VGA_putchar('0');
            VGA_print("E+00");
            return;
        }

        // print integer part
        while (lead_digits-- != 0)
        {
            VGA_putchar(*Buf++);
            dec--;
        }


        if (flags & FMTUSING_HASDPT) VGA_putchar('.');

        while (post_digits-- != 0) VGA_putchar(*Buf++);
        VGA_putchar('E');
        if (dec < 0)
        {
            dec = (signed char) -dec;   // take absolute value
            VGA_putchar('-');
        }
        else VGA_putchar('+');
        VGA_putchar((char)((dec / 10) + '0'));
        VGA_putchar((char)((dec % 10) + '0'));

        // Add post sign if appropriate
        if (PostSign)
        {
            char sign_char;

            sign_char = (char)((flags & FMTUSING_PLUS) ? '+' : ' ');
            if (sign) sign_char = '-';
            VGA_putchar(sign_char);
        }
    }
    else   // standard format
    {
        BYTE NumCommas, LeadZeros;
        signed char NumPad;
        BIT FillZero;

        LeadZeros = 0;
        FillZero = FALSE;
		if (dec < 0)
        {
            LeadZeros = (BYTE) -dec;
            dec = 0;
        }

		if (flags & FMTUSING_COMMA) NumCommas = (BYTE)((dec - 1) / 3);
		else NumCommas = 0;

		NumPad = (signed char)(lead_digits - dec - NumCommas);

        // the sign will take one space
		if (!PostSign && (sign || PrePlus))  NumPad--;

        // The dollar sign takes one space
        if (flags & FMTUSING_DOLLAR) NumPad--;

        // Do error char if error
		if (NumPad < 0)
        {
            if (!(flags & FMTUSING_PLAIN)) VGA_putchar('%'); // field overflow
            lead_digits -= NumPad;
            NumPad = 0;
        }

        // special case for a leading zero when there is no integer part
        if (dec == 0 && NumPad)
        {
        	NumPad--;
            FillZero = TRUE;
        }

        // Do padding - either blanks of asterisks
		while (NumPad)
		{
			VGA_putchar((char)((flags & FMTUSING_STAR) ?  '*' : ' '));
            NumPad--;
		}

        // Do sign
	   	if (!(flags & FMTUSING_POSTSIGN))   // sign at the begining
        {
            if (sign) VGA_putchar('-');
            else if (PrePlus) VGA_putchar('+');
        }

        // Do dollar
        if (flags & FMTUSING_DOLLAR) VGA_putchar('$');

        // do special case leading zero
        if (FillZero) VGA_putchar('0');

        // Do integer digits
		if (lead_digits > 0)
		{
            BYTE i;

			for (i = 0; i != dec; i++)
			{
                // print the digit
            	if (*Buf) VGA_putchar(*Buf++);
                else VGA_putchar('0');

                // Do commas if required
                if (NumCommas && ((dec - i) % 3 == 1))
                {
                	VGA_putchar(',');
                    NumCommas--;
                }
            }

		}


        // Do decimal point if requested
        if (flags & FMTUSING_HASDPT) VGA_putchar('.');

        // Do fractional digits
//        Buf += dec;
		while (post_digits--)
		{
    	    if (LeadZeros)    						// Do leading zeros
            {
            	VGA_putchar('0');
                LeadZeros--;
            }
            else if (*Buf) VGA_putchar(*Buf++);		// print actual digit
            else VGA_putchar('0');       			// print trailing zero
		}

        // Do trailing sign if specified
		if (flags & FMTUSING_POSTSIGN)
        {
            if (sign) VGA_putchar('-');
            else VGA_putchar((char)((flags & FMTUSING_PLUS) ? '+' : ' '));
		}

    }  // else standard format


}


