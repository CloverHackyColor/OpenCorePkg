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
//#include "../../Include/Library/kernel_patcher.h"

#define KERNEL_MAX_SIZE 80000000

static UINTN FindMemMask(const UINT8 *Source, UINTN SourceSize, const UINT8 *Search,
                  UINTN SearchSize, const UINT8 *MaskSearch, UINTN MaskSize);
static UINTN SearchAndReplaceMask(UINT8 *Source, UINT64 SourceSize, const UINT8 *Search,
                           const UINT8 *MaskSearch, UINTN SearchSize,
                           const UINT8 *Replace, const UINT8 *MaskReplace, INTN MaxReplaces);

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
  if (result != KERNEL_MAX_SIZE) {
    return result + DataOff;
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
  return SearchAndReplaceMask(Data, DataSize, Pattern, PatternMask, PatternSize,
                              Replace, ReplaceMask, Count);
}

////
//// Searches Source for Search pattern of size SearchSize
//// and returns the number of occurences.
////
//static UINTN SearchAndCount(const UINT8 *Source, UINT64 SourceSize, const UINT8 *Search, UINTN SearchSize)
//{
//  UINTN        NumFounds = 0;
//  const UINT8  *End = Source + SourceSize;
//
//  while (Source < End) {
//    if (CompareMem(Source, Search, SearchSize) == 0) {
//      NumFounds++;
//      Source += SearchSize;
//    } else {
//      Source++;
//    }
//  }
//  return NumFounds;
//}

////
//// Searches Source for Search pattern of size SearchSize
//// and replaces it with Replace up to MaxReplaces times.
//// If MaxReplaces <= 0, then there is no restriction on number of replaces.
//// Replace should have the same size as Search.
//// Returns number of replaces done.
////
//static UINTN SearchAndReplace(UINT8 *Source, UINT64 SourceSize, const UINT8 *Search, UINTN SearchSize, const UINT8 *Replace, INTN MaxReplaces)
//{
//  UINTN     NumReplaces = 0;
//  BOOLEAN   NoReplacesRestriction = MaxReplaces <= 0;
//  //  UINT8     *Begin = Source;
//  UINT8     *End = Source + SourceSize;
//  if (!Source || !Search || !Replace || !SearchSize) {
//    return 0;
//  }
//
//  while ((Source < End) && (NoReplacesRestriction || (MaxReplaces > 0))) {
//    if (CompareMem(Source, Search, SearchSize) == 0) {
//      //     printf("  found pattern at %llx\n", (UINTN)(Source - Begin));
//      CopyMem(Source, Replace, SearchSize);
//      NumReplaces++;
//      MaxReplaces--;
//      Source += SearchSize;
//    } else {
//      Source++;
//    }
//  }
//  return NumReplaces;
//}

static BOOLEAN CompareMemMask(const UINT8 *Source, const UINT8 *Search, UINTN SearchSize, const UINT8 *Mask, UINTN MaskSize)
{
  UINT8 M;
  
  if (!Mask || MaskSize == 0) {
    return !CompareMem(Source, Search, SearchSize);
  }
  for (UINTN Ind = 0; Ind < SearchSize; Ind++) {
    if (Ind < MaskSize)
      M = *Mask++;
    else M = 0xFF;
    if ((*Source++ & M) != (*Search++ & M)) {
      return FALSE;
    }
  }
  return TRUE;
}

static VOID CopyMemMask(UINT8 *Dest, const UINT8 *Replace, const UINT8 *Mask, UINTN SearchSize)
{
  UINT8 M, D;
  // the procedure is called from SearchAndReplaceMask with own check but for future it is better to check twice
  if (!Dest || !Replace) {
    return;
  }
  
  if (!Mask) {
    CopyMem(Dest, Replace, SearchSize); //old behavior
    return;
  }
  for (UINTN Ind = 0; Ind < SearchSize; Ind++) {
    M = *Mask++;
    D = *Dest;
    *Dest++ = ((D ^ *Replace++) & M) ^ D;
  }
}

//// search a pattern like
//// call task or jmp address
////return the address next to the command
//// 0 if not found
//static UINTN FindRelative32(const UINT8 *Source, UINTN Start, UINTN SourceSize, UINTN taskLocation)
//{
//  INT32 Offset; //can be negative, so 0xFFFFFFFF == -1
//  for (UINTN i = Start; i < Start + SourceSize - 4; ++i) {
//    Offset = (INT32)((UINT32)Source[i] + ((UINT32)Source[i+1]<<8) + ((UINT32)Source[i+2]<<16) + ((UINT32)Source[i+3]<<24)); //should not use *(UINT32*) because of alignment
//    if (taskLocation == i + Offset + 4) {
//      return (i+4);
//    }
//  }
//  return 0;
//}

static UINTN FindMemMask(const UINT8 *Source, UINTN SourceSize, const UINT8 *Search, UINTN SearchSize, const UINT8 *MaskSearch, UINTN MaskSize)
{
  if (!Source || !Search || !SearchSize) {
    return KERNEL_MAX_SIZE;
  }
  
  for (UINTN i = 0; i < SourceSize - SearchSize; ++i) {
    if (CompareMemMask(&Source[i], Search, SearchSize, MaskSearch, MaskSize)) {
      return i;
    }
  }
  return KERNEL_MAX_SIZE;
}

static UINTN SearchAndReplaceMask(UINT8 *Source, UINT64 SourceSize, const UINT8 *Search, const UINT8 *MaskSearch, UINTN SearchSize,
                           const UINT8 *Replace, const UINT8 *MaskReplace, INTN MaxReplaces)
{
  UINTN     NumReplaces = 0;
  BOOLEAN   NoReplacesRestriction = MaxReplaces <= 0;
  UINT8     *End = Source + SourceSize;
  if (!Source || !Search || !Replace || !SearchSize) {
    return 0;
  }
  while ((Source < End) && (NoReplacesRestriction || (MaxReplaces > 0))) {
    if (CompareMemMask((const UINT8 *)Source, Search, SearchSize, MaskSearch, SearchSize)) {
      CopyMemMask(Source, Replace, MaskReplace, SearchSize);
      NumReplaces++;
      MaxReplaces--;
      Source += SearchSize;
    } else {
      Source++;
    }
    
  }
  
  return NumReplaces;
}


//static UINTN SearchAndReplaceTxt(UINT8 *Source, UINT64 SourceSize, const UINT8 *Search, UINTN SearchSize, const UINT8 *Replace, INTN MaxReplaces)
//{
//  UINTN     NumReplaces = 0;
//  UINTN     Skip = 0;
//  BOOLEAN   NoReplacesRestriction = MaxReplaces <= 0;
//  UINT8     *End = Source + SourceSize;
//  const UINT8     *SearchEnd = Search + SearchSize;
//  const UINT8     *Pos = NULL;
//  UINT8     *SourcePos = NULL;
//  UINT8     *FirstMatch = Source;
//  if (!Source || !Search || !Replace || !SearchSize) {
//    return 0;
//  }
//
//  while (((Source + SearchSize) <= End) &&
//         (NoReplacesRestriction || (MaxReplaces > 0))) { // num replaces
//    while (*Source != '\0') {  //comparison
//      Pos = Search;
//      FirstMatch = Source;
//      Skip = 0;
//      while (*Source != '\0' && Pos != SearchEnd) {
//        if (*Source <= 0x20) { //skip invisibles in sources
//          Source++;
//          Skip++;
//          continue;
//        }
//        if (*Source != *Pos) {
//          break;
//        }
//        //       printf("%c", *Source);
//        Source++;
//        Pos++;
//      }
//
//      if (Pos == SearchEnd) { // pattern found
//        SourcePos = FirstMatch;
//        break;
//      }
//      else
//        SourcePos = NULL;
//
//      Source = FirstMatch + 1;
//      /*      if (Pos != Search) {
//       printf("\n");
//       } */
//
//    }
//
//    if (!SourcePos) {
//      break;
//    }
//    CopyMem(SourcePos, Replace, SearchSize);
//    SetMem(SourcePos + SearchSize, Skip, 0x20); //fill skip places with spaces
//    NumReplaces++;
//    MaxReplaces--;
//    Source = FirstMatch + SearchSize + Skip;
//  }
//  return NumReplaces;
//}

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

  if (PatternSize == 0 || DataSize == 0 || (DataOff < 0) || (UINT32)DataOff >= DataSize || DataSize - DataOff < PatternSize) {
    return -1;
  }

  while (DataOff + PatternSize < DataSize) {
    Matches = TRUE;
    for (Index = 0; Index < PatternSize; ++Index) {
      if ((PatternMask == NULL && Data[DataOff + Index] != Pattern[Index])
      || (PatternMask != NULL && (Data[DataOff + Index] & PatternMask[Index]) != Pattern[Index])) {
        Matches = FALSE;
        break;
      }
    }

    if (Matches) {
      return DataOff;
    }
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
      if (ReplaceMask == NULL) {
        CopyMem (&Data[DataOff], Replace, PatternSize);
      } else {
        for (UINTN Index = 0; Index < PatternSize; ++Index) {
          Data[DataOff + Index] = (Data[DataOff + Index] & ~ReplaceMask[Index]) | (Replace[Index] & ReplaceMask[Index]);
        }
      }
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
