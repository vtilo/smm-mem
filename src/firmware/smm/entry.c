#include "internal.h"

SmmState Smm = { 0 };

EFI_STATUS EFIAPI SmmEntry( EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable ) {
  EFI_STATUS Status;

  ( void )ImageHandle;

  MemoryClear( &Smm, sizeof( Smm ) );
  Smm.SystemTable = SystemTable;
  Smm.SwSmiValue = SW_SMI_VALUE;
  SerialInit();
  LogText( "mem smm init\n" );
  Status = InitSmm();

  if ( !EFI_ERROR( Status ) ) {
    Status = RegisterConfigComm();
    ( void )Status;
    Status = ConfigureFromPublishedTable();

    if ( EFI_ERROR( Status ) ) {
      LogStatus( "smm published config failed ", Status );
    }
  } else {
    LogStatus( "smm init failed ", Status );
  }

  LogText( "mem smm done\n" );
  return EFI_SUCCESS;
}