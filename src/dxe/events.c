#include "internal.h"

static EFI_GUID readyToBootGuid = {
  0x7ce88fb3, 0x4bd7, 0x4679, { 0x87, 0xa8, 0xa8, 0xd8, 0xde, 0xe5, 0x0d, 0x2b } };

static VOID EFIAPI retrySetup( EFI_EVENT event, VOID *context ) {
  EFI_STATUS wmiStatus;
  EFI_STATUS smmStatus;
  ( void )event;
  ( void )context;

  if ( dxe.wmiInstalled != 0 && dxe.configured != 0 ) {
    return;
  }
  wmiStatus = EFI_SUCCESS;
  smmStatus = EFI_SUCCESS;
  if ( dxe.wmiInstalled == 0 ) {
    wmiStatus = installWmi();
  }
  if ( dxe.configured == 0 ) {
    smmStatus = configureSmm();
  }
  if ( !EFI_ERROR( wmiStatus ) && !EFI_ERROR( smmStatus ) && dxe.retryTimerEvent != 0 ) {
    smmStatus = dxe.systemTable->BootServices->SetTimer( dxe.retryTimerEvent, TimerCancel, 0 );
    if ( EFI_ERROR( smmStatus ) ) {
      logStatus( "dxe retry timer cancel failed ", smmStatus );
    } else {
      logText( "dxe retry timer canceled\n" );
    }
  }
}

VOID registerRetryTimer( VOID ) {
  EFI_STATUS status;

  if ( dxe.retryTimerEvent != 0 || ( dxe.wmiInstalled != 0 && dxe.configured != 0 ) ) {
    return;
  }
  status = dxe.systemTable->BootServices->CreateEvent(
    EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, retrySetup, 0, &dxe.retryTimerEvent );
  if ( EFI_ERROR( status ) ) {
    dxe.retryTimerEvent = 0;
    logStatus( "dxe retry timer create failed ", status );
    return;
  }
  status =
    dxe.systemTable->BootServices->SetTimer( dxe.retryTimerEvent, TimerPeriodic, 5000000ULL );
  if ( EFI_ERROR( status ) ) {
    dxe.systemTable->BootServices->CloseEvent( dxe.retryTimerEvent );
    dxe.retryTimerEvent = 0;
    logStatus( "dxe retry timer start failed ", status );
  } else {
    logText( "dxe retry timer registered\n" );
  }
}

VOID registerSmmCommNotify( VOID ) {
  EFI_STATUS status;

  if ( dxe.smmCommEvent != 0 || dxe.configured != 0 ) {
    return;
  }
  status = dxe.systemTable->BootServices->CreateEvent(
    EVT_NOTIFY_SIGNAL, TPL_CALLBACK, retrySetup, 0, &dxe.smmCommEvent );
  if ( EFI_ERROR( status ) ) {
    dxe.smmCommEvent = 0;
    logStatus( "dxe smm communication notify create failed ", status );
    return;
  }
  status = dxe.systemTable->BootServices->RegisterProtocolNotify(
    &smmCommunicationGuid, dxe.smmCommEvent, &dxe.smmCommRegistration );
  if ( EFI_ERROR( status ) ) {
    dxe.systemTable->BootServices->CloseEvent( dxe.smmCommEvent );
    dxe.smmCommEvent = 0;
    logStatus( "dxe smm communication notify failed ", status );
  } else {
    logText( "dxe smm communication notify registered\n" );
  }
}

VOID registerAcpiNotify( VOID ) {
  EFI_STATUS status;

  if ( dxe.acpiEvent != 0 || dxe.wmiInstalled != 0 ) {
    return;
  }
  status = dxe.systemTable->BootServices->CreateEvent(
    EVT_NOTIFY_SIGNAL, TPL_CALLBACK, retrySetup, 0, &dxe.acpiEvent );
  if ( EFI_ERROR( status ) ) {
    dxe.acpiEvent = 0;
    logStatus( "dxe acpi notify create failed ", status );
    return;
  }
  status = dxe.systemTable->BootServices->RegisterProtocolNotify(
    &acpiTableGuid, dxe.acpiEvent, &dxe.acpiRegistration );
  if ( EFI_ERROR( status ) ) {
    dxe.systemTable->BootServices->CloseEvent( dxe.acpiEvent );
    dxe.acpiEvent = 0;
    logStatus( "dxe acpi notify failed ", status );
  } else {
    logText( "dxe acpi notify registered\n" );
  }
}

VOID registerReadyToBootRetry( VOID ) {
  EFI_STATUS status;

  if ( dxe.readyToBootEvent != 0 || ( dxe.wmiInstalled != 0 && dxe.configured != 0 ) ||
       dxe.systemTable->BootServices->CreateEventEx == 0 ) {
    if ( dxe.systemTable->BootServices->CreateEventEx == 0 ) {
      logText( "dxe ready retry unavailable\n" );
    }
    return;
  }
  status = dxe.systemTable->BootServices->CreateEventEx(
    EVT_NOTIFY_SIGNAL, TPL_CALLBACK, retrySetup, 0, &readyToBootGuid, &dxe.readyToBootEvent );
  if ( EFI_ERROR( status ) ) {
    dxe.readyToBootEvent = 0;
    logStatus( "dxe ready retry failed ", status );
  } else {
    logText( "dxe ready retry registered\n" );
  }
}
