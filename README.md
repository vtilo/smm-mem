<p align="center">
    <h3 align="center">SmmMem</h3>
    <p align="center"><i>Driverless Windows memory access through SMM with a usermode API</i></p>
</p>

## About

SmmMem allows a normal usermode application to read, write, translate, and resolve Windows memory through an SMM handler running in ring -2. The client side does not require a custom kernel driver. Usermode talks to ACPI WMI, ACPI rings a software SMI, and the SMM driver handles the memory request.

The project consists of two firmware components: `Dxe.efi` for the ACPI WMI doorbell and mailbox setup, and `Smm.efi` for the actual memory API running inside SMM.

## API

```c
Init();
Close();
Ping();

FindProcessByPid(pid, &process);
FindProcessByName("notepad.exe", &process);

TranslateVirt(pid, va, &pa);

ReadVirt(pid, va, buffer, size);
WriteVirt(pid, va, buffer, size);

ReadPhys(pa, buffer, size);
WritePhys(pa, buffer, size);

FindModule(&process, "module.dll", &module);
FindKernelModule("ntoskrnl.exe", &module);
FindExport(&module, "PsInitialSystemProcess", &address);

Dump(&module, callback, context);
```

To use the API in your own application, include `Api.h` and compile `Client.c` with `API_ONLY` defined:

```bat
cl /nologo /W4 /O2 /DAPI_ONLY my_app.c src\Client.c
```

`Client.exe` is only a small sanity-check tool. Your own application should link against `Client.c` directly.

Full API documentation is available in [API.md](API.md).

## Support

* x64 UEFI firmware with AMI Aptio V style PI SMM support
* Windows 10 or Windows 11 for the Windows client
* Tested on an ASUS AM5 platform with AMI Aptio V firmware

## Usage

1. Open an [x64 Visual Studio developer command prompt](https://learn.microsoft.com/en-us/visualstudio/ide/reference/command-prompt-powershell?view=visualstudio) and run `src\build.cmd`
2. Add `Dxe.efi` and `Smm.efi` to your firmware and flash it onto your board. Easiest way is to replace existing DXE and SMM modules (check out the [general UEFITool guide](https://winraid.level1techs.com/t/guide-howto-extract-insert-replace-efi-bios-modules/32122))
3. Boot Windows and run `Client.exe ping` to check whether the ACPI WMI doorbell and SMM handler are reachable
4. Build your own usermode application with `Api.h` and `Client.c`, then call the API functions directly

This project requires firmware modification. Flashing a bad image can brick your motherboard. Use only on hardware you can recover and only if you understand what you are replacing.

If you are not sure how this project works or how to use it, just open an issue and I will help you out