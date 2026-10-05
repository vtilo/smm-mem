static void CloseFastTransport( void ) {
  if ( Api.Staging != NULL ) {
    VirtualUnlock( Api.Staging, BUFFER_TRANSFER_SIZE );
    VirtualFree( Api.Staging, 0, MEM_RELEASE );
    Api.Staging = NULL;
  }

  if ( Api.FastBlock != NULL ) {
    Api.CloseBlock( Api.FastBlock );
    Api.FastBlock = NULL;
  }

  Api.FastAvailable = 0;
  Api.FastInstance = FAST_WMI_INSTANCE_COUNT;
}

static void CloseTransport( void ) {
  CloseFastTransport();

  if ( Api.CloseBlock != NULL && Api.Block != NULL ) {
    Api.CloseBlock( Api.Block );
  }

  if ( Api.Library != NULL ) {
    FreeLibrary( Api.Library );
  }

  ZeroMemory( &Api, sizeof( Api ) );
  Api.Instance = WMI_INSTANCE_COUNT;
  Api.FastInstance = FAST_WMI_INSTANCE_COUNT;
}

static int OpenTransport( void ) {
  CloseTransport();

  Api.Library = LoadLibraryW( L"Advapi32.dll" );

  if ( Api.Library == NULL ) {
    return 0;
  }

  Api.OpenBlock = ( WmiOpenBlockFn )GetProcAddress( Api.Library, "WmiOpenBlock" );
  Api.ExecuteMethod = ( WmiExecuteMethodFn )GetProcAddress( Api.Library, "WmiExecuteMethodW" );
  Api.CloseBlock = ( WmiCloseBlockFn )GetProcAddress( Api.Library, "WmiCloseBlock" );

  if ( Api.OpenBlock == NULL || Api.ExecuteMethod == NULL || Api.CloseBlock == NULL ) {
    CloseTransport();
    return 0;
  }

  ULONG Status = Api.OpenBlock( &MemoryGuid, WMI_GUID_EXECUTE, &Api.Block );

  if ( Status != ERROR_SUCCESS ) {
    CloseTransport();
    return 0;
  }

  Api.Pid = GetCurrentProcessId();
  Api.Sequence = GetTickCount64();

  return 1;
}

int Init( void ) {
  AcquireSRWLockExclusive( &ApiLock );
  int Success = OpenTransport();
  ReleaseSRWLockExclusive( &ApiLock );

  return Success;
}

void Close( void ) {
  AcquireSRWLockExclusive( &ApiLock );
  CloseTransport();
  ReleaseSRWLockExclusive( &ApiLock );
}

static void InitRequest( REQUEST *Request, uint32_t Command ) {
  ZeroMemory( Request, REQUEST_SIZE );
  Request->Magic = REQ_MAGIC;
  Request->Command = Command;
}

static int ExecuteRequest( HANDLE Block,
  const wchar_t *Instance,
  uint32_t InputSize,
  uint32_t ResponseSize,
  REQUEST *Request,
  RESPONSE *Response ) {
  uint8_t Output[RESPONSE_SIZE];
  ULONG OutputSize = ResponseSize;

  ZeroMemory( Output, sizeof( Output ) );

  ULONG Status =
    Api.ExecuteMethod( Block, Instance, WMI_METHOD_ID, InputSize, Request, &OutputSize, Output );

  if ( Status != ERROR_SUCCESS ) {
    // Only missing destinations prove that a write was never dispatched
    return Status == ERROR_WMI_GUID_NOT_FOUND || Status == ERROR_WMI_INSTANCE_NOT_FOUND ? -1 : 0;
  }

  if ( OutputSize < offsetof( RESPONSE, Data ) || OutputSize > ResponseSize ) {
    return 0;
  }

  CopyMemory( Response, Output, sizeof( *Response ) );

  if ( Response->Magic != RESP_MAGIC || Response->Command != Request->Command ||
       Response->Sequence != Request->Sequence || Response->DataSize > RESPONSE_DATA_SIZE ||
       OutputSize < offsetof( RESPONSE, Data ) + Response->DataSize ) {
    return 0;
  }

  return 1;
}

static int SendBlock( HANDLE Block,
  const wchar_t *const *Instances,
  size_t Count,
  size_t *Selected,
  uint32_t InputSize,
  uint32_t ResponseSize,
  REQUEST *Request,
  RESPONSE *Response ) {
  Request->Sequence = ++Api.Sequence;

  size_t Previous = *Selected;

  if ( Previous < Count ) {
    int Result =
      ExecuteRequest( Block, Instances[Previous], InputSize, ResponseSize, Request, Response );

    if ( Result >= 0 ) {
      return Result;
    }

    *Selected = Count;
  }

  for ( size_t Index = 0; Index < Count; Index++ ) {
    if ( Index == Previous ) {
      continue;
    }

    int Result =
      ExecuteRequest( Block, Instances[Index], InputSize, ResponseSize, Request, Response );

    if ( Result < 0 ) {
      continue;
    }

    if ( Result != 0 ) {
      *Selected = Index;
    }

    return Result;
  }

  return 0;
}

static int SendLegacy( REQUEST *Request, RESPONSE *Response ) {
  return SendBlock( Api.Block,
    MemoryInstances,
    WMI_INSTANCE_COUNT,
    &Api.Instance,
    REQUEST_SIZE,
    RESPONSE_SIZE,
    Request,
    Response );
}

static int SendFast( REQUEST *Request, RESPONSE *Response ) {
  return SendBlock( Api.FastBlock,
    FastMemoryInstances,
    FAST_WMI_INSTANCE_COUNT,
    &Api.FastInstance,
    FAST_REQUEST_SIZE,
    FAST_RESPONSE_SIZE,
    Request,
    Response );
}

static int PrepareFastTransport( void ) {
#ifdef SMM_DISABLE_FAST
  return 0;
#else

  if ( Api.FastChecked || Api.Block == NULL ) {
    return Api.FastAvailable;
  }

  uint8_t Input[REQUEST_SIZE];
  REQUEST *Request = ( REQUEST * )Input;
  RESPONSE Response;

  InitRequest( Request, PING );

  if ( !SendLegacy( Request, &Response ) || Response.Status != STATUS_OK ) {
    return 0;
  }

  Api.FastChecked = 1;

  if ( Response.Result != FAST_CAPABILITY ) {
    return 0;
  }

  if ( Api.OpenBlock( &FastMemoryGuid, WMI_GUID_EXECUTE, &Api.FastBlock ) != ERROR_SUCCESS ) {
    Api.FastBlock = NULL;
    return 0;
  }

  if ( !SendFast( Request, &Response ) || Response.Status != STATUS_OK ||
       Response.Result != FAST_CAPABILITY ) {
    CloseFastTransport();
    return 0;
  }

  Api.FastAvailable = 1;
  Api.Staging = ( uint8_t * )VirtualAlloc(
    NULL, BUFFER_TRANSFER_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );

  if ( Api.Staging != NULL && !VirtualLock( Api.Staging, BUFFER_TRANSFER_SIZE ) ) {
    VirtualFree( Api.Staging, 0, MEM_RELEASE );
    Api.Staging = NULL;
  }

  return 1;
#endif
}

static int FitsFastRequest( const REQUEST *Request ) {
  switch ( Request->Command ) {
  case PING:
  case TRANSLATE_VIRT:
    return Request->DataSize == 0;
  case READ_PHYS:
    return Request->Arg2 <= FAST_INLINE_READ_SIZE;
  case READ_VIRT:
    return Request->Arg3 <= FAST_INLINE_READ_SIZE;
  case WRITE_PHYS:
  case WRITE_VIRT:
    return Request->DataSize <= FAST_INLINE_WRITE_SIZE;
  case READ_VIRT_BUFFER:
  case WRITE_VIRT_BUFFER:
  case READ_PHYS_BUFFER:
  case WRITE_PHYS_BUFFER:
    return Request->DataSize == sizeof( TRANSFER_BUFFER );
  default:
    return 0;
  }
}

static int SendRequest( REQUEST *Request, RESPONSE *Response ) {
  if ( Api.Block == NULL ) {
    return 0;
  }

  if ( FitsFastRequest( Request ) && PrepareFastTransport() ) {
    int Success = SendFast( Request, Response );

    if ( !Success ) {
      // Do not retry an operation which may already have performed a write
      CloseFastTransport();
    }

    return Success;
  }

  return SendLegacy( Request, Response );
}