Usermode talks to ACPI WMI, ACPI rings a software SMI, and SMM handles the memory request. Constains DXE module for the ACPI/WMI doorbell and one SMM module for the memory API

API:

- `Init`, `Close`, `Ping`
- `FindProcessByPid`, `FindProcessByName`
- `TranslateVirt`
- `ReadVirt`, `WriteVirt`
- `ReadPhys`, `WritePhys`
- `FindModule`, `FindKernelModule`, `FindExport`
- `Dump`