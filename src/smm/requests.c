#include "internal.h"

VOID reply( RESPONSE *response,
  REQUEST *request,
  EFI_STATUS status,
  UINT64 result,
  const VOID *data,
  UINT32 dataSize ) {
  UINT32 statusCode;

  memoryClear( response, sizeof( *response ) );
  response->Magic = RESP_MAGIC;
  statusCode = ( UINT32 )status;
  response->Status = EFI_ERROR( status ) ? ( statusCode != 0 ? statusCode : 1U ) : 0;
  response->Command = request->Command;
  response->Sequence = request->Sequence;
  response->Result = result;
  if ( dataSize > RESPONSE_DATA_SIZE ) {
    dataSize = RESPONSE_DATA_SIZE;
  }
  response->DataSize = dataSize;
  if ( data != 0 && dataSize != 0 ) {
    memoryCopy( response->Data, data, dataSize );
  }
}

EFI_STATUS handleRequest( REQUEST *request, RESPONSE *response ) {
  PROCESS_INFO process;
  ProcessContext context;
  MODULE_INFO module;
  EFI_STATUS status;
  UINT32 size;
  UINT64 pa = 0;
  UINT64 address = 0;
  UINT8 scratch[RESPONSE_DATA_SIZE];

  memoryClear( &process, sizeof( process ) );
  memoryClear( &module, sizeof( module ) );
  memoryClear( scratch, sizeof( scratch ) );
  if ( request->Magic != REQ_MAGIC ) {
    return EFI_NOT_FOUND;
  }
  size = ( UINT32 )request->Arg3;
  if ( request->Command == PING ) {
    reply( response,
      request,
      EFI_SUCCESS,
      ( smm.features & FEATURE_BUFFER_TRANSFER ) ? FAST_CAPABILITY : 0,
      0,
      0 );
    return EFI_SUCCESS;
  }
  if ( request->Command >= READ_VIRT_BUFFER && request->Command <= WRITE_PHYS_BUFFER ) {
    UINT64 transferred = 0;

    status = transferBuffer( request, &transferred );
    reply( response, request, status, transferred, 0, 0 );
    return status;
  }
  if ( request->Command == READ_PHYS ) {
    size = ( UINT32 )request->Arg2;
    if ( size > RESPONSE_DATA_SIZE ) {
      reply( response, request, EFI_INVALID_PARAMETER, 0, 0, 0 );
      return EFI_INVALID_PARAMETER;
    }
    status = copyPhysical( request->Arg1, scratch, size, 0 );
    reply( response, request, status, request->Arg1, scratch, EFI_ERROR( status ) ? 0 : size );
    return status;
  }
  if ( request->Command == WRITE_PHYS ) {
    status = request->DataSize <= RESPONSE_DATA_SIZE
               ? copyPhysical( request->Arg1, request->Data, request->DataSize, 1 )
               : EFI_INVALID_PARAMETER;
    reply( response, request, status, request->Arg1, 0, 0 );
    return status;
  }
  if ( request->Command == FIND_PROCESS_PID ) {
    status = findProcessPid( ( UINT32 )request->Arg1, &process );
    reply( response,
      request,
      status,
      process.Eprocess,
      &process,
      EFI_ERROR( status ) ? 0 : sizeof( process ) );
    return status;
  }
  if ( request->Command == FIND_PROCESS_NAME ) {
    status = findProcessName( ( char * )request->Data, &process );
    reply( response,
      request,
      status,
      process.Eprocess,
      &process,
      EFI_ERROR( status ) ? 0 : sizeof( process ) );
    return status;
  }
  if ( request->Command == TRANSLATE_VIRT ) {
    status = resolveProcess( ( UINT32 )request->Arg1, &context );
    if ( !EFI_ERROR( status ) ) {
      status = translateVirtual( context.cr3, request->Arg2, &pa );
    }
    reply( response, request, status, EFI_ERROR( status ) ? 0 : pa, 0, 0 );
    return status;
  }
  if ( request->Command == READ_VIRT ) {
    if ( size > RESPONSE_DATA_SIZE ) {
      reply( response, request, EFI_INVALID_PARAMETER, 0, 0, 0 );
      return EFI_INVALID_PARAMETER;
    }
    status = resolveProcess( ( UINT32 )request->Arg1, &context );
    if ( !EFI_ERROR( status ) ) {
      status = copyVirtual( context.cr3, request->Arg2, scratch, size, 0 );
    }
    reply( response, request, status, request->Arg2, scratch, EFI_ERROR( status ) ? 0 : size );
    return status;
  }
  if ( request->Command == WRITE_VIRT ) {
    if ( request->DataSize > RESPONSE_DATA_SIZE ) {
      reply( response, request, EFI_INVALID_PARAMETER, 0, 0, 0 );
      return EFI_INVALID_PARAMETER;
    }
    status = resolveProcess( ( UINT32 )request->Arg1, &context );
    if ( !EFI_ERROR( status ) ) {
      status = copyVirtual( context.cr3, request->Arg2, request->Data, request->DataSize, 1 );
    }
    reply( response, request, status, request->Arg2, 0, 0 );
    return status;
  }
  if ( request->Command == FIND_MODULE ) {
    status = findUserModule( ( UINT32 )request->Arg1, ( char * )request->Data, &module );
    reply(
      response, request, status, module.Base, &module, EFI_ERROR( status ) ? 0 : sizeof( module ) );
    return status;
  }
  if ( request->Command == FIND_KERNEL_MODULE ) {
    status = findKernelModule( ( char * )request->Data, &module );
    reply(
      response, request, status, module.Base, &module, EFI_ERROR( status ) ? 0 : sizeof( module ) );
    return status;
  }
  if ( request->Command == FIND_EXPORT ) {
    if ( request->DataSize <= sizeof( MODULE_INFO ) ) {
      reply( response, request, EFI_INVALID_PARAMETER, 0, 0, 0 );
      return EFI_INVALID_PARAMETER;
    }
    memoryCopy( &module, request->Data, sizeof( module ) );
    address = 0;
    status = resolveExport( module.Cr3 != 0 ? module.Cr3 : smm.kernelCr3,
      module.Base,
      ( char * )( request->Data + sizeof( module ) ),
      &address );
    reply( response, request, status, address, 0, 0 );
    return status;
  }
  reply( response, request, EFI_UNSUPPORTED, 0, 0, 0 );
  return EFI_UNSUPPORTED;
}

EFI_STATUS processRequest( VOID ) {
  REQUEST *request;
  RESPONSE *response;

  if ( smm.mailboxPhysical == 0 || smm.mailboxSize < MAILBOX_SIZE ) {
    return EFI_NOT_FOUND;
  }
  request = ( REQUEST * )( UINTN )smm.mailboxPhysical;
  response = ( RESPONSE * )( UINTN )( smm.mailboxPhysical + RESPONSE_OFFSET );
  return handleRequest( request, response );
}
