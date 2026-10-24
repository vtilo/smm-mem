<p align="center">
    <h3 align="center">SmmMem</h3>
    <p align="center"><i>Driverless Windows memory access through SMM with a usermode API</i></p>
</p>

## About

SmmMem exposes a source-only Windows API for reading, writing, translating, and resolving memory through an SMM handler running in ring -2. The API does not require a custom kernel driver. Usermode talks to ACPI WMI, ACPI rings a software SMI, and the SMM module handles the memory request.

https://github.com/user-attachments/assets/08513a12-0f89-47c3-982a-565fa84a9a5b

The project consists of two firmware components: `Dxe.efi` for the ACPI WMI doorbell and mailbox setup, and `Smm.efi` for the actual memory API running inside SMM.

## Source layout

- `src/api/api.h` is the public API header; compile `src/api/api.c` into your application.
- `src/api/internal/` contains the private transport and API implementation.
- `src/firmware/dxe/` sets up the ACPI WMI interface and shared mailbox during boot.
- `src/firmware/smm/` handles memory requests, process discovery, modules, and exports.
- `src/firmware/common/` contains shared firmware helpers and UEFI declarations.
- `src/protocol.h` defines the request and response format used by the API and firmware.
- `tests/` contains explicit VM-only smoke tests and benchmarks.

The API uses one ACPI WMI device with 64-byte requests and responses. A private, locked 4 KiB buffer carries larger memory transfers and lookup data. Each firmware transfer is bounded to 4 KiB; large application reads are split without requiring callers to change their code.

Update the API and both firmware modules together. Protocol version 2 replaces the old interfaces. `Init()` checks the protocol and prepares the transfer buffer, returning failure if either step is unavailable.

## API

```c
Init();
Close();
Ping();

FindProcessByPid( Pid, &Process );
FindProcessByName( "notepad.exe", &Process );

TranslateVirt( Pid, Va, &Pa );

ReadVirt( Pid, Va, Buffer, Size );
WriteVirt( Pid, Va, Buffer, Size );

ReadPhys( Pa, Buffer, Size );
WritePhys( Pa, Buffer, Size );

FindModule( &Process, "module.dll", &Module );
FindKernelModule( "ntoskrnl.exe", &Module );
FindExport( &Module, "PsInitialSystemProcess", &Address );

Dump( &Module, Callback, Context );
```

To use the API, include `api.h` and compile `src/api/api.c` into your application. It always builds as API implementation code and has no application entry point.

When copying the API sources into another project, keep `src/api/` and `src/protocol.h` in the same relative layout. Only `api.c` needs to be compiled; the `.inl` files are included privately.

```bat
cl /nologo /W4 /O2 /I src\api my_app.c src\api\api.c
```

The default build produces only `Dxe.efi` and `Smm.efi`. Smoke tests and benchmarks are separate development targets run inside a VM; see [tests/README.md](tests/README.md).

Full API documentation is available in [API.md](API.md).

## Support

* x64 UEFI firmware with AMI Aptio V style PI SMM support
* Windows 10 or Windows 11 for the API
* Tested on an ASUS AM5 platform with AMI Aptio V firmware

## Usage

1. Open an [x64 Visual Studio developer command prompt](https://learn.microsoft.com/en-us/visualstudio/ide/reference/command-prompt-powershell?view=visualstudio) and run `build.cmd`
2. Add `Dxe.efi` and `Smm.efi` to your firmware and flash it onto your board. Easiest way is to replace existing DXE and SMM modules (check out the [general UEFITool guide](https://winraid.level1techs.com/t/guide-howto-extract-insert-replace-efi-bios-modules/32122))
3. Build your application with `src/api/api.h` and `src/api/api.c`.
4. Boot Windows and call `Init()` followed by `Ping()` to check whether the firmware is reachable, then use the remaining API functions.

This project requires firmware modification. Flashing a bad image can brick your motherboard. Use only on hardware you can recover and only if you understand what you are replacing.

If you are not sure how this project works or how to use it, just open an issue and I will help you out