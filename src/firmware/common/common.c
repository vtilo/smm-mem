#include "common.h"

#pragma intrinsic( __inbyte )
#pragma intrinsic( __outbyte )
#pragma function( memcpy )
#pragma function( memset )

VOID *memcpy( VOID *Destination, const VOID *Source, size_t Size ) {
  MemoryCopy( Destination, Source, Size );

  return Destination;
}

VOID *memset( VOID *Destination, int Value, size_t Size ) {
  __stosb( ( UINT8 * )Destination, ( UINT8 )Value, Size );

  return Destination;
}

VOID SerialInit( VOID ) {
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

static VOID SerialWrite( char Value ) {
#if SERIAL

  if ( Value == '\n' ) {
    SerialWrite( '\r' );
  }

  UINTN Remaining = 100000;

  while ( !( __inbyte( COM1_PORT + 5 ) & 0x20 ) && Remaining-- ) {
  }

  __outbyte( COM1_PORT, ( UINT8 )Value );
#else
  ( void )Value;
#endif
}

VOID LogText( const char *Text ) {
  while ( *Text ) {
    SerialWrite( *Text++ );
  }
}

VOID LogHex( UINT64 Value ) {
  const char *Digits = "0123456789ABCDEF";

  for ( int Shift = 60; Shift >= 0; Shift -= 4 ) {
    SerialWrite( Digits[( Value >> Shift ) & 15] );
  }
}

VOID LogStatus( const char *Text, EFI_STATUS Status ) {
  LogText( Text );
  LogHex( Status );
  LogText( "\n" );
}

EFI_GUID ConfigCommGuid = {
  0x7cde8c32, 0x4a4b, 0x4fa7, { 0x91, 0x89, 0x2f, 0x35, 0x95, 0x23, 0x11, 0x80 } };
EFI_GUID WmiGuid = MEMORY_WMI_GUID;
EFI_GUID ConfigGuid = {
  0x3d171a47, 0x0fa8, 0x4c86, { 0x9d, 0x54, 0x54, 0x2d, 0x5c, 0x9d, 0x90, 0x68 } };
EFI_GUID SmmBase2Guid = {
  0xf4ccbfb7, 0xf6e0, 0x47fd, { 0x9d, 0xd4, 0x10, 0xa8, 0xf1, 0x50, 0xc1, 0x91 } };
EFI_GUID SmmSwDispatchGuid = {
  0x18a3c6dc, 0x5eea, 0x48c8, { 0xa1, 0xc1, 0xb5, 0x33, 0x89, 0xf9, 0x89, 0x99 } };
EFI_GUID SmmCommunicationGuid = {
  0xc68ed8e2, 0x9dc6, 0x4cbd, { 0x9d, 0x94, 0xdb, 0x65, 0xac, 0xc5, 0xc3, 0x32 } };
EFI_GUID AcpiTableGuid = {
  0xffe06bdd, 0x6107, 0x46a6, { 0x7b, 0xb2, 0x5a, 0x9c, 0x7e, 0xc5, 0x27, 0x5c } };

EFI_GUID FastWmiGuid = FAST_MEMORY_WMI_GUID;