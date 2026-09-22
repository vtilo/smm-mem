int Dump( const MODULE_INFO *module, DUMP_CALLBACK callback, void *context ) {
  if ( module == NULL || module->Pid == 0 || module->Base == 0 || module->Size == 0 ||
       module->Base > UINT64_MAX - ( module->Size - 1 ) ) {
    return 0;
  }

  uint8_t buffer[BUFFER_TRANSFER_SIZE];
  uint64_t done = 0;

  while ( done < module->Size ) {
    uint64_t address = module->Base + done;
    uint64_t remaining = module->Size - done;
    uint32_t chunk =
      remaining > sizeof( buffer ) ? ( uint32_t )sizeof( buffer ) : ( uint32_t )remaining;
    uint32_t pageLeft = 0x1000U - ( uint32_t )( address & 0xFFFU );
    if ( chunk > pageLeft ) {
      chunk = pageLeft;
    }

    if ( !ReadVirt( module->Pid, address, buffer, chunk ) ) {
      ZeroMemory( buffer, chunk );
    }

    if ( callback != NULL && !callback( address, buffer, chunk, context ) ) {
      return 0;
    }

    done += chunk;
  }

  return 1;
}
