#include "internal.h"

EFI_STATUS EFIAPI swSmiHandler(
  EFI_HANDLE dispatchHandle, const VOID *context, VOID *commBuffer, UINTN *commBufferSize ) {
  ( void )dispatchHandle;
  ( void )context;
  ( void )commBuffer;
  ( void )commBufferSize;
  ( void )processRequest();
  return EFI_SUCCESS;
}

EFI_STATUS registerSwSmi( VOID ) {
  EFI_SMM_SW_DISPATCH2_PROTOCOL *swDispatch = 0;
  EFI_SMM_SW_REGISTER_CONTEXT swContext;
  EFI_LOCATE_PROTOCOL locate;
  EFI_STATUS status;

  if ( smm.swHandle != 0 ) {
    logText( "smm sw smi already registered\n" );
    return EFI_SUCCESS;
  }
  locate = ( EFI_LOCATE_PROTOCOL )smm.table->SmmLocateProtocol;
  status = locate( &smmSwDispatchGuid, 0, ( VOID ** )&swDispatch );
  if ( EFI_ERROR( status ) || swDispatch == 0 || swDispatch->Register == 0 ) {
    logStatus( "smm sw dispatch unavailable ", EFI_ERROR( status ) ? status : EFI_NOT_FOUND );
    return EFI_ERROR( status ) ? status : EFI_NOT_FOUND;
  }
  if ( ( UINTN )smm.swSmiValue > swDispatch->MaximumSwiValue ) {
    logText( "smm sw value out of range max=0x" );
    logHex( swDispatch->MaximumSwiValue );
    logText( " requested=0x" );
    logHex( smm.swSmiValue );
    logText( "\n" );
    return EFI_INVALID_PARAMETER;
  }
  swContext.SwSmiInputValue = smm.swSmiValue;
  status = swDispatch->Register( swDispatch, swSmiHandler, &swContext, &smm.swHandle );
  if ( EFI_ERROR( status ) ) {
    logStatus( "smm sw smi register failed ", status );
  } else {
    logText( "smm sw smi registered value=0x" );
    logHex( smm.swSmiValue );
    logText( "\n" );
  }
  return status;
}

EFI_STATUS validateSmst( VOID ) {
  if ( smm.table == 0 ) {
    logText( "smm smst missing\n" );
    return EFI_NOT_FOUND;
  }
  if ( smm.table->SmmLocateProtocol == 0 ) {
    logText( "smm locate protocol missing\n" );
    return EFI_NOT_FOUND;
  }
  if ( smm.table->SmmIo.Mem.Read == 0 ) {
    logText( "smm mem read missing\n" );
    return EFI_NOT_FOUND;
  }
  if ( smm.table->SmmIo.Mem.Write == 0 ) {
    logText( "smm mem write missing\n" );
    return EFI_NOT_FOUND;
  }
  return EFI_SUCCESS;
}

EFI_STATUS initSmm( VOID ) {
  EFI_SMM_BASE2_PROTOCOL *smmBase = 0;
  EFI_STATUS status;

  if ( smm.systemTable == 0 || smm.systemTable->BootServices == 0 ) {
    logText( "smm system table invalid\n" );
    return EFI_INVALID_PARAMETER;
  }
  status = smm.systemTable->BootServices->LocateProtocol( &smmBase2Guid, 0, ( VOID ** )&smmBase );
  if ( EFI_ERROR( status ) || smmBase == 0 ) {
    logStatus( "smm base2 unavailable ", EFI_ERROR( status ) ? status : EFI_NOT_FOUND );
    return EFI_ERROR( status ) ? status : EFI_NOT_FOUND;
  }
  status = smmBase->GetSmstLocation( smmBase, &smm.table );
  if ( EFI_ERROR( status ) ) {
    logStatus( "smm get smst failed ", status );
    return status;
  }
  status = validateSmst();
  if ( EFI_ERROR( status ) ) {
    return status;
  }
  logText( "smm smst ok cpus=0x" );
  logHex( smm.table->NumberOfCpus );
  logText( "\n" );
  return EFI_SUCCESS;
}

EFI_STATUS applyConfig( CONFIG *config, const char *source ) {
  EFI_STATUS status;

  if ( config == 0 || config->Magic != CONFIG_MAGIC || config->MailboxPhysical == 0 ||
       config->MailboxSize < MAILBOX_SIZE ) {
    logText( "smm config rejected from " );
    logText( source );
    if ( config != 0 ) {
      logText( " magic=0x" );
      logHex( config->Magic );
      logText( " mailbox=0x" );
      logHex( config->MailboxPhysical );
      logText( " size=0x" );
      logHex( config->MailboxSize );
    }
    logText( "\n" );
    return EFI_INVALID_PARAMETER;
  }
  smm.mailboxPhysical = config->MailboxPhysical;
  smm.mailboxSize = config->MailboxSize;
  if ( config->SwSmiValue != 0 ) {
    smm.swSmiValue = config->SwSmiValue;
  }
  logText( "smm config from " );
  logText( source );
  logText( " mailbox=0x" );
  logHex( smm.mailboxPhysical );
  logText( " size=0x" );
  logHex( smm.mailboxSize );
  logText( " sw=0x" );
  logHex( smm.swSmiValue );
  logText( "\n" );
  status = registerSwSmi();
  return status;
}

EFI_STATUS EFIAPI configCommHandler(
  EFI_HANDLE dispatchHandle, const VOID *context, VOID *commBuffer, UINTN *commBufferSize ) {
  CONFIG *config;

  ( void )dispatchHandle;
  ( void )context;
  logText( "smm config comm received\n" );
  if ( commBuffer == 0 || commBufferSize == 0 || *commBufferSize < sizeof( CONFIG ) ) {
    logText( "smm config comm invalid buffer\n" );
    return EFI_SUCCESS;
  }
  config = ( CONFIG * )commBuffer;
  smm.features = 0;
  config->Status = applyConfig( config, "comm" );
  if ( !EFI_ERROR( config->Status ) && *commBufferSize >= sizeof( CONFIG_EXTENDED ) ) {
    CONFIG_EXTENDED *extended = ( CONFIG_EXTENDED * )commBuffer;

    if ( extended->ExtensionMagic == FAST_CONFIG_MAGIC && extended->Reserved == 0 ) {
      smm.features = extended->Features & FEATURE_BUFFER_TRANSFER;
    }
  }
  logText( "smm transport features=0x" );
  logHex( smm.features );
  logText( " bytes=0x" );
  logHex( *commBufferSize );
  logText( "\n" );
  return EFI_SUCCESS;
}

EFI_STATUS registerConfigComm( VOID ) {
  EFI_STATUS status;

  if ( smm.commHandle != 0 ) {
    logText( "smm config comm already registered\n" );
    return EFI_SUCCESS;
  }
  if ( smm.table->SmiHandlerRegister == 0 ) {
    logText( "smm config comm register unavailable\n" );
    return EFI_NOT_FOUND;
  }
  status = smm.table->SmiHandlerRegister( configCommHandler, &configCommGuid, &smm.commHandle );
  if ( EFI_ERROR( status ) ) {
    logStatus( "smm config comm register failed ", status );
  } else {
    logText( "smm config comm registered\n" );
  }
  return status;
}

EFI_STATUS configureFromPublishedTable( VOID ) {
  CONFIG *config = 0;
  UINTN index;

  if ( smm.systemTable == 0 || smm.systemTable->ConfigurationTable == 0 ) {
    logText( "smm published config table unavailable\n" );
    return EFI_NOT_FOUND;
  }
  for ( index = 0; index < smm.systemTable->NumberOfTableEntries; index++ ) {
    EFI_CONFIGURATION_TABLE *table = &smm.systemTable->ConfigurationTable[index];
    if ( guidEqual( &table->VendorGuid, &configGuid ) ) {
      config = ( CONFIG * )table->VendorTable;
      break;
    }
  }
  if ( config == 0 ) {
    logText( "smm published config missing\n" );
    return EFI_NOT_FOUND;
  }
  return applyConfig( config, "published" );
}
