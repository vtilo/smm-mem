#include "internal.h"

static BOOLEAN HasTerminator( const UINT8 *Name, UINT32 Size ) {
  for ( UINT32 Index = 0; Index < Size; Index++ ) {
    if ( Name[Index] == 0 ) {
      return 1;
    }
  }

  return 0;
}

EFI_STATUS QueryBuffer( REQUEST *Request, UINT64 *Result, UINT32 *Size ) {
  ProcessContext Caller;
  PROCESS_INFO Process = { 0 };
  MODULE_INFO Module = { 0 };
  UINT8 Input[QUERY_INPUT_SIZE];
  VOID *Output = 0;
  UINT32 OutputSize = 0;
  UINT32 RequiredSize;
  EFI_STATUS Status;

  *Result = 0;
  *Size = 0;

  if ( Request->Size > QUERY_INPUT_SIZE ) {
    return EFI_INVALID_PARAMETER;
  }

  switch ( Request->Command ) {
  case FIND_PROCESS_PID:
    if ( Request->Size != 0 ) {
      return EFI_INVALID_PARAMETER;
    }

    Output = &Process;
    OutputSize = sizeof( Process );
    break;
  case FIND_PROCESS_NAME:
    Output = &Process;
    OutputSize = sizeof( Process );
    break;
  case FIND_MODULE:
  case FIND_KERNEL_MODULE:
    Output = &Module;
    OutputSize = sizeof( Module );
    break;
  case FIND_EXPORT:
    if ( Request->Size <= sizeof( Module ) ) {
      return EFI_INVALID_PARAMETER;
    }

    break;
  default:
    return EFI_UNSUPPORTED;
  }

  RequiredSize = Request->Size > OutputSize ? Request->Size : OutputSize;
  Status = ResolveCallerBuffer( Request, RequiredSize, &Caller );

  if ( EFI_ERROR( Status ) ) {
    return Status;
  }

  if ( Request->Size != 0 ) {
    Status = CopyVirtual( Caller.Cr3, Request->Buffer, Input, Request->Size, 0 );

    if ( EFI_ERROR( Status ) ) {
      return Status;
    }
  }

  if ( Request->Command == FIND_EXPORT ) {
    UINT32 NameSize = Request->Size - ( UINT32 )sizeof( Module );

    if ( !HasTerminator( Input + sizeof( Module ), NameSize ) ) {
      return EFI_INVALID_PARAMETER;
    }

    MemoryCopy( &Module, Input, sizeof( Module ) );
    return ResolveExport( Module.Cr3 != 0 ? Module.Cr3 : Smm.KernelCr3,
      Module.Base,
      ( const char * )( Input + sizeof( Module ) ),
      Result );
  }

  if ( Request->Command != FIND_PROCESS_PID && !HasTerminator( Input, Request->Size ) ) {
    return EFI_INVALID_PARAMETER;
  }

  switch ( Request->Command ) {
  case FIND_PROCESS_PID:
    Status = FindProcessPid( Request->Pid, &Process );
    break;
  case FIND_PROCESS_NAME:
    Status = FindProcessName( ( const char * )Input, &Process );
    break;
  case FIND_MODULE:
    Status = FindUserModule( Request->Pid, ( const char * )Input, &Module );
    break;
  default:
    Status = ResolveKernelModule( ( const char * )Input, &Module );
    break;
  }

  if ( EFI_ERROR( Status ) ) {
    return Status;
  }

  Status = CopyVirtual( Caller.Cr3, Request->Buffer, Output, OutputSize, 1 );

  if ( !EFI_ERROR( Status ) ) {
    *Result = Request->Command == FIND_PROCESS_PID || Request->Command == FIND_PROCESS_NAME
                ? Process.Eprocess
                : Module.Base;
    *Size = OutputSize;
  }

  return Status;
}