#include "internal.h"

static UINT32 Signature32( char A, char B, char C, char D ) {
  return ( UINT32 )( UINT8 )A | ( ( UINT32 )( UINT8 )B << 8 ) | ( ( UINT32 )( UINT8 )C << 16 ) |
         ( ( UINT32 )( UINT8 )D << 24 );
}

static UINT8 AcpiChecksum( const UINT8 *Buffer, UINTN Size ) {
  UINT8 Sum = 0;

  while ( Size-- ) {
    Sum = ( UINT8 )( Sum + *Buffer++ );
  }

  return ( UINT8 )( 0 - Sum );
}

static UINT8 *EmitName( UINT8 *Out, const char Name[4] ) {
  MemoryCopy( Out, Name, 4 );

  return Out + 4;
}

static UINT8 *EmitBytes( UINT8 *Out, const VOID *Data, UINTN Size ) {
  MemoryCopy( Out, Data, Size );

  return Out + Size;
}

static UINT8 *EmitDwordConst( UINT8 *Out, UINT32 Value ) {
  *Out++ = 0x0C;
  *Out++ = ( UINT8 )Value;
  *Out++ = ( UINT8 )( Value >> 8 );
  *Out++ = ( UINT8 )( Value >> 16 );
  *Out++ = ( UINT8 )( Value >> 24 );

  return Out;
}

static UINT8 *EmitString( UINT8 *Out, const char *Text ) {
  *Out++ = 0x0D;

  while ( *Text ) {
    *Out++ = ( UINT8 )*Text++;
  }

  *Out++ = 0;

  return Out;
}

static UINT8 *ReservePkgLen( UINT8 *Out ) {
  *Out++ = 0;
  *Out++ = 0;

  return Out;
}

static UINT8 *EmitPkgLength( UINT8 *Out, UINTN Length ) {
  if ( Length < 0x40U ) {
    *Out++ = ( UINT8 )Length;
  } else if ( Length < 0x1000U ) {
    *Out++ = ( UINT8 )( 0x40U | ( Length & 0x0FU ) );
    *Out++ = ( UINT8 )( ( Length >> 4 ) & 0xFFU );
  } else if ( Length < 0x100000U ) {
    *Out++ = ( UINT8 )( 0x80U | ( Length & 0x0FU ) );
    *Out++ = ( UINT8 )( ( Length >> 4 ) & 0xFFU );
    *Out++ = ( UINT8 )( ( Length >> 12 ) & 0xFFU );
  } else {
    *Out++ = ( UINT8 )( 0xC0U | ( Length & 0x0FU ) );
    *Out++ = ( UINT8 )( ( Length >> 4 ) & 0xFFU );
    *Out++ = ( UINT8 )( ( Length >> 12 ) & 0xFFU );
    *Out++ = ( UINT8 )( ( Length >> 20 ) & 0xFFU );
  }

  return Out;
}

static VOID PatchPkgLen( UINT8 *Pkg, UINTN Length ) {
  Pkg[0] = ( UINT8 )( 0x40U | ( Length & 0x0FU ) );
  Pkg[1] = ( UINT8 )( ( Length >> 4 ) & 0xFFU );
}

static UINT8 *EmitMutex( UINT8 *Out ) {
  *Out++ = 0x5C;
  *Out++ = 0x2F;
  *Out++ = 3;
  Out = EmitName( Out, "_SB_" );
  Out = EmitName( Out, "SMMM" );

  return EmitName( Out, "SMML" );
}

UINTN BuildSsdt( UINT8 *Buffer, UINTN Capacity ) {
  AcpiHeader *Header = ( AcpiHeader * )Buffer;
  UINT8 *Out = Buffer + sizeof( AcpiHeader );
  UINT8 *ScopePkg;
  UINT8 *DevicePkg;
  UINT8 *ReqFieldPkg;
  UINT8 *OutFieldPkg;
  UINT8 *MethodPkg;
  UINT8 Wdg[20];

  if ( Capacity < sizeof( Dxe.Ssdt ) ) {
    return 0;
  }

  MemoryClear( Buffer, Capacity );
  Header->Signature = Signature32( 'S', 'S', 'D', 'T' );
  Header->Revision = 2;
  MemoryCopy( Header->OemId, "MEMDEV", 6 );
  MemoryCopy( Header->OemTableId, "MEMDEV  ", 8 );
  Header->OemRevision = 1;
  Header->CreatorId = Signature32( 'S', 'M', 'M', 'M' );
  Header->CreatorRevision = 1;

  *Out++ = 0x10;
  ScopePkg = Out;
  Out = ReservePkgLen( Out );
  *Out++ = 0x5C;
  Out = EmitName( Out, "_SB_" );

  *Out++ = 0x5B;
  *Out++ = 0x82;
  DevicePkg = Out;
  Out = ReservePkgLen( Out );
  Out = EmitName( Out, "SMMM" );

  *Out++ = 0x5B;
  *Out++ = 0x01;
  Out = EmitName( Out, "SMML" );
  *Out++ = 0;

  *Out++ = 0x08;
  Out = EmitName( Out, "_HID" );
  Out = EmitString( Out, "PNP0C14" );
  *Out++ = 0x08;
  Out = EmitName( Out, "_UID" );
  Out = EmitString( Out, "Mem" );

  MemoryClear( Wdg, sizeof( Wdg ) );
  MemoryCopy( Wdg, &WmiGuid, sizeof( WmiGuid ) );
  Wdg[16] = 'B';
  Wdg[17] = 'D';
  Wdg[18] = 1;
  Wdg[19] = 0x02;
  *Out++ = 0x08;
  Out = EmitName( Out, "_WDG" );
  *Out++ = 0x11;
  *Out++ = 0x17;
  *Out++ = 0x0A;
  *Out++ = ( UINT8 )sizeof( Wdg );
  Out = EmitBytes( Out, Wdg, sizeof( Wdg ) );

  *Out++ = 0x5B;
  *Out++ = 0x80;
  Out = EmitName( Out, "SMIR" );
  *Out++ = 0x01;
  Out = EmitDwordConst( Out, 0xB2U );
  *Out++ = 0x0A;
  *Out++ = 0x01;

  *Out++ = 0x5B;
  *Out++ = 0x81;
  *Out++ = 0x0B;
  Out = EmitName( Out, "SMIR" );
  *Out++ = 0x01;
  Out = EmitName( Out, "SMIC" );
  *Out++ = 0x08;

  *Out++ = 0x5B;
  *Out++ = 0x80;
  Out = EmitName( Out, "MREQ" );
  *Out++ = 0x00;
  Out = EmitDwordConst( Out, ( UINT32 )Dxe.MailboxPhysical );
  Out = EmitDwordConst( Out, REQUEST_SIZE );

  *Out++ = 0x5B;
  *Out++ = 0x81;
  ReqFieldPkg = Out;
  Out = ReservePkgLen( Out );
  Out = EmitName( Out, "MREQ" );
  *Out++ = 0x01;
  Out = EmitName( Out, "WMRQ" );
  Out = EmitPkgLength( Out, REQUEST_SIZE * 8U );
  PatchPkgLen( ReqFieldPkg, ( UINTN )( Out - ReqFieldPkg ) );

  *Out++ = 0x5B;
  *Out++ = 0x80;
  Out = EmitName( Out, "MOUT" );
  *Out++ = 0x00;
  Out = EmitDwordConst( Out, ( UINT32 )( Dxe.MailboxPhysical + RESPONSE_OFFSET ) );
  Out = EmitDwordConst( Out, RESPONSE_SIZE );

  *Out++ = 0x5B;
  *Out++ = 0x81;
  OutFieldPkg = Out;
  Out = ReservePkgLen( Out );
  Out = EmitName( Out, "MOUT" );
  *Out++ = 0x01;
  Out = EmitName( Out, "WOUT" );
  Out = EmitPkgLength( Out, RESPONSE_SIZE * 8U );
  PatchPkgLen( OutFieldPkg, ( UINTN )( Out - OutFieldPkg ) );

  *Out++ = 0x14;
  MethodPkg = Out;
  Out = ReservePkgLen( Out );
  Out = EmitName( Out, "WMBD" );
  *Out++ = 0x03;
  *Out++ = 0x5B;
  *Out++ = 0x23;
  Out = EmitMutex( Out );
  *Out++ = 0xFF;
  *Out++ = 0xFF;

  *Out++ = 0x70;
  *Out++ = 0x6A;
  Out = EmitName( Out, "WMRQ" );
  *Out++ = 0x70;
  *Out++ = 0x0A;
  *Out++ = ( UINT8 )SW_SMI_VALUE;
  Out = EmitName( Out, "SMIC" );

  *Out++ = 0x70;
  Out = EmitName( Out, "WOUT" );
  *Out++ = 0x60;
  *Out++ = 0x5B;
  *Out++ = 0x27;
  Out = EmitMutex( Out );
  *Out++ = 0xA4;
  *Out++ = 0x60;

  PatchPkgLen( MethodPkg, ( UINTN )( Out - MethodPkg ) );
  PatchPkgLen( DevicePkg, ( UINTN )( Out - DevicePkg ) );
  PatchPkgLen( ScopePkg, ( UINTN )( Out - ScopePkg ) );
  Header->Length = ( UINT32 )( Out - Buffer );
  Header->Checksum = AcpiChecksum( Buffer, Header->Length );

  return Header->Length;
}