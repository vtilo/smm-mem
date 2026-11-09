#include "internal.h"

#pragma intrinsic( _InterlockedCompareExchange )
#pragma intrinsic( _InterlockedExchange )

static EFI_GUID CommunicationRegionGuid = {
  0x4e28ca50, 0xd582, 0x44ac, { 0xa1, 0x1f, 0xe3, 0xd5, 0x65, 0x26, 0xdb, 0x34 } };

EFI_STATUS AllocateMailbox( VOID ) {
  EFI_STATUS Status;
  UINTN Pages = ( MAILBOX_SIZE + 0xFFFU ) / 0x1000U;

  if ( Dxe.MailboxPhysical != 0 ) {
    return EFI_SUCCESS;
  }

  // The AML mailbox address is encoded as a DWord
  Dxe.MailboxPhysical = 0xFFFFFFFFULL;
  Status = Dxe.SystemTable->BootServices->AllocatePages(
    AllocateMaxAddress, EFI_RUNTIME_SERVICES_DATA, Pages, &Dxe.MailboxPhysical );

  if ( EFI_ERROR( Status ) ) {
    Dxe.MailboxPhysical = 0;

    return Status;
  }

  MemoryClear( ( VOID * )( UINTN )Dxe.MailboxPhysical, MAILBOX_SIZE );

  return EFI_SUCCESS;
}

EFI_STATUS PublishConfig( VOID ) {
  EFI_STATUS Status;

  if ( Dxe.SystemTable->BootServices->InstallConfigurationTable == 0 ) {
    return EFI_UNSUPPORTED;
  }

  MemoryClear( &Dxe.PublishedConfig, sizeof( Dxe.PublishedConfig ) );
  Dxe.PublishedConfig.Magic = CONFIG_MAGIC;
  Dxe.PublishedConfig.MailboxPhysical = Dxe.MailboxPhysical;
  Dxe.PublishedConfig.MailboxSize = MAILBOX_SIZE;
  Dxe.PublishedConfig.SwSmiValue = SW_SMI_VALUE;
  Status =
    Dxe.SystemTable->BootServices->InstallConfigurationTable( &ConfigGuid, &Dxe.PublishedConfig );

  return Status;
}

static MemoryDescriptor *AcquireSmmCommRegion( UINTN RequiredSize, BOOLEAN LogIt ) {
  CommunicationRegionTable *Table = 0;
  UINT8 *Entry;
  UINTN Index;

  if ( Dxe.SystemTable == 0 || Dxe.SystemTable->ConfigurationTable == 0 ) {
    if ( LogIt ) {
      LogText( "dxe config table unavailable\n" );
    }

    return 0;
  }

  for ( Index = 0; Index < Dxe.SystemTable->NumberOfTableEntries; Index++ ) {
    EFI_CONFIGURATION_TABLE *Config = &Dxe.SystemTable->ConfigurationTable[Index];

    if ( GuidEqual( &Config->VendorGuid, &CommunicationRegionGuid ) ) {
      Table = ( CommunicationRegionTable * )Config->VendorTable;
      break;
    }
  }

  if ( Table == 0 || Table->Version != 1 || Table->NumberOfEntries == 0 ||
       Table->DescriptorSize < sizeof( MemoryDescriptor ) ) {
    if ( LogIt ) {
      if ( Table == 0 ) {
        LogText( "dxe smm comm region table missing\n" );
      } else {
        LogText( "dxe smm comm region table rejected version=0x" );
        LogHex( Table->Version );
        LogText( " entries=0x" );
        LogHex( Table->NumberOfEntries );
        LogText( " desc=0x" );
        LogHex( Table->DescriptorSize );
        LogText( "\n" );
      }
    }

    return 0;
  }

  Entry = ( UINT8 * )( Table + 1 );

  for ( Index = 0; Index < Table->NumberOfEntries; Index++ ) {
    MemoryDescriptor *Desc = ( MemoryDescriptor * )( Entry + ( Index * Table->DescriptorSize ) );
    UINT64 Bytes = Desc->NumberOfPages << 12;

    if ( Desc->PhysicalStart == 0 || Bytes < RequiredSize ) {
      continue;
    }

    // Type marks availability in the comm region table
    if ( ( UINT32 )_InterlockedCompareExchange( ( volatile long * )&Desc->Type,
           EFI_RESERVED_MEMORY_TYPE, EFI_CONVENTIONAL_MEMORY ) != EFI_CONVENTIONAL_MEMORY ) {
      continue;
    }

    if ( LogIt ) {
      LogText( "dxe smm comm region base=0x" );
      LogHex( Desc->PhysicalStart );
      LogText( " size=0x" );
      LogHex( Bytes );
      LogText( " type=0x" );
      LogHex( Desc->Type );
      LogText( "\n" );
    }

    return Desc;
  }

  if ( LogIt ) {
    LogText( "dxe no usable smm comm region\n" );
  }

  return 0;
}

EFI_STATUS ConfigureSmm( VOID ) {
  EFI_SMM_COMMUNICATION_PROTOCOL *SmmComm = 0;
  EFI_SMM_COMMUNICATE_HEADER *Header;
  CONFIG *Config;
  VOID *CommBuffer;
  MemoryDescriptor *CommRegion;
  UINT32 Attempt;
  BOOLEAN LogIt;
  EFI_STATUS Status;
  UINTN CommSize;

  Attempt = ++Dxe.ConfigureAttempts;
  LogIt = ( Attempt <= 3 );

  if ( LogIt ) {
    LogText( "dxe configure smm attempt=0x" );
    LogHex( Attempt );
    LogText( "\n" );
  }

  Status =
    Dxe.SystemTable->BootServices->LocateProtocol( &SmmCommunicationGuid, 0, ( VOID ** )&SmmComm );

  if ( EFI_ERROR( Status ) || SmmComm == 0 ) {
    if ( LogIt ) {
      LogStatus(
        "dxe smm communication unavailable ", EFI_ERROR( Status ) ? Status : EFI_NOT_FOUND );
    }

    return EFI_ERROR( Status ) ? Status : EFI_NOT_FOUND;
  }

  CommSize = sizeof( EFI_SMM_COMMUNICATE_HEADER ) + sizeof( CONFIG ) + 16;
  CommRegion = AcquireSmmCommRegion( CommSize, LogIt );

  if ( CommRegion == 0 ) {
    if ( LogIt ) {
      LogText( "dxe smm configure no comm buffer\n" );
    }

    return EFI_NOT_FOUND;
  }

  CommBuffer = ( VOID * )( UINTN )CommRegion->PhysicalStart;
  MemoryClear( CommBuffer, CommSize );
  Header = ( EFI_SMM_COMMUNICATE_HEADER * )CommBuffer;
  Header->HeaderGuid = ConfigCommGuid;
  Header->MessageLength = sizeof( CONFIG );
  Config = ( CONFIG * )Header->Data;
  Config->Magic = CONFIG_MAGIC;
  Config->MailboxPhysical = Dxe.MailboxPhysical;
  Config->MailboxSize = MAILBOX_SIZE;
  Config->SwSmiValue = SW_SMI_VALUE;
  Config->Status = EFI_NOT_FOUND;
  Status = SmmComm->Communicate( SmmComm, Header, &CommSize );

  if ( !EFI_ERROR( Status ) ) {
    Status = Config->Status;
  }

  _InterlockedExchange( ( volatile long * )&CommRegion->Type, EFI_CONVENTIONAL_MEMORY );

  if ( !EFI_ERROR( Status ) ) {
    Dxe.Configured = 1;
    LogText( "dxe smm configure ok\n" );
  } else if ( LogIt ) {
    LogStatus( "dxe smm configure failed ", Status );
    LogText( "dxe smm communicate size=0x" );
    LogHex( CommSize );
    LogText( "\n" );
  }

  return Status;
}