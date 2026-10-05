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

static int QueryName(
  uint32_t Command, uint32_t Pid, const char *Name, void *Result, uint32_t ResultSize ) {
  uint32_t Size = GetNameSize( Name, RESPONSE_DATA_SIZE );

  if ( Size == 0 || Result == NULL ) {
    return 0;
  }

  uint8_t Input[REQUEST_SIZE];
  REQUEST *Request = ( REQUEST * )Input;
  RESPONSE Response;

  InitRequest( Request, Command );
  Request->Arg1 = Pid;
  Request->DataSize = Size;
  CopyMemory( Request->Data, Name, Size );

  AcquireSRWLockExclusive( &ApiLock );
  int Success = SendRequest( Request, &Response ) && Response.Status == STATUS_OK &&
                Response.DataSize >= ResultSize;

  if ( Success ) {
    CopyMemory( Result, Response.Data, ResultSize );
  }

  ReleaseSRWLockExclusive( &ApiLock );

  return Success;
}

int Ping( void ) {
  uint8_t Input[REQUEST_SIZE];
  REQUEST *Request = ( REQUEST * )Input;
  RESPONSE Response;

  InitRequest( Request, PING );

  AcquireSRWLockExclusive( &ApiLock );
  int Success = SendRequest( Request, &Response ) && Response.Status == STATUS_OK;
  ReleaseSRWLockExclusive( &ApiLock );

  return Success;
}

int FindProcessByPid( uint32_t Pid, PROCESS_INFO *Process ) {
  if ( Process == NULL ) {
    return 0;
  }

  uint8_t Input[REQUEST_SIZE];
  REQUEST *Request = ( REQUEST * )Input;
  RESPONSE Response;

  InitRequest( Request, FIND_PROCESS_PID );
  Request->Arg1 = Pid;

  AcquireSRWLockExclusive( &ApiLock );
  int Success = SendRequest( Request, &Response ) && Response.Status == STATUS_OK &&
                Response.DataSize >= sizeof( *Process );

  if ( Success ) {
    CopyMemory( Process, Response.Data, sizeof( *Process ) );
  }

  ReleaseSRWLockExclusive( &ApiLock );

  return Success;
}

int FindProcessByName( const char *Name, PROCESS_INFO *Process ) {
  return QueryName( FIND_PROCESS_NAME, 0, Name, Process, sizeof( *Process ) );
}

int TranslateVirt( uint32_t Pid, uint64_t Address, uint64_t *Physical ) {
  if ( Physical == NULL ) {
    return 0;
  }

  uint8_t Input[REQUEST_SIZE];
  REQUEST *Request = ( REQUEST * )Input;
  RESPONSE Response;

  InitRequest( Request, TRANSLATE_VIRT );
  Request->Arg1 = Pid;
  Request->Arg2 = Address;

  AcquireSRWLockExclusive( &ApiLock );
  int Success = SendRequest( Request, &Response ) && Response.Status == STATUS_OK;

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

  uint32_t Size = GetNameSize( Name, RESPONSE_DATA_SIZE - sizeof( *Module ) );

  if ( Size == 0 ) {
    return 0;
  }

  uint8_t Input[REQUEST_SIZE];
  REQUEST *Request = ( REQUEST * )Input;
  RESPONSE Response;

  InitRequest( Request, FIND_EXPORT );
  Request->DataSize = ( uint32_t )sizeof( *Module ) + Size;
  CopyMemory( Request->Data, Module, sizeof( *Module ) );
  CopyMemory( Request->Data + sizeof( *Module ), Name, Size );

  AcquireSRWLockExclusive( &ApiLock );
  int Success = SendRequest( Request, &Response ) && Response.Status == STATUS_OK;

  if ( Success ) {
    *Address = Response.Result;
  }

  ReleaseSRWLockExclusive( &ApiLock );

  return Success;
}