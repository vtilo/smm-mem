#ifndef SMM_PRIVATE_H
#define SMM_PRIVATE_H

#include <intrin.h>
#include "../common/common.h"

#pragma intrinsic( __inbyte )
#pragma intrinsic( __outbyte )
#pragma intrinsic( __readcr0 )
#pragma intrinsic( __writecr0 )
#pragma intrinsic( __readcr3 )
#pragma intrinsic( __readcr4 )
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
  const EFI_SMM_CPU_PROTOCOL *Self, UINTN Width, UINT32 Reg, UINTN CpuIndex, VOID *Buffer );

typedef EFI_STATUS( EFIAPI *WRITE_SAVE_STATE )(
  const EFI_SMM_CPU_PROTOCOL *Self, UINTN Width, UINT32 Reg, UINTN CpuIndex, const VOID *Buffer );

typedef EFI_STATUS( EFIAPI *SMM_ALLOCATE_PAGES )(
  EFI_ALLOCATE_TYPE Type, UINT32 MemoryType, UINTN Pages, EFI_PHYSICAL_ADDRESS *Memory );

struct EFI_SMM_CPU_PROTOCOL {
  READ_SAVE_STATE ReadSaveState;
  WRITE_SAVE_STATE WriteSaveState;
};

typedef struct {
  UINT32 Pid;
  UINT64 Eprocess;
} ProcessHint;

typedef struct {
  EFI_SYSTEM_TABLE *SystemTable;
  EFI_SMM_SYSTEM_TABLE2 *Table;
  EFI_SMM_CPU_PROTOCOL *Cpu;
  EFI_PHYSICAL_ADDRESS MailboxPhysical;
  UINT32 MailboxSize;
  UINT32 SwSmiValue;
  EFI_HANDLE CommHandle;
  EFI_HANDLE SwHandle;
  UINT64 KernelCr3;
  UINT64 KernelBase;
  UINT64 SystemProcess;
  UINT64 LoadedModuleList;
  UINT32 PidOffset;
  UINT32 LinksOffset;
  UINT32 NameOffset;
  UINT32 PebOffset;
  UINT32 Cr3Offset;
  UINT64 PhysicalMask;
  UINT32 MapReady;
  UINT64 MapWindow;
  UINT64 MapPdpt;
  UINT64 MapWindowBase;
  UINT32 MapWindowBaseValid;
  BOOLEAN KernelStable;
  ProcessHint ProcessHints[PROCESS_HINT_COUNT];
  UINT32 NextProcessHint;
} SmmState;

typedef struct {
  UINT64 Eprocess;
  UINT64 Cr3;
} ProcessContext;

extern SmmState Smm;
EFI_STATUS ResolveProcess( UINT32 Pid, ProcessContext *Process );
EFI_STATUS ResolveCallerBuffer( const REQUEST *Request, UINT32 Size, ProcessContext *Caller );
EFI_STATUS TransferBuffer( REQUEST *Request, UINT64 *Transferred );
EFI_STATUS QueryBuffer( REQUEST *Request, UINT64 *Result, UINT32 *Size );

VOID WritePte( volatile UINT64 *Entry, UINT64 Value );
BOOLEAN IsKernelPointer( UINT64 Value );
BOOLEAN IsUserPointer( UINT64 Value );
EFI_STATUS InitMapWindow( VOID );
EFI_STATUS MapWindow( UINT64 Address );
EFI_STATUS CopyPhysical( UINT64 Address, VOID *Buffer, UINTN Size, BOOLEAN Write );
UINT64 PhysicalMask( VOID );
UINT64 PteAddress( UINT64 Entry );
EFI_STATUS ReadPhysical64( UINT64 Address, UINT64 *Value );
EFI_STATUS TranslateVirtual( UINT64 Cr3, UINT64 Va, UINT64 *Pa );
EFI_STATUS CopyVirtual( UINT64 Cr3, UINT64 Va, VOID *Buffer, UINTN Size, BOOLEAN Write );
EFI_STATUS ReadVirtual64( UINT64 Cr3, UINT64 Va, UINT64 *Value );
EFI_STATUS ReadVirtual32( UINT64 Cr3, UINT64 Va, UINT32 *Value );
EFI_STATUS ReadVirtual16( UINT64 Cr3, UINT64 Va, UINT16 *Value );
VOID CopyText( char *Out, UINTN OutSize, const char *Text );
char LowerChar( char Ch );
BOOLEAN SameName( const char *A, const char *B );
const char *BaseName( const char *Path );
BOOLEAN SameBaseName( const char *Path, const char *Name );
EFI_STATUS ReadAscii( UINT64 Cr3, UINT64 Va, char *Out, UINTN OutSize );
EFI_STATUS ReadUnicodeName( UINT64 Cr3, UINT64 StringAddress, char *Out, UINTN OutSize );
EFI_STATUS ResolveExport( UINT64 Cr3, UINT64 Base, const char *Name, UINT64 *Address );
VOID LocateSmmCpu( VOID );
EFI_STATUS ReadSavedCr3( UINTN CpuIndex, UINT64 *Cr3 );
BOOLEAN LooksLikeProcessList( UINT64 Eprocess, UINT32 PidOffset );
EFI_STATUS ResolveListLayout( VOID );
EFI_STATUS ResolveNameOffset( VOID );
EFI_STATUS ResolveCr3Offset( VOID );
EFI_STATUS TryKernelCr3( UINT64 Cr3, UINT64 Lstar );
EFI_STATUS InitKernel( VOID );
EFI_STATUS ResolveProcessLayout( VOID );
EFI_STATUS GetProcessCr3( UINT64 Eprocess, UINT64 *Cr3 );
EFI_STATUS FindPeb( UINT64 Eprocess, UINT64 Cr3, UINT64 *Peb );
EFI_STATUS FindImageBase( UINT64 Eprocess, UINT64 Cr3, UINT64 *Base );
EFI_STATUS ReadProcessImagePath( UINT64 Eprocess, UINT64 Cr3, char *Out, UINTN OutSize );
EFI_STATUS FillProcessInfo( UINT64 Eprocess, PROCESS_INFO *Info );
EFI_STATUS FindProcessPid( UINT32 Pid, PROCESS_INFO *Info );
EFI_STATUS FindProcessName( const char *Name, PROCESS_INFO *Info );
EFI_STATUS ReadLdrEntry(
  UINT64 Cr3, UINT64 Link, UINT32 Bias, char *Name, UINTN NameSize, UINT64 *Base, UINT32 *Size );
EFI_STATUS FindUserModule( UINT32 Pid, const char *Name, MODULE_INFO *Module );
EFI_STATUS ResolveKernelModule( const char *Name, MODULE_INFO *Module );
VOID Reply( RESPONSE *Response,
  REQUEST *Request,
  EFI_STATUS Status,
  UINT64 Result,
  const VOID *Data,
  UINT32 Size );
EFI_STATUS HandleRequest( REQUEST *Request, RESPONSE *Response );
EFI_STATUS ProcessRequest( VOID );
EFI_STATUS EFIAPI SwSmiHandler(
  EFI_HANDLE DispatchHandle, const VOID *Context, VOID *CommBuffer, UINTN *CommBufferSize );
EFI_STATUS RegisterSwSmi( VOID );
EFI_STATUS ValidateSmst( VOID );
EFI_STATUS InitSmm( VOID );
EFI_STATUS ApplyConfig( CONFIG *Config, const char *Source );
EFI_STATUS EFIAPI ConfigCommHandler(
  EFI_HANDLE DispatchHandle, const VOID *Context, VOID *CommBuffer, UINTN *CommBufferSize );
EFI_STATUS RegisterConfigComm( VOID );
EFI_STATUS ConfigureFromPublishedTable( VOID );

#endif