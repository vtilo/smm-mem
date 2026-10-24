#include "internal.h"

VOID Reply( RESPONSE *Response,
  REQUEST *Request,
  EFI_STATUS Status,
  UINT64 Result,
  const VOID *Data,
  UINT32 Size ) {
  UINT32 StatusCode = ( UINT32 )Status;

  MemoryClear( Response, sizeof( *Response ) );
  Response->Magic = RESP_MAGIC;
  Response->Status = EFI_ERROR( Status ) ? ( StatusCode != 0 ? StatusCode : 1U ) : STATUS_OK;
  Response->Command = Request->Command;
  Response->Sequence = Request->Sequence;
  Response->Result = Result;
  Response->Version = PROTOCOL_VERSION;
  Response->Size = Size;

  if ( Data != 0 && Size != 0 ) {
    if ( Size > INLINE_READ_SIZE ) {
      Response->Status = 1;
      Response->Size = 0;
      return;
    }

    MemoryCopy( Response->Data, Data, Size );
  }
}

static EFI_STATUS HandleMemory( REQUEST *Request, RESPONSE *Response ) {
  ProcessContext Context;
  BOOLEAN Write = Request->Command == WRITE_PHYS || Request->Command == WRITE_VIRT;
  BOOLEAN VirtualMemory = Request->Command == READ_VIRT || Request->Command == WRITE_VIRT;
  UINT8 Data[INLINE_READ_SIZE] = { 0 };
  UINT64 Transferred = 0;
  EFI_STATUS Status;

  if ( Request->Buffer != 0 ) {
    Status = TransferBuffer( Request, &Transferred );
    Reply( Response, Request, Status, 0, 0, ( UINT32 )Transferred );
    return Status;
  }

  if ( Request->Size == 0 || Request->Size > ( Write ? INLINE_WRITE_SIZE : INLINE_READ_SIZE ) ||
       Request->Address > ~0ULL - ( Request->Size - 1U ) ) {
    Reply( Response, Request, EFI_INVALID_PARAMETER, 0, 0, 0 );
    return EFI_INVALID_PARAMETER;
  }

  VOID *Bytes = Write ? Request->Data : Data;

  if ( VirtualMemory ) {
    Status = ResolveProcess( Request->Pid, &Context );

    if ( !EFI_ERROR( Status ) ) {
      Status = CopyVirtual( Context.Cr3, Request->Address, Bytes, Request->Size, Write );
    }
  } else {
    Status = CopyPhysical( Request->Address, Bytes, Request->Size, Write );
  }

  Reply( Response,
    Request,
    Status,
    0,
    Write || EFI_ERROR( Status ) ? 0 : Data,
    EFI_ERROR( Status ) ? 0 : Request->Size );

  return Status;
}

EFI_STATUS HandleRequest( REQUEST *Request, RESPONSE *Response ) {
  ProcessContext Context;
  EFI_STATUS Status;
  UINT64 Result = 0;
  UINT32 Size = 0;

  if ( Request->Magic != REQ_MAGIC ) {
    MemoryClear( Response, sizeof( *Response ) );
    return EFI_NOT_FOUND;
  }

  if ( Request->Command == PING ) {
    Status = Request->Size == 0 && Request->Buffer == 0 ? EFI_SUCCESS : EFI_INVALID_PARAMETER;
    Reply( Response, Request, Status, PROTOCOL_VERSION, 0, 0 );
    return Status;
  }

  if ( Request->Command == READ_PHYS || Request->Command == WRITE_PHYS ||
       Request->Command == READ_VIRT || Request->Command == WRITE_VIRT ) {
    return HandleMemory( Request, Response );
  }

  if ( Request->Command >= FIND_PROCESS_PID && Request->Command <= FIND_EXPORT ) {
    Status = QueryBuffer( Request, &Result, &Size );
    Reply( Response, Request, Status, Result, 0, Size );
    return Status;
  }

  if ( Request->Command == TRANSLATE_VIRT ) {
    if ( Request->Size != 0 || Request->Buffer != 0 ) {
      Reply( Response, Request, EFI_INVALID_PARAMETER, 0, 0, 0 );
      return EFI_INVALID_PARAMETER;
    }

    Status = ResolveProcess( Request->Pid, &Context );

    if ( !EFI_ERROR( Status ) ) {
      Status = TranslateVirtual( Context.Cr3, Request->Address, &Result );
    }

    Reply( Response, Request, Status, EFI_ERROR( Status ) ? 0 : Result, 0, 0 );
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