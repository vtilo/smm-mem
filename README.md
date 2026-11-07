<p align="center">
    <h3 align="center">SmmMem</h3>
    <p align="center"><i>Driverless Windows memory access through SMM with a usermode API</i></p>
</p>

## About

This project was developed to let a regular usermode application request memory operations from SMM. It supports physical and virtual reads/writes, VA to PA translation, process and module lookups, export resolution and memory dumps. The API provides a **direct ring 3 → ring -2 communication interface**, allowing your application to request these operations through ACPI WMI without requiring a kernel driver

https://github.com/user-attachments/assets/08513a12-0f89-47c3-982a-565fa84a9a5b

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

The default build produces only `Dxe.efi` and `Smm.efi`.

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