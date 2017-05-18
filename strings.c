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
STRINGS.C - String allocation routines.
*/


#include "Bas51.h"

// =========================================
// Temporary String stack
// The temporary string stack is used for temporary string storage by the
// evaluator.  The stack is cleared upon the next command that is executed, so
// there is no need for an individual free().  It is essentially a temporary
// malloc() that gets automatically free()'ed when the instruction is over.
// =========================================


// This function clears the entire temporary string stack.

void FreeTempAlloc(void)
{
    BasicVars.TempStringBot = BasicVars.GosubStackTop;
    BasicVars.TempStringTop = BasicVars.GosubStackTop;
}


// Reserve string space on the temp string stack.  Up to 255 bytes can be
// reserved.  NumBytes == Acutal number of bytes.
// Return Word address of first byte in buffer.
// Return zero if not enough RAM available.

WORD TempAlloc(BYTE NumBytes)
{
    WORD Addr;

    Addr = BasicVars.TempStringTop;
    BasicVars.TempStringTop += (WORD) NumBytes;

    // check for a collision with string space
    if (BasicVars.TempStringTop >= (WORD) BasicVars.StringBot)
    {
        FreeTempAlloc();
        Addr = 0;
        SyntaxErrorCode = ERROR_INSUFFICIENT_MEMORY;
    }

    return(Addr);
}


// Returns TRUE if string points to programs space.
// If it does, then its a constant and can't be written to.

BIT StringPtrIsConst(WORD StrPtr)
{
    return((BIT)(StrPtr > 0 && StrPtr < BasicVars.VarStart));
}


// STRING STACK
// String buffers in this area are composed of a single control byte plus a
// data area.  The data area is reserved in multiples of 16 bytes.  The minimum
// allocation size is 16 bytes.  Once allocated, the buffer size will not
// change, but different strings may be stored in it at different times.
//
// The control byte preceeds the data area.  The lower 4 bits indicate the
// number of 16 byte blocks that were allocated plus one.  So zero indicates
// one 16 byte block.  When a buffer pointer is returned, it is returned as the
// first byte after the control byte.
//
// A set of 16 linked lists are maintained for keeping track of the buffers
// when they are freed.  Each linked list corresponds to a particular number of
// blocks.  The first two bytes after the control byte of the freed buffer are
// used for the NEXT pointer in the list.  When a string buffer is freed, it is
// added in memory descending order to the linked list that represents its
// block count.
//
// When a string buffer is allocated, the free list for that particular block
// count requested is searched.  If found, a pointer to that buffer is returned
// and the node is removed from the free list.  If a suitable free buffer
// cannot be located, a new buffer is created at StringBot.
//
// However, when a string buffer is re-allocated, different things happen
// depending on the requested block count and the current block count of the
// buffer.  First, the free list for the requested block count is searched.
// If the requested block count is the same or smaller than the current block
// count, AND if the found free buffer is higher in memory, then the current
// buffer is freed, and the free buffer removed from he list and a pointer to
// it returned.  If a free buffer was not found, OR was found lower in memory,
// then the original buffer is returned instead.  If the requested block count
// is larger, then the original buffer is freed and the found free buffer is
// removed from the list and a pointer to it returned.  If a free buffer was
// not found, a new buffer of the new size is created at StringBot.
//
// When a string buffer is freed, a check is made to see if its on the bottom
// of the string stack, that is, if it equals StringBot.  If so, then rather
// than adding it to the free list, the StringBot pointer is simply moved up.
//
// By using this method of string management, the strings will migrate toward
// the top of memory.  Every time a string is written to, it can be relocated.
//
// ===========================================================================



// Because there is a root pointer for every possible block count size, we
// don't actually search the list, but rather take the first block.  So to
// remove the buffer from the free list, it is a simple matter of changing the
// root pointer to the second buffer if it exists or NULL.  It should be noted
// that the free list pointers point to the control byte, not the data area.

void PopFree(BYTE BlkCnt)
{
    WORD Root;

    Root = BasicVars.FreeList[BlkCnt];
    if (Root)
        BasicVars.FreeList[BlkCnt] = ReadRandomWord((WORD)(Root + 1));
}

// Inserting a node into the free list is more difficult.  The free list must
// be traversed in order to insert the node in descending memory order.  The
// BufPtr pointer points to the control byte and must be a previously allocated
// String Buffer.  No check for illegal buffers is made.

void InsertFree(WORD BufPtr)
{
    BYTE BlkCnt;
    WORD BlkPtr, PrevBlk;

    BlkCnt = ReadRandom51(BufPtr);
    if (BlkCnt & 0xF0)
    {
    	// something bad went wrong
        SyntaxErrorCode = ERROR_INTERNAL_BAS51_ERROR;
        return;
    }

	// traverse list
    BlkPtr = BasicVars.FreeList[BlkCnt];
    PrevBlk = NULL;
    while (BlkPtr > BufPtr)
    {
        // Save old address
        PrevBlk = BlkPtr;

        // get next link in chain
        BlkPtr = ReadRandomWord((WORD)(BlkPtr + 1));
    }

    // At this point, BlkPtr is the address of a block that is lower in
    // memory than BufPtr.   The freed buffer will be inserted between
    // PrevBlk and BlkPtr.

    WriteRandomWord((WORD)(BufPtr + 1), BlkPtr);  // point to former first node
    if (PrevBlk) WriteRandomWord((WORD)(PrevBlk + 1), BufPtr);  // insert node
    else BasicVars.FreeList[BlkCnt] = BufPtr;  // root node
}





// StringAlloc()
// Allocate Durable String space and return Pointer to first byte in buffer.
// Assumes that a previously allocated block does not exist.
// When a string buffer is allocated, the free list for that particular block
// count requested is searched.  If found, a pointer to that buffer is returned
// and the node is removed from the free list.  If a suitable free buffer
// cannot be located, a new buffer is created at StringBot.
// Return NULL if there is insufficient memory.


WORD StringAlloc(BYTE BlkCnt)
{
    WORD ptr;

    ptr = BasicVars.FreeList[BlkCnt];
    if (ptr)  // We found a suitable free buffer
    {
        PopFree(BlkCnt);   // remove free buffer from free list
        ptr++;
    }
    else   // not found, must allocate new
    {
        BasicVars.StringBot -= (WORD)((BlkCnt + 1) * 16 + 1);
        ptr = BasicVars.StringBot;
        WriteRandom51(ptr++, BlkCnt);   // Write Control Byte
	    if (ptr <= BasicVars.TempStringTop)
		{
        	SyntaxErrorCode = ERROR_INSUFFICIENT_MEMORY;
	        ptr = NULL;
    	}
    }

    return(ptr);
}




// When a string buffer is freed, it is added in memory descending order to
// the linked list that represents its block count.  A check is made to see if
// its on the bottom of the string stack, that is, if it equals StringBot.  If
// so, then rather than adding it to the free list, the StringBot pointer is
// simply moved up.
// On entry, BufPtr points to the first byte of the data area.


void FreeStringAlloc(WORD BufPtr)
{
    BYTE BlkCnt;

    if (BufPtr == NULL) return;      // check NULL
    BufPtr--;                        // point to control byte
    // Check bounds
    if (BufPtr < BasicVars.StringBot || BufPtr >= BasicVars.CmdLine) return;

    if (BufPtr == BasicVars.StringBot)   // special case if at bottom
    {
	    BlkCnt = ReadRandom51(BufPtr); 	// find blocks used by buffer
        BasicVars.StringBot += (WORD)((BlkCnt + 1) * 16 + 1);
    }
    else InsertFree(BufPtr);    // not at bottom, insert into free list
}

// Return the allocated buffer size

//WORD GetBufAllocSize(WORD BufPtr)
//{
 //  return( (WORD) ReadRandom51((WORD)(BufPtr - 1)) );
//}

// RE-String Alloc
// Attempts to make previously allocated space at least BufLen bytes long.
// Returns pointer if successful or NULL if not enough memory.
// When a string buffer is re-allocated, different things happen
// depending on the requested block count and the current block count of the
// buffer.  First, the free list for the requested block count is searched.
// If the requested block count is the same or smaller than the current block
// count, AND if the found free buffer is higher in memory, then the current
// buffer is freed, and the free buffer removed from he list and a pointer to
// it returned.  If a free buffer was not found, OR was found lower in memory,
// then the original buffer is returned instead.  If the requested block count
// is larger, then the original buffer is freed and the found free buffer is
// removed from the list and a pointer to it returned.  If a free buffer was
// not found, a new buffer of the new size is created at StringBot.

WORD ReStringAlloc(WORD BufPtr, BYTE BufLen)
{
    BYTE NewBlks, BlkCnt;

    // Function must return NULL if BufLen is zero.
    if (BufLen == 0)
    {
        FreeStringAlloc(BufPtr);
        return(NULL);
    }

    NewBlks = (BYTE)(BufLen / 16);  // Calculate number of blocks requested

    // Just allocate new space if BufPtr is NULL or points to constant space
    if (BufPtr == NULL || StringPtrIsConst(BufPtr))
      	return(StringAlloc(NewBlks));  // Allocate new space

    // If we are here, then BufPtr points to string space.
    BlkCnt = ReadRandom51((WORD)(BufPtr - 1));
    if (BlkCnt & 0xF0)
    {
    	// something bad went wrong
        SyntaxErrorCode = ERROR_INTERNAL_BAS51_ERROR;
        return(NULL);
    }


    if (NewBlks <= BlkCnt)  // new block is same size or smaller
    {
        // Can we find a higher spot?
        if (BasicVars.FreeList[NewBlks] > BufPtr)
	    {
            // Move to higher spot
    	    FreeStringAlloc(BufPtr);   // free old buffer
        	BufPtr = (WORD)(BasicVars.FreeList[NewBlks] + 1);
	        PopFree(NewBlks);
        }
        return(BufPtr);
    }
    else     // new block is bigger, must alloc new buffer.
    {
        FreeStringAlloc(BufPtr);
        return(StringAlloc(NewBlks));
    }

}







