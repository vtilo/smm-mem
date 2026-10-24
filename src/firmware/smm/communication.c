#include "internal.h"

EFI_STATUS EFIAPI SwSmiHandler(
  EFI_HANDLE DispatchHandle, const VOID *Context, VOID *CommBuffer, UINTN *CommBufferSize ) {
  ( void )DispatchHandle;
  ( void )Context;
  ( void )CommBuffer;
  ( void )CommBufferSize;
  ( void )ProcessRequest();
  return EFI_SUCCESS;
}

EFI_STATUS RegisterSwSmi( VOID ) {
  EFI_SMM_SW_DISPATCH2_PROTOCOL *SwDispatch = 0;
  EFI_SMM_SW_REGISTER_CONTEXT SwContext;
  EFI_LOCATE_PROTOCOL Locate;
  EFI_STATUS Status;

  if ( Smm.SwHandle != 0 ) {
    LogText( "smm sw smi already registered\n" );
    return EFI_SUCCESS;
  }

  Locate = ( EFI_LOCATE_PROTOCOL )Smm.Table->SmmLocateProtocol;
  Status = Locate( &SmmSwDispatchGuid, 0, ( VOID ** )&SwDispatch );

  if ( EFI_ERROR( Status ) || SwDispatch == 0 || SwDispatch->Register == 0 ) {
    LogStatus( "smm sw dispatch unavailable ", EFI_ERROR( Status ) ? Status : EFI_NOT_FOUND );
    return EFI_ERROR( Status ) ? Status : EFI_NOT_FOUND;
  }

  if ( ( UINTN )Smm.SwSmiValue > SwDispatch->MaximumSwiValue ) {
    LogText( "smm sw value out of range max=0x" );
    LogHex( SwDispatch->MaximumSwiValue );
    LogText( " requested=0x" );
    LogHex( Smm.SwSmiValue );
    LogText( "\n" );
    return EFI_INVALID_PARAMETER;
  }

  SwContext.SwSmiInputValue = Smm.SwSmiValue;
  Status = SwDispatch->Register( SwDispatch, SwSmiHandler, &SwContext, &Smm.SwHandle );

  if ( EFI_ERROR( Status ) ) {
    LogStatus( "smm sw smi register failed ", Status );
  } else {
    LogText( "smm sw smi registered value=0x" );
    LogHex( Smm.SwSmiValue );
    LogText( "\n" );
  }

  return Status;
}

EFI_STATUS ValidateSmst( VOID ) {
  if ( Smm.Table == 0 ) {
    LogText( "smm smst missing\n" );
    return EFI_NOT_FOUND;
  }

  if ( Smm.Table->SmmLocateProtocol == 0 ) {
    LogText( "smm locate protocol missing\n" );
    return EFI_NOT_FOUND;
  }

  if ( Smm.Table->SmmIo.Mem.Read == 0 ) {
    LogText( "smm mem read missing\n" );
    return EFI_NOT_FOUND;
  }

  if ( Smm.Table->SmmIo.Mem.Write == 0 ) {
    LogText( "smm mem write missing\n" );
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

EFI_STATUS InitSmm( VOID ) {
  EFI_SMM_BASE2_PROTOCOL *SmmBase = 0;
  EFI_STATUS Status;

  if ( Smm.SystemTable == 0 || Smm.SystemTable->BootServices == 0 ) {
    LogText( "smm system table invalid\n" );
    return EFI_INVALID_PARAMETER;
  }

  Status = Smm.SystemTable->BootServices->LocateProtocol( &SmmBase2Guid, 0, ( VOID ** )&SmmBase );

  if ( EFI_ERROR( Status ) || SmmBase == 0 ) {
    LogStatus( "smm base2 unavailable ", EFI_ERROR( Status ) ? Status : EFI_NOT_FOUND );
    return EFI_ERROR( Status ) ? Status : EFI_NOT_FOUND;
  }

  Status = SmmBase->GetSmstLocation( SmmBase, &Smm.Table );

  if ( EFI_ERROR( Status ) ) {
    LogStatus( "smm get smst failed ", Status );
    return Status;
  }

  Status = ValidateSmst();

  if ( EFI_ERROR( Status ) ) {
    return Status;
  }

  LogText( "smm smst ok cpus=0x" );
  LogHex( Smm.Table->NumberOfCpus );
  LogText( "\n" );
  return EFI_SUCCESS;
}

EFI_STATUS ApplyConfig( CONFIG *Config, const char *Source ) {
  EFI_STATUS Status;

  if ( Config == 0 || Config->Magic != CONFIG_MAGIC || Config->MailboxPhysical == 0 ||
       Config->MailboxSize < MAILBOX_SIZE ) {
    LogText( "smm config rejected from " );
    LogText( Source );

    if ( Config != 0 ) {
      LogText( " magic=0x" );
      LogHex( Config->Magic );
      LogText( " mailbox=0x" );
      LogHex( Config->MailboxPhysical );
      LogText( " size=0x" );
      LogHex( Config->MailboxSize );
    }

    LogText( "\n" );
    return EFI_INVALID_PARAMETER;
  }

  Smm.MailboxPhysical = Config->MailboxPhysical;
  Smm.MailboxSize = Config->MailboxSize;

  if ( Config->SwSmiValue != 0 ) {
    Smm.SwSmiValue = Config->SwSmiValue;
  }

  LogText( "smm config from " );
  LogText( Source );
  LogText( " mailbox=0x" );
  LogHex( Smm.MailboxPhysical );
  LogText( " size=0x" );
  LogHex( Smm.MailboxSize );
  LogText( " sw=0x" );
  LogHex( Smm.SwSmiValue );
  LogText( "\n" );
  Status = RegisterSwSmi();
  return Status;
}

EFI_STATUS EFIAPI ConfigCommHandler(
  EFI_HANDLE DispatchHandle, const VOID *Context, VOID *CommBuffer, UINTN *CommBufferSize ) {
  CONFIG *Config;

  ( void )DispatchHandle;
  ( void )Context;
  LogText( "smm config comm received\n" );

  if ( CommBuffer == 0 || CommBufferSize == 0 || *CommBufferSize < sizeof( CONFIG ) ) {
    LogText( "smm config comm invalid buffer\n" );
    return EFI_SUCCESS;
  }

  Config = ( CONFIG * )CommBuffer;
  Config->Status = ApplyConfig( Config, "comm" );
  LogText( "smm config bytes=0x" );
  LogHex( *CommBufferSize );
  LogText( "\n" );
  return EFI_SUCCESS;
}

EFI_STATUS RegisterConfigComm( VOID ) {
  EFI_STATUS Status;

  if ( Smm.CommHandle != 0 ) {
    LogText( "smm config comm already registered\n" );
    return EFI_SUCCESS;
  }

  if ( Smm.Table->SmiHandlerRegister == 0 ) {
    LogText( "smm config comm register unavailable\n" );
    return EFI_NOT_FOUND;
  }

  Status = Smm.Table->SmiHandlerRegister( ConfigCommHandler, &ConfigCommGuid, &Smm.CommHandle );

  if ( EFI_ERROR( Status ) ) {
    LogStatus( "smm config comm register failed ", Status );
  } else {
    LogText( "smm config comm registered\n" );
  }

  return Status;
}

EFI_STATUS ConfigureFromPublishedTable( VOID ) {
  CONFIG *Config = 0;
  UINTN Index;

  if ( Smm.SystemTable == 0 || Smm.SystemTable->ConfigurationTable == 0 ) {
    LogText( "smm published config table unavailable\n" );
    return EFI_NOT_FOUND;
  }

  for ( Index = 0; Index < Smm.SystemTable->NumberOfTableEntries; Index++ ) {
    EFI_CONFIGURATION_TABLE *Table = &Smm.SystemTable->ConfigurationTable[Index];

    if ( GuidEqual( &Table->VendorGuid, &ConfigGuid ) ) {
      Config = ( CONFIG * )Table->VendorTable;
      break;
    }
  }

  if ( Config == 0 ) {
    LogText( "smm published config missing\n" );
    return EFI_NOT_FOUND;
  }

  return ApplyConfig( Config, "published" );
}