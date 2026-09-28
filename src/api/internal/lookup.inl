static uint32_t nameSize( const char *name, uint32_t capacity ) {
  if ( name == NULL ) {
    return 0;
  }

  for ( uint32_t index = 0; index < capacity; index++ ) {
    if ( name[index] == 0 ) {
      return index + 1;
    }
  }

  return 0;
}

static int queryName(
  uint32_t command, uint32_t pid, const char *name, void *result, uint32_t resultSize ) {
  uint32_t size = nameSize( name, RESPONSE_DATA_SIZE );
  if ( size == 0 || result == NULL ) {
    return 0;
  }

  uint8_t input[REQUEST_SIZE];
  REQUEST *request = ( REQUEST * )input;
  RESPONSE response;
  requestInit( request, command );
  request->Arg1 = pid;
  request->DataSize = size;
  CopyMemory( request->Data, name, size );

  AcquireSRWLockExclusive( &clientLock );
  int success = requestSend( request, &response ) && response.Status == STATUS_OK &&
                response.DataSize >= resultSize;
  if ( success ) {
    CopyMemory( result, response.Data, resultSize );
  }

  ReleaseSRWLockExclusive( &clientLock );
  return success;
}

int Ping( void ) {
  uint8_t input[REQUEST_SIZE];
  REQUEST *request = ( REQUEST * )input;
  RESPONSE response;
  requestInit( request, PING );

  AcquireSRWLockExclusive( &clientLock );
  int success = requestSend( request, &response ) && response.Status == STATUS_OK;
  ReleaseSRWLockExclusive( &clientLock );

  return success;
}

int FindProcessByPid( uint32_t pid, PROCESS_INFO *process ) {
  if ( process == NULL ) {
    return 0;
  }

  uint8_t input[REQUEST_SIZE];
  REQUEST *request = ( REQUEST * )input;
  RESPONSE response;
  requestInit( request, FIND_PROCESS_PID );
  request->Arg1 = pid;

  AcquireSRWLockExclusive( &clientLock );
  int success = requestSend( request, &response ) && response.Status == STATUS_OK &&
                response.DataSize >= sizeof( *process );
  if ( success ) {
    CopyMemory( process, response.Data, sizeof( *process ) );
  }

  ReleaseSRWLockExclusive( &clientLock );
  return success;
}

int FindProcessByName( const char *name, PROCESS_INFO *process ) {
  return queryName( FIND_PROCESS_NAME, 0, name, process, sizeof( *process ) );
}

int TranslateVirt( uint32_t pid, uint64_t address, uint64_t *physical ) {
  if ( physical == NULL ) {
    return 0;
  }

  uint8_t input[REQUEST_SIZE];
  REQUEST *request = ( REQUEST * )input;
  RESPONSE response;
  requestInit( request, TRANSLATE_VIRT );
  request->Arg1 = pid;
  request->Arg2 = address;

  AcquireSRWLockExclusive( &clientLock );
  int success = requestSend( request, &response ) && response.Status == STATUS_OK;
  if ( success ) {
    *physical = response.Result;
  }

  ReleaseSRWLockExclusive( &clientLock );
  return success;
}

int FindModule( const PROCESS_INFO *process, const char *name, MODULE_INFO *module ) {
  if ( process == NULL ) {
    return 0;
  }

  return queryName( FIND_MODULE, process->Pid, name, module, sizeof( *module ) );
}

int FindKernelModule( const char *name, MODULE_INFO *module ) {
  return queryName( FIND_KERNEL_MODULE, 0, name, module, sizeof( *module ) );
}

int FindExport( const MODULE_INFO *module, const char *name, uint64_t *address ) {
  if ( module == NULL || address == NULL ) {
    return 0;
  }

  uint32_t size = nameSize( name, RESPONSE_DATA_SIZE - sizeof( *module ) );
  if ( size == 0 ) {
    return 0;
  }

  uint8_t input[REQUEST_SIZE];
  REQUEST *request = ( REQUEST * )input;
  RESPONSE response;
  requestInit( request, FIND_EXPORT );
  request->DataSize = ( uint32_t )sizeof( *module ) + size;
  CopyMemory( request->Data, module, sizeof( *module ) );
  CopyMemory( request->Data + sizeof( *module ), name, size );

  AcquireSRWLockExclusive( &clientLock );
  int success = requestSend( request, &response ) && response.Status == STATUS_OK;
  if ( success ) {
    *address = response.Result;
  }

  ReleaseSRWLockExclusive( &clientLock );
  return success;
}
