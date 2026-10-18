#include "internal.h"

VOID Reply( RESPONSE *Response,
  REQUEST *Request,
  EFI_STATUS Status,
  UINT64 Result,
  const VOID *Data,
  UINT32 DataSize ) {
  UINT32 StatusCode;

  MemoryClear( Response, sizeof( *Response ) );
  Response->Magic = RESP_MAGIC;
  StatusCode = ( UINT32 )Status;
  Response->Status = EFI_ERROR( Status ) ? ( StatusCode != 0 ? StatusCode : 1U ) : 0;
  Response->Command = Request->Command;
  Response->Sequence = Request->Sequence;
  Response->Result = Result;

  if ( DataSize > RESPONSE_DATA_SIZE ) {
    DataSize = RESPONSE_DATA_SIZE;
  }

  Response->DataSize = DataSize;

  if ( Data != 0 && DataSize != 0 ) {
    MemoryCopy( Response->Data, Data, DataSize );
  }
}

EFI_STATUS HandleRequest( REQUEST *Request, RESPONSE *Response ) {
  PROCESS_INFO Process;
  ProcessContext Context;
  MODULE_INFO Module;
  EFI_STATUS Status;
  UINT32 Size;
  UINT64 Pa = 0;
  UINT64 Address = 0;
  UINT8 Scratch[RESPONSE_DATA_SIZE];

  MemoryClear( &Process, sizeof( Process ) );
  MemoryClear( &Module, sizeof( Module ) );
  MemoryClear( Scratch, sizeof( Scratch ) );

  if ( Request->Magic != REQ_MAGIC ) {
    return EFI_NOT_FOUND;
  }

  Size = ( UINT32 )Request->Arg3;

  if ( Request->Command == PING ) {
    Reply( Response,
      Request,
      EFI_SUCCESS,
      ( Smm.Features & FEATURE_BUFFER_TRANSFER ) ? FAST_CAPABILITY : 0,
      0,
      0 );
    return EFI_SUCCESS;
  }

  if ( Request->Command >= READ_VIRT_BUFFER && Request->Command <= WRITE_PHYS_BUFFER ) {
    UINT64 Transferred = 0;

    Status = TransferBuffer( Request, &Transferred );
    Reply( Response, Request, Status, Transferred, 0, 0 );
    return Status;
  }

  if ( Request->Command == READ_PHYS ) {
    Size = ( UINT32 )Request->Arg2;

    if ( Size > RESPONSE_DATA_SIZE ) {
      Reply( Response, Request, EFI_INVALID_PARAMETER, 0, 0, 0 );
      return EFI_INVALID_PARAMETER;
    }

    Status = CopyPhysical( Request->Arg1, Scratch, Size, 0 );
    Reply( Response, Request, Status, Request->Arg1, Scratch, EFI_ERROR( Status ) ? 0 : Size );
    return Status;
  }

  if ( Request->Command == WRITE_PHYS ) {
    Status = Request->DataSize <= RESPONSE_DATA_SIZE
               ? CopyPhysical( Request->Arg1, Request->Data, Request->DataSize, 1 )
               : EFI_INVALID_PARAMETER;
    Reply( Response, Request, Status, Request->Arg1, 0, 0 );
    return Status;
  }

  if ( Request->Command == FIND_PROCESS_PID ) {
    Status = FindProcessPid( ( UINT32 )Request->Arg1, &Process );
    Reply( Response,
      Request,
      Status,
      Process.Eprocess,
      &Process,
      EFI_ERROR( Status ) ? 0 : sizeof( Process ) );

    return Status;
  }

  if ( Request->Command == FIND_PROCESS_NAME ) {
    Status = FindProcessName( ( char * )Request->Data, &Process );
    Reply( Response,
      Request,
      Status,
      Process.Eprocess,
      &Process,
      EFI_ERROR( Status ) ? 0 : sizeof( Process ) );

    return Status;
  }

  if ( Request->Command == TRANSLATE_VIRT ) {
    Status = ResolveProcess( ( UINT32 )Request->Arg1, &Context );

    if ( !EFI_ERROR( Status ) ) {
      Status = TranslateVirtual( Context.Cr3, Request->Arg2, &Pa );
    }

    Reply( Response, Request, Status, EFI_ERROR( Status ) ? 0 : Pa, 0, 0 );
    return Status;
  }

  if ( Request->Command == READ_VIRT ) {
    if ( Size > RESPONSE_DATA_SIZE ) {
      Reply( Response, Request, EFI_INVALID_PARAMETER, 0, 0, 0 );
      return EFI_INVALID_PARAMETER;
    }

    Status = ResolveProcess( ( UINT32 )Request->Arg1, &Context );

    if ( !EFI_ERROR( Status ) ) {
      Status = CopyVirtual( Context.Cr3, Request->Arg2, Scratch, Size, 0 );
    }

    Reply( Response, Request, Status, Request->Arg2, Scratch, EFI_ERROR( Status ) ? 0 : Size );
    return Status;
  }

  if ( Request->Command == WRITE_VIRT ) {
    if ( Request->DataSize > RESPONSE_DATA_SIZE ) {
      Reply( Response, Request, EFI_INVALID_PARAMETER, 0, 0, 0 );
      return EFI_INVALID_PARAMETER;
    }

    Status = ResolveProcess( ( UINT32 )Request->Arg1, &Context );

    if ( !EFI_ERROR( Status ) ) {
      Status = CopyVirtual( Context.Cr3, Request->Arg2, Request->Data, Request->DataSize, 1 );
    }

    Reply( Response, Request, Status, Request->Arg2, 0, 0 );
    return Status;
  }

  if ( Request->Command == FIND_MODULE ) {
    Status = FindUserModule( ( UINT32 )Request->Arg1, ( char * )Request->Data, &Module );
    Reply(
      Response, Request, Status, Module.Base, &Module, EFI_ERROR( Status ) ? 0 : sizeof( Module ) );
    return Status;
  }

  if ( Request->Command == FIND_KERNEL_MODULE ) {
    Status = ResolveKernelModule( ( char * )Request->Data, &Module );
    Reply(
      Response, Request, Status, Module.Base, &Module, EFI_ERROR( Status ) ? 0 : sizeof( Module ) );
    return Status;
  }

  if ( Request->Command == FIND_EXPORT ) {
    if ( Request->DataSize <= sizeof( MODULE_INFO ) ) {
      Reply( Response, Request, EFI_INVALID_PARAMETER, 0, 0, 0 );
      return EFI_INVALID_PARAMETER;
    }

    MemoryCopy( &Module, Request->Data, sizeof( Module ) );
    Address = 0;
    Status = ResolveExport( Module.Cr3 != 0 ? Module.Cr3 : Smm.KernelCr3,
      Module.Base,
      ( char * )( Request->Data + sizeof( Module ) ),
      &Address );
    Reply( Response, Request, Status, Address, 0, 0 );
    return Status;
  }

  Reply( Response, Request, EFI_UNSUPPORTED, 0, 0, 0 );
  return EFI_UNSUPPORTED;
}

EFI_STATUS ProcessRequest( VOID ) {
  REQUEST *Request;
  RESPONSE *Response;

  if ( Smm.MailboxPhysical == 0 || Smm.MailboxSize < MAILBOX_SIZE ) {
    return EFI_NOT_FOUND;
  }

  Request = ( REQUEST * )( UINTN )Smm.MailboxPhysical;
  Response = ( RESPONSE * )( UINTN )( Smm.MailboxPhysical + RESPONSE_OFFSET );
  return HandleRequest( Request, Response );
}