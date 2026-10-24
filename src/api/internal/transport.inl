static void CloseTransport( void ) {
  if ( Api.Staging != NULL ) {
    VirtualUnlock( Api.Staging, BUFFER_TRANSFER_SIZE );
    VirtualFree( Api.Staging, 0, MEM_RELEASE );
  }

  if ( Api.CloseBlock != NULL && Api.Block != NULL ) {
    Api.CloseBlock( Api.Block );
  }

  if ( Api.Library != NULL ) {
    FreeLibrary( Api.Library );
  }

  ZeroMemory( &Api, sizeof( Api ) );
  Api.Instance = WMI_INSTANCE_COUNT;
}

static void InitRequest( REQUEST *Request, uint32_t Command ) {
  ZeroMemory( Request, sizeof( *Request ) );
  Request->Magic = REQ_MAGIC;
  Request->Command = Command;
  Request->CallerPid = Api.Pid;
}

static int ExecuteRequest( const wchar_t *Instance, REQUEST *Request, RESPONSE *Response ) {
  RESPONSE Output;
  ULONG OutputSize = sizeof( Output );

  ZeroMemory( &Output, sizeof( Output ) );

  ULONG Status = Api.ExecuteMethod(
    Api.Block, Instance, WMI_METHOD_ID, sizeof( *Request ), Request, &OutputSize, &Output );

  if ( Status != ERROR_SUCCESS ) {
    // Only missing destinations prove that a write was never dispatched
    return Status == ERROR_WMI_GUID_NOT_FOUND || Status == ERROR_WMI_INSTANCE_NOT_FOUND ? -1 : 0;
  }

  uint32_t Capacity = Request->Buffer != 0 ? BUFFER_TRANSFER_SIZE : INLINE_READ_SIZE;

  if ( OutputSize != sizeof( Output ) || Output.Magic != RESP_MAGIC ||
       Output.Command != Request->Command || Output.Sequence != Request->Sequence ||
       Output.Version != PROTOCOL_VERSION || Output.Size > Capacity ) {
    return 0;
  }

  *Response = Output;

  return 1;
}

static int SendRequest( REQUEST *Request, RESPONSE *Response ) {
  if ( Api.Block == NULL || Api.ExecuteMethod == NULL ) {
    return 0;
  }

  Request->Sequence = ++Api.Sequence;

  size_t Previous = Api.Instance;

  if ( Previous < WMI_INSTANCE_COUNT ) {
    int Result = ExecuteRequest( MemoryInstances[Previous], Request, Response );

    if ( Result >= 0 ) {
      return Result;
    }

    Api.Instance = WMI_INSTANCE_COUNT;
  }

  for ( size_t Index = 0; Index < WMI_INSTANCE_COUNT; Index++ ) {
    if ( Index == Previous ) {
      continue;
    }

    int Result = ExecuteRequest( MemoryInstances[Index], Request, Response );

    if ( Result < 0 ) {
      continue;
    }

    if ( Result != 0 ) {
      Api.Instance = Index;
    }

    return Result;
  }

  return 0;
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

  if ( Api.OpenBlock == NULL || Api.ExecuteMethod == NULL || Api.CloseBlock == NULL ||
       Api.OpenBlock( &MemoryGuid, WMI_GUID_EXECUTE, &Api.Block ) != ERROR_SUCCESS ) {
    CloseTransport();

    return 0;
  }

  Api.Staging = ( uint8_t * )VirtualAlloc(
    NULL, BUFFER_TRANSFER_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );

  if ( Api.Staging == NULL ) {
    CloseTransport();

    return 0;
  }

  if ( !VirtualLock( Api.Staging, BUFFER_TRANSFER_SIZE ) ) {
    VirtualFree( Api.Staging, 0, MEM_RELEASE );
    Api.Staging = NULL;
    CloseTransport();

    return 0;
  }

  Api.Pid = GetCurrentProcessId();
  Api.Sequence = GetTickCount64();

  REQUEST Request;
  RESPONSE Response;

  InitRequest( &Request, PING );

  if ( !SendRequest( &Request, &Response ) || Response.Status != STATUS_OK || Response.Size != 0 ||
       Response.Result != PROTOCOL_VERSION ) {
    CloseTransport();

    return 0;
  }

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