#include "internal.h"

static EFI_GUID ReadyToBootGuid = {
  0x7ce88fb3, 0x4bd7, 0x4679, { 0x87, 0xa8, 0xa8, 0xd8, 0xde, 0xe5, 0x0d, 0x2b } };

static VOID EFIAPI RetrySetup( EFI_EVENT Event, VOID *Context ) {
  EFI_STATUS WmiStatus;
  EFI_STATUS SmmStatus;
  ( void )Event;
  ( void )Context;

  if ( Dxe.WmiInstalled != 0 && Dxe.Configured != 0 ) {
    return;
  }

  WmiStatus = EFI_SUCCESS;
  SmmStatus = EFI_SUCCESS;

  if ( Dxe.WmiInstalled == 0 ) {
    WmiStatus = InstallWmi();
  }

  if ( Dxe.Configured == 0 ) {
    SmmStatus = ConfigureSmm();
  }

  if ( !EFI_ERROR( WmiStatus ) && !EFI_ERROR( SmmStatus ) && Dxe.RetryTimerEvent != 0 ) {
    SmmStatus = Dxe.SystemTable->BootServices->SetTimer( Dxe.RetryTimerEvent, TimerCancel, 0 );

    if ( EFI_ERROR( SmmStatus ) ) {
      LogStatus( "dxe retry timer cancel failed ", SmmStatus );
    } else {
      LogText( "dxe retry timer canceled\n" );
    }
  }
}

VOID RegisterRetryTimer( VOID ) {
  EFI_STATUS Status;

  if ( Dxe.RetryTimerEvent != 0 || ( Dxe.WmiInstalled != 0 && Dxe.Configured != 0 ) ) {
    return;
  }

  Status = Dxe.SystemTable->BootServices->CreateEvent(
    EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, RetrySetup, 0, &Dxe.RetryTimerEvent );

  if ( EFI_ERROR( Status ) ) {
    Dxe.RetryTimerEvent = 0;
    LogStatus( "dxe retry timer create failed ", Status );
    return;
  }

  Status =
    Dxe.SystemTable->BootServices->SetTimer( Dxe.RetryTimerEvent, TimerPeriodic, 5000000ULL );

  if ( EFI_ERROR( Status ) ) {
    Dxe.SystemTable->BootServices->CloseEvent( Dxe.RetryTimerEvent );
    Dxe.RetryTimerEvent = 0;
    LogStatus( "dxe retry timer start failed ", Status );
  } else {
    LogText( "dxe retry timer registered\n" );
  }
}

VOID RegisterSmmCommNotify( VOID ) {
  EFI_STATUS Status;

  if ( Dxe.SmmCommEvent != 0 || Dxe.Configured != 0 ) {
    return;
  }

  Status = Dxe.SystemTable->BootServices->CreateEvent(
    EVT_NOTIFY_SIGNAL, TPL_CALLBACK, RetrySetup, 0, &Dxe.SmmCommEvent );

  if ( EFI_ERROR( Status ) ) {
    Dxe.SmmCommEvent = 0;
    LogStatus( "dxe smm communication notify create failed ", Status );
    return;
  }

  Status = Dxe.SystemTable->BootServices->RegisterProtocolNotify(
    &SmmCommunicationGuid, Dxe.SmmCommEvent, &Dxe.SmmCommRegistration );

  if ( EFI_ERROR( Status ) ) {
    Dxe.SystemTable->BootServices->CloseEvent( Dxe.SmmCommEvent );
    Dxe.SmmCommEvent = 0;
    LogStatus( "dxe smm communication notify failed ", Status );
  } else {
    LogText( "dxe smm communication notify registered\n" );
  }
}

VOID RegisterAcpiNotify( VOID ) {
  EFI_STATUS Status;

  if ( Dxe.AcpiEvent != 0 || Dxe.WmiInstalled != 0 ) {
    return;
  }

  Status = Dxe.SystemTable->BootServices->CreateEvent(
    EVT_NOTIFY_SIGNAL, TPL_CALLBACK, RetrySetup, 0, &Dxe.AcpiEvent );

  if ( EFI_ERROR( Status ) ) {
    Dxe.AcpiEvent = 0;
    LogStatus( "dxe acpi notify create failed ", Status );
    return;
  }

  Status = Dxe.SystemTable->BootServices->RegisterProtocolNotify(
    &AcpiTableGuid, Dxe.AcpiEvent, &Dxe.AcpiRegistration );

  if ( EFI_ERROR( Status ) ) {
    Dxe.SystemTable->BootServices->CloseEvent( Dxe.AcpiEvent );
    Dxe.AcpiEvent = 0;
    LogStatus( "dxe acpi notify failed ", Status );
  } else {
    LogText( "dxe acpi notify registered\n" );
  }
}

VOID RegisterReadyToBootRetry( VOID ) {
  EFI_STATUS Status;

  if ( Dxe.ReadyToBootEvent != 0 || ( Dxe.WmiInstalled != 0 && Dxe.Configured != 0 ) ||
       Dxe.SystemTable->BootServices->CreateEventEx == 0 ) {
    if ( Dxe.SystemTable->BootServices->CreateEventEx == 0 ) {
      LogText( "dxe ready retry unavailable\n" );
    }

    return;
  }

  Status = Dxe.SystemTable->BootServices->CreateEventEx(
    EVT_NOTIFY_SIGNAL, TPL_CALLBACK, RetrySetup, 0, &ReadyToBootGuid, &Dxe.ReadyToBootEvent );

  if ( EFI_ERROR( Status ) ) {
    Dxe.ReadyToBootEvent = 0;
    LogStatus( "dxe ready retry failed ", Status );
  } else {
    LogText( "dxe ready retry registered\n" );
  }
}