static uint32_t GetBufferCommand( uint32_t Command ) {
  switch ( Command ) {
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

static int TransferMemory(
  uint32_t Command, uint32_t Pid, uint64_t Address, void *Buffer, uint32_t Size ) {
  if ( Size == 0 ) {
    return 1;
  }

  if ( Buffer == NULL || Address > UINT64_MAX - ( Size - 1U ) ) {
    return 0;
  }

  uint8_t Input[REQUEST_SIZE];
  REQUEST *Request = ( REQUEST * )Input;
  RESPONSE Response;
  uint32_t Done = 0;
  uint32_t PreviousSize = 0;
  int Physical = Command == READ_PHYS || Command == WRITE_PHYS;
  int Writing = Command == WRITE_PHYS || Command == WRITE_VIRT;
  int Success = 1;

  AcquireSRWLockExclusive( &ApiLock );
  PrepareFastTransport();

  uint32_t InlineSize = Writing ? FAST_INLINE_WRITE_SIZE : FAST_INLINE_READ_SIZE;
  int Buffered = Api.FastAvailable && Api.Staging != NULL && Size > InlineSize;
  uint32_t Capacity = Buffered ? BUFFER_TRANSFER_SIZE : RESPONSE_DATA_SIZE;

  // Keep unused wire bytes zero without clearing 4 KiB for every chunk
  InitRequest( Request, Buffered ? GetBufferCommand( Command ) : Command );

  if ( Buffered ) {
    TRANSFER_BUFFER *Transfer = ( TRANSFER_BUFFER * )Request->Data;

    Request->DataSize = sizeof( *Transfer );
    Transfer->Pid = Api.Pid;
    Transfer->Address = ( uint64_t )( uintptr_t )Api.Staging;
  }

  while ( Done < Size ) {
    uint32_t Chunk = Size - Done;

    if ( Chunk > Capacity ) {
      Chunk = Capacity;
    }

    Request->Arg1 = Physical ? Address + Done : Pid;
    Request->Arg2 = Physical ? Chunk : Address + Done;
    Request->Arg3 = Physical ? 0 : Chunk;

    if ( Writing ) {
      if ( Buffered ) {
        CopyMemory( Api.Staging, ( const uint8_t * )Buffer + Done, Chunk );
      } else {
        Request->DataSize = Chunk;
        CopyMemory( Request->Data, ( const uint8_t * )Buffer + Done, Chunk );

        if ( Chunk < PreviousSize ) {
          ZeroMemory( Request->Data + Chunk, PreviousSize - Chunk );
        }

        PreviousSize = Chunk;
        Request->Arg2 = Physical ? 0 : Address + Done;
        Request->Arg3 = 0;
      }
    }

    if ( !SendRequest( Request, &Response ) || Response.Status != STATUS_OK ||
         ( Buffered && Response.Result != Chunk ) ||
         ( !Buffered && !Writing && Response.DataSize != Chunk ) ) {
      Success = 0;
      break;
    }

    if ( !Writing ) {
      CopyMemory( ( uint8_t * )Buffer + Done, Buffered ? Api.Staging : Response.Data, Chunk );
    }

    Done += Chunk;
  }

  ReleaseSRWLockExclusive( &ApiLock );

  return Success;
}

int ReadPhys( uint64_t Address, void *Buffer, uint32_t Size ) {
  return TransferMemory( READ_PHYS, 0, Address, Buffer, Size );
}

int WritePhys( uint64_t Address, const void *Buffer, uint32_t Size ) {
  return TransferMemory( WRITE_PHYS, 0, Address, ( void * )Buffer, Size );
}

int ReadVirt( uint32_t Pid, uint64_t Address, void *Buffer, uint32_t Size ) {
  return TransferMemory( READ_VIRT, Pid, Address, Buffer, Size );
}

int WriteVirt( uint32_t Pid, uint64_t Address, const void *Buffer, uint32_t Size ) {
  return TransferMemory( WRITE_VIRT, Pid, Address, ( void * )Buffer, Size );
}