static uint32_t bufferCommand( uint32_t command ) {
  switch ( command ) {
  case READ_VIRT:
    return READ_VIRT_BUFFER;
  case WRITE_VIRT:
    return WRITE_VIRT_BUFFER;
  case READ_PHYS:
    return READ_PHYS_BUFFER;
  default:
    return WRITE_PHYS_BUFFER;
  }
}

static int transferMemory(
  uint32_t command, uint32_t pid, uint64_t address, void *buffer, uint32_t size ) {
  if ( size == 0 ) {
    return 1;
  }

  if ( buffer == NULL || address > UINT64_MAX - ( size - 1U ) ) {
    return 0;
  }

  uint8_t input[REQUEST_SIZE];
  REQUEST *request = ( REQUEST * )input;
  RESPONSE response;
  uint32_t done = 0;
  uint32_t previousSize = 0;
  int physical = command == READ_PHYS || command == WRITE_PHYS;
  int writing = command == WRITE_PHYS || command == WRITE_VIRT;
  int success = 1;

  AcquireSRWLockExclusive( &clientLock );
  prepareFastClient();

  uint32_t inlineSize = writing ? FAST_INLINE_WRITE_SIZE : FAST_INLINE_READ_SIZE;
  int buffered = client.fastAvailable && client.staging != NULL && size > inlineSize;
  uint32_t capacity = buffered ? BUFFER_TRANSFER_SIZE : RESPONSE_DATA_SIZE;

  /* Keep unused wire bytes zero without clearing 4 KiB for every chunk. */
  requestInit( request, buffered ? bufferCommand( command ) : command );
  if ( buffered ) {
    TRANSFER_BUFFER *transfer = ( TRANSFER_BUFFER * )request->Data;
    request->DataSize = sizeof( *transfer );
    transfer->Pid = client.pid;
    transfer->Address = ( uint64_t )( uintptr_t )client.staging;
  }

  while ( done < size ) {
    uint32_t chunk = size - done;
    if ( chunk > capacity ) {
      chunk = capacity;
    }

    request->Arg1 = physical ? address + done : pid;
    request->Arg2 = physical ? chunk : address + done;
    request->Arg3 = physical ? 0 : chunk;

    if ( writing ) {
      if ( buffered ) {
        CopyMemory( client.staging, ( const uint8_t * )buffer + done, chunk );
      } else {
        request->DataSize = chunk;
        CopyMemory( request->Data, ( const uint8_t * )buffer + done, chunk );
        if ( chunk < previousSize ) {
          ZeroMemory( request->Data + chunk, previousSize - chunk );
        }

        previousSize = chunk;
        request->Arg2 = physical ? 0 : address + done;
        request->Arg3 = 0;
      }
    }

    if ( !requestSend( request, &response ) || response.Status != STATUS_OK ||
         ( buffered && response.Result != chunk ) ||
         ( !buffered && !writing && response.DataSize != chunk ) ) {
      success = 0;
      break;
    }

    if ( !writing ) {
      CopyMemory( ( uint8_t * )buffer + done, buffered ? client.staging : response.Data, chunk );
    }

    done += chunk;
  }

  ReleaseSRWLockExclusive( &clientLock );
  return success;
}

int ReadPhys( uint64_t address, void *buffer, uint32_t size ) {
  return transferMemory( READ_PHYS, 0, address, buffer, size );
}

int WritePhys( uint64_t address, const void *buffer, uint32_t size ) {
  return transferMemory( WRITE_PHYS, 0, address, ( void * )buffer, size );
}

int ReadVirt( uint32_t pid, uint64_t address, void *buffer, uint32_t size ) {
  return transferMemory( READ_VIRT, pid, address, buffer, size );
}

int WriteVirt( uint32_t pid, uint64_t address, const void *buffer, uint32_t size ) {
  return transferMemory( WRITE_VIRT, pid, address, ( void * )buffer, size );
}
