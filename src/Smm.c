#include "smm/internal.h"

SmmState smm = { 0 };

EFI_STATUS EFIAPI SmmEntry( EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE *systemTable ) {
  EFI_STATUS status;
  ( void )imageHandle;

  memoryClear( &smm, sizeof( smm ) );
  smm.systemTable = systemTable;
  smm.swSmiValue = SW_SMI_VALUE;
  serialInit();
  logText( "mem smm init\n" );
  status = initSmm();
  if ( !EFI_ERROR( status ) ) {
    status = registerConfigComm();
    ( void )status;
    status = configureFromPublishedTable();
    if ( EFI_ERROR( status ) ) {
      logStatus( "smm published config failed ", status );
    }
  } else {
    logStatus( "smm init failed ", status );
  }
  logText( "mem smm done\n" );
  return EFI_SUCCESS;
}
