#ifndef SMM_MEM_UEFI_H
#define SMM_MEM_UEFI_H

typedef unsigned char UINT8;
typedef unsigned char BOOLEAN;
typedef unsigned short UINT16;
typedef unsigned int UINT32;
typedef unsigned long long UINT64;
typedef unsigned long long UINTN;
typedef unsigned long long size_t;
typedef unsigned short CHAR16;
typedef void VOID;
typedef VOID *EFI_HANDLE;
typedef VOID *EFI_EVENT;
typedef UINTN EFI_TPL;
typedef UINT64 EFI_STATUS;
typedef UINT64 EFI_PHYSICAL_ADDRESS;

#define EFIAPI __cdecl

#define EFI_ERROR_BIT 0x8000000000000000ULL
#define EFIERR( Code ) ( ( EFI_STATUS )( EFI_ERROR_BIT | ( UINT64 )( Code ) ) )

#define EFI_SUCCESS ( ( EFI_STATUS )0 )
#define EFI_INVALID_PARAMETER EFIERR( 2 )
#define EFI_UNSUPPORTED EFIERR( 3 )
#define EFI_OUT_OF_RESOURCES EFIERR( 9 )
#define EFI_NOT_FOUND EFIERR( 14 )
#define EFI_ERROR( Status ) ( ( ( ( EFI_STATUS )( Status ) ) & EFI_ERROR_BIT ) != 0 )

#define EFI_RUNTIME_SERVICES_DATA 6U
#define SERIAL 1U
#define COM1_PORT 0x3F8U

typedef struct {
  UINT32 Data1;
  UINT16 Data2;
  UINT16 Data3;
  UINT8 Data4[8];
} EFI_GUID;

typedef struct {
  UINT64 Signature;
  UINT32 Revision;
  UINT32 HeaderSize;
  UINT32 CRC32;
  UINT32 Reserved;
} EFI_TABLE_HEADER;

typedef struct {
  EFI_GUID VendorGuid;
  VOID *VendorTable;
} EFI_CONFIGURATION_TABLE;

typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
typedef struct EFI_SYSTEM_TABLE EFI_SYSTEM_TABLE;
typedef struct EFI_ACPI_TABLE_PROTOCOL EFI_ACPI_TABLE_PROTOCOL;
typedef struct EFI_SMM_COMMUNICATION_PROTOCOL EFI_SMM_COMMUNICATION_PROTOCOL;
typedef struct EFI_SMM_BASE2_PROTOCOL EFI_SMM_BASE2_PROTOCOL;
typedef struct EFI_SMM_SYSTEM_TABLE2 EFI_SMM_SYSTEM_TABLE2;
typedef struct EFI_SMM_SW_DISPATCH2_PROTOCOL EFI_SMM_SW_DISPATCH2_PROTOCOL;

typedef EFI_STATUS( EFIAPI *EFI_LOCATE_PROTOCOL )(
  EFI_GUID *Protocol, VOID *Registration, VOID **Interface );
typedef enum { AllocateAnyPages, AllocateMaxAddress, AllocateAddress } EFI_ALLOCATE_TYPE;
typedef EFI_STATUS( EFIAPI *EFI_ALLOCATE_PAGES )(
  EFI_ALLOCATE_TYPE Type, UINT32 MemoryType, UINTN Pages, EFI_PHYSICAL_ADDRESS *Memory );
typedef EFI_STATUS( EFIAPI *EFI_INSTALL_CONFIGURATION_TABLE )( EFI_GUID *Guid, VOID *Table );
typedef VOID( EFIAPI *EFI_EVENT_NOTIFY )( EFI_EVENT Event, VOID *Context );
typedef enum { TimerCancel, TimerPeriodic, TimerRelative } EFI_TIMER_DELAY;
typedef EFI_STATUS( EFIAPI *EFI_CREATE_EVENT )( UINT32 Type,
  EFI_TPL NotifyTpl,
  EFI_EVENT_NOTIFY NotifyFunction,
  VOID *NotifyContext,
  EFI_EVENT *Event );
typedef EFI_STATUS( EFIAPI *EFI_SET_TIMER )(
  EFI_EVENT Event, EFI_TIMER_DELAY Type, UINT64 TriggerTime );
typedef EFI_STATUS( EFIAPI *EFI_CLOSE_EVENT )( EFI_EVENT Event );
typedef EFI_STATUS( EFIAPI *EFI_REGISTER_PROTOCOL_NOTIFY )(
  EFI_GUID *Protocol, EFI_EVENT Event, VOID **Registration );
typedef EFI_STATUS( EFIAPI *EFI_CREATE_EVENT_EX )( UINT32 Type,
  EFI_TPL NotifyTpl,
  EFI_EVENT_NOTIFY NotifyFunction,
  const VOID *NotifyContext,
  const EFI_GUID *EventGroup,
  EFI_EVENT *Event );

#define EVT_TIMER 0x80000000U
#define EVT_NOTIFY_SIGNAL 0x00000200U
#define TPL_CALLBACK 8U

struct EFI_BOOT_SERVICES {
  EFI_TABLE_HEADER Hdr;
  VOID *RaiseTPL;
  VOID *RestoreTPL;
  EFI_ALLOCATE_PAGES AllocatePages;
  VOID *FreePages;
  VOID *GetMemoryMap;
  VOID *AllocatePool;
  VOID *FreePool;
  EFI_CREATE_EVENT CreateEvent;
  EFI_SET_TIMER SetTimer;
  VOID *WaitForEvent;
  VOID *SignalEvent;
  EFI_CLOSE_EVENT CloseEvent;
  VOID *CheckEvent;
  VOID *InstallProtocolInterface;
  VOID *ReinstallProtocolInterface;
  VOID *UninstallProtocolInterface;
  VOID *HandleProtocol;
  VOID *Reserved;
  EFI_REGISTER_PROTOCOL_NOTIFY RegisterProtocolNotify;
  VOID *LocateHandle;
  VOID *LocateDevicePath;
  EFI_INSTALL_CONFIGURATION_TABLE InstallConfigurationTable;
  VOID *LoadImage;
  VOID *StartImage;
  VOID *Exit;
  VOID *UnloadImage;
  VOID *ExitBootServices;
  VOID *GetNextMonotonicCount;
  VOID *Stall;
  VOID *SetWatchdogTimer;
  VOID *ConnectController;
  VOID *DisconnectController;
  VOID *OpenProtocol;
  VOID *CloseProtocol;
  VOID *OpenProtocolInformation;
  VOID *ProtocolsPerHandle;
  VOID *LocateHandleBuffer;
  EFI_LOCATE_PROTOCOL LocateProtocol;
  VOID *InstallMultipleProtocolInterfaces;
  VOID *UninstallMultipleProtocolInterfaces;
  VOID *CalculateCrc32;
  VOID *CopyMem;
  VOID *SetMem;
  EFI_CREATE_EVENT_EX CreateEventEx;
};

struct EFI_SYSTEM_TABLE {
  EFI_TABLE_HEADER Hdr;
  CHAR16 *FirmwareVendor;
  UINT32 FirmwareRevision;
  EFI_HANDLE ConsoleInHandle;
  VOID *ConIn;
  EFI_HANDLE ConsoleOutHandle;
  VOID *ConOut;
  EFI_HANDLE StandardErrorHandle;
  VOID *StdErr;
  VOID *RuntimeServices;
  EFI_BOOT_SERVICES *BootServices;
  UINTN NumberOfTableEntries;
  EFI_CONFIGURATION_TABLE *ConfigurationTable;
};

typedef EFI_STATUS( EFIAPI *EFI_INSTALL_ACPI_TABLE )( const EFI_ACPI_TABLE_PROTOCOL *This,
  VOID *AcpiTableBuffer,
  UINTN AcpiTableBufferSize,
  UINTN *TableKey );
typedef EFI_STATUS( EFIAPI *EFI_UNINSTALL_ACPI_TABLE )(
  const EFI_ACPI_TABLE_PROTOCOL *This, UINTN TableKey );

struct EFI_ACPI_TABLE_PROTOCOL {
  EFI_INSTALL_ACPI_TABLE InstallAcpiTable;
  EFI_UNINSTALL_ACPI_TABLE UninstallAcpiTable;
};

typedef EFI_STATUS( EFIAPI *EFI_SMM_COMMUNICATE )(
  const EFI_SMM_COMMUNICATION_PROTOCOL *This, VOID *CommBuffer, UINTN *CommSize );

struct EFI_SMM_COMMUNICATION_PROTOCOL {
  EFI_SMM_COMMUNICATE Communicate;
};

typedef EFI_STATUS( EFIAPI *EFI_SMM_HANDLER_ENTRY_POINT2 )(
  EFI_HANDLE DispatchHandle, const VOID *Context, VOID *CommBuffer, UINTN *CommBufferSize );
typedef EFI_STATUS( EFIAPI *EFI_SMM_INTERRUPT_REGISTER )(
  EFI_SMM_HANDLER_ENTRY_POINT2 Handler, const EFI_GUID *HandlerType, EFI_HANDLE *DispatchHandle );
typedef EFI_STATUS( EFIAPI *EFI_SMM_INSIDE_OUT2 )(
  const EFI_SMM_BASE2_PROTOCOL *This, BOOLEAN *InSmram );
typedef EFI_STATUS( EFIAPI *EFI_SMM_GET_SMST_LOCATION2 )(
  const EFI_SMM_BASE2_PROTOCOL *This, EFI_SMM_SYSTEM_TABLE2 **Smst );

struct EFI_SMM_BASE2_PROTOCOL {
  EFI_SMM_INSIDE_OUT2 InSmm;
  EFI_SMM_GET_SMST_LOCATION2 GetSmstLocation;
};

typedef struct {
  UINTN SwSmiInputValue;
} EFI_SMM_SW_REGISTER_CONTEXT;

typedef EFI_STATUS( EFIAPI *EFI_SMM_SW_REGISTER2 )( const EFI_SMM_SW_DISPATCH2_PROTOCOL *This,
  EFI_SMM_HANDLER_ENTRY_POINT2 DispatchFunction,
  EFI_SMM_SW_REGISTER_CONTEXT *RegisterContext,
  EFI_HANDLE *DispatchHandle );
typedef EFI_STATUS( EFIAPI *EFI_SMM_SW_UNREGISTER2 )(
  const EFI_SMM_SW_DISPATCH2_PROTOCOL *This, EFI_HANDLE DispatchHandle );

struct EFI_SMM_SW_DISPATCH2_PROTOCOL {
  EFI_SMM_SW_REGISTER2 Register;
  EFI_SMM_SW_UNREGISTER2 UnRegister;
  UINTN MaximumSwiValue;
};

typedef struct {
  VOID *Read;
  VOID *Write;
} EFI_SMM_IO_ACCESS2;

typedef struct {
  EFI_SMM_IO_ACCESS2 Mem;
  EFI_SMM_IO_ACCESS2 Io;
} EFI_SMM_CPU_IO2_PROTOCOL;

struct EFI_SMM_SYSTEM_TABLE2 {
  EFI_TABLE_HEADER Hdr;
  UINT16 *SmmFirmwareVendor;
  UINT32 SmmFirmwareRevision;
  VOID *SmmInstallConfigurationTable;
  EFI_SMM_CPU_IO2_PROTOCOL SmmIo;
  VOID *SmmAllocatePool;
  VOID *SmmFreePool;
  VOID *SmmAllocatePages;
  VOID *SmmFreePages;
  VOID *SmmStartupThisAp;
  UINTN CurrentlyExecutingCpu;
  UINTN NumberOfCpus;
  UINTN *CpuSaveStateSize;
  VOID **CpuSaveState;
  UINTN NumberOfTableEntries;
  VOID *SmmConfigurationTable;
  VOID *SmmInstallProtocolInterface;
  VOID *SmmUninstallProtocolInterface;
  VOID *SmmHandleProtocol;
  VOID *SmmRegisterProtocolNotify;
  VOID *SmmLocateHandle;
  VOID *SmmLocateProtocol;
  VOID *SmiManage;
  EFI_SMM_INTERRUPT_REGISTER SmiHandlerRegister;
  VOID *SmiHandlerUnRegister;
};

#pragma pack( push, 1 )
typedef struct {
  EFI_GUID HeaderGuid;
  UINTN MessageLength;
  UINT8 Data[1];
} EFI_SMM_COMMUNICATE_HEADER;
#pragma pack( pop )
#endif