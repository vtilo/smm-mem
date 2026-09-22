# API

All public functions are declared in `src\Api.h`. The implementation is in `src\Client.c`.

To use the API from your own program, include `Api.h` and compile `Client.c` with `API_ONLY` defined.

When copying the client sources into another project, keep `src/client/`, `src/shared/protocol.h`, `Api.h`, and `Client.c` in the same relative layout. Only `Client.c` needs to be compiled.

```bat
cl /nologo /W4 /O2 /DAPI_ONLY my_app.c src\Client.c
```

Every API function returning `int` returns nonzero on success and `0` on failure. `Close()` has no return value.

Functions can fail if `Init()` was not called, the ACPI WMI device is not present, the SMM handler is not configured, the process/module/export does not exist, or the requested memory is not mapped.

Names are ASCII strings and are compared case-insensitively.

Current public name buffers are limited to `NAME_SIZE`, which is 64 bytes including the null terminator.

Large read and write requests are chunked automatically by the usermode client. If you are using the API normally, you do not need to care about the internal WMI packet size.

## Transport and lifetime

The library serializes its transport state within a process. Firmware ACPI methods serialize access to the shared mailbox across callers. A large read or write can contain several requests and is not an atomic snapshot.

Keep the caller's buffers valid until the API returns. The optional fast path owns its own locked staging page; it does not lock or change the residency of the caller's buffers. If fast-path negotiation or staging allocation is unavailable, calls retain the legacy transport.

`Dump()` callback chunk sizes may vary up to a page. Consume the supplied `Size` rather than assuming a fixed chunk length.

Define `SMM_DISABLE_FAST` when compiling `Client.c` to force the legacy transport. The default negotiates the fast endpoint and falls back when it is unavailable.

A failed transfer may have completed earlier chunks. Writes are not automatically replayed after an ambiguous transport failure. Each firmware transfer is limited to 4 KiB so a large API call does not become one unbounded SMM operation.

## Types

### `PROCESS_INFO`

```c
typedef struct {
  uint32_t Pid;
  uint32_t Reserved;
  uint64_t Eprocess;
  uint64_t Cr3;
  uint64_t ImageBase;
  char Name[NAME_SIZE];
} PROCESS_INFO;
```

| Field | Meaning |
| --- | --- |
| `Pid` | Windows process ID. |
| `Eprocess` | Kernel virtual address of the target `EPROCESS`. |
| `Cr3` | Page table base used for virtual memory translation. |
| `ImageBase` | Main image base of the process, if it could be resolved. |
| `Name` | Process image name, limited to `NAME_SIZE`. |

### `MODULE_INFO`

```c
typedef struct {
  uint32_t Pid;
  uint32_t Reserved;
  uint64_t Base;
  uint64_t Size;
  uint64_t Cr3;
  char Name[NAME_SIZE];
} MODULE_INFO;
```

| Field | Meaning |
| --- | --- |
| `Pid` | Owning process PID for user modules. Kernel modules use the system process context. |
| `Base` | Loaded virtual base address of the module. |
| `Size` | Loaded image size in memory. |
| `Cr3` | Page table base used to access this module. |
| `Name` | Module name, limited to `NAME_SIZE`. |

### `DUMP_CALLBACK`

```c
typedef int (*DUMP_CALLBACK)(uint64_t Address,
                             const void *Data,
                             uint32_t Size,
                             void *Context);
```

`Dump()` calls this function for each streamed chunk.

| Argument | Meaning |
| --- | --- |
| `Address` | Virtual address of this chunk in the target module. |
| `Data` | Pointer to the bytes read for this chunk. |
| `Size` | Chunk size in bytes. |
| `Context` | User pointer passed to `Dump()`. |

Return nonzero to continue dumping. Return `0` to stop.

## Init

```c
int Init(void);
```

Opens the ACPI WMI block used to communicate with the firmware doorbell.

Call this once before using any other API function.

Returns nonzero if the WMI block was opened successfully.

## Close

```c
void Close(void);
```

Closes the WMI handle and releases the loaded Windows API library.

Call this when your application is done using the API.

## Ping

```c
int Ping(void);
```

Sends a minimal request to the SMM handler and verifies that the full path works:

```text
usermode -> ACPI WMI -> software SMI -> SMM -> response
```

Returns nonzero if SMM answered correctly.

This is mostly useful as a sanity check before calling the memory APIs.

## FindProcessByPid

```c
int FindProcessByPid(uint32_t Pid, PROCESS_INFO *Process);
```

Finds a process by PID and fills `PROCESS_INFO`.

| Argument | Meaning |
| --- | --- |
| `Pid` | Target process ID. |
| `Process` | Output pointer that receives process information. |

Returns nonzero if the process was found.

On success, `Process` receives PID, `EPROCESS`, CR3, image base if available, and image name.

The process name comes from the normal Windows process image name, not from a full file path.

## FindProcessByName

```c
int FindProcessByName(const char *Name, PROCESS_INFO *Process);
```

Finds a process by image name and fills `PROCESS_INFO`.

| Argument | Meaning |
| --- | --- |
| `Name` | Process image name, for example `notepad.exe`. |
| `Process` | Output pointer that receives process information. |

Returns nonzero if the process was found.

The comparison is case-insensitive.

The name is the process image name, for example `notepad.exe`, not the full path to the executable.

## TranslateVirt

```c
int TranslateVirt(uint32_t Pid, uint64_t Va, uint64_t *Pa);
```

Translates a process virtual address to a physical address.

| Argument | Meaning |
| --- | --- |
| `Pid` | Target process ID. |
| `Va` | Virtual address in that process. |
| `Pa` | Output pointer that receives the physical address. |

Returns nonzero if the VA is mapped and translation succeeded.

## ReadVirt

```c
int ReadVirt(uint32_t Pid, uint64_t Address, void *Buffer, uint32_t Size);
```

Reads virtual memory from a target process.

| Argument | Meaning |
| --- | --- |
| `Pid` | Target process ID. |
| `Address` | Virtual address to read from. |
| `Buffer` | Output buffer that receives the bytes. |
| `Size` | Number of bytes to read. |

Returns nonzero if the full range was read successfully.

The client automatically splits larger reads into small requests. The SMM side translates each page and handles ranges that cross page boundaries.

## WriteVirt

```c
int WriteVirt(uint32_t Pid, uint64_t Address, const void *Buffer,
              uint32_t Size);
```

Writes virtual memory into a target process.

| Argument | Meaning |
| --- | --- |
| `Pid` | Target process ID. |
| `Address` | Virtual address to write to. |
| `Buffer` | Bytes to write. |
| `Size` | Number of bytes to write. |

Returns nonzero if the full range was written successfully.

The client automatically splits larger writes into small requests. The SMM side translates each page and writes to the resulting physical memory.

## ReadPhys

```c
int ReadPhys(uint64_t Address, void *Buffer, uint32_t Size);
```

Reads physical memory.

| Argument | Meaning |
| --- | --- |
| `Address` | Physical address to read from. |
| `Buffer` | Output buffer that receives the bytes. |
| `Size` | Number of bytes to read. |

Returns nonzero if the full range was read successfully.

Use this when you already have a physical address or after calling `TranslateVirt()`.

## WritePhys

```c
int WritePhys(uint64_t Address, const void *Buffer, uint32_t Size);
```

Writes physical memory.

| Argument | Meaning |
| --- | --- |
| `Address` | Physical address to write to. |
| `Buffer` | Bytes to write. |
| `Size` | Number of bytes to write. |

Returns nonzero if the full range was written successfully.

This does not perform process-aware translation. It writes to the physical address directly.

## FindModule

```c
int FindModule(const PROCESS_INFO *Process, const char *Name,
               MODULE_INFO *Module);
```

Finds a usermode module loaded in a target process.

| Argument | Meaning |
| --- | --- |
| `Process` | Process returned by `FindProcessByPid()` or `FindProcessByName()`. |
| `Name` | Module name, for example `kernel32.dll`. |
| `Module` | Output pointer that receives module information. |

Returns nonzero if the module was found.

On success, `Module` receives the owning PID, module base, module size, CR3, and module name.

This uses the process loader module lists. It is meant for normally loaded DLLs and EXEs inside a process, not hidden or manually mapped regions.

## FindKernelModule

```c
int FindKernelModule(const char *Name, MODULE_INFO *Module);
```

Finds a loaded kernel image, such as `ntoskrnl.exe`, `hal.dll`, or a `.sys` driver.

| Argument | Meaning |
| --- | --- |
| `Name` | Kernel module name, for example `ntoskrnl.exe` or `ndis.sys`. |
| `Module` | Output pointer that receives module information. |

Returns nonzero if the kernel module was found.

On success, `Module` receives the kernel module base, loaded image size, kernel CR3, and module name.

This uses the normal loaded kernel module list. It is meant for normally loaded kernel images, not hidden or unlinked drivers.

## FindExport

```c
int FindExport(const MODULE_INFO *Module, const char *Name,
               uint64_t *Address);
```

Resolves an exported function or symbol from a module.

| Argument | Meaning |
| --- | --- |
| `Module` | Module returned by `FindModule()` or `FindKernelModule()`. |
| `Name` | Export name to resolve. |
| `Address` | Output pointer that receives the resolved virtual address. |

Returns nonzero if the export was found.

For user modules, this resolves inside the target process address space. For kernel modules, this resolves inside kernel address space.

## Dump

```c
int Dump(const MODULE_INFO *Module, DUMP_CALLBACK Callback, void *Context);
```

Streams a module's loaded memory range through a callback.

| Argument | Meaning |
| --- | --- |
| `Module` | Module returned by `FindModule()` or `FindKernelModule()`. |
| `Callback` | Function called for every dumped chunk. |
| `Context` | User pointer passed back to the callback. |

Returns nonzero if the full range was streamed successfully.

If a chunk cannot be read, the current implementation passes a zero-filled chunk to the callback and continues. If the callback returns `0`, dumping stops and `Dump()` returns `0`.

`Dump()` is a memory streamer, not a PE rebuilder. It does not fix imports, rebuild sections, repair headers, or reconstruct a packed image. That work belongs to your dumping or analysis tooling after the raw memory is collected.

`Dump()` streams `Module->Base` through `Module->Base + Module->Size`. It does not walk VADs, search for manually mapped regions, or decide what belongs to a process by itself.