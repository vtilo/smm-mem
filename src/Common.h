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


static EFI_GUID gConfigCommGuid = {
    0x7cde8c32,
    0x4a4b,
    0x4fa7,
    {0x91, 0x89, 0x2f, 0x35, 0x95, 0x23, 0x11, 0x80}};
static EFI_GUID gWmiGuid = {
    0xa0c9f8de,
    0x0b71,
    0x42a8,
    {0xb9, 0x67, 0xe5, 0x38, 0xea, 0xcb, 0x6f, 0x21}};
static EFI_GUID gConfigGuid = {
    0x3d171a47,
    0x0fa8,
    0x4c86,
    {0x9d, 0x54, 0x54, 0x2d, 0x5c, 0x9d, 0x90, 0x68}};
static EFI_GUID gEfiSmmBase2ProtocolGuid = {
    0xf4ccbfb7,
    0xf6e0,
    0x47fd,
    {0x9d, 0xd4, 0x10, 0xa8, 0xf1, 0x50, 0xc1, 0x91}};
static EFI_GUID gEfiSmmSwDispatch2ProtocolGuid = {
    0x18a3c6dc,
    0x5eea,
    0x48c8,
    {0xa1, 0xc1, 0xb5, 0x33, 0x89, 0xf9, 0x89, 0x99}};
static EFI_GUID gEfiSmmCommunicationProtocolGuid = {
    0xc68ed8e2,
    0x9dc6,
    0x4cbd,
    {0x9d, 0x94, 0xdb, 0x65, 0xac, 0xc5, 0xc3, 0x32}};
static EFI_GUID gEfiAcpiTableProtocolGuid = {
    0xffe06bdd,
    0x6107,
    0x46a6,
    {0x7b, 0xb2, 0x5a, 0x9c, 0x7e, 0xc5, 0x27, 0x5c}};

#endif
