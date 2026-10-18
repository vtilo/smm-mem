#include "internal.h"

typedef struct {
  UINT32 Characteristics;
  UINT32 Timestamp;
  UINT16 MajorVersion;
  UINT16 MinorVersion;
  UINT32 Name;
  UINT32 OrdinalBase;
  UINT32 Functions;
  UINT32 Names;
  UINT32 FunctionTable;
  UINT32 NameTable;
  UINT32 OrdinalTable;
} ExportDirectory;

static int CompareExportName( const char *Left, const char *Right ) {
  while ( *Left != 0 && *Left == *Right ) {
    Left++;
    Right++;
  }

  return ( int )( unsigned char )*Left - ( int )( unsigned char )*Right;
}

static EFI_STATUS ReadExportName( UINT64 Cr3,
  UINT64 Base,
  const ExportDirectory *Directory,
  UINT32 Index,
  char *Name,
  UINTN Size ) {
  UINT32 Rva;

  if ( ReadVirtual32( Cr3, Base + Directory->NameTable + ( UINT64 )Index * 4, &Rva ) !=
       EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  return ReadAscii( Cr3, Base + Rva, Name, Size );
}

static EFI_STATUS ResolveExportIndex(
  UINT64 Cr3, UINT64 Base, const ExportDirectory *Directory, UINT32 Index, UINT64 *Address ) {
  UINT16 Ordinal;
  UINT32 Rva;

  if ( ReadVirtual16( Cr3, Base + Directory->OrdinalTable + ( UINT64 )Index * 2, &Ordinal ) !=
         EFI_SUCCESS ||
       Ordinal >= Directory->Functions ||
       ReadVirtual32( Cr3, Base + Directory->FunctionTable + ( UINT64 )Ordinal * 4, &Rva ) !=
         EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  *Address = Base + Rva;
  return EFI_SUCCESS;
}

EFI_STATUS ResolveExport( UINT64 Cr3, UINT64 Base, const char *Name, UINT64 *Address ) {
  UINT16 Mz;
  UINT32 Pe;
  UINT32 Signature;
  UINT16 Magic;
  UINT64 DirectoryOffset;
  UINT32 ExportRva;
  ExportDirectory Directory;
  UINT32 Low = 0;
  UINT32 High;
  UINT32 Index;

  if ( ReadVirtual16( Cr3, Base, &Mz ) != EFI_SUCCESS || Mz != 0x5A4D ||
       ReadVirtual32( Cr3, Base + 0x3C, &Pe ) != EFI_SUCCESS ||
       ReadVirtual32( Cr3, Base + Pe, &Signature ) != EFI_SUCCESS || Signature != 0x00004550 ||
       ReadVirtual16( Cr3, Base + Pe + 0x18, &Magic ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  DirectoryOffset = Base + Pe + 0x18 + ( Magic == 0x20B ? 0x70 : 0x60 );

  if ( ReadVirtual32( Cr3, DirectoryOffset, &ExportRva ) != EFI_SUCCESS || ExportRva == 0 ||
       CopyVirtual( Cr3, Base + ExportRva, &Directory, sizeof( Directory ), 0 ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  if ( Directory.Names > 65536 ) {
    Directory.Names = 65536;
  }

  High = Directory.Names;

  // Keep a linear fallback for case insensitive requests and unusual export tables

  while ( Low < High ) {
    UINT32 Middle = Low + ( High - Low ) / 2;
    char ExportName[128];
    int Order;

    if ( ReadExportName( Cr3, Base, &Directory, Middle, ExportName, sizeof( ExportName ) ) !=
         EFI_SUCCESS ) {
      break;
    }

    Order = CompareExportName( ExportName, Name );

    if ( Order == 0 ) {
      return ResolveExportIndex( Cr3, Base, &Directory, Middle, Address );
    }

    if ( Order < 0 ) {
      Low = Middle + 1;
    } else {
      High = Middle;
    }
  }

  for ( Index = 0; Index < Directory.Names; Index++ ) {
    char ExportName[128];

    if ( ReadExportName( Cr3, Base, &Directory, Index, ExportName, sizeof( ExportName ) ) !=
           EFI_SUCCESS ||
         !SameName( ExportName, Name ) ) {
      continue;
    }

    return ResolveExportIndex( Cr3, Base, &Directory, Index, Address );
  }

  return EFI_NOT_FOUND;
}