static void closeFastClient( void ) {
  if ( client.staging != NULL ) {
    VirtualUnlock( client.staging, BUFFER_TRANSFER_SIZE );
    VirtualFree( client.staging, 0, MEM_RELEASE );
    client.staging = NULL;
  }

  if ( client.fastBlock != NULL ) {
    client.closeBlock( client.fastBlock );
    client.fastBlock = NULL;
  }

  client.fastAvailable = 0;
  client.fastInstance = FAST_WMI_INSTANCE_COUNT;
}

static void closeClient( void ) {
  closeFastClient();

  if ( client.closeBlock != NULL && client.block != NULL ) {
    client.closeBlock( client.block );
  }

  if ( client.library != NULL ) {
    FreeLibrary( client.library );
  }

  ZeroMemory( &client, sizeof( client ) );
  client.instance = WMI_INSTANCE_COUNT;
  client.fastInstance = FAST_WMI_INSTANCE_COUNT;
}

static int openClient( void ) {
  closeClient();

  client.library = LoadLibraryW( L"Advapi32.dll" );
  if ( client.library == NULL ) {
    return 0;
  }

  client.openBlock = ( WmiOpenBlockFn )GetProcAddress( client.library, "WmiOpenBlock" );
  client.executeMethod =
    ( WmiExecuteMethodFn )GetProcAddress( client.library, "WmiExecuteMethodW" );
  client.closeBlock = ( WmiCloseBlockFn )GetProcAddress( client.library, "WmiCloseBlock" );

  if ( client.openBlock == NULL || client.executeMethod == NULL || client.closeBlock == NULL ) {
    closeClient();
    return 0;
  }

  ULONG status = client.openBlock( &memoryGuid, WMI_GUID_EXECUTE, &client.block );
  if ( status != ERROR_SUCCESS ) {
    closeClient();
    return 0;
  }

  client.pid = GetCurrentProcessId();
  client.sequence = GetTickCount64();
  return 1;
}

int Init( void ) {
  AcquireSRWLockExclusive( &clientLock );
  int success = openClient();
  ReleaseSRWLockExclusive( &clientLock );

  return success;
}

void Close( void ) {
  AcquireSRWLockExclusive( &clientLock );
  closeClient();
  ReleaseSRWLockExclusive( &clientLock );
}

static void requestInit( REQUEST *request, uint32_t command ) {
  ZeroMemory( request, REQUEST_SIZE );
  request->Magic = REQ_MAGIC;
  request->Command = command;
}

static int executeRequest( HANDLE block,
  const wchar_t *instance,
  uint32_t inputSize,
  uint32_t responseSize,
  REQUEST *request,
  RESPONSE *response ) {
  uint8_t output[RESPONSE_SIZE];
  ULONG outputSize = responseSize;
  ZeroMemory( output, sizeof( output ) );

  ULONG status =
    client.executeMethod( block, instance, WMI_METHOD_ID, inputSize, request, &outputSize, output );
  if ( status != ERROR_SUCCESS ) {
    /* Only missing destinations prove that a write was never dispatched. */
    return status == ERROR_WMI_GUID_NOT_FOUND || status == ERROR_WMI_INSTANCE_NOT_FOUND ? -1 : 0;
  }

  if ( outputSize < offsetof( RESPONSE, Data ) || outputSize > responseSize ) {
    return 0;
  }

  CopyMemory( response, output, sizeof( *response ) );
  if ( response->Magic != RESP_MAGIC || response->Command != request->Command ||
       response->Sequence != request->Sequence || response->DataSize > RESPONSE_DATA_SIZE ||
       outputSize < offsetof( RESPONSE, Data ) + response->DataSize ) {
    return 0;
  }

  return 1;
}

static int sendBlock( HANDLE block,
  const wchar_t *const *instances,
  size_t count,
  size_t *selected,
  uint32_t inputSize,
  uint32_t responseSize,
  REQUEST *request,
  RESPONSE *response ) {
  request->Sequence = ++client.sequence;

  /* Remember discovery, but fall back if the firmware instance disappears. */
  size_t previous = *selected;
  if ( previous < count ) {
    int result =
      executeRequest( block, instances[previous], inputSize, responseSize, request, response );
    if ( result >= 0 ) {
      return result;
    }

    *selected = count;
  }

  for ( size_t index = 0; index < count; index++ ) {
    if ( index == previous ) {
      continue;
    }

    int result =
      executeRequest( block, instances[index], inputSize, responseSize, request, response );
    if ( result < 0 ) {
      continue;
    }

    if ( result != 0 ) {
      *selected = index;
    }

    return result;
  }

  return 0;
}

static int sendLegacy( REQUEST *request, RESPONSE *response ) {
  return sendBlock( client.block,
    memoryInstances,
    WMI_INSTANCE_COUNT,
    &client.instance,
    REQUEST_SIZE,
    RESPONSE_SIZE,
    request,
    response );
}

static int sendFast( REQUEST *request, RESPONSE *response ) {
  return sendBlock( client.fastBlock,
    fastMemoryInstances,
    FAST_WMI_INSTANCE_COUNT,
    &client.fastInstance,
    FAST_REQUEST_SIZE,
    FAST_RESPONSE_SIZE,
    request,
    response );
}

static int prepareFastClient( void ) {
#ifdef SMM_DISABLE_FAST
  return 0;
#else
  if ( client.fastChecked || client.block == NULL ) {
    return client.fastAvailable;
  }

  uint8_t input[REQUEST_SIZE];
  REQUEST *request = ( REQUEST * )input;
  RESPONSE response;
  requestInit( request, PING );

  if ( !sendLegacy( request, &response ) || response.Status != STATUS_OK ) {
    return 0;
  }

  client.fastChecked = 1;
  if ( response.Result != FAST_CAPABILITY ) {
    return 0;
  }

  if ( client.openBlock( &fastMemoryGuid, WMI_GUID_EXECUTE, &client.fastBlock ) != ERROR_SUCCESS ) {
    client.fastBlock = NULL;
    return 0;
  }

  if ( !sendFast( request, &response ) || response.Status != STATUS_OK ||
       response.Result != FAST_CAPABILITY ) {
    closeFastClient();
    return 0;
  }

  client.fastAvailable = 1;
  client.staging = ( uint8_t * )VirtualAlloc(
    NULL, BUFFER_TRANSFER_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
  if ( client.staging != NULL && !VirtualLock( client.staging, BUFFER_TRANSFER_SIZE ) ) {
    VirtualFree( client.staging, 0, MEM_RELEASE );
    client.staging = NULL;
  }

  return 1;
#endif
}

static int fastRequestFits( const REQUEST *request ) {
  switch ( request->Command ) {
  case PING:
  case TRANSLATE_VIRT:
    return request->DataSize == 0;
  case READ_PHYS:
    return request->Arg2 <= FAST_INLINE_READ_SIZE;
  case READ_VIRT:
    return request->Arg3 <= FAST_INLINE_READ_SIZE;
  case WRITE_PHYS:
  case WRITE_VIRT:
    return request->DataSize <= FAST_INLINE_WRITE_SIZE;
  case READ_VIRT_BUFFER:
  case WRITE_VIRT_BUFFER:
  case READ_PHYS_BUFFER:
  case WRITE_PHYS_BUFFER:
    return request->DataSize == sizeof( TRANSFER_BUFFER );
  default:
    return 0;
  }
}

static int requestSend( REQUEST *request, RESPONSE *response ) {
  if ( client.block == NULL ) {
    return 0;
  }

  if ( fastRequestFits( request ) && prepareFastClient() ) {
    int success = sendFast( request, response );
    if ( !success ) {
      /* Do not retry an operation which may already have performed a write. */
      closeFastClient();
    }

    return success;
  }

  return sendLegacy( request, response );
}
