/** @file
  Processor or Compiler specific defines for all supported processors.

  This file is stand alone self consistent set of definitions.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef CPER_BASETYPES_H
#define CPER_BASETYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <limits.h>

///
/// 8-byte unsigned value
///
typedef unsigned long long UINT64;
///
/// 8-byte signed value
///
typedef long long INT64;
///
/// 4-byte unsigned value
///
typedef unsigned int UINT32;
///
/// 4-byte signed value
///
typedef int INT32;
///
/// 2-byte unsigned value
///
typedef unsigned short UINT16;
///
/// 2-byte Character.  Unless otherwise specified all strings are stored in the
/// UTF-16 encoding format as defined by Unicode 2.1 and ISO/IEC 10646 standards.
///
typedef unsigned short CHAR16;
///
/// 2-byte signed value
///
typedef short INT16;
///
/// Logical Boolean.  1-byte value containing 0 for FALSE or a 1 for TRUE.  Other
/// values are undefined.
///
typedef unsigned char BOOLEAN;
///
/// 1-byte unsigned value
///
typedef unsigned char UINT8;
///
/// 1-byte Character
///
typedef char CHAR8;
///
/// 1-byte signed value
///
typedef signed char INT8;

///
/// Bit field definitions, matching the UEFI/EDK2 convention. BIT0 through
/// BIT31 are 32-bit values; BIT32 through BIT63 are 64-bit values.
///@{
#define BIT0  0x00000001
#define BIT1  0x00000002
#define BIT2  0x00000004
#define BIT3  0x00000008
#define BIT4  0x00000010
#define BIT5  0x00000020
#define BIT6  0x00000040
#define BIT7  0x00000080
#define BIT8  0x00000100
#define BIT9  0x00000200
#define BIT10 0x00000400
#define BIT11 0x00000800
#define BIT12 0x00001000
#define BIT13 0x00002000
#define BIT14 0x00004000
#define BIT15 0x00008000
#define BIT16 0x00010000
#define BIT17 0x00020000
#define BIT18 0x00040000
#define BIT19 0x00080000
#define BIT20 0x00100000
#define BIT21 0x00200000
#define BIT22 0x00400000
#define BIT23 0x00800000
#define BIT24 0x01000000
#define BIT25 0x02000000
#define BIT26 0x04000000
#define BIT27 0x08000000
#define BIT28 0x10000000
#define BIT29 0x20000000
#define BIT30 0x40000000
#define BIT31 0x80000000
#define BIT32 0x0000000100000000ULL
#define BIT33 0x0000000200000000ULL
#define BIT34 0x0000000400000000ULL
#define BIT35 0x0000000800000000ULL
#define BIT36 0x0000001000000000ULL
#define BIT37 0x0000002000000000ULL
#define BIT38 0x0000004000000000ULL
#define BIT39 0x0000008000000000ULL
#define BIT40 0x0000010000000000ULL
#define BIT41 0x0000020000000000ULL
#define BIT42 0x0000040000000000ULL
#define BIT43 0x0000080000000000ULL
#define BIT44 0x0000100000000000ULL
#define BIT45 0x0000200000000000ULL
#define BIT46 0x0000400000000000ULL
#define BIT47 0x0000800000000000ULL
#define BIT48 0x0001000000000000ULL
#define BIT49 0x0002000000000000ULL
#define BIT50 0x0004000000000000ULL
#define BIT51 0x0008000000000000ULL
#define BIT52 0x0010000000000000ULL
#define BIT53 0x0020000000000000ULL
#define BIT54 0x0040000000000000ULL
#define BIT55 0x0080000000000000ULL
#define BIT56 0x0100000000000000ULL
#define BIT57 0x0200000000000000ULL
#define BIT58 0x0400000000000000ULL
#define BIT59 0x0800000000000000ULL
#define BIT60 0x1000000000000000ULL
#define BIT61 0x2000000000000000ULL
#define BIT62 0x4000000000000000ULL
#define BIT63 0x8000000000000000ULL
///@}

//
// Basical data type definitions introduced in UEFI.
//
typedef struct {
	UINT32 Data1;
	UINT16 Data2;
	UINT16 Data3;
	UINT8 Data4[8];
} EFI_GUID;

///
/// The timestamp correlates to the time when the error information was collected
/// by the system software and may not necessarily represent the time of the error
/// event. The timestamp contains the local time in BCD format.
///
typedef struct {
	UINT8 Seconds;
	UINT8 Minutes;
	UINT8 Hours;
	UINT8 Flag;
	UINT8 Day;
	UINT8 Month;
	UINT8 Year;
	UINT8 Century;
} EFI_ERROR_TIME_STAMP;

/**
  Returns a 16-bit signature built from 2 ASCII characters.

  This macro returns a 16-bit value built from the two ASCII characters specified
  by A and B.

  @param  A    The first ASCII character.
  @param  B    The second ASCII character.

  @return A 16-bit value built from the two ASCII characters specified by A and B.

**/
#define SIGNATURE_16(A, B) ((A) | ((B) << 8))
/**
  Returns a 32-bit signature built from 4 ASCII characters.

  This macro returns a 32-bit value built from the four ASCII characters specified
  by A, B, C, and D.

  @param  A    The first ASCII character.
  @param  B    The second ASCII character.
  @param  C    The third ASCII character.
  @param  D    The fourth ASCII character.

  @return A 32-bit value built from the two ASCII characters specified by A, B,
          C and D.

**/
#define SIGNATURE_32(A, B, C, D)                                               \
	(SIGNATURE_16(A, B) | (SIGNATURE_16(C, D) << 16))

#ifdef __cplusplus
}
#endif

#endif
