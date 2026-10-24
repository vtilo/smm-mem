#include "internal.h"

EFI_STATUS InstallWmi( VOID ) {
  EFI_ACPI_TABLE_PROTOCOL *Acpi = 0;

  if ( Dxe.WmiInstalled ) {
    return EFI_SUCCESS;
  }

  EFI_STATUS Status =
    Dxe.SystemTable->BootServices->LocateProtocol( &AcpiTableGuid, 0, ( VOID ** )&Acpi );

  if ( EFI_ERROR( Status ) || !Acpi || !Acpi->InstallAcpiTable ) {
    return EFI_ERROR( Status ) ? Status : EFI_NOT_FOUND;
  }

  UINTN Size = BuildSsdt( Dxe.Ssdt, sizeof( Dxe.Ssdt ) );

  if ( !Size ) {
    return EFI_OUT_OF_RESOURCES;
  }

  UINTN Key = 0;
  Status = Acpi->InstallAcpiTable( Acpi, Dxe.Ssdt, Size, &Key );

  if ( !EFI_ERROR( Status ) ) {
    Dxe.WmiInstalled = 1;
    LogText( "dxe ssdt installed\n" );
  }

  return Status;
}