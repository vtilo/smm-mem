#ifndef DXE_PRIVATE_H
#define DXE_PRIVATE_H

#include "../common/common.h"

typedef struct {
  UINT32 Signature;
  UINT32 Length;
  UINT8 Revision;
  UINT8 Checksum;
  UINT8 OemId[6];
  UINT8 OemTableId[8];
  UINT32 OemRevision;
  UINT32 CreatorId;
  UINT32 CreatorRevision;
} AcpiHeader;

typedef struct {
  UINT32 Type;
  UINT32 Pad;
  UINT64 PhysicalStart;
  UINT64 VirtualStart;
  UINT64 NumberOfPages;
  UINT64 Attribute;
} MemoryDescriptor;

typedef struct {
  UINT32 Version;
  UINT32 NumberOfEntries;
  UINT32 DescriptorSize;
  UINT32 Reserved;
} CommunicationRegionTable;

#define EFI_RESERVED_MEMORY_TYPE 0U
#define EFI_CONVENTIONAL_MEMORY 7U
#define EFI_ACPI_MEMORY_NVS 10U

typedef struct {
  EFI_SYSTEM_TABLE *SystemTable;
  EFI_PHYSICAL_ADDRESS MailboxPhysical;
  EFI_EVENT RetryTimerEvent;
  EFI_EVENT ReadyToBootEvent;
  EFI_EVENT SmmCommEvent;
  EFI_EVENT AcpiEvent;
  VOID *SmmCommRegistration;
  VOID *AcpiRegistration;
  UINT32 Configured;
  UINT32 ConfigureAttempts;
  UINT32 WmiInstalled;
  UINT32 FastWmiInstalled;
  UINT32 WmiAttempts;
  CONFIG PublishedConfig;
  UINT8 Ssdt[512];
  UINT8 FastSsdt[512];
} DxeState;

extern DxeState Dxe;
UINTN BuildSsdt( UINT8 *Buffer, UINTN Capacity, BOOLEAN Fast );
EFI_STATUS AllocateMailbox( VOID );
EFI_STATUS InstallWmi( VOID );
EFI_STATUS PublishConfig( VOID );
EFI_STATUS ConfigureSmm( VOID );
VOID RegisterRetryTimer( VOID );
VOID RegisterSmmCommNotify( VOID );
VOID RegisterAcpiNotify( VOID );
VOID RegisterReadyToBootRetry( VOID );

#endif