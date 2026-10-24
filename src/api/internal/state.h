#ifndef API_STATE_H
#define API_STATE_H

typedef ULONG( WINAPI *WmiOpenBlockFn )( GUID *Guid, DWORD Access, HANDLE *Block );
typedef ULONG( WINAPI *WmiExecuteMethodFn )( HANDLE Block,
  const wchar_t *Instance,
  ULONG Method,
  ULONG InputSize,
  void *Input,
  ULONG *OutputSize,
  void *Output );
typedef ULONG( WINAPI *WmiCloseBlockFn )( HANDLE Block );

typedef struct {
  HMODULE Library;
  WmiOpenBlockFn OpenBlock;
  WmiExecuteMethodFn ExecuteMethod;
  WmiCloseBlockFn CloseBlock;
  HANDLE Block;
  size_t Instance;
  uint8_t *Staging;
  uint32_t Pid;
  uint64_t Sequence;
} ApiState;

static ApiState Api;
static SRWLOCK ApiLock = SRWLOCK_INIT;
static GUID MemoryGuid = MEMORY_WMI_GUID;

static const wchar_t *const MemoryInstances[] = {
  L"ACPI\\PNP0C14\\Mem_0", L"ACPI\\PNP0C14\\SMMM_0", L"ACPI\\PNP0C14\\0_0", L"" };

#define WMI_GUID_EXECUTE 0x0010U
#define WMI_METHOD_ID 1U
#define WMI_INSTANCE_COUNT ( sizeof( MemoryInstances ) / sizeof( MemoryInstances[0] ) )

#endif