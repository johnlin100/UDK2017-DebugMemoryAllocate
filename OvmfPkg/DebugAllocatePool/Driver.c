//
// Developed by John Lin.
//

#include "Driver.h"

#define DEBUG_PRINT_LOADED_IMAGE_PROTOCOL_FILEPATH 0

//
// Global Variable
//
UINTN         mCallerReturnAddress;
UINTN         mOriginalFunctionAddress;
SPIN_LOCK     mInProcessSpinLock;

//
// Temp variable to store AllocatePool function call information.
//
EFI_MEMORY_TYPE mPoolType;
UINTN mSize;
VOID *mBuffer;
EFI_STATUS mStatus;
UINTN mOffset;

//
// Get Image Name from the UI section of the FFS.
//
CHAR16 *
GetImageName (
  EFI_LOADED_IMAGE_PROTOCOL *Image
  )
{
  EFI_STATUS                        Status;
  EFI_DEVICE_PATH_PROTOCOL          *DevPath;
  EFI_DEVICE_PATH_PROTOCOL          *DevPathNode;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *FvFilePath;
  VOID                              *Buffer;
  UINTN                             BufferSize;
  UINT32                            AuthenticationStatus;
  EFI_GUID                          *NameGuid;
  EFI_FIRMWARE_VOLUME_PROTOCOL      *FV;
  EFI_FIRMWARE_VOLUME2_PROTOCOL     *FV2;

  FV          = NULL;
  FV2         = NULL;
  Buffer      = NULL;
  BufferSize  = 0;

  DevPath     = Image->FilePath;

  if (DevPath == NULL) {
    DEBUG ((DEBUG_ERROR, "Image File Path is NULL\n"));
    return NULL;
  }

#if DEBUG_PRINT_LOADED_IMAGE_PROTOCOL_FILEPATH == 1
  //
  // Print the Device Path Protocol which represents the FV File Path of the Loaded Image Protocol.
  // FV File Device Node is defined in PI 1.5 Spec. 8.3 Firmware File Media Device Path.
  //
  CHAR16 *DevPathText = NULL;
  DevPathText = ConvertDevicePathToText (DevPath, TRUE, FALSE);
  if (DevPathText == NULL) {
    DEBUG ((DEBUG_ERROR, "Could not convert Image File Path to Text\n"));
  } else {
    DEBUG ((DEBUG_ERROR, "Image File Path : %s\n", DevPathText));
    gBS->FreePool (DevPathText);
  }
#endif

  DevPathNode = DevPath;

  //
  // Looping the Device Path Node in the Firmware Volume Device Path to find the Name Guid.
  //
  while (!IsDevicePathEnd (DevPathNode)) {
    //
    // Find the Fv File path
    //
    NameGuid = EfiGetNameGuidFromFwVolDevicePathNode ((MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *) DevPathNode);
    if (NameGuid != NULL) {
      FvFilePath = (MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *) DevPathNode;
      Status = gBS->HandleProtocol (
                    Image->DeviceHandle,
                    &gEfiFirmwareVolumeProtocolGuid,
                    &FV
                    );
      if (!EFI_ERROR (Status)) {
        Status = FV->ReadSection (
                      FV,
                      &FvFilePath->FvFileName,
                      EFI_SECTION_USER_INTERFACE,
                      0,
                      &Buffer,
                      &BufferSize,
                      &AuthenticationStatus
                      );
        if (!EFI_ERROR (Status)) {
          break;
        }

        Buffer = NULL;
      } else {
        Status = gBS->HandleProtocol (
                      Image->DeviceHandle,
                      &gEfiFirmwareVolume2ProtocolGuid,
                      &FV2
                      );
        if (!EFI_ERROR (Status)) {
          Status = FV2->ReadSection (
                          FV2,
                          &FvFilePath->FvFileName,
                          EFI_SECTION_USER_INTERFACE,
                          0,
                          &Buffer,
                          &BufferSize,
                          &AuthenticationStatus
                          );
          if (!EFI_ERROR (Status)) {
            break;
          }

          Buffer = NULL;
        }
      }
    }
    //
    // Next device path node
    //
    DevPathNode = NextDevicePathNode (DevPathNode);
  }

  return Buffer;
}

//
// Get Load Image Protocol By Address
// Address - An unsigned integer reporesent the caller return address.
// return  - A pointer to the Loaded Image Protocol of which the Address is inside the image area.
//
EFI_LOADED_IMAGE_PROTOCOL *
GetImageByAddress (
  UINTN Address,
  UINTN *Offset
  )
{
  EFI_STATUS Status;
  EFI_HANDLE                    *HandleBuffer;
  UINTN                         HandleCount;
  UINTN                         i;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;

  //
  // Get All Load Image Protocol.
  //
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiLoadedImageProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer);
                 
  if (EFI_ERROR (Status)) {
    return NULL;
  }
 
  LoadedImage = NULL;
  
  //
  // Seek the EFI Image within which the Address is
  // Address > Image Base &&
  // Address < Image Base + Image Size
  //
  for (i = 0; i < HandleCount; i ++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer [i],
                    &gEfiLoadedImageProtocolGuid,
                    (VOID **) &LoadedImage);
    
    if (!EFI_ERROR (Status)) {
      //
      // Address is inside the image area or not?
      //
      if (Address > (UINTN) LoadedImage->ImageBase && Address <= (UINTN) LoadedImage->ImageBase + LoadedImage->ImageSize) {
        *Offset = Address - (UINTN) (LoadedImage->ImageBase);
        break;
      }
    }
    LoadedImage = NULL;
  }

  //
  // Free the handle buffer.
  //
  gBS->FreePool (HandleBuffer);
 
  return LoadedImage;
}

//
// Keep AllocatePool Information
//
VOID
KeepAllocatePoolInfo (
  EFI_MEMORY_TYPE PoolType,
  UINTN Size,
  VOID **Buffer,
  EFI_STATUS Status
  )
{
  mPoolType = PoolType;
  mSize = Size;
  if (Buffer != NULL) {
    mBuffer = *Buffer;
  } else {
    mBuffer = NULL;
  }
  mStatus = Status;
}

//
// Print Caller function's information.
//

VOID
PrintCallerInformation (
  UINTN Address
  )
{
  EFI_LOADED_IMAGE_PROTOCOL *CallerImage;
  CHAR16 *ImageName;
  UINTN Offset;
 
  CallerImage = GetImageByAddress (Address, &Offset);
 
  if (CallerImage == NULL) {
    DEBUG ((DEBUG_ERROR, "Unknown Caller return Address = 0x%lx", Address));
  } else {
    ImageName = GetImageName (CallerImage);
    if (ImageName == NULL) {
      DEBUG ((DEBUG_ERROR, "Unknow Name + 0x%08lx   ", ImageName, Offset));
    } else {
      DEBUG ((DEBUG_ERROR, "%-10s + 0x%08lx   ", ImageName, Offset));
      gBS->FreePool (ImageName);
    }
  }
  DEBUG ((DEBUG_ERROR, " Size = %8d  Memory Type = %x\n", mSize, mPoolType));
}

//
// The Hook Function of AllocatePool
//
EFI_STATUS
AllocatePoolHook (
  EFI_MEMORY_TYPE PoolType,
  UINTN Size,
  VOID **Buffer
  )
{
  EFI_STATUS Status;
  UINTN      StackAddr;
  EFI_ALLOCATE_POOL AllocatePool;
  
  AllocatePool = (EFI_ALLOCATE_POOL) mOriginalFunctionAddress;

  //
  // Check if the Hook function is accessed again.
  //
  if (AcquireSpinLockOrFail (&mInProcessSpinLock)) {
    
    //
    // Get the return address of the caller function.
    //
    StackAddr = (UINTN) _AddressOfReturnAddress();
    mCallerReturnAddress = (UINTN) *((VOID **) StackAddr);
    
    KeepAllocatePoolInfo (PoolType, Size, NULL, EFI_SUCCESS);
    
    PrintCallerInformation (mCallerReturnAddress);
    
    //
    // Call real AllocatePool function.
    //
    Status = AllocatePool (PoolType, Size, Buffer);
 
    KeepAllocatePoolInfo (PoolType, Size, Buffer, Status);
   
    ReleaseSpinLock (&mInProcessSpinLock);
  } else {
    Status = AllocatePool (PoolType, Size, Buffer);
  }
 
  return Status;
}

//
// Driver Entry Point
// Initialize the global variable & Hook the AllocatePool function.
//
EFI_STATUS
DriverEntryPoint (
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_BOOT_SERVICES *BootService;

  mOriginalFunctionAddress = (UINTN) NULL;
 
  BootService = SystemTable->BootServices;
 
  //
  // Keep original AllocatePool function pointer then replace to hook funnction.
  //
  mOriginalFunctionAddress = (UINTN) BootService->AllocatePool;
  BootService->AllocatePool = (EFI_ALLOCATE_POOL) AllocatePoolHook;
  
  InitializeSpinLock (&mInProcessSpinLock);

  return EFI_SUCCESS;
}