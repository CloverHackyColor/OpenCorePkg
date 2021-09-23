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
#include <Library/OcGuardLib.h>
#include <Library/OcMiscLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>

STATIC
BOOLEAN
InternalFindPattern (
  IN CONST UINT8   *Pattern,
  IN CONST UINT8   *PatternMask OPTIONAL,
  IN CONST UINT32  PatternSize,
  IN CONST UINT8   *Data,
  IN UINT32        DataSize,
  IN UINT32        *DataOff
  )
{
  UINT32   Index;
  UINT32   LastOffset;
  UINT32   CurrentOffset;

  ASSERT (DataSize >= PatternSize);

  if (PatternSize == 0) {
    return FALSE;
  }

  CurrentOffset = *DataOff;
  LastOffset = DataSize - PatternSize;

  if (PatternMask == NULL) {
    while (CurrentOffset <= LastOffset) {
      for (Index = 0; Index < PatternSize; ++Index) {
        if (Data[CurrentOffset + Index] != Pattern[Index]) {
          break;
        }
      }

      if (Index == PatternSize) {
        *DataOff = CurrentOffset;
        return TRUE;
      }
      ++CurrentOffset;
    }
  } else {
    while (CurrentOffset <= LastOffset) {
      for (Index = 0; Index < PatternSize; ++Index) {
        if ((Data[CurrentOffset + Index] & PatternMask[Index]) != Pattern[Index]) {
          break;
        }
      }

      if (Index == PatternSize) {
        *DataOff = CurrentOffset;
        return TRUE;
      }
      ++CurrentOffset;
    }
  }

  return FALSE;
}

BOOLEAN
FindPattern (
  IN CONST UINT8   *Pattern,
  IN CONST UINT8   *PatternMask OPTIONAL,
  IN CONST UINT32  PatternSize,
  IN CONST UINT8   *Data,
  IN UINT32        DataSize,
  IN UINT32        *DataOff
  )
{
  if (DataSize < PatternSize) {
    return FALSE;
  }

  return InternalFindPattern (
    Pattern,
    PatternMask,
    PatternSize,
    Data,
    DataSize,
    DataOff
    );
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
  UINT32  DataOff;
  BOOLEAN Found;

  if (DataSize < PatternSize) {
    return 0;
  }

  ReplaceCount = 0;
  DataOff = 0;

  while (TRUE) {
    Found = InternalFindPattern (
      Pattern,
      PatternMask,
      PatternSize,
      Data,
      DataSize,
      &DataOff
      );

    if (!Found) {
      break;
    }

    //
    // DataOff + PatternSize - 1 is guaranteed to be a valid offset here. As
    // DataSize can at most be MAX_UINT32, the maximum valid offset is
    // MAX_UINT32 - 1. In consequence, DataOff + PatternSize cannot wrap around.
    //

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
        Data[DataOff + Index] = (Data[DataOff + Index] & ~ReplaceMask[Index]) | (Replace[Index] & ReplaceMask[Index]);
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

  return ReplaceCount;
}
