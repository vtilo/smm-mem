#include "internal.h"

DxeState Dxe;

EFI_STATUS EFIAPI DxeEntry( EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable ) {
  EFI_STATUS Status;
  ( void )ImageHandle;

  Dxe.SystemTable = SystemTable;
  SerialInit();
  LogText( "mem dxe init\n" );
  Status = AllocateMailbox();

  if ( EFI_ERROR( Status ) ) {
    LogStatus( "dxe mailbox failed ", Status );

    return EFI_SUCCESS;
  }

  LogText( "dxe mailbox base=0x" );
  LogHex( Dxe.MailboxPhysical );
  LogText( " size=0x" );
  LogHex( MAILBOX_SIZE );
  LogText( " sw=0x" );
  LogHex( SW_SMI_VALUE );
  LogText( "\n" );
  Status = PublishConfig();

  if ( EFI_ERROR( Status ) ) {
    LogStatus( "dxe config publish failed ", Status );
  } else {
    LogText( "dxe config published\n" );
  }

  Status = InstallWmi();

  if ( EFI_ERROR( Status ) ) {
    LogStatus( "dxe wmi install failed ", Status );
  }

  Status = ConfigureSmm();

  if ( Dxe.WmiInstalled == 0 || Dxe.Configured == 0 ) {
    RegisterRetryTimer();
    RegisterReadyToBootRetry();
    RegisterSmmCommNotify();
    RegisterAcpiNotify();
  }

  LogText( "mem dxe done\n" );

  return EFI_SUCCESS;
}