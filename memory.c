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
MEMORY.C - Secondary RAM access routines.  The TRS-8051 hardware has the bulk
of its RAM located in an external chip connected to the 8051's SPI port.
Because of this, direct access to the memory is impossible and these routines
are necessary. 
*/


#include "bas51.h"


// This is the memory manager for the 8051.
// BAS51 needs to have the following memory functions:
//
// void MemInit51(void);         // Initialization routine
// void ReadBlock51(BYTE *dest, WORD addr, BYTE Len);   // read a block into buffer
// void WriteBlock51(WORD addr, BYTE *src, BYTE Len);  // Write buffer to RAM
// void SetStream51(WORD addr);  // Sets the initial stream read address
// WORD GetStreamAddr(void);  // Get the physical addr of next byte to read.
// BYTE ReadStream51(void);    // Returns the next byte from the stream.
// BYTE RandomRead51(WORD addr);  // Reads RAM byte.
// void RandomWrite51(WORD addr, BYTE data);  // Write RAM byte
// void MemMove51(WORD dest, WORD src, WORD len);  // hardware specific memmove()
//


#define STREAM_BUF_SIZE    64
BYTE StreamBuf[STREAM_BUF_SIZE];   // Stream is read 64 bytes at a time
WORD StreamBase;                   // Source address in RAM.
BYTE StreamIdx;                    // index of next byte in StreamBuf[] to read.
BIT  StreamDirtyFlag;              // Indicates Buffer needs to be re-read.




// Initialization routine
void MemInit51(void)
{
    BYTE B;
    WORD W;

    // Compute RAMTOP
    B = 0xFF;
    do
    {
        W = (WORD)(((WORD) B << 8) | 0xFF);
        WriteRandom51(W, 77);
        if (ReadRandom51(W) == 77)
        {
		    BasicVars.RamTop = W;
            break;
        }
        B--;
    } while (B != 0);

    // RAMTOP is the number of RAM bytes - 1
    BasicVars.CmdLine = (WORD)(BasicVars.RamTop - 255);

    // Stream functions cannot be used to read address 0 (NULL PTR)
    StreamBase = 0;
    StreamIdx = STREAM_BUF_SIZE;   // forces next read to fill buffer

    // Initialize Basic Vars Structure
    ClearEverything();
}



// Return the Total Bytes free and not encumbered by other stuff.

WORD BytesFree(void)
{
    return((WORD)(BasicVars.StringBot - BasicVars.GosubStackTop));
}


// Get the physical addr of next byte to read.

WORD GetStreamAddr(void)
{
    return((WORD)(StreamBase + StreamIdx));
}


// Sets the initial stream read address and fills the buffer.
// Addr cannot be set to NULL.

void SetStream51(WORD addr)
{
    // check to see if we already have the requested address in the buffer
    if (StreamBase == 0 ||
        addr < StreamBase ||
        addr >= StreamBase + STREAM_BUF_SIZE)
            StreamDirtyFlag = TRUE;

    if (StreamDirtyFlag)   // we have ro read the buffer
    {
        ReadBlock51(StreamBuf, addr, STREAM_BUF_SIZE);
        StreamBase = addr;
        StreamIdx = 0;
        StreamDirtyFlag = FALSE;
    }
    else
    {
        // we already have it in the buffer
        StreamIdx = (BYTE)(addr - StreamBase);
    }
}

// Put back the last char read from the stream so it can be read again

//void UnGetStream51(void)
//{
//    StreamIdx--;
//}

// Return next byte from stream without incrementing pointer

BYTE PeekStream51(void)
{
    return(ReadRandom51(GetStreamAddr()));
}

// Returns the next byte from the stream.

BYTE ReadStream51(void)
{
    if (StreamIdx == STREAM_BUF_SIZE)
    {
        // we have to read another 64 bytes into the buffer
        SetStream51((WORD)(StreamBase + STREAM_BUF_SIZE));
    }

    // just return the next byte in buffer without incrementing
    return(StreamBuf[StreamIdx++]);
}

// Read two bytes from the stream as a WORD
WORD ReadStreamWord(void)
{
    WORD X;

    X = ReadStream51();
    X = (WORD)((X << 8) | ReadStream51());

    return(X);
}

long ReadStreamLong(void)
{
    UVAL_DATA dat;
    BYTE b;

    for (b = 0; b != 4; b++) dat.bVal[b] = ReadStream51();

    return(dat.LVal);
}

float ReadStreamFloat(void)
{
    UVAL_DATA dat;

    dat.LVal = ReadStreamLong();

    return(dat.fVal);
}

void StreamSkip(BYTE BytesToSkip)
{
	SetStream51((WORD)(GetStreamAddr() + BytesToSkip));
}

// Reads RAM byte.
BYTE ReadRandom51(WORD addr)
{
    BYTE dest;

	ReadBlock51(&dest, addr, 1);

    return(dest);
}


// Read Random Word
WORD ReadRandomWord(WORD addr)
{
    return((WORD)((ReadRandom51(addr) << 8) | ReadRandom51((WORD)(addr + 1))));
}

// Read Random DWORD
long ReadRandomLong(WORD addr)
{
    long dat;

    ReadBlock51((BYTE *) &dat, addr, sizeof(UVAL_DATA));

    return(dat);
}


// Write RAM byte
void WriteRandom51(WORD addr, BYTE data)
{
	WriteBlock51(addr, &data, 1);
}

// Write Random Word
void WriteRandomWord(WORD addr, WORD val)
{
    WriteRandom51(addr, (BYTE)(val >> 8));        // high byte first
    WriteRandom51((WORD)(addr + 1), (BYTE)(val & 0xFF));  // Then low byte
}

// Write Random Long
void WriteRandomLong(WORD addr, long val)
{
	WriteBlock51(addr, (BYTE *) &val, sizeof(UVAL_DATA));
    if (addr < BasicVars.VarStart ||
        addr >= BasicVars.CmdLine) StreamDirtyFlag = TRUE;
}




// hardware specific memmove()
void MemMove51(WORD dest, WORD src, WORD Len)
{
    WORD Len2;
    BYTE Temp[64];

    if (Len == 0 || dest == src)	return;	 // all done

    if (dest < src)   // Copy Forwards
    {
        while (Len)
        {
            // get length to copy
            Len2 = (WORD) min(sizeof(Temp), Len);

            // Read block into buffer
            ReadBlock51(Temp, src, Len2);

            // Write RAM from buffer
            WriteBlock51(dest, Temp, Len2);

            // update pointers
            src += Len2;
            dest += Len2;
            Len -= Len2;
        }
    }
    else     // Copy Backwards
    {
        // start at the end
        dest += Len;
        src += Len;

        while (Len)
        {
            // get length to copy
            Len2 = (WORD) min(sizeof(Temp), Len);

            // update pointers
            src -= Len2;
            dest -= Len2;
            Len -= Len2;

            // Read block into buffer
            ReadBlock51(Temp, src, Len2);

            // Write RAM from buffer
            WriteBlock51(dest, Temp, Len2);


        }
    }

}






