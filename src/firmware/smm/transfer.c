#include "internal.h"

typedef struct {
  UINT64 cr3;
  UINT64 address;
  UINT64 page;
  UINT64 physical;
  BOOLEAN virtualMemory;
  BOOLEAN mapped;
} TransferRange;

static BOOLEAN validUserRange( UINT64 address, UINT64 size ) {
  return size != 0 && address >= 0x10000 && address < 0x0000800000000000ULL &&
         size <= 0x0000800000000000ULL - address;
}

static EFI_STATUS validateRange( UINT64 cr3, UINT64 address, UINT32 size ) {
  UINT32 done = 0;

  while ( done < size ) {
    UINT64 current = address + done;
    UINT64 physical;
    UINT32 chunk = ( UINT32 )( PAGE_SIZE - ( current & ( PAGE_SIZE - 1 ) ) );

    if ( translateVirtual( cr3, current, &physical ) != EFI_SUCCESS ) {
      return EFI_NOT_FOUND;
    }
    if ( chunk > size - done ) {
      chunk = size - done;
    }
    done += chunk;
  }

  return EFI_SUCCESS;
}

static EFI_STATUS mapRange( TransferRange *range, UINT64 offset, UINT64 *physical ) {
  UINT64 address = range->address + offset;
  UINT64 page = address & PAGE_MASK;

  if ( !range->virtualMemory ) {
    *physical = address;
    return EFI_SUCCESS;
  }

  if ( !range->mapped || page != range->page ) {
    if ( translateVirtual( range->cr3, page, &range->physical ) != EFI_SUCCESS ) {
      return EFI_NOT_FOUND;
    }
    range->page = page;
    range->mapped = 1;
  }

  *physical = range->physical + ( address & ( PAGE_SIZE - 1 ) );
  return EFI_SUCCESS;
}

EFI_STATUS transferBuffer( REQUEST *request, UINT64 *transferred ) {
  TRANSFER_BUFFER buffer;
  ProcessContext caller;
  ProcessContext target = { 0 };
  TransferRange source = { 0 };
  TransferRange destination = { 0 };
  UINT64 targetAddress;
  UINT64 requestedSize;
  UINT32 size;
  UINT32 done = 0;
  BOOLEAN write;
  BOOLEAN virtualMemory;
  UINT8 scratch[512];

  *transferred = 0;
  if ( ( smm.features & FEATURE_BUFFER_TRANSFER ) == 0 ) {
    return EFI_UNSUPPORTED;
  }
  if ( request->DataSize != sizeof( buffer ) ) {
    return EFI_INVALID_PARAMETER;
  }

  memoryCopy( &buffer, request->Data, sizeof( buffer ) );
  if ( buffer.Pid == 0 || buffer.Pid == 4 ) {
    return EFI_INVALID_PARAMETER;
  }
  virtualMemory = request->Command == READ_VIRT_BUFFER || request->Command == WRITE_VIRT_BUFFER;
  write = request->Command == WRITE_VIRT_BUFFER || request->Command == WRITE_PHYS_BUFFER;
  targetAddress = virtualMemory ? request->Arg2 : request->Arg1;
  requestedSize = virtualMemory ? request->Arg3 : request->Arg2;
  if ( requestedSize == 0 || requestedSize > BUFFER_TRANSFER_SIZE ||
       !validUserRange( buffer.Address, requestedSize ) ||
       targetAddress + requestedSize < targetAddress ) {
    return EFI_INVALID_PARAMETER;
  }
  size = ( UINT32 )requestedSize;

  if ( resolveProcess( buffer.Pid, &caller ) != EFI_SUCCESS ||
       validateRange( caller.cr3, buffer.Address, size ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }
  if ( virtualMemory ) {
    if ( ( UINT32 )request->Arg1 == buffer.Pid ) {
      target = caller;
    } else {
      if ( resolveProcess( ( UINT32 )request->Arg1, &target ) != EFI_SUCCESS ) {
        return EFI_NOT_FOUND;
      }
    }
    if ( validateRange( target.cr3, targetAddress, size ) != EFI_SUCCESS ) {
      return EFI_NOT_FOUND;
    }
  }

  source.cr3 = write ? caller.cr3 : ( virtualMemory ? target.cr3 : 0 );
  source.address = write ? buffer.Address : targetAddress;
  source.virtualMemory = write || virtualMemory;
  destination.cr3 = write ? ( virtualMemory ? target.cr3 : 0 ) : caller.cr3;
  destination.address = write ? targetAddress : buffer.Address;
  destination.virtualMemory = !write || virtualMemory;

  while ( done < size ) {
    UINT64 from;
    UINT64 to;
    UINT32 chunk = size - done;
    UINT32 sourceLeft = ( UINT32 )( PAGE_SIZE - ( ( source.address + done ) & ( PAGE_SIZE - 1 ) ) );
    UINT32 destinationLeft =
      ( UINT32 )( PAGE_SIZE - ( ( destination.address + done ) & ( PAGE_SIZE - 1 ) ) );
    EFI_STATUS status;

    if ( chunk > sizeof( scratch ) ) {
      chunk = sizeof( scratch );
    }
    if ( chunk > sourceLeft ) {
      chunk = sourceLeft;
    }
    if ( chunk > destinationLeft ) {
      chunk = destinationLeft;
    }
    status = mapRange( &source, done, &from );
    if ( EFI_ERROR( status ) ) {
      return status;
    }
    status = mapRange( &destination, done, &to );
    if ( EFI_ERROR( status ) ) {
      return status;
    }
    status = copyPhysical( from, scratch, chunk, 0 );
    if ( EFI_ERROR( status ) ) {
      return status;
    }
    status = copyPhysical( to, scratch, chunk, 1 );
    if ( EFI_ERROR( status ) ) {
      return status;
    }
    done += chunk;
    *transferred = done;
  }

  return EFI_SUCCESS;
}
