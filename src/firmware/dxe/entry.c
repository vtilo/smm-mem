#include "internal.h"

DxeState dxe;

EFI_STATUS EFIAPI DxeEntry( EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE *systemTable ) {
  EFI_STATUS status;
  ( void )imageHandle;

  dxe.systemTable = systemTable;
  serialInit();
  logText( "mem dxe init\n" );
  status = allocateMailbox();
  if ( EFI_ERROR( status ) ) {
    logStatus( "dxe mailbox failed ", status );
    return EFI_SUCCESS;
  }
  logText( "dxe mailbox base=0x" );
  logHex( dxe.mailboxPhysical );
  logText( " size=0x" );
  logHex( MAILBOX_SIZE );
  logText( " sw=0x" );
  logHex( SW_SMI_VALUE );
  logText( "\n" );
  status = publishConfig();
  if ( EFI_ERROR( status ) ) {
    logStatus( "dxe config publish failed ", status );
  } else {
    logText( "dxe config published\n" );
  }
  status = installWmi();
  if ( EFI_ERROR( status ) ) {
    logStatus( "dxe wmi install failed ", status );
  }
  status = configureSmm();
  if ( dxe.wmiInstalled == 0 || dxe.configured == 0 ) {
    registerRetryTimer();
    registerReadyToBootRetry();
    registerSmmCommNotify();
    registerAcpiNotify();
  }
  logText( "mem dxe done\n" );
  return EFI_SUCCESS;
}
