/** @file
  OpenCore driver.

Copyright (c) 2019, vit9696. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <OpenCore.h>
#include <Uefi.h>

#include <Guid/OcVariable.h>

#include <Protocol/DevicePath.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/OcBootstrap.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/VMwareDebug.h>

#include <Library/DebugLib.h>
#include <Library/OcDebugLogLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/OcAppleBootPolicyLib.h>
#include <Library/OcBootManagementLib.h>
#include <Library/OcConfigurationLib.h>
#include <Library/OcConsoleLib.h>
#include <Library/OcCpuLib.h>
#include <Library/OcDevicePathLib.h>
#include <Library/OcStorageLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>


OC_GLOBAL_CONFIG
mOpenCoreConfiguration;


OC_STORAGE_CONTEXT
mOpenCoreStorage;


OC_CPU_INFO
mOpenCoreCpuInfo;


OC_RSA_PUBLIC_KEY *
mOpenCoreVaultKey;

STATIC
OC_PRIVILEGE_CONTEXT
mOpenCorePrivilege;

EFI_HANDLE
mLoadHandle;


EFI_STATUS
EFIAPI
OcStartImage_2 (
  IN  OC_BOOT_ENTRY               *Chosen,
  IN  EFI_HANDLE                  ImageHandle,
  OUT UINTN                       *ExitDataSize,
  OUT CHAR16                      **ExitData    OPTIONAL
  )
{
  EFI_STATUS                       Status;
  EFI_CONSOLE_CONTROL_SCREEN_MODE  OldMode;

  OldMode = OcConsoleControlSetMode (EfiConsoleControlScreenGraphics);

  Status = gBS->StartImage (
    ImageHandle,
    ExitDataSize,
    ExitData
    );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "OC: Boot failed - %r\n", Status));
  }

  OcConsoleControlSetMode (OldMode);

  return Status;
}


VOID
OcMain (
  IN OC_STORAGE_CONTEXT        *Storage,
  IN EFI_DEVICE_PATH_PROTOCOL  *LoadPath OPTIONAL
  )
{
  EFI_STATUS                Status;
#ifndef CLOVER_BUILD
  OC_PRIVILEGE_CONTEXT      *Privilege;
#endif

  DEBUG ((DEBUG_INFO, "OC: OcMiscEarlyInit...\n"));
  Status = OcMiscEarlyInit (
    Storage,
    &mOpenCoreConfiguration,
    mOpenCoreVaultKey
    );

  if (EFI_ERROR (Status)) {
    return;
  }
#ifndef CLOVER_BUILD
  //TODO: it's double Clover calculation so it is better to make copy
  // mOpenCoreCpuInfo <- gCPUStructure
  OcCpuScanProcessor (&mOpenCoreCpuInfo);
#endif

  DEBUG ((DEBUG_INFO, "OC: OcLoadNvramSupport...\n"));
  OcLoadNvramSupport (Storage, &mOpenCoreConfiguration);
  DEBUG ((DEBUG_INFO, "OC: OcMiscMiddleInit...\n"));
  OcMiscMiddleInit (Storage, &mOpenCoreConfiguration, LoadPath, &mLoadHandle);
  DEBUG ((DEBUG_INFO, "OC: OcLoadUefiSupport...\n"));
  OcLoadUefiSupport (Storage, &mOpenCoreConfiguration, &mOpenCoreCpuInfo);
#ifndef CLOVER_BUILD
  DEBUG ((DEBUG_INFO, "OC: OcLoadAcpiSupport...\n"));
  OcLoadAcpiSupport (&mOpenCoreStorage, &mOpenCoreConfiguration);

  DEBUG ((DEBUG_INFO, "OC: OcLoadPlatformSupport...\n"));
  OcLoadPlatformSupport (&mOpenCoreConfiguration, &mOpenCoreCpuInfo);

  DEBUG ((DEBUG_INFO, "OC: OcLoadDevPropsSupport...\n"));
  OcLoadDevPropsSupport (&mOpenCoreConfiguration);
#endif
  DEBUG ((DEBUG_INFO, "OC: OcMiscLateInit...\n"));
  OcMiscLateInit (Storage, &mOpenCoreConfiguration);
  DEBUG ((DEBUG_INFO, "OC: OcLoadKernelSupport...\n"));
  OcLoadKernelSupport (&mOpenCoreStorage, &mOpenCoreConfiguration, &mOpenCoreCpuInfo);

  if (mOpenCoreConfiguration.Misc.Security.EnablePassword) {
    mOpenCorePrivilege.CurrentLevel = OcPrivilegeUnauthorized;
    mOpenCorePrivilege.Hash         = mOpenCoreConfiguration.Misc.Security.PasswordHash;
    mOpenCorePrivilege.Salt         = OC_BLOB_GET (&mOpenCoreConfiguration.Misc.Security.PasswordSalt);
    mOpenCorePrivilege.SaltSize     = mOpenCoreConfiguration.Misc.Security.PasswordSalt.Size;

    #ifndef CLOVER_BUILD
      Privilege = &mOpenCorePrivilege;
    #endif
  } else {
    #ifndef CLOVER_BUILD
      Privilege = NULL;
    #endif
  }

  DEBUG ((DEBUG_INFO, "OC: All green, starting boot management...\n"));


//
//  CHAR16* UnicodeDevicePath = NULL; (void)UnicodeDevicePath;
//
//  UINTN                   HandleCount = 0;
//  EFI_HANDLE              *Handles = NULL;
//  Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &HandleCount, &Handles);
//  UINTN HandleIndex = 0;
//  for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
//    EFI_DEVICE_PATH_PROTOCOL* DevicePath = DevicePathFromHandle(Handles[HandleIndex]);
//    UnicodeDevicePath = ConvertDevicePathToText(DevicePath, FALSE, FALSE);
//    if ( StrCmp(L"PciRoot(0x0)/Pci(0x11,0x0)/Pci(0x5,0x0)/Sata(0x1,0x0,0x0)/HD(2,GPT,25B8F381-DC5C-40C4-BCF2-9B22412964BE,0x64028,0x4F9BFB0)/VenMedia(BE74FCF7-0B7C-49F3-9147-01F4042E6842,B1F3810AD9513533B3E3169C3640360D)", UnicodeDevicePath) == 0 ) break;
//  }
//  if ( HandleIndex < HandleCount )
//  {
//    EFI_DEVICE_PATH_PROTOCOL* jfkImagePath = FileDevicePath(Handles[HandleIndex], L"\\System\\Library\\CoreServices\\boot.efi");
//    UnicodeDevicePath = ConvertDevicePathToText (jfkImagePath, FALSE, FALSE);
//
//    EFI_HANDLE EntryHandle = NULL;
//    Status = gBS->LoadImage (
//      FALSE,
//      gImageHandle,
//      jfkImagePath,
//      NULL,
//      0,
//      &EntryHandle
//      );
//  //  OcLoadBootEntry (Context, Context->
//    Status = gBS->StartImage (EntryHandle, 0, NULL);
//  }else{
//  }


//Slice - to start recovery we want it
#ifndef CLOVER_BUILD
  OcMiscBoot (
    &mOpenCoreStorage,
    &mOpenCoreConfiguration,
    Privilege,
    OcStartImage_2,
    FALSE, //mOpenCoreConfiguration.Uefi.Quirks.RequestBootVarRouting,
    mLoadHandle
    );
#endif
}


EFI_STATUS
EFIAPI
OcBootstrapRerun (
  IN OC_BOOTSTRAP_PROTOCOL            *This,
  IN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *FileSystem,
  IN EFI_DEVICE_PATH_PROTOCOL         *LoadPath OPTIONAL
  )
{
  EFI_STATUS          Status;

  DEBUG ((DEBUG_INFO, "OC: ReRun executed!\n"));

  ++This->NestedCount;

  if (This->NestedCount == 1) {
    mOpenCoreVaultKey = OcGetVaultKey (This);







    /*
     * Jief : I declare these var here, to keep my modification grouped together and ease copy/paste each time a new version is pulled.
     */

    int oc_unused_symbol_trick = 0;
    /*
     * Leave a signature that wouldn't be stripped in release version
     */
    static const char* opencore_revision = "OpenCore revision: " OPEN_CORE_VERSION;
    oc_unused_symbol_trick += opencore_revision[0]; // to keep it used
    /*
     * Leave a marker for BootloaderChooser, to prevent confusion (an efi file from a folder EFI\OCXXX that would use file from EFI\OC)
     */
    static const char* path_independant = "path_independant";
    oc_unused_symbol_trick += path_independant[0]; // to keep it used

    EFI_LOADED_IMAGE_PROTOCOL* LoadedImage = NULL;
    Status = gBS->HandleProtocol (
      gImageHandle,
      &gEfiLoadedImageProtocolGuid,
      (VOID **) &LoadedImage
      );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "OC: Failed to locate loaded image - %r\n", Status));
      return EFI_NOT_FOUND;
    }

    // Extract dirname from LoadedImage->FilePath
    CHAR16* UnicodeDevicePath = NULL;
    if ( LoadedImage->FilePath ) {
      UnicodeDevicePath = ConvertDevicePathToText(LoadedImage->FilePath, FALSE, FALSE);
      CHAR16* p = UnicodeDevicePath + StrLen(UnicodeDevicePath);
      while ( p > UnicodeDevicePath  &&  *p != L'\\' ) {
        p -= 1;
      }
      *p = 0;
    }
    // If not dirname is found (LoadedImage->FilePath can be NULL), use OPEN_CORE_ROOT_PATH as before.
    if ( UnicodeDevicePath == NULL  ||  *UnicodeDevicePath == 0 )
    {

      Status = OcStorageInitFromFs (
        &mOpenCoreStorage,
        FileSystem,
        OPEN_CORE_ROOT_PATH,
        mOpenCoreVaultKey
        );

    }else{

      Status = OcStorageInitFromFs (
        &mOpenCoreStorage,
        FileSystem,
        UnicodeDevicePath,
        mOpenCoreVaultKey
        );
      if ( UnicodeDevicePath != NULL ) FreePool (UnicodeDevicePath);
    }






    if (!EFI_ERROR (Status)) {
      OcMain (&mOpenCoreStorage, LoadPath);
      OcStorageFree (&mOpenCoreStorage);
    } else {
      DEBUG ((DEBUG_ERROR, "OC: Failed to open root FS - %r!\n", Status));
      if (Status == EFI_SECURITY_VIOLATION) {
        CpuDeadLoop (); ///< Should not return.
      }
    }
  } else {
    DEBUG ((DEBUG_WARN, "OC: Nested ReRun is not supported\n"));
    Status = EFI_ALREADY_STARTED;
  }

  --This->NestedCount;

  return Status;
}

STATIC
EFI_HANDLE
EFIAPI
OcGetLoadHandle (
  IN OC_BOOTSTRAP_PROTOCOL            *This
  )
{
  return mLoadHandle;
}

#include <Library/SerialPortLib.h>

OC_BOOTSTRAP_PROTOCOL
mOpenCoreBootStrap = {
  .Revision      = OC_BOOTSTRAP_PROTOCOL_REVISION,
  .NestedCount   = 0,
  .VaultKey      = NULL,
  .ReRun         = OcBootstrapRerun,
  .GetLoadHandle = OcGetLoadHandle,
};

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                        Status;
  EFI_LOADED_IMAGE_PROTOCOL         *LoadedImage;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL   *FileSystem;
  EFI_HANDLE                        BootstrapHandle;
  OC_BOOTSTRAP_PROTOCOL             *Bootstrap;
  EFI_DEVICE_PATH_PROTOCOL          *AbsPath;

#ifdef OC_TARGET_DEBUG
  SerialPortInitialize();
  Status = SerialPortWrite ((UINT8 *)"Starting OpenCore...", AsciiStrLen("Starting OpenCore..."));
  gBS->Stall(2000000);
#endif

  DEBUG ((DEBUG_INFO, "OC: Starting OpenCore...\n"));

  //
  // We have just started by bootstrap or manually at EFI/OC/OpenCore.efi.
  // When bootstrap runs us, we only install the protocol.
  // Otherwise we do self start.
  //

  Bootstrap = NULL;
  Status = gBS->LocateProtocol (
    &gOcBootstrapProtocolGuid,
    NULL,
    (VOID **) &Bootstrap
    );

  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "OC: Found previous image, aborting\n"));
    return EFI_ALREADY_STARTED;
  }

  LoadedImage = NULL;
  Status = gBS->HandleProtocol (
    ImageHandle,
    &gEfiLoadedImageProtocolGuid,
    (VOID **) &LoadedImage
    );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "OC: Failed to locate loaded image - %r\n", Status));
    return EFI_NOT_FOUND;
  }

  BootstrapHandle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
    &BootstrapHandle,
    &gOcBootstrapProtocolGuid,
    &mOpenCoreBootStrap,
    NULL
    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "OC: Failed to install bootstrap protocol - %r\n", Status));
    return Status;
  }

  DebugPrintDevicePath (DEBUG_INFO, "OC: Booter path", LoadedImage->FilePath);

  if (LoadedImage->FilePath == NULL) {
    DEBUG ((DEBUG_INFO, "OC: Booted from bootstrap\n"));
    return EFI_SUCCESS;
  }

  //
  // Obtain the file system device path
  //
  FileSystem = LocateFileSystem (
    LoadedImage->DeviceHandle,
    LoadedImage->FilePath
    );

  AbsPath = AbsoluteDevicePath (LoadedImage->DeviceHandle, LoadedImage->FilePath);

  //
  // Return success in either case to let rerun work afterwards.
  //
  if (FileSystem != NULL) {
    mOpenCoreBootStrap.ReRun (&mOpenCoreBootStrap, FileSystem, AbsPath);
    DEBUG ((DEBUG_ERROR, "OC: Failed to boot\n"));
  } else {
    DEBUG ((DEBUG_ERROR, "OC: Failed to locate file system\n"));
  }

  if (AbsPath != NULL) {
    FreePool (AbsPath);
  }

  return EFI_SUCCESS;
}
