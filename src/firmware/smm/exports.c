#include "internal.h"

typedef struct {
  UINT32 characteristics;
  UINT32 timestamp;
  UINT16 majorVersion;
  UINT16 minorVersion;
  UINT32 name;
  UINT32 ordinalBase;
  UINT32 functions;
  UINT32 names;
  UINT32 functionTable;
  UINT32 nameTable;
  UINT32 ordinalTable;
} ExportDirectory;

static int compareExportName( const char *left, const char *right ) {
  while ( *left != 0 && *left == *right ) {
    left++;
    right++;
  }

  return ( int )( unsigned char )*left - ( int )( unsigned char )*right;
}

static EFI_STATUS readExportName( UINT64 cr3,
  UINT64 base,
  const ExportDirectory *directory,
  UINT32 index,
  char *name,
  UINTN size ) {
  UINT32 rva;

  if ( readVirtual32( cr3, base + directory->nameTable + ( UINT64 )index * 4, &rva ) !=
       EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  return readAscii( cr3, base + rva, name, size );
}

static EFI_STATUS resolveExportIndex(
  UINT64 cr3, UINT64 base, const ExportDirectory *directory, UINT32 index, UINT64 *address ) {
  UINT16 ordinal;
  UINT32 rva;

  if ( readVirtual16( cr3, base + directory->ordinalTable + ( UINT64 )index * 2, &ordinal ) !=
         EFI_SUCCESS ||
       ordinal >= directory->functions ||
       readVirtual32( cr3, base + directory->functionTable + ( UINT64 )ordinal * 4, &rva ) !=
         EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  *address = base + rva;
  return EFI_SUCCESS;
}

EFI_STATUS resolveExport( UINT64 cr3, UINT64 base, const char *name, UINT64 *address ) {
  UINT16 mz;
  UINT32 pe;
  UINT32 signature;
  UINT16 magic;
  UINT64 directoryOffset;
  UINT32 exportRva;
  ExportDirectory directory;
  UINT32 low = 0;
  UINT32 high;
  UINT32 index;

  if ( readVirtual16( cr3, base, &mz ) != EFI_SUCCESS || mz != 0x5A4D ||
       readVirtual32( cr3, base + 0x3C, &pe ) != EFI_SUCCESS ||
       readVirtual32( cr3, base + pe, &signature ) != EFI_SUCCESS || signature != 0x00004550 ||
       readVirtual16( cr3, base + pe + 0x18, &magic ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  directoryOffset = base + pe + 0x18 + ( magic == 0x20B ? 0x70 : 0x60 );
  if ( readVirtual32( cr3, directoryOffset, &exportRva ) != EFI_SUCCESS || exportRva == 0 ||
       copyVirtual( cr3, base + exportRva, &directory, sizeof( directory ), 0 ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  if ( directory.names > 65536 ) {
    directory.names = 65536;
  }
  high = directory.names;

  // PE export names are normally sorted. Keep the original linear fallback
  // for case-insensitive requests and images with an unusual name table.
  while ( low < high ) {
    UINT32 middle = low + ( high - low ) / 2;
    char exportName[128];
    int order;

    if ( readExportName( cr3, base, &directory, middle, exportName, sizeof( exportName ) ) !=
         EFI_SUCCESS ) {
      break;
    }
    order = compareExportName( exportName, name );
    if ( order == 0 ) {
      return resolveExportIndex( cr3, base, &directory, middle, address );
    }
    if ( order < 0 ) {
      low = middle + 1;
    } else {
      high = middle;
    }
  }

  for ( index = 0; index < directory.names; index++ ) {
    char exportName[128];

    if ( readExportName( cr3, base, &directory, index, exportName, sizeof( exportName ) ) !=
           EFI_SUCCESS ||
         !sameName( exportName, name ) ) {
      continue;
    }

    return resolveExportIndex( cr3, base, &directory, index, address );
  }

  return EFI_NOT_FOUND;
}
