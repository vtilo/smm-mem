#ifndef COMMON_H
#define COMMON_H

#include <intrin.h>
#include "shared/uefi.h"
#include "shared/protocol.h"

#pragma intrinsic( __movsb )
#pragma intrinsic( __stosb )

static __forceinline VOID memoryCopy( VOID *destination, const VOID *source, UINTN size ) {
  __movsb( ( UINT8 * )destination, ( const UINT8 * )source, size );
}

static __forceinline VOID memoryClear( VOID *buffer, UINTN size ) {
  __stosb( ( UINT8 * )buffer, 0, size );
}

static __forceinline BOOLEAN guidEqual( const EFI_GUID *left, const EFI_GUID *right ) {
  const UINT8 *a = ( const UINT8 * )left;
  const UINT8 *b = ( const UINT8 * )right;

  for ( UINTN i = 0; i < sizeof( EFI_GUID ); i++ ) {
    if ( a[i] != b[i] ) {
      return 0;
    }
  }

  return 1;
}

VOID serialInit( VOID );
VOID logText( const char *text );
VOID logHex( UINT64 value );
VOID logStatus( const char *text, EFI_STATUS status );

extern EFI_GUID configCommGuid;
extern EFI_GUID wmiGuid;
extern EFI_GUID configGuid;
extern EFI_GUID smmBase2Guid;
extern EFI_GUID smmSwDispatchGuid;
extern EFI_GUID smmCommunicationGuid;
extern EFI_GUID acpiTableGuid;
extern EFI_GUID fastWmiGuid;

#endif
