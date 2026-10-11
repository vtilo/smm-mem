#include "internal.h"

static EFI_STATUS InstallWmiTable( BOOLEAN Fast ) {
  EFI_ACPI_TABLE_PROTOCOL *Acpi = 0;
  UINT32 *Installed = Fast ? &Dxe.FastWmiInstalled : &Dxe.WmiInstalled;
  UINT8 *Buffer = Fast ? Dxe.FastSsdt : Dxe.Ssdt;

  if ( *Installed ) {
    return EFI_SUCCESS;
  }

  EFI_STATUS Status =
    Dxe.SystemTable->BootServices->LocateProtocol( &AcpiTableGuid, 0, ( VOID ** )&Acpi );

  if ( EFI_ERROR( Status ) || !Acpi || !Acpi->InstallAcpiTable ) {
    return EFI_ERROR( Status ) ? Status : EFI_NOT_FOUND;
  }

  UINTN Size = BuildSsdt( Buffer, sizeof( Dxe.Ssdt ), Fast );

  if ( !Size ) {
    return EFI_OUT_OF_RESOURCES;
  }

  UINTN Key = 0;
  Status = Acpi->InstallAcpiTable( Acpi, Buffer, Size, &Key );

  if ( !EFI_ERROR( Status ) ) {
    *Installed = 1;

    if ( Fast ) {
      // Refresh features if SMM was configured before ACPI became available
      Dxe.Configured = 0;
    }

    LogText( Fast ? "dxe fast ssdt installed\n" : "dxe ssdt installed\n" );
  }

  return Status;
}

EFI_STATUS InstallWmi( VOID ) {
  Dxe.WmiAttempts++;
  EFI_STATUS Status = InstallWmiTable( 0 );

  if ( EFI_ERROR( Status ) ) {
    return Status;
  }

  // Keep legacy access available if the optional fast table fails
  Status = InstallWmiTable( 1 );

  if ( EFI_ERROR( Status ) && Dxe.WmiAttempts <= 3 ) {
    LogStatus( "dxe fast ssdt unavailable ", Status );
  }

  return EFI_SUCCESS;
}