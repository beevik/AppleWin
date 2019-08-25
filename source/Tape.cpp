/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: This module is created for emulation of the 8bit character mode (mode 1) switch, 
 * which is located in $c060, and so far does not intend to emulate a tape device.
 *
 * Author: Various
 *
 * In comments, UTAIIe is an abbreviation for a reference to "Understanding the Apple //e" by James Sather
 */

#include "StdAfx.h"
#include "Keyboard.h"
#include "Memory.h"

static bool g_CapsLockAllowed = false;

//---------------------------------------------------------------------------

BYTE __stdcall TapeRead(WORD, WORD address, BYTE, BYTE, ULONG nExecutedCycles)
{
    /*
    If retrieving KeybGetKeycode(); causes problems uCurrentKeystroke shall be added 
    in the submission variables and it shall be added by the TapeRead caller
    i.e. BYTE __stdcall TapeRead (WORD, WORD address, BYTE, BYTE, ULONG nExecutedCycles) shall become
         BYTE __stdcall TapeRead (WORD, WORD address, BYTE, BYTE, ULONG nExecutedCycles, BYTE uCurrentKeystroke)
    */

    return MemReadFloatingBus(1, nExecutedCycles); // TAPEIN has high bit 1 when input is low or not connected (UTAIIe page 7-5, 7-6)
}

/*
In case s.o. decides to develop tape device emulation, this function may be renamed,
because tape is not written in $C060
*/
BYTE __stdcall TapeWrite(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nExecutedCycles)
{
    return MemReadFloatingBus(nExecutedCycles);
}

bool GetCapsLockAllowed(void)
{
    return g_CapsLockAllowed;
}
