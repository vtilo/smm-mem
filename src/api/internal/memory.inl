static int TransferMemory(
  uint32_t Command, uint32_t Pid, uint64_t Address, void *Buffer, uint32_t Size ) {
  if ( Size == 0 ) {
    return 1;
  }

  if ( Buffer == NULL || Address > UINT64_MAX - ( Size - 1U ) ) {
    return 0;
  }

  int Physical = Command == READ_PHYS || Command == WRITE_PHYS;
  int Writing = Command == WRITE_PHYS || Command == WRITE_VIRT;
  uint32_t InlineSize = Writing ? INLINE_WRITE_SIZE : INLINE_READ_SIZE;
  int Buffered = Size > InlineSize;
  uint32_t Capacity = Buffered ? BUFFER_TRANSFER_SIZE : InlineSize;
  uint32_t Done = 0;
  int Success = 1;
  REQUEST Request;
  RESPONSE Response;

  AcquireSRWLockExclusive( &ApiLock );

  if ( Api.Block == NULL || Api.Staging == NULL ) {
    ReleaseSRWLockExclusive( &ApiLock );

    return 0;
  }

  InitRequest( &Request, Command );
  Request.Pid = Physical ? 0 : Pid;
  Request.Buffer = Buffered ? ( uint64_t )( uintptr_t )Api.Staging : 0;

  while ( Done < Size ) {
    uint32_t Chunk = Size - Done;

    if ( Chunk > Capacity ) {
      Chunk = Capacity;
    }

    Request.Address = Address + Done;
    Request.Size = Chunk;

    if ( Writing ) {
      CopyMemory( Buffered ? Api.Staging : Request.Data, ( const uint8_t * )Buffer + Done, Chunk );
    }

    if ( !SendRequest( &Request, &Response ) || Response.Status != STATUS_OK ||
         Response.Size != Chunk ) {
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