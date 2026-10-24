#include "internal.h"

typedef struct {
  UINT64 Cr3;
  UINT64 Address;
  UINT64 Page;
  UINT64 Physical;
  BOOLEAN VirtualMemory;
  BOOLEAN Mapped;
} TransferRange;

static BOOLEAN ValidUserRange( UINT64 Address, UINT32 Size ) {
  return Size != 0 && Address >= 0x10000 && Address < 0x0000800000000000ULL &&
         Size <= 0x0000800000000000ULL - Address;
}

static EFI_STATUS ValidateRange( UINT64 Cr3, UINT64 Address, UINT32 Size ) {
  UINT32 Done = 0;

  while ( Done < Size ) {
    UINT64 Current = Address + Done;
    UINT64 Physical;
    UINT32 Chunk = ( UINT32 )( PAGE_SIZE - ( Current & ( PAGE_SIZE - 1 ) ) );

    if ( TranslateVirtual( Cr3, Current, &Physical ) != EFI_SUCCESS ) {
      return EFI_NOT_FOUND;
    }

    if ( Chunk > Size - Done ) {
      Chunk = Size - Done;
    }

    Done += Chunk;
  }

  return EFI_SUCCESS;
}

EFI_STATUS ResolveCallerBuffer( const REQUEST *Request, UINT32 Size, ProcessContext *Caller ) {
  if ( Size == 0 || Size > BUFFER_TRANSFER_SIZE || Request->CallerPid == 0 ||
       Request->CallerPid == 4 || !ValidUserRange( Request->Buffer, Size ) ) {
    return EFI_INVALID_PARAMETER;
  }

  if ( ResolveProcess( Request->CallerPid, Caller ) != EFI_SUCCESS ||
       ValidateRange( Caller->Cr3, Request->Buffer, Size ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

static EFI_STATUS MapRange( TransferRange *Range, UINT64 Offset, UINT64 *Physical ) {
  UINT64 Address = Range->Address + Offset;
  UINT64 Page = Address & PAGE_MASK;

  if ( !Range->VirtualMemory ) {
    *Physical = Address;
    return EFI_SUCCESS;
  }

  if ( !Range->Mapped || Page != Range->Page ) {
    if ( TranslateVirtual( Range->Cr3, Page, &Range->Physical ) != EFI_SUCCESS ) {
      return EFI_NOT_FOUND;
    }

    Range->Page = Page;
    Range->Mapped = 1;
  }

  *Physical = Range->Physical + ( Address & ( PAGE_SIZE - 1 ) );
  return EFI_SUCCESS;
}

EFI_STATUS TransferBuffer( REQUEST *Request, UINT64 *Transferred ) {
  ProcessContext Caller;
  ProcessContext Target = { 0 };
  TransferRange Source = { 0 };
  TransferRange Destination = { 0 };
  UINT32 Done = 0;
  BOOLEAN Write = Request->Command == WRITE_VIRT || Request->Command == WRITE_PHYS;
  BOOLEAN VirtualMemory = Request->Command == READ_VIRT || Request->Command == WRITE_VIRT;
  UINT8 Scratch[512];
  EFI_STATUS Status;

  *Transferred = 0;

  if ( Request->Size == 0 || Request->Size > BUFFER_TRANSFER_SIZE ||
       Request->Address > ~0ULL - ( Request->Size - 1U ) ) {
    return EFI_INVALID_PARAMETER;
  }

  Status = ResolveCallerBuffer( Request, Request->Size, &Caller );

  if ( EFI_ERROR( Status ) ) {
    return Status;
  }

  if ( VirtualMemory ) {
    if ( Request->Pid == Request->CallerPid ) {
      Target = Caller;
    } else if ( ResolveProcess( Request->Pid, &Target ) != EFI_SUCCESS ) {
      return EFI_NOT_FOUND;
    }

    if ( ValidateRange( Target.Cr3, Request->Address, Request->Size ) != EFI_SUCCESS ) {
      return EFI_NOT_FOUND;
    }
  }

  Source.Cr3 = Write ? Caller.Cr3 : ( VirtualMemory ? Target.Cr3 : 0 );
  Source.Address = Write ? Request->Buffer : Request->Address;
  Source.VirtualMemory = Write || VirtualMemory;
  Destination.Cr3 = Write ? ( VirtualMemory ? Target.Cr3 : 0 ) : Caller.Cr3;
  Destination.Address = Write ? Request->Address : Request->Buffer;
  Destination.VirtualMemory = !Write || VirtualMemory;

  while ( Done < Request->Size ) {
    UINT64 From;
    UINT64 To;
    UINT32 Chunk = Request->Size - Done;
    UINT32 SourceLeft = ( UINT32 )( PAGE_SIZE - ( ( Source.Address + Done ) & ( PAGE_SIZE - 1 ) ) );
    UINT32 DestinationLeft =
      ( UINT32 )( PAGE_SIZE - ( ( Destination.Address + Done ) & ( PAGE_SIZE - 1 ) ) );

    if ( Chunk > sizeof( Scratch ) ) {
      Chunk = sizeof( Scratch );
    }

    if ( Chunk > SourceLeft ) {
      Chunk = SourceLeft;
    }

    if ( Chunk > DestinationLeft ) {
      Chunk = DestinationLeft;
    }

    Status = MapRange( &Source, Done, &From );

    if ( EFI_ERROR( Status ) ) {
      return Status;
    }

    Status = MapRange( &Destination, Done, &To );

    if ( EFI_ERROR( Status ) ) {
      return Status;
    }

    Status = CopyPhysical( From, Scratch, Chunk, 0 );

    if ( EFI_ERROR( Status ) ) {
      return Status;
    }

    Status = CopyPhysical( To, Scratch, Chunk, 1 );

    if ( EFI_ERROR( Status ) ) {
      return Status;
    }

    Done += Chunk;
    *Transferred = Done;
  }

  return EFI_SUCCESS;
}