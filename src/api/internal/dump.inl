int Dump( const MODULE_INFO *Module, DUMP_CALLBACK Callback, void *Context ) {
  if ( Module == NULL || Module->Pid == 0 || Module->Base == 0 || Module->Size == 0 ||
       Module->Base > UINT64_MAX - ( Module->Size - 1 ) ) {
    return 0;
  }

  uint8_t Buffer[BUFFER_TRANSFER_SIZE];
  uint64_t Done = 0;

  while ( Done < Module->Size ) {
    uint64_t Address = Module->Base + Done;
    uint64_t Remaining = Module->Size - Done;
    uint32_t Chunk =
      Remaining > sizeof( Buffer ) ? ( uint32_t )sizeof( Buffer ) : ( uint32_t )Remaining;
    uint32_t PageLeft = 0x1000U - ( uint32_t )( Address & 0xFFFU );

    if ( Chunk > PageLeft ) {
      Chunk = PageLeft;
    }

    if ( !ReadVirt( Module->Pid, Address, Buffer, Chunk ) ) {
      ZeroMemory( Buffer, Chunk );
    }

    if ( Callback != NULL && !Callback( Address, Buffer, Chunk, Context ) ) {
      return 0;
    }

    Done += Chunk;
  }

  return 1;
}