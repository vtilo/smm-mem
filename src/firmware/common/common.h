#ifndef COMMON_H
#define COMMON_H

#include <intrin.h>
#include "uefi.h"
#include "../../protocol.h"

#pragma intrinsic( __movsb )
#pragma intrinsic( __stosb )

static __forceinline VOID MemoryCopy( VOID *Destination, const VOID *Source, UINTN Size ) {
  __movsb( ( UINT8 * )Destination, ( const UINT8 * )Source, Size );
}

static __forceinline VOID MemoryClear( VOID *Buffer, UINTN Size ) {
  __stosb( ( UINT8 * )Buffer, 0, Size );
}

static __forceinline BOOLEAN GuidEqual( const EFI_GUID *Left, const EFI_GUID *Right ) {
  const UINT8 *A = ( const UINT8 * )Left;
  const UINT8 *B = ( const UINT8 * )Right;

  for ( UINTN I = 0; I < sizeof( EFI_GUID ); I++ ) {
    if ( A[I] != B[I] ) {
      return 0;
    }
  }

  return 1;
}

VOID SerialInit( VOID );
VOID LogText( const char *Text );
VOID LogHex( UINT64 Value );
VOID LogStatus( const char *Text, EFI_STATUS Status );

extern EFI_GUID ConfigCommGuid;
extern EFI_GUID WmiGuid;
extern EFI_GUID ConfigGuid;
extern EFI_GUID SmmBase2Guid;
extern EFI_GUID SmmSwDispatchGuid;
extern EFI_GUID SmmCommunicationGuid;
extern EFI_GUID AcpiTableGuid;

#endif