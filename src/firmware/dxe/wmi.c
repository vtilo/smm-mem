#include "internal.h"

static EFI_STATUS installWmiTable( BOOLEAN fast ) {
  EFI_ACPI_TABLE_PROTOCOL *acpi = 0;
  UINT32 *installed = fast ? &dxe.fastWmiInstalled : &dxe.wmiInstalled;
  UINT8 *buffer = fast ? dxe.fastSsdt : dxe.ssdt;

  if ( *installed ) {
    return EFI_SUCCESS;
  }

  EFI_STATUS status =
    dxe.systemTable->BootServices->LocateProtocol( &acpiTableGuid, 0, ( VOID ** )&acpi );
  if ( EFI_ERROR( status ) || !acpi || !acpi->InstallAcpiTable ) {
    return EFI_ERROR( status ) ? status : EFI_NOT_FOUND;
  }

  UINTN size = buildSsdt( buffer, sizeof( dxe.ssdt ), fast );
  if ( !size ) {
    return EFI_OUT_OF_RESOURCES;
  }

  UINTN key = 0;
  status = acpi->InstallAcpiTable( acpi, buffer, size, &key );
  if ( !EFI_ERROR( status ) ) {
    *installed = 1;
    if ( fast ) {
      // Refresh features if SMM was configured before ACPI became available.
      dxe.configured = 0;
    }
    logText( fast ? "dxe fast ssdt installed\n" : "dxe ssdt installed\n" );
  }

  return status;
}

EFI_STATUS installWmi( VOID ) {
  dxe.wmiAttempts++;
  EFI_STATUS status = installWmiTable( 0 );
  if ( EFI_ERROR( status ) ) {
    return status;
  }

  // The optional device never prevents legacy clients from working.
  status = installWmiTable( 1 );
  if ( EFI_ERROR( status ) && dxe.wmiAttempts <= 3 ) {
    logStatus( "dxe fast ssdt unavailable ", status );
  }

  return EFI_SUCCESS;
}
