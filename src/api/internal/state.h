#ifndef CLIENT_STATE_H
#define CLIENT_STATE_H

typedef ULONG( WINAPI *WmiOpenBlockFn )( GUID *guid, DWORD access, HANDLE *block );
typedef ULONG( WINAPI *WmiExecuteMethodFn )( HANDLE block,
  const wchar_t *instance,
  ULONG method,
  ULONG inputSize,
  void *input,
  ULONG *outputSize,
  void *output );
typedef ULONG( WINAPI *WmiCloseBlockFn )( HANDLE block );

typedef struct {
  HMODULE library;
  WmiOpenBlockFn openBlock;
  WmiExecuteMethodFn executeMethod;
  WmiCloseBlockFn closeBlock;
  HANDLE block;
  size_t instance;
  HANDLE fastBlock;
  size_t fastInstance;
  int fastChecked;
  int fastAvailable;
  uint8_t *staging;
  uint32_t pid;
  uint64_t sequence;
} ClientState;

static ClientState client;
static SRWLOCK clientLock = SRWLOCK_INIT;

static GUID memoryGuid = MEMORY_WMI_GUID;

static GUID fastMemoryGuid = FAST_MEMORY_WMI_GUID;

static const wchar_t *const memoryInstances[] = {
  L"ACPI\\PNP0C14\\Mem_0", L"ACPI\\PNP0C14\\SMMM_0", L"ACPI\\PNP0C14\\0_0", L"" };

static const wchar_t *const fastMemoryInstances[] = {
  L"ACPI\\PNP0C14\\MemFast_0", L"ACPI\\PNP0C14\\SMMF_0", L"" };

#define WMI_GUID_EXECUTE 0x0010U
#define WMI_METHOD_ID 1U
#define WMI_INSTANCE_COUNT ( sizeof( memoryInstances ) / sizeof( memoryInstances[0] ) )
#define FAST_WMI_INSTANCE_COUNT ( sizeof( fastMemoryInstances ) / sizeof( fastMemoryInstances[0] ) )

#endif
