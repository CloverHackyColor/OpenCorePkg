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
//    if ( result < MAX_INT32 - DataOff ) {
    if ( result < MAX_UINTN - DataOff ) {
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
  UINT32   Index;

  ASSERT (DataOff >= 0);

  if (PatternSize == 0 || DataSize == 0 || (DataOff < 0) || (UINT32)DataOff >= DataSize || DataSize - DataOff < PatternSize) {
    return -1;
  }

  if (PatternMask == NULL) {
    while (DataOff + PatternSize <= DataSize) {
      for (Index = 0; Index < PatternSize; ++Index) {
        if (Data[DataOff + Index] != Pattern[Index]) {
          break;
        }
      }

      if (Index == PatternSize) {
        return DataOff;
      }
      ++DataOff;
    }
  } else {
    while (DataOff + PatternSize <= DataSize) {
      for (Index = 0; Index < PatternSize; ++Index) {
        if ((Data[DataOff + Index] & PatternMask[Index]) != Pattern[Index]) {
          break;
        }
      }

      if (Index == PatternSize) {
        return DataOff;
      }
      ++DataOff;
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
      DEBUG((DEBUG_VERBOSE, "Replace " ));
      for (UINTN Index = 0; Index < PatternSize; ++Index) {
        DEBUG((DEBUG_VERBOSE, "%02X", Pattern[Index]));
      }
      if ( PatternMask ) {
        DEBUG((DEBUG_VERBOSE, "/" ));
        for (UINTN Index = 0; Index < PatternSize; ++Index) {
          DEBUG((DEBUG_VERBOSE, "%02X", PatternMask[Index]));
        }
        DEBUG((DEBUG_VERBOSE, "(" ));
        for (UINTN Index = 0; Index < PatternSize; ++Index) {
          DEBUG((DEBUG_VERBOSE, "%02X", Data[DataOff + Index]));
        }
        DEBUG((DEBUG_VERBOSE, ")" ));
      }
      DEBUG((DEBUG_VERBOSE, " by " ));

      if (ReplaceMask == NULL) {
        CopyMem (&Data[DataOff], (void*)Replace, PatternSize);
      } else {
        for (UINTN Index = 0; Index < PatternSize; ++Index) {
          Data[DataOff + Index] = (Data[DataOff + Index] & ~ReplaceMask[Index]) | (Replace[Index] & ReplaceMask[Index]);
        }
      }

      for (UINTN Index = 0; Index < PatternSize; ++Index) {
        DEBUG((DEBUG_VERBOSE, "%02X", Replace[Index]));
      }
      if ( ReplaceMask ) {
        DEBUG((DEBUG_VERBOSE, "/" ));
        for (UINTN Index = 0; Index < PatternSize; ++Index) {
          DEBUG((DEBUG_VERBOSE, "%02X", ReplaceMask[Index]));
        }
        DEBUG((DEBUG_VERBOSE, "(" ));
        for (UINTN Index = 0; Index < PatternSize; ++Index) {
          DEBUG((DEBUG_VERBOSE, "%02X", Data[DataOff + Index]));
        }
        DEBUG((DEBUG_VERBOSE, ")" ));
      }

      DEBUG((DEBUG_VERBOSE, " at ofs:%X\n", DataOff));

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
