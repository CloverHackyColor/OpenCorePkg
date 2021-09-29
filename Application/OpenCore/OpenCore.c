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

#include <Library/OcMainLib.h>
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

#ifndef CLOVER_BUILD
STATIC
#endif
OC_GLOBAL_CONFIG
mOpenCoreConfiguration;

#ifndef CLOVER_BUILD
STATIC
#endif
OC_STORAGE_CONTEXT
mOpenCoreStorage;

#ifndef CLOVER_BUILD
STATIC
#endif
OC_CPU_INFO
mOpenCoreCpuInfo;

STATIC
UINT8
mOpenCoreBooterHash[SHA1_DIGEST_SIZE];

STATIC
OC_RSA_PUBLIC_KEY *
mOpenCoreVaultKey;

STATIC
OC_PRIVILEGE_CONTEXT
mOpenCorePrivilege;

STATIC
EFI_HANDLE
mStorageHandle;

STATIC
EFI_DEVICE_PATH_PROTOCOL *
mStoragePath;

STATIC
CHAR16 *
mStorageRoot;

#ifndef CLOVER_BUILD
STATIC
#endif
EFI_STATUS
EFIAPI
OcStartImage (
  IN  OC_BOOT_ENTRY               *Chosen,
  IN  EFI_HANDLE                  ImageHandle,
  OUT UINTN                       *ExitDataSize,
  OUT CHAR16                      **ExitData    OPTIONAL,
  IN  BOOLEAN                     LaunchInText
  )
{
  EFI_STATUS                       Status;
  EFI_CONSOLE_CONTROL_SCREEN_MODE  OldMode;

  OldMode = OcConsoleControlSetMode (
    LaunchInText ? EfiConsoleControlScreenText : EfiConsoleControlScreenGraphics
    );

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

#ifndef CLOVER_BUILD
STATIC
#endif
VOID
OcMain (
  IN OC_STORAGE_CONTEXT        *Storage,
  IN EFI_DEVICE_PATH_PROTOCOL  *LoadPath
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
  OcMiscMiddleInit (
    Storage,
    &mOpenCoreConfiguration,
    mStorageRoot,
    LoadPath,
    mStorageHandle,
    mOpenCoreConfiguration.Booter.Quirks.ForceBooterSignature ? mOpenCoreBooterHash : NULL
    );
  DEBUG ((DEBUG_INFO, "OC: OcLoadUefiSupport...\n"));
  OcLoadUefiSupport (Storage, &mOpenCoreConfiguration, &mOpenCoreCpuInfo, mOpenCoreBooterHash);
#ifndef CLOVER_BUILD
  DEBUG_CODE_BEGIN ();
  DEBUG ((DEBUG_INFO, "OC: OcMiscLoadSystemReport...\n"));
  OcMiscLoadSystemReport (&mOpenCoreConfiguration, mStorageHandle);
  DEBUG_CODE_END ();
#endif
  // KEEP OcLoadAcpiSupport, even for Clover Build. This will do nothing except if USE_OC_SECTION_Acpi is defined in main.cpp (hybrid mode)
  DEBUG ((DEBUG_INFO, "OC: OcLoadAcpiSupport...\n"));
  OcLoadAcpiSupport (&mOpenCoreStorage, &mOpenCoreConfiguration);
#ifndef CLOVER_BUILD
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


#ifndef CLOVER_BUILD
  OcMiscBoot (
    &mOpenCoreStorage,
    &mOpenCoreConfiguration,
    Privilege,
    OcStartImage,
    mOpenCoreConfiguration.Uefi.Quirks.RequestBootVarRouting,
    mStorageHandle
    );
#endif
}

STATIC
EFI_STATUS
OcBootstrap (
  IN EFI_HANDLE                       DeviceHandle,
  IN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *FileSystem,
  IN EFI_DEVICE_PATH_PROTOCOL         *LoadPath
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *RemainingPath;
  UINTN                     StoragePathSize;

  mOpenCoreVaultKey = OcGetVaultKey ();
  mStorageHandle = DeviceHandle;

  //
  // Calculate root path (never freed).
  //
  RemainingPath = NULL;
  mStorageRoot = OcCopyDevicePathFullName (LoadPath, &RemainingPath);
  //
  // Skipping this or later failing to call UnicodeGetParentDirectory means
  // we got valid path to the root of the partition. This happens when
  // OpenCore.efi was loaded from e.g. firmware and then bootstrapped
  // on a different partition.
  //
  if (mStorageRoot == NULL) {
    DEBUG ((DEBUG_ERROR, "OC: Failed to get launcher path\n"));
    return EFI_UNSUPPORTED;
  }
  DEBUG ((DEBUG_INFO, "OC: Storage root %s\n", mStorageRoot));

  ASSERT (RemainingPath != NULL);

  if (!UnicodeGetParentDirectory (mStorageRoot)) {
    DEBUG ((DEBUG_ERROR, "OC: Failed to get launcher root path\n"));
    FreePool (mStorageRoot);
    return EFI_UNSUPPORTED;
  }

  StoragePathSize = (UINTN) RemainingPath - (UINTN) LoadPath;
  mStoragePath = AllocatePool (StoragePathSize + END_DEVICE_PATH_LENGTH);
  if (mStoragePath == NULL) {
    FreePool (mStorageRoot);
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (mStoragePath, LoadPath, StoragePathSize);
  SetDevicePathEndNode ((UINT8 *) mStoragePath + StoragePathSize);

  Status = OcStorageInitFromFs (
    &mOpenCoreStorage,
    FileSystem,
    mStorageHandle,
    mStoragePath,
    mStorageRoot,
    mOpenCoreVaultKey
    );

  if (!EFI_ERROR (Status)) {
    extern OC_STORAGE_CONTEXT* mOpenCoreStoragePtr;
    mOpenCoreStoragePtr = &mOpenCoreStorage;

    OcMain (&mOpenCoreStorage, LoadPath);
    OcStorageFree (&mOpenCoreStorage);
  } else {
    DEBUG ((DEBUG_ERROR, "OC: Failed to open root FS - %r!\n", Status));
    if (Status == EFI_SECURITY_VIOLATION) {
      CpuDeadLoop (); ///< Should not return.
    }
  }

  return Status;
}

STATIC
EFI_HANDLE
EFIAPI
OcGetLoadHandle (
  IN OC_BOOTSTRAP_PROTOCOL            *This
  )
{
  return mStorageHandle;
}

#include <Library/SerialPortLib.h>

STATIC
OC_BOOTSTRAP_PROTOCOL
mOpenCoreBootStrap = {
  .Revision      = OC_BOOTSTRAP_PROTOCOL_REVISION,
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

  if (LoadedImage->DeviceHandle == NULL) {
    DEBUG ((DEBUG_INFO, "OC: Missing boot device\n"));
    //
    // This is not critical as boot path may be complete.
    //
  }

  if (LoadedImage->FilePath == NULL) {
    DEBUG ((DEBUG_ERROR, "OC: Missing boot path\n"));
    return EFI_INVALID_PARAMETER;
  }

  DebugPrintDevicePath (DEBUG_INFO, "OC: Booter path", LoadedImage->FilePath);

  //
  // Obtain the file system device path
  //
  FileSystem = OcLocateFileSystem (
    LoadedImage->DeviceHandle,
    LoadedImage->FilePath
    );
  if (FileSystem == NULL) {
    DEBUG ((DEBUG_ERROR, "OC: Failed to locate file system\n"));
    return EFI_INVALID_PARAMETER;
  }

  AbsPath = AbsoluteDevicePath (LoadedImage->DeviceHandle, LoadedImage->FilePath);
  if (AbsPath == NULL) {
    DEBUG ((DEBUG_ERROR, "OC: Failed to allocate absolute path\n"));
    return EFI_OUT_OF_RESOURCES;
  }  

  DebugPrintDevicePath (DEBUG_INFO, "OC: Absolute booter path", LoadedImage->FilePath);

  BootstrapHandle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
    &BootstrapHandle,
    &gOcBootstrapProtocolGuid,
    &mOpenCoreBootStrap,
    NULL
    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "OC: Failed to install bootstrap protocol - %r\n", Status));
    FreePool (AbsPath);
    return Status;
  }

  OcBootstrap (LoadedImage->DeviceHandle, FileSystem, AbsPath);
  DEBUG ((DEBUG_ERROR, "OC: Failed to boot\n"));
  CpuDeadLoop ();

  return EFI_SUCCESS;
}
