/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "sys/platform.h"

#include "sys/win32/win_local.h"

#include <lmerr.h>
#include <lmcons.h>
#include <lmwksta.h>
#include <errno.h>
#include <fcntl.h>
#include <direct.h>
#include <io.h>
#include <conio.h>

/*
================
Sys_GetSystemRam

	returns amount of physical memory in MB
================
*/
int Sys_GetSystemRam( void ) {
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof ( statex );
	GlobalMemoryStatusEx (&statex);
	int physRam = statex.ullTotalPhys / ( 1024 * 1024 );
	// HACK: For some reason, ullTotalPhys is sometimes off by a meg or two, so we round up to the nearest 16 megs
	physRam = ( physRam + 8 ) & ~15;
	return physRam;
}


/*
================
Sys_GetDriveFreeSpace
returns in megabytes
================
*/
int Sys_GetDriveFreeSpace( const char *path ) {
	DWORDLONG lpFreeBytesAvailable;
	DWORDLONG lpTotalNumberOfBytes;
	DWORDLONG lpTotalNumberOfFreeBytes;
	int ret = 26;
	//FIXME: see why this is failing on some machines
	if ( ::GetDiskFreeSpaceEx( path, (PULARGE_INTEGER)&lpFreeBytesAvailable, (PULARGE_INTEGER)&lpTotalNumberOfBytes, (PULARGE_INTEGER)&lpTotalNumberOfFreeBytes ) ) {
		ret = ( double )( lpFreeBytesAvailable ) / ( 1024.0 * 1024.0 );
	}
	return ret;
}

/*
================
Sys_LockMemory
================
*/
bool Sys_LockMemory( void *ptr, int bytes ) {
	return ( VirtualLock( ptr, (SIZE_T)bytes ) != FALSE );
}

/*
================
Sys_UnlockMemory
================
*/
bool Sys_UnlockMemory( void *ptr, int bytes ) {
	return ( VirtualUnlock( ptr, (SIZE_T)bytes ) != FALSE );
}

/*
================
Sys_SetPhysicalWorkMemory
================
*/
void Sys_SetPhysicalWorkMemory( int minBytes, int maxBytes ) {
	::SetProcessWorkingSetSize( GetCurrentProcess(), minBytes, maxBytes );
}


//for old sdk compatibility

#define PROLOGUE_SIGNATURE 0x00EC8B55

/*
==================
Sym_Init
==================
*/
void Sym_Init(long addr) {
}

/*
==================
Sym_Shutdown
==================
*/
void Sym_Shutdown(void) {
}

/*
==================
Sym_GetFuncInfo
==================
*/
void Sym_GetFuncInfo(long addr, idStr& module, idStr& funcName) {
	module = "";
	sprintf(funcName, "0x%08x", addr);
}

/*
==================
GetFuncAddr
==================
*/
address_t GetFuncAddr(address_t midPtPtr) {
	long temp;
	do {
		temp = (long)(*(long*)midPtPtr);
		if ((temp & 0x00FFFFFF) == PROLOGUE_SIGNATURE) {
			break;
		}
		midPtPtr--;
	} while (true);

	return midPtPtr;
}

/*
==================
GetCallerAddr
==================
*/
address_t GetCallerAddr(long _ebp) {
	long midPtPtr;
	long res = 0;

	__asm {
		mov		eax, _ebp
		mov		ecx, [eax]		// check for end of stack frames list
		test	ecx, ecx		// check for zero stack frame
		jz		label
		mov		eax, [eax + 4]	// get the ret address
		test	eax, eax		// check for zero return address
		jz		label
		mov		midPtPtr, eax
	}
	res = GetFuncAddr(midPtPtr);
label:
	return res;
}



/*
==================
Sys_GetCallStack
 use /Oy option
==================
*/
void Sys_GetCallStack(address_t* callStack, const int callStackSize) {
#if 1 //def _DEBUG
	int i;
	long m_ebp;

	__asm {
		mov eax, ebp
		mov m_ebp, eax
	}
	// skip last two functions
	m_ebp = *((long*)m_ebp);
	m_ebp = *((long*)m_ebp);
	// list functions
	for (i = 0; i < callStackSize; i++) {
		callStack[i] = GetCallerAddr(m_ebp);
		if (callStack[i] == 0) {
			break;
		}
		m_ebp = *((long*)m_ebp);
	}
#else
	int i = 0;
#endif
	while (i < callStackSize) {
		callStack[i++] = 0;
	}
}

/*
==================
Sys_GetCallStackStr
==================
*/
const char* Sys_GetCallStackStr(const address_t* callStack, const int callStackSize) {
	static char string[MAX_STRING_CHARS * 2];
	int index, i;
	idStr module, funcName;

	index = 0;
	for (i = callStackSize - 1; i >= 0; i--) {
		Sym_GetFuncInfo(callStack[i], module, funcName);
		index += sprintf(string + index, " -> %s", funcName.c_str());
	}
	return string;
}

/*
==================
Sys_GetCallStackCurStr
==================
*/
const char* Sys_GetCallStackCurStr(int depth) {
	address_t* callStack;

	callStack = (address_t*)_alloca(depth * sizeof(address_t));
	Sys_GetCallStack(callStack, depth);
	return Sys_GetCallStackStr(callStack, depth);
}

/*
==================
Sys_GetCallStackCurAddressStr
==================
*/
const char* Sys_GetCallStackCurAddressStr(int depth) {
	static char string[MAX_STRING_CHARS * 2];
	address_t* callStack;
	int index, i;

	callStack = (address_t*)_alloca(depth * sizeof(address_t));
	Sys_GetCallStack(callStack, depth);

	index = 0;
	for (i = depth - 1; i >= 0; i--) {
		index += sprintf(string + index, " -> 0x%08x", callStack[i]);
	}
	return string;
}

/*
==================
Sys_ShutdownSymbols
==================
*/
void Sys_ShutdownSymbols(void) {
	Sym_Shutdown();
}