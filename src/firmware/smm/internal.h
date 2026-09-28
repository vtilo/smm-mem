#ifndef SMM_PRIVATE_H
#define SMM_PRIVATE_H

#include <intrin.h>
#include "../common/common.h"

#pragma intrinsic( __inbyte )
#pragma intrinsic( __outbyte )
#pragma intrinsic( __readcr0 )
#pragma intrinsic( __writecr0 )
#pragma intrinsic( __readcr3 )
#pragma intrinsic( __writecr3 )
#pragma intrinsic( __readmsr )
#pragma intrinsic( __cpuidex )

#define PAGE_SIZE 0x1000ULL
#define PAGE_MASK 0xFFFFFFFFFFFFF000ULL
#define PAGE_2MB_SIZE 0x200000ULL
#define PAGE_2MB_MASK 0xFFFFFFFFFFE00000ULL
#define PAGE_1GB_SIZE 0x40000000ULL
#define PAGE_1GB_MASK 0xFFFFFFFFC0000000ULL
#define PTE_PRESENT 1ULL
#define PTE_RW 2ULL
#define PTE_LARGE ( 1ULL << 7 )
#define PAGE_TABLE_FLAGS ( PTE_PRESENT | PTE_RW )
#define CR0_WP ( 1ULL << 16 )
#define MSR_LSTAR 0xC0000082U
#define SAVE_STATE_CR3 53U
#define PROCESS_HINT_COUNT 4U

typedef struct EFI_SMM_CPU_PROTOCOL EFI_SMM_CPU_PROTOCOL;
typedef EFI_STATUS( EFIAPI *READ_SAVE_STATE )(
  const EFI_SMM_CPU_PROTOCOL *self, UINTN width, UINT32 reg, UINTN cpuIndex, VOID *buffer );
typedef EFI_STATUS( EFIAPI *WRITE_SAVE_STATE )(
  const EFI_SMM_CPU_PROTOCOL *self, UINTN width, UINT32 reg, UINTN cpuIndex, const VOID *buffer );
typedef EFI_STATUS( EFIAPI *SMM_ALLOCATE_PAGES )(
  EFI_ALLOCATE_TYPE type, UINT32 memoryType, UINTN pages, EFI_PHYSICAL_ADDRESS *memory );

struct EFI_SMM_CPU_PROTOCOL {
  READ_SAVE_STATE ReadSaveState;
  WRITE_SAVE_STATE WriteSaveState;
};

typedef struct {
  UINT32 pid;
  UINT64 eprocess;
} ProcessHint;

typedef struct {
  EFI_SYSTEM_TABLE *systemTable;
  EFI_SMM_SYSTEM_TABLE2 *table;
  EFI_SMM_CPU_PROTOCOL *cpu;
  EFI_PHYSICAL_ADDRESS mailboxPhysical;
  UINT32 mailboxSize;
  UINT32 swSmiValue;
  EFI_HANDLE commHandle;
  EFI_HANDLE swHandle;
  UINT64 kernelCr3;
  UINT64 kernelBase;
  UINT64 systemProcess;
  UINT64 loadedModuleList;
  UINT32 pidOffset;
  UINT32 linksOffset;
  UINT32 nameOffset;
  UINT32 pebOffset;
  UINT32 cr3Offset;
  UINT64 physicalMask;
  UINT32 mapReady;
  UINT64 mapWindow;
  UINT64 mapPdpt;
  UINT64 mapWindowBase;
  UINT32 mapWindowBaseValid;
  BOOLEAN kernelStable;
  UINT32 features;
  ProcessHint processHints[PROCESS_HINT_COUNT];
  UINT32 nextProcessHint;
} SmmState;

typedef struct {
  UINT64 eprocess;
  UINT64 cr3;
} ProcessContext;

extern SmmState smm;
EFI_STATUS resolveProcess( UINT32 pid, ProcessContext *process );
EFI_STATUS transferBuffer( REQUEST *request, UINT64 *transferred );

VOID writePte( volatile UINT64 *entry, UINT64 value );
BOOLEAN isKernelPointer( UINT64 value );
BOOLEAN isUserPointer( UINT64 value );
EFI_STATUS initMapWindow( VOID );
EFI_STATUS mapWindow( UINT64 address );
EFI_STATUS copyPhysical( UINT64 address, VOID *buffer, UINTN size, BOOLEAN write );
UINT64 physicalMask( VOID );
UINT64 pteAddress( UINT64 entry );
EFI_STATUS readPhysical64( UINT64 address, UINT64 *value );
EFI_STATUS translateVirtual( UINT64 cr3, UINT64 va, UINT64 *pa );
EFI_STATUS copyVirtual( UINT64 cr3, UINT64 va, VOID *buffer, UINTN size, BOOLEAN write );
EFI_STATUS readVirtual64( UINT64 cr3, UINT64 va, UINT64 *value );
EFI_STATUS readVirtual32( UINT64 cr3, UINT64 va, UINT32 *value );
EFI_STATUS readVirtual16( UINT64 cr3, UINT64 va, UINT16 *value );
VOID copyText( char *out, UINTN outSize, const char *text );
char lowerChar( char ch );
BOOLEAN sameName( const char *a, const char *b );
const char *baseName( const char *path );
BOOLEAN sameBaseName( const char *path, const char *name );
EFI_STATUS readAscii( UINT64 cr3, UINT64 va, char *out, UINTN outSize );
EFI_STATUS readUnicodeName( UINT64 cr3, UINT64 stringAddress, char *out, UINTN outSize );
EFI_STATUS resolveExport( UINT64 cr3, UINT64 base, const char *name, UINT64 *address );
VOID locateSmmCpu( VOID );
EFI_STATUS readSavedCr3( UINTN cpuIndex, UINT64 *cr3 );
BOOLEAN looksLikeProcessList( UINT64 eprocess, UINT32 pidOffset );
EFI_STATUS resolveListLayout( VOID );
EFI_STATUS resolveNameOffset( VOID );
EFI_STATUS resolveCr3Offset( VOID );
EFI_STATUS tryKernelCr3( UINT64 cr3, UINT64 lstar );
EFI_STATUS initKernel( VOID );
EFI_STATUS resolveProcessLayout( VOID );
EFI_STATUS getProcessCr3( UINT64 eprocess, UINT64 *cr3 );
EFI_STATUS findPeb( UINT64 eprocess, UINT64 cr3, UINT64 *peb );
EFI_STATUS findImageBase( UINT64 eprocess, UINT64 cr3, UINT64 *base );
EFI_STATUS readProcessImagePath( UINT64 eprocess, UINT64 cr3, char *out, UINTN outSize );
EFI_STATUS fillProcessInfo( UINT64 eprocess, PROCESS_INFO *info );
EFI_STATUS findProcessPid( UINT32 pid, PROCESS_INFO *info );
EFI_STATUS findProcessName( const char *name, PROCESS_INFO *info );
EFI_STATUS readLdrEntry(
  UINT64 cr3, UINT64 link, UINT32 bias, char *name, UINTN nameSize, UINT64 *base, UINT32 *size );
EFI_STATUS findUserModule( UINT32 pid, const char *name, MODULE_INFO *module );
EFI_STATUS findKernelModule( const char *name, MODULE_INFO *module );
VOID reply( RESPONSE *response,
  REQUEST *request,
  EFI_STATUS status,
  UINT64 result,
  const VOID *data,
  UINT32 dataSize );
EFI_STATUS handleRequest( REQUEST *request, RESPONSE *response );
EFI_STATUS processRequest( VOID );
EFI_STATUS EFIAPI swSmiHandler(
  EFI_HANDLE dispatchHandle, const VOID *context, VOID *commBuffer, UINTN *commBufferSize );
EFI_STATUS registerSwSmi( VOID );
EFI_STATUS validateSmst( VOID );
EFI_STATUS initSmm( VOID );
EFI_STATUS applyConfig( CONFIG *config, const char *source );
EFI_STATUS EFIAPI configCommHandler(
  EFI_HANDLE dispatchHandle, const VOID *context, VOID *commBuffer, UINTN *commBufferSize );
EFI_STATUS registerConfigComm( VOID );
EFI_STATUS configureFromPublishedTable( VOID );

#endif
