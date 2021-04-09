/** @file
  Copyright (C) 2019, vit9696. All rights reserved.

  All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/OcMiscLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>

#ifdef CLOVER_BUILD

#include "../../../rEFIt_UEFI/Platform/MemoryOperation.h"


INT32
FindPattern (
  IN CONST UINT8   *Pattern,
  IN CONST UINT8   *PatternMask OPTIONAL,
  IN CONST UINT32  PatternSize,
  IN CONST UINT8   *Data,
  IN UINT32        DataSize,
  IN INT32         DataOff
  )
{

//  ASSERT (DataOff >= 0);

  if (PatternSize == 0 || DataSize == 0 || (DataOff < 0) || (UINT32)DataOff >= DataSize || DataSize - DataOff < PatternSize) {
    return -1;
  }

  UINTN result = FindMemMask(Data + DataOff, DataSize, Pattern, PatternSize, PatternMask, PatternSize);
  if (result != MAX_UINTN) {
    if ( result < (UINT32)(MAX_INT32 - DataOff) ) { // safe cast, MAX_INT32 - DataOff (which is >= 0) is always >= 0
      return (INT32)(result + DataOff);
    }
  }
  return -1;
}

UINT32
ApplyPatch (
  IN CONST UINT8   *Pattern,
  IN CONST UINT8   *PatternMask OPTIONAL,
  IN CONST UINT32  PatternSize,
  IN CONST UINT8   *Replace,
  IN CONST UINT8   *ReplaceMask OPTIONAL,
  IN UINT8         *Data,
  IN UINT32        DataSize,
  IN UINT32        Count,
  IN UINT32        Skip
  )
{
  UINTN res = SearchAndReplaceMask(Data, DataSize, Pattern, PatternMask, PatternSize, Replace, ReplaceMask, Count, Skip);
  if ( res < MAX_UINT32 ) {
    return (UINT32)res;
  }else{
    return MAX_UINT32;
  }
}


#else

INT32
FindPattern (
  IN CONST UINT8   *Pattern,
  IN CONST UINT8   *PatternMask OPTIONAL,
  IN CONST UINT32  PatternSize,
  IN CONST UINT8   *Data,
  IN UINT32        DataSize,
  IN INT32         DataOff
  )
{
  BOOLEAN  Matches;
  UINT32   Index;

  ASSERT (DataOff >= 0);

  if (PatternSize == 0 || DataSize == 0 || (DataOff < 0) || (UINT32)DataOff >= DataSize || DataSize - (UINT32)DataOff < PatternSize) {
    return -1;
  }

  while ((UINT32)DataOff + PatternSize < DataSize) {
    Matches = TRUE;
    for (Index = 0; Index < PatternSize; ++Index) {
      if ((PatternMask == NULL && Data[(UINT32)DataOff + Index] != Pattern[Index])
      || (PatternMask != NULL && (Data[(UINT32)DataOff + Index] & PatternMask[Index]) != Pattern[Index])) {
        Matches = FALSE;
        break;
      }
    }

    if (Matches) {
      return DataOff;
    }
    // Theoretically, DataOff can overflow because DataSize is UINT32.
    ++DataOff;
  }

  return -1;
}

UINT32
ApplyPatch (
  IN CONST UINT8   *Pattern,
  IN CONST UINT8   *PatternMask OPTIONAL,
  IN CONST UINT32  PatternSize,
  IN CONST UINT8   *Replace,
  IN CONST UINT8   *ReplaceMask OPTIONAL,
  IN UINT8         *Data,
  IN UINT32        DataSize,
  IN UINT32        Count,
  IN UINT32        Skip
  )
{
  UINT32  ReplaceCount;
  INT32   DataOff;

  ReplaceCount = 0;
  DataOff = 0;

  do {
    DataOff = FindPattern (Pattern, PatternMask, PatternSize, Data, DataSize, DataOff);

    if (DataOff >= 0) {
      //
      // Skip this finding if requested.
      //
      if (Skip > 0) {
        --Skip;
        DataOff += PatternSize;
        continue;
      }

      //
      // Perform replacement.
      //
      UINTN length =
          (
               AsciiStrLen("Replace ") +
               PatternSize*2 +
               (PatternMask ? 1+PatternSize*2+1+PatternSize*2+1 : 0) +
               AsciiStrLen(" by ") +
               PatternSize*2 +
               (ReplaceMask ? 1+PatternSize*2+1+PatternSize*2+1 : 0) +
               AsciiStrLen(" at ofs:%X\n") + 8 // +16 for %X
          ) * 1 + 1; // *2 for CHAR16, +1 for \0
      CHAR8* buf = AllocateZeroPool(length);
      AsciiSPrint(buf, length, "%aReplace ", buf );
      for (UINTN Index = 0; Index < PatternSize; ++Index) {
        AsciiSPrint(buf, length, "%a%02X", buf, Pattern[Index]);
      }
      if ( PatternMask ) {
        AsciiSPrint(buf, length, "%a/", buf );
        for (UINTN Index = 0; Index < PatternSize; ++Index) {
          AsciiSPrint(buf, length, "%a%02X", buf, PatternMask[Index]);
        }
        AsciiSPrint(buf, length, "%a(", buf );
        for (UINTN Index = 0; Index < PatternSize; ++Index) {
          AsciiSPrint(buf, length, "%a%02X", buf, Data[(UINT32)DataOff + Index]); // Safe cast, DataOff is >= 0
        }
        AsciiSPrint(buf, length, "%a)", buf );
      }
      AsciiSPrint(buf, length, "%a by ", buf );

      if (ReplaceMask == NULL) {
        CopyMem (&Data[DataOff], (void*)Replace, PatternSize);
      } else {
        for (UINTN Index = 0; Index < PatternSize; ++Index) {
          Data[(UINT32)DataOff + Index] = (Data[(UINT32)DataOff + Index] & ~ReplaceMask[Index]) | (Replace[Index] & ReplaceMask[Index]); // Safe cast, DataOff is >= 0
        }
      }

      for (UINTN Index = 0; Index < PatternSize; ++Index) {
        AsciiSPrint(buf, length, "%a%02X", buf, Replace[Index]);
      }
      if ( ReplaceMask ) {
        AsciiSPrint(buf, length, "%a/", buf );
        for (UINTN Index = 0; Index < PatternSize; ++Index) {
          AsciiSPrint(buf, length, "%a%02X", buf, ReplaceMask[Index]);
        }
        AsciiSPrint(buf, length, "%a(", buf );
        for (UINTN Index = 0; Index < PatternSize; ++Index) {
          AsciiSPrint(buf, length, "%a%02X", buf, Data[(UINT32)DataOff + Index]); // Safe cast, DataOff is >= 0
        }
        AsciiSPrint(buf, length, "%a)", buf );
      }

      AsciiSPrint(buf, length, "%a at ofs:%X\n", buf, DataOff);
      DEBUG((DEBUG_VERBOSE, "%a", buf ));
      FreePool(buf);

      ++ReplaceCount;
      DataOff += PatternSize;

      //
      // Check replace count if requested.
      //
      if (Count > 0) {
        --Count;
        if (Count == 0) {
          break;
        }
      }
    }

  } while (DataOff >= 0);

  return ReplaceCount;
}

#endif
