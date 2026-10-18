#include "internal.h"

typedef struct {
  UINT64 Cr3;
  UINT64 Address;
  UINT64 Page;
  UINT64 Physical;
  BOOLEAN VirtualMemory;
  BOOLEAN Mapped;
} TransferRange;

static BOOLEAN ValidUserRange( UINT64 Address, UINT64 Size ) {
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
  TRANSFER_BUFFER Buffer;
  ProcessContext Caller;
  ProcessContext Target = { 0 };
  TransferRange Source = { 0 };
  TransferRange Destination = { 0 };
  UINT64 TargetAddress;
  UINT64 RequestedSize;
  UINT32 Size;
  UINT32 Done = 0;
  BOOLEAN Write;
  BOOLEAN VirtualMemory;
  UINT8 Scratch[512];

  *Transferred = 0;

  if ( ( Smm.Features & FEATURE_BUFFER_TRANSFER ) == 0 ) {
    return EFI_UNSUPPORTED;
  }

  if ( Request->DataSize != sizeof( Buffer ) ) {
    return EFI_INVALID_PARAMETER;
  }

  MemoryCopy( &Buffer, Request->Data, sizeof( Buffer ) );

  if ( Buffer.Pid == 0 || Buffer.Pid == 4 ) {
    return EFI_INVALID_PARAMETER;
  }

  VirtualMemory = Request->Command == READ_VIRT_BUFFER || Request->Command == WRITE_VIRT_BUFFER;
  Write = Request->Command == WRITE_VIRT_BUFFER || Request->Command == WRITE_PHYS_BUFFER;
  TargetAddress = VirtualMemory ? Request->Arg2 : Request->Arg1;
  RequestedSize = VirtualMemory ? Request->Arg3 : Request->Arg2;

  if ( RequestedSize == 0 || RequestedSize > BUFFER_TRANSFER_SIZE ||
       !ValidUserRange( Buffer.Address, RequestedSize ) ||
       TargetAddress + RequestedSize < TargetAddress ) {
    return EFI_INVALID_PARAMETER;
  }

  Size = ( UINT32 )RequestedSize;

  if ( ResolveProcess( Buffer.Pid, &Caller ) != EFI_SUCCESS ||
       ValidateRange( Caller.Cr3, Buffer.Address, Size ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  if ( VirtualMemory ) {
    if ( ( UINT32 )Request->Arg1 == Buffer.Pid ) {
      Target = Caller;
    } else {
      if ( ResolveProcess( ( UINT32 )Request->Arg1, &Target ) != EFI_SUCCESS ) {
        return EFI_NOT_FOUND;
      }
    }

    if ( ValidateRange( Target.Cr3, TargetAddress, Size ) != EFI_SUCCESS ) {
      return EFI_NOT_FOUND;
    }
  }

  Source.Cr3 = Write ? Caller.Cr3 : ( VirtualMemory ? Target.Cr3 : 0 );
  Source.Address = Write ? Buffer.Address : TargetAddress;
  Source.VirtualMemory = Write || VirtualMemory;
  Destination.Cr3 = Write ? ( VirtualMemory ? Target.Cr3 : 0 ) : Caller.Cr3;
  Destination.Address = Write ? TargetAddress : Buffer.Address;
  Destination.VirtualMemory = !Write || VirtualMemory;

  while ( Done < Size ) {
    UINT64 From;
    UINT64 To;
    UINT32 Chunk = Size - Done;
    UINT32 SourceLeft = ( UINT32 )( PAGE_SIZE - ( ( Source.Address + Done ) & ( PAGE_SIZE - 1 ) ) );
    UINT32 DestinationLeft =
      ( UINT32 )( PAGE_SIZE - ( ( Destination.Address + Done ) & ( PAGE_SIZE - 1 ) ) );
    EFI_STATUS Status;

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