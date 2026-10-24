static uint32_t GetNameSize( const char *Name, uint32_t Capacity ) {
  if ( Name == NULL ) {
    return 0;
  }

  for ( uint32_t Index = 0; Index < Capacity; Index++ ) {
    if ( Name[Index] == 0 ) {
      return Index + 1;
    }
  }

  return 0;
}

static int QueryBuffer( uint32_t Command,
  uint32_t Pid,
  const void *Input,
  uint32_t InputSize,
  void *Result,
  uint32_t ResultSize,
  uint64_t *Value ) {
  REQUEST Request;
  RESPONSE Response;

  AcquireSRWLockExclusive( &ApiLock );

  if ( Api.Block == NULL || Api.Staging == NULL ) {
    ReleaseSRWLockExclusive( &ApiLock );

    return 0;
  }

  if ( InputSize != 0 ) {
    CopyMemory( Api.Staging, Input, InputSize );
  }

  InitRequest( &Request, Command );
  Request.Pid = Pid;
  Request.Size = InputSize;
  Request.Buffer = ( uint64_t )( uintptr_t )Api.Staging;

  int Success = SendRequest( &Request, &Response ) && Response.Status == STATUS_OK &&
                Response.Size == ResultSize;

  if ( Success && ResultSize != 0 ) {
    CopyMemory( Result, Api.Staging, ResultSize );
  }

  if ( Success && Value != NULL ) {
    *Value = Response.Result;
  }

  ReleaseSRWLockExclusive( &ApiLock );

  return Success;
}

static int QueryName(
  uint32_t Command, uint32_t Pid, const char *Name, void *Result, uint32_t ResultSize ) {
  uint32_t Size = GetNameSize( Name, QUERY_INPUT_SIZE );

  if ( Size == 0 || Result == NULL ) {
    return 0;
  }

  return QueryBuffer( Command, Pid, Name, Size, Result, ResultSize, NULL );
}

int Ping( void ) {
  REQUEST Request;
  RESPONSE Response;

  AcquireSRWLockExclusive( &ApiLock );
  InitRequest( &Request, PING );

  int Success = SendRequest( &Request, &Response ) && Response.Status == STATUS_OK &&
                Response.Size == 0 && Response.Result == PROTOCOL_VERSION;

  ReleaseSRWLockExclusive( &ApiLock );

  return Success;
}

int FindProcessByPid( uint32_t Pid, PROCESS_INFO *Process ) {
  if ( Process == NULL ) {
    return 0;
  }

  return QueryBuffer( FIND_PROCESS_PID, Pid, NULL, 0, Process, sizeof( *Process ), NULL );
}

int FindProcessByName( const char *Name, PROCESS_INFO *Process ) {
  return QueryName( FIND_PROCESS_NAME, 0, Name, Process, sizeof( *Process ) );
}

int TranslateVirt( uint32_t Pid, uint64_t Address, uint64_t *Physical ) {
  if ( Physical == NULL ) {
    return 0;
  }

  REQUEST Request;
  RESPONSE Response;

  AcquireSRWLockExclusive( &ApiLock );
  InitRequest( &Request, TRANSLATE_VIRT );
  Request.Pid = Pid;
  Request.Address = Address;

  int Success =
    SendRequest( &Request, &Response ) && Response.Status == STATUS_OK && Response.Size == 0;

  if ( Success ) {
    *Physical = Response.Result;
  }

  ReleaseSRWLockExclusive( &ApiLock );

  return Success;
}

int FindModule( const PROCESS_INFO *Process, const char *Name, MODULE_INFO *Module ) {
  if ( Process == NULL ) {
    return 0;
  }

  return QueryName( FIND_MODULE, Process->Pid, Name, Module, sizeof( *Module ) );
}

int FindKernelModule( const char *Name, MODULE_INFO *Module ) {
  return QueryName( FIND_KERNEL_MODULE, 0, Name, Module, sizeof( *Module ) );
}

int FindExport( const MODULE_INFO *Module, const char *Name, uint64_t *Address ) {
  if ( Module == NULL || Address == NULL ) {
    return 0;
  }

  uint32_t Size = GetNameSize( Name, QUERY_INPUT_SIZE - sizeof( *Module ) );

  if ( Size == 0 ) {
    return 0;
  }

  uint8_t Input[QUERY_INPUT_SIZE];

  CopyMemory( Input, Module, sizeof( *Module ) );
  CopyMemory( Input + sizeof( *Module ), Name, Size );

  return QueryBuffer(
    FIND_EXPORT, 0, Input, ( uint32_t )sizeof( *Module ) + Size, NULL, 0, Address );
}