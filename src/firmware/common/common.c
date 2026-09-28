#include "common.h"

#pragma intrinsic( __inbyte )
#pragma intrinsic( __outbyte )
#pragma function( memcpy )
#pragma function( memset )

VOID *memcpy( VOID *destination, const VOID *source, size_t size ) {
  memoryCopy( destination, source, size );
  return destination;
}

VOID *memset( VOID *destination, int value, size_t size ) {
  __stosb( ( UINT8 * )destination, ( UINT8 )value, size );
  return destination;
}

VOID serialInit( VOID ) {
#if SERIAL
  __outbyte( COM1_PORT + 1, 0 );
  __outbyte( COM1_PORT + 3, 0x80 );
  __outbyte( COM1_PORT, 1 );
  __outbyte( COM1_PORT + 1, 0 );
  __outbyte( COM1_PORT + 3, 3 );
  __outbyte( COM1_PORT + 2, 0xC7 );
  __outbyte( COM1_PORT + 4, 0x0B );
  __outbyte( 0x80, 0 );
#endif
}

static VOID serialWrite( char value ) {
#if SERIAL
  if ( value == '\n' ) {
    serialWrite( '\r' );
  }

  UINTN remaining = 100000;
  while ( !( __inbyte( COM1_PORT + 5 ) & 0x20 ) && remaining-- ) {
  }
  __outbyte( COM1_PORT, ( UINT8 )value );
#else
  ( void )value;
#endif
}

VOID logText( const char *text ) {
  while ( *text ) {
    serialWrite( *text++ );
  }
}

VOID logHex( UINT64 value ) {
  const char *digits = "0123456789ABCDEF";
  for ( int shift = 60; shift >= 0; shift -= 4 ) {
    serialWrite( digits[( value >> shift ) & 15] );
  }
}

VOID logStatus( const char *text, EFI_STATUS status ) {
  logText( text );
  logHex( status );
  logText( "\n" );
}
EFI_GUID configCommGuid = {
  0x7cde8c32, 0x4a4b, 0x4fa7, { 0x91, 0x89, 0x2f, 0x35, 0x95, 0x23, 0x11, 0x80 } };
EFI_GUID wmiGuid = MEMORY_WMI_GUID;
EFI_GUID configGuid = {
  0x3d171a47, 0x0fa8, 0x4c86, { 0x9d, 0x54, 0x54, 0x2d, 0x5c, 0x9d, 0x90, 0x68 } };
EFI_GUID smmBase2Guid = {
  0xf4ccbfb7, 0xf6e0, 0x47fd, { 0x9d, 0xd4, 0x10, 0xa8, 0xf1, 0x50, 0xc1, 0x91 } };
EFI_GUID smmSwDispatchGuid = {
  0x18a3c6dc, 0x5eea, 0x48c8, { 0xa1, 0xc1, 0xb5, 0x33, 0x89, 0xf9, 0x89, 0x99 } };
EFI_GUID smmCommunicationGuid = {
  0xc68ed8e2, 0x9dc6, 0x4cbd, { 0x9d, 0x94, 0xdb, 0x65, 0xac, 0xc5, 0xc3, 0x32 } };
EFI_GUID acpiTableGuid = {
  0xffe06bdd, 0x6107, 0x46a6, { 0x7b, 0xb2, 0x5a, 0x9c, 0x7e, 0xc5, 0x27, 0x5c } };

EFI_GUID fastWmiGuid = FAST_MEMORY_WMI_GUID;
