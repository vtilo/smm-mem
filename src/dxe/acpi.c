#include "internal.h"

static UINT32 signature32( char a, char b, char c, char d ) {
  return ( UINT32 )( UINT8 )a | ( ( UINT32 )( UINT8 )b << 8 ) | ( ( UINT32 )( UINT8 )c << 16 ) |
         ( ( UINT32 )( UINT8 )d << 24 );
}

static UINT8 acpiChecksum( const UINT8 *buffer, UINTN size ) {
  UINT8 sum = 0;
  while ( size-- ) {
    sum = ( UINT8 )( sum + *buffer++ );
  }
  return ( UINT8 )( 0 - sum );
}

static UINT8 *emitName( UINT8 *out, const char name[4] ) {
  memoryCopy( out, name, 4 );
  return out + 4;
}

static UINT8 *emitBytes( UINT8 *out, const VOID *data, UINTN size ) {
  memoryCopy( out, data, size );
  return out + size;
}

static UINT8 *emitDwordConst( UINT8 *out, UINT32 value ) {
  *out++ = 0x0C;
  *out++ = ( UINT8 )value;
  *out++ = ( UINT8 )( value >> 8 );
  *out++ = ( UINT8 )( value >> 16 );
  *out++ = ( UINT8 )( value >> 24 );
  return out;
}

static UINT8 *emitString( UINT8 *out, const char *text ) {
  *out++ = 0x0D;
  while ( *text ) {
    *out++ = ( UINT8 )*text++;
  }
  *out++ = 0;
  return out;
}

static UINT8 *reservePkgLen( UINT8 *out ) {
  *out++ = 0;
  *out++ = 0;
  return out;
}

static UINT8 *emitPkgLength( UINT8 *out, UINTN length ) {
  if ( length < 0x40U ) {
    *out++ = ( UINT8 )length;
  } else if ( length < 0x1000U ) {
    *out++ = ( UINT8 )( 0x40U | ( length & 0x0FU ) );
    *out++ = ( UINT8 )( ( length >> 4 ) & 0xFFU );
  } else if ( length < 0x100000U ) {
    *out++ = ( UINT8 )( 0x80U | ( length & 0x0FU ) );
    *out++ = ( UINT8 )( ( length >> 4 ) & 0xFFU );
    *out++ = ( UINT8 )( ( length >> 12 ) & 0xFFU );
  } else {
    *out++ = ( UINT8 )( 0xC0U | ( length & 0x0FU ) );
    *out++ = ( UINT8 )( ( length >> 4 ) & 0xFFU );
    *out++ = ( UINT8 )( ( length >> 12 ) & 0xFFU );
    *out++ = ( UINT8 )( ( length >> 20 ) & 0xFFU );
  }
  return out;
}

static VOID patchPkgLen( UINT8 *pkg, UINTN length ) {
  pkg[0] = ( UINT8 )( 0x40U | ( length & 0x0FU ) );
  pkg[1] = ( UINT8 )( ( length >> 4 ) & 0xFFU );
}

static UINT8 *emitMutex( UINT8 *out ) {
  *out++ = 0x5C;
  *out++ = 0x2F;
  *out++ = 3;
  out = emitName( out, "_SB_" );
  out = emitName( out, "SMMM" );
  return emitName( out, "SMML" );
}

UINTN buildSsdt( UINT8 *buffer, UINTN capacity, BOOLEAN fast ) {
  AcpiHeader *header = ( AcpiHeader * )buffer;
  UINT8 *out = buffer + sizeof( AcpiHeader );
  UINT8 *scopePkg;
  UINT8 *devicePkg;
  UINT8 *reqFieldPkg;
  UINT8 *outFieldPkg;
  UINT8 *methodPkg;
  UINT8 wdg[20];

  if ( capacity < sizeof( dxe.ssdt ) ) {
    return 0;
  }
  memoryClear( buffer, capacity );
  header->Signature = signature32( 'S', 'S', 'D', 'T' );
  header->Revision = 2;
  memoryCopy( header->OemId, "MEMDEV", 6 );
  memoryCopy( header->OemTableId, fast ? "MEMFAST " : "MEMDEV  ", 8 );
  header->OemRevision = 1;
  header->CreatorId = signature32( 'S', 'M', 'M', 'M' );
  header->CreatorRevision = 1;

  *out++ = 0x10;
  scopePkg = out;
  out = reservePkgLen( out );
  *out++ = 0x5C;
  out = emitName( out, "_SB_" );

  *out++ = 0x5B;
  *out++ = 0x82;
  devicePkg = out;
  out = reservePkgLen( out );
  out = emitName( out, fast ? "SMMF" : "SMMM" );

  if ( !fast ) {
    *out++ = 0x5B;
    *out++ = 0x01;
    out = emitName( out, "SMML" );
    *out++ = 0;
  }

  *out++ = 0x08;
  out = emitName( out, "_HID" );
  out = emitString( out, "PNP0C14" );
  *out++ = 0x08;
  out = emitName( out, "_UID" );
  out = emitString( out, fast ? "MemFast" : "Mem" );

  memoryClear( wdg, sizeof( wdg ) );
  memoryCopy( wdg, fast ? &fastWmiGuid : &wmiGuid, sizeof( wmiGuid ) );
  wdg[16] = 'B';
  wdg[17] = 'D';
  wdg[18] = 1;
  wdg[19] = 0x02;
  *out++ = 0x08;
  out = emitName( out, "_WDG" );
  *out++ = 0x11;
  *out++ = 0x17;
  *out++ = 0x0A;
  *out++ = ( UINT8 )sizeof( wdg );
  out = emitBytes( out, wdg, sizeof( wdg ) );

  *out++ = 0x5B;
  *out++ = 0x80;
  out = emitName( out, "SMIR" );
  *out++ = 0x01;
  out = emitDwordConst( out, 0xB2U );
  *out++ = 0x0A;
  *out++ = 0x01;

  *out++ = 0x5B;
  *out++ = 0x81;
  *out++ = 0x0B;
  out = emitName( out, "SMIR" );
  *out++ = 0x01;
  out = emitName( out, "SMIC" );
  *out++ = 0x08;

  *out++ = 0x5B;
  *out++ = 0x80;
  out = emitName( out, "MREQ" );
  *out++ = 0x00;
  out = emitDwordConst( out, ( UINT32 )dxe.mailboxPhysical );
  out = emitDwordConst( out, ( fast ? FAST_REQUEST_SIZE : REQUEST_SIZE ) );

  *out++ = 0x5B;
  *out++ = 0x81;
  reqFieldPkg = out;
  out = reservePkgLen( out );
  out = emitName( out, "MREQ" );
  *out++ = 0x01;
  out = emitName( out, "WMRQ" );
  out = emitPkgLength( out, ( fast ? FAST_REQUEST_SIZE : REQUEST_SIZE ) * 8U );
  patchPkgLen( reqFieldPkg, ( UINTN )( out - reqFieldPkg ) );

  *out++ = 0x5B;
  *out++ = 0x80;
  out = emitName( out, "MOUT" );
  *out++ = 0x00;
  out = emitDwordConst( out, ( UINT32 )( dxe.mailboxPhysical + RESPONSE_OFFSET ) );
  out = emitDwordConst( out, ( fast ? FAST_RESPONSE_SIZE : RESPONSE_SIZE ) );

  *out++ = 0x5B;
  *out++ = 0x81;
  outFieldPkg = out;
  out = reservePkgLen( out );
  out = emitName( out, "MOUT" );
  *out++ = 0x01;
  out = emitName( out, "WOUT" );
  out = emitPkgLength( out, ( fast ? FAST_RESPONSE_SIZE : RESPONSE_SIZE ) * 8U );
  patchPkgLen( outFieldPkg, ( UINTN )( out - outFieldPkg ) );

  *out++ = 0x14;
  methodPkg = out;
  out = reservePkgLen( out );
  out = emitName( out, "WMBD" );
  *out++ = 0x03;
  *out++ = 0x5B;
  *out++ = 0x23;
  out = emitMutex( out );
  *out++ = 0xFF;
  *out++ = 0xFF;

  *out++ = 0x70;
  *out++ = 0x6A;
  out = emitName( out, "WMRQ" );
  *out++ = 0x70;
  *out++ = 0x0A;
  *out++ = ( UINT8 )SW_SMI_VALUE;
  out = emitName( out, "SMIC" );

  *out++ = 0x70;
  out = emitName( out, "WOUT" );
  *out++ = 0x60;
  *out++ = 0x5B;
  *out++ = 0x27;
  out = emitMutex( out );
  *out++ = 0xA4;
  *out++ = 0x60;

  patchPkgLen( methodPkg, ( UINTN )( out - methodPkg ) );
  patchPkgLen( devicePkg, ( UINTN )( out - devicePkg ) );
  patchPkgLen( scopePkg, ( UINTN )( out - scopePkg ) );
  header->Length = ( UINT32 )( out - buffer );
  header->Checksum = acpiChecksum( buffer, header->Length );
  return header->Length;
}
