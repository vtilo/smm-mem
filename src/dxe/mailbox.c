#include "internal.h"

static EFI_GUID communicationRegionGuid = {
  0x4e28ca50, 0xd582, 0x44ac, { 0xa1, 0x1f, 0xe3, 0xd5, 0x65, 0x26, 0xdb, 0x34 } };

EFI_STATUS allocateMailbox( VOID ) {
  EFI_STATUS status;
  UINTN pages = ( MAILBOX_SIZE + 0xFFFU ) / 0x1000U;

  if ( dxe.mailboxPhysical != 0 ) {
    return EFI_SUCCESS;
  }
  // The legacy AML mailbox address is encoded as a DWord.
  dxe.mailboxPhysical = 0xFFFFFFFFULL;
  status = dxe.systemTable->BootServices->AllocatePages(
    AllocateMaxAddress, EFI_RUNTIME_SERVICES_DATA, pages, &dxe.mailboxPhysical );
  if ( EFI_ERROR( status ) ) {
    dxe.mailboxPhysical = 0;
    return status;
  }
  memoryClear( ( VOID * )( UINTN )dxe.mailboxPhysical, MAILBOX_SIZE );
  return EFI_SUCCESS;
}

EFI_STATUS publishConfig( VOID ) {
  EFI_STATUS status;

  if ( dxe.systemTable->BootServices->InstallConfigurationTable == 0 ) {
    return EFI_UNSUPPORTED;
  }
  memoryClear( &dxe.publishedConfig, sizeof( dxe.publishedConfig ) );
  dxe.publishedConfig.Magic = CONFIG_MAGIC;
  dxe.publishedConfig.MailboxPhysical = dxe.mailboxPhysical;
  dxe.publishedConfig.MailboxSize = MAILBOX_SIZE;
  dxe.publishedConfig.SwSmiValue = SW_SMI_VALUE;
  status =
    dxe.systemTable->BootServices->InstallConfigurationTable( &configGuid, &dxe.publishedConfig );
  return status;
}

static VOID *findSmmCommRegion( UINTN requiredSize, UINT32 *originalType, BOOLEAN logIt ) {
  CommunicationRegionTable *table = 0;
  UINT8 *entry;
  UINTN index;

  *originalType = 0xFFFFFFFFU;
  if ( dxe.systemTable == 0 || dxe.systemTable->ConfigurationTable == 0 ) {
    if ( logIt ) {
      logText( "dxe config table unavailable\n" );
    }
    return 0;
  }
  for ( index = 0; index < dxe.systemTable->NumberOfTableEntries; index++ ) {
    EFI_CONFIGURATION_TABLE *config = &dxe.systemTable->ConfigurationTable[index];
    if ( guidEqual( &config->VendorGuid, &communicationRegionGuid ) ) {
      table = ( CommunicationRegionTable * )config->VendorTable;
      break;
    }
  }
  if ( table == 0 || table->Version != 1 || table->NumberOfEntries == 0 ||
       table->DescriptorSize < sizeof( MemoryDescriptor ) ) {
    if ( logIt ) {
      if ( table == 0 ) {
        logText( "dxe smm comm region table missing\n" );
      } else {
        logText( "dxe smm comm region table rejected version=0x" );
        logHex( table->Version );
        logText( " entries=0x" );
        logHex( table->NumberOfEntries );
        logText( " desc=0x" );
        logHex( table->DescriptorSize );
        logText( "\n" );
      }
    }
    return 0;
  }
  entry = ( UINT8 * )( table + 1 );
  for ( index = 0; index < table->NumberOfEntries; index++ ) {
    MemoryDescriptor *desc = ( MemoryDescriptor * )( entry + ( index * table->DescriptorSize ) );
    UINT64 bytes = desc->NumberOfPages << 12;
    if ( bytes < requiredSize ) {
      continue;
    }
    if ( desc->Type == EFI_RESERVED_MEMORY_TYPE || desc->Type == EFI_RUNTIME_SERVICES_DATA ||
         desc->Type == EFI_ACPI_MEMORY_NVS || desc->Type == EFI_CONVENTIONAL_MEMORY ) {
      *originalType = desc->Type;
      if ( logIt ) {
        logText( "dxe smm comm region base=0x" );
        logHex( desc->PhysicalStart );
        logText( " size=0x" );
        logHex( bytes );
        logText( " type=0x" );
        logHex( *originalType );
        logText( "\n" );
      }
      if ( desc->Type == EFI_CONVENTIONAL_MEMORY ) {
        desc->Type = EFI_RESERVED_MEMORY_TYPE;
      }
      return ( VOID * )( UINTN )desc->PhysicalStart;
    }
  }
  if ( logIt ) {
    logText( "dxe no usable smm comm region\n" );
  }
  return 0;
}

static VOID restoreSmmCommRegionType( VOID *commBuffer, UINT32 originalType ) {
  CommunicationRegionTable *table = 0;
  UINT8 *entry;
  UINTN index;

  if ( commBuffer == 0 || originalType != EFI_CONVENTIONAL_MEMORY || dxe.systemTable == 0 ||
       dxe.systemTable->ConfigurationTable == 0 ) {
    return;
  }
  for ( index = 0; index < dxe.systemTable->NumberOfTableEntries; index++ ) {
    EFI_CONFIGURATION_TABLE *config = &dxe.systemTable->ConfigurationTable[index];
    if ( guidEqual( &config->VendorGuid, &communicationRegionGuid ) ) {
      table = ( CommunicationRegionTable * )config->VendorTable;
      break;
    }
  }
  if ( table == 0 ) {
    return;
  }
  entry = ( UINT8 * )( table + 1 );
  for ( index = 0; index < table->NumberOfEntries; index++ ) {
    MemoryDescriptor *desc = ( MemoryDescriptor * )( entry + ( index * table->DescriptorSize ) );
    if ( ( VOID * )( UINTN )desc->PhysicalStart == commBuffer ) {
      desc->Type = originalType;
      return;
    }
  }
}

EFI_STATUS configureSmm( VOID ) {
  EFI_SMM_COMMUNICATION_PROTOCOL *smmComm = 0;
  EFI_SMM_COMMUNICATE_HEADER *header;
  CONFIG_EXTENDED *message;
  CONFIG *config;
  VOID *commBuffer;
  UINT32 originalRegionType;
  UINT32 attempt;
  BOOLEAN logIt;
  EFI_STATUS status;
  UINTN commSize;

  attempt = ++dxe.configureAttempts;
  logIt = ( attempt <= 3 );
  if ( logIt ) {
    logText( "dxe configure smm attempt=0x" );
    logHex( attempt );
    logText( "\n" );
  }
  status =
    dxe.systemTable->BootServices->LocateProtocol( &smmCommunicationGuid, 0, ( VOID ** )&smmComm );
  if ( EFI_ERROR( status ) || smmComm == 0 ) {
    if ( logIt ) {
      logStatus(
        "dxe smm communication unavailable ", EFI_ERROR( status ) ? status : EFI_NOT_FOUND );
    }
    return EFI_ERROR( status ) ? status : EFI_NOT_FOUND;
  }
  commSize = sizeof( EFI_SMM_COMMUNICATE_HEADER ) + sizeof( CONFIG_EXTENDED ) + 16;
  commBuffer = findSmmCommRegion( commSize, &originalRegionType, logIt );
  if ( commBuffer == 0 ) {
    if ( logIt ) {
      logText( "dxe smm configure no comm buffer\n" );
    }
    return EFI_NOT_FOUND;
  }
  memoryClear( commBuffer, commSize );
  header = ( EFI_SMM_COMMUNICATE_HEADER * )commBuffer;
  header->HeaderGuid = configCommGuid;
  header->MessageLength = sizeof( CONFIG_EXTENDED );
  message = ( CONFIG_EXTENDED * )header->Data;
  config = &message->Base;
  message->ExtensionMagic = FAST_CONFIG_MAGIC;
  message->Features = dxe.fastWmiInstalled ? FEATURE_BUFFER_TRANSFER : 0;
  config->Magic = CONFIG_MAGIC;
  config->MailboxPhysical = dxe.mailboxPhysical;
  config->MailboxSize = MAILBOX_SIZE;
  config->SwSmiValue = SW_SMI_VALUE;
  config->Status = EFI_NOT_FOUND;
  status = smmComm->Communicate( smmComm, header, &commSize );
  if ( !EFI_ERROR( status ) ) {
    status = config->Status;
  }
  restoreSmmCommRegionType( commBuffer, originalRegionType );
  if ( !EFI_ERROR( status ) ) {
    dxe.configured = 1;
    logText( "dxe smm configure ok\n" );
  } else if ( logIt ) {
    logStatus( "dxe smm configure failed ", status );
    logText( "dxe smm communicate size=0x" );
    logHex( commSize );
    logText( "\n" );
  }
  return status;
}
