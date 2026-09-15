#ifndef DXE_PRIVATE_H
#define DXE_PRIVATE_H

#include "../Common.h"

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
  EFI_SYSTEM_TABLE *systemTable;
  EFI_PHYSICAL_ADDRESS mailboxPhysical;
  EFI_EVENT retryTimerEvent;
  EFI_EVENT readyToBootEvent;
  EFI_EVENT smmCommEvent;
  EFI_EVENT acpiEvent;
  VOID *smmCommRegistration;
  VOID *acpiRegistration;
  UINT32 configured;
  UINT32 configureAttempts;
  UINT32 wmiInstalled;
  UINT32 fastWmiInstalled;
  UINT32 wmiAttempts;
  CONFIG publishedConfig;
  UINT8 ssdt[512];
  UINT8 fastSsdt[512];
} DxeState;

extern DxeState dxe;
UINTN buildSsdt( UINT8 *buffer, UINTN capacity, BOOLEAN fast );
EFI_STATUS allocateMailbox( VOID );
EFI_STATUS installWmi( VOID );
EFI_STATUS publishConfig( VOID );
EFI_STATUS configureSmm( VOID );
VOID registerRetryTimer( VOID );
VOID registerSmmCommNotify( VOID );
VOID registerAcpiNotify( VOID );
VOID registerReadyToBootRetry( VOID );

#endif
