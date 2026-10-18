#include "internal.h"

EFI_STATUS ReadLdrEntry(
  UINT64 Cr3, UINT64 Link, UINT32 Bias, char *Name, UINTN NameSize, UINT64 *Base, UINT32 *Size ) {
  if ( ReadVirtual64( Cr3, Link + 0x30 - Bias, Base ) != EFI_SUCCESS ||
       ReadVirtual32( Cr3, Link + 0x40 - Bias, Size ) != EFI_SUCCESS || !IsUserPointer( *Base ) ||
       ReadUnicodeName( Cr3, Link + 0x58 - Bias, Name, NameSize ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

EFI_STATUS FindUserModule( UINT32 Pid, const char *Name, MODULE_INFO *Module ) {
  ProcessContext Process;
  UINT64 Peb;
  UINT64 Ldr;
  UINT32 ListOffsets[] = { 0x10, 0x20, 0x30 };
  UINT32 Biases[] = { 0x00, 0x10, 0x20 };
  UINT32 Guard;
  UINTN Index;

  if ( ResolveProcess( Pid, &Process ) != EFI_SUCCESS ||
       FindPeb( Process.Eprocess, Process.Cr3, &Peb ) != EFI_SUCCESS ||
       ReadVirtual64( Process.Cr3, Peb + 0x18, &Ldr ) != EFI_SUCCESS || !IsUserPointer( Ldr ) ) {
    return EFI_NOT_FOUND;
  }

  for ( Index = 0; Index < sizeof( ListOffsets ) / sizeof( ListOffsets[0] ); Index++ ) {
    UINT64 Head = Ldr + ListOffsets[Index];
    UINT64 Link;

    if ( ReadVirtual64( Process.Cr3, Head, &Link ) != EFI_SUCCESS ) {
      continue;
    }

    for ( Guard = 0; Guard < 1024 && IsUserPointer( Link ) && Link != Head; Guard++ ) {
      char ModuleName[NAME_SIZE];
      UINT64 Base;
      UINT32 Size;

      MemoryClear( ModuleName, sizeof( ModuleName ) );

      if ( ReadLdrEntry(
             Process.Cr3, Link, Biases[Index], ModuleName, sizeof( ModuleName ), &Base, &Size ) ==
             EFI_SUCCESS &&
           ( Name[0] == 0 || SameName( ModuleName, Name ) ) ) {
        MemoryClear( Module, sizeof( *Module ) );
        Module->Pid = Pid;
        Module->Base = Base;
        Module->Size = Size;
        Module->Cr3 = Process.Cr3;
        CopyText( Module->Name, sizeof( Module->Name ), ModuleName );
        return EFI_SUCCESS;
      }

      if ( ReadVirtual64( Process.Cr3, Link, &Link ) != EFI_SUCCESS ) {
        break;
      }
    }
  }

  return EFI_NOT_FOUND;
}

EFI_STATUS ResolveKernelModule( const char *Name, MODULE_INFO *Module ) {
  UINT64 Head;
  UINT64 Link;
  UINT32 Guard;

  if ( InitKernel() != EFI_SUCCESS || Smm.LoadedModuleList == 0 ) {
    return EFI_NOT_FOUND;
  }

  Head = Smm.LoadedModuleList;

  if ( ReadVirtual64( Smm.KernelCr3, Head, &Link ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  for ( Guard = 0; Guard < 1024 && IsKernelPointer( Link ) && Link != Head; Guard++ ) {
    char ModuleName[NAME_SIZE];
    UINT64 Base;
    UINT32 Size;

    if ( ReadVirtual64( Smm.KernelCr3, Link + 0x30, &Base ) == EFI_SUCCESS &&
         ReadVirtual32( Smm.KernelCr3, Link + 0x40, &Size ) == EFI_SUCCESS &&
         ReadUnicodeName( Smm.KernelCr3, Link + 0x58, ModuleName, sizeof( ModuleName ) ) ==
           EFI_SUCCESS &&
         ( Name[0] == 0 || SameName( ModuleName, Name ) ) ) {
      MemoryClear( Module, sizeof( *Module ) );
      Module->Pid = 4;
      Module->Base = Base;
      Module->Size = Size;
      Module->Cr3 = Smm.KernelCr3;
      CopyText( Module->Name, sizeof( Module->Name ), ModuleName );
      return EFI_SUCCESS;
    }

    if ( ReadVirtual64( Smm.KernelCr3, Link, &Link ) != EFI_SUCCESS ) {
      break;
    }
  }

  return EFI_NOT_FOUND;
}