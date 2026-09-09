#include "internal.h"

VOID writePte( volatile UINT64 *entry, UINT64 value ) {
  UINT64 cr0;

  cr0 = __readcr0();
  __writecr0( cr0 & ~CR0_WP );
  *entry = value;
  __writecr0( cr0 );
}

BOOLEAN isKernelPointer( UINT64 value ) {
  return value >= 0xFFFF800000000000ULL;
}

BOOLEAN isUserPointer( UINT64 value ) {
  return value >= 0x10000ULL && value < 0x0000800000000000ULL;
}

EFI_STATUS initMapWindow( VOID ) {
  SMM_ALLOCATE_PAGES allocatePages;
  volatile UINT64 *pml4;
  EFI_PHYSICAL_ADDRESS memory;
  EFI_STATUS status;
  UINT64 cr3;
  UINT64 mask;
  UINTN index;

  if ( smm.mapReady != 0 ) {
    return EFI_SUCCESS;
  }
  if ( smm.table == 0 || smm.table->SmmAllocatePages == 0 ) {
    return EFI_UNSUPPORTED;
  }
  cr3 = __readcr3() & PAGE_MASK;
  mask = physicalMask();
  pml4 = ( volatile UINT64 * )( UINTN )( cr3 & mask );
  for ( index = 1; index < 256; index++ ) {
    if ( ( pml4[index] & PTE_PRESENT ) == 0 ) {
      break;
    }
  }
  if ( index == 256 ) {
    return EFI_OUT_OF_RESOURCES;
  }
  allocatePages = ( SMM_ALLOCATE_PAGES )smm.table->SmmAllocatePages;
  memory = 0xFFFFFFFFULL;
  status = allocatePages( AllocateMaxAddress, EFI_RUNTIME_SERVICES_DATA, 1, &memory );
  if ( EFI_ERROR( status ) ) {
    return status;
  }
  memoryClear( ( VOID * )( UINTN )memory, ( UINTN )PAGE_SIZE );
  smm.mapPdpt = memory;
  smm.mapWindowBase = 0;
  smm.mapWindowBaseValid = 0;
  smm.mapWindow = ( ( UINT64 )index ) << 39;
  writePte( &pml4[index], ( smm.mapPdpt & mask ) | PAGE_TABLE_FLAGS );
  __writecr3( __readcr3() );
  smm.mapReady = 1;
  return EFI_SUCCESS;
}

EFI_STATUS mapWindow( UINT64 address ) {
  EFI_STATUS status;
  UINT64 entry;
  UINT64 base;
  UINT64 mask;

  status = initMapWindow();
  if ( EFI_ERROR( status ) ) {
    return status;
  }
  mask = physicalMask();
  base = address & PAGE_1GB_MASK & mask;
  if ( smm.mapWindowBaseValid != 0 && smm.mapWindowBase == base ) {
    return EFI_SUCCESS;
  }
  entry = base | PAGE_TABLE_FLAGS | PTE_LARGE;
  writePte( &( ( volatile UINT64 * )( UINTN )smm.mapPdpt )[0], entry );
  __writecr3( __readcr3() );
  smm.mapWindowBase = base;
  smm.mapWindowBaseValid = 1;
  return EFI_SUCCESS;
}

EFI_STATUS copyPhysical( UINT64 address, VOID *buffer, UINTN size, BOOLEAN write ) {
  volatile UINT8 *window;
  UINT8 *bytes;
  EFI_STATUS status;
  UINTN chunk;
  UINTN index;
  UINT64 offset;

  bytes = ( UINT8 * )buffer;
  while ( size != 0 ) {
    offset = address & ( PAGE_1GB_SIZE - 1ULL );
    chunk = ( UINTN )( PAGE_1GB_SIZE - offset );
    if ( chunk > size ) {
      chunk = size;
    }
    status = mapWindow( address );
    if ( EFI_ERROR( status ) ) {
      return status;
    }
    window = ( volatile UINT8 * )( UINTN )( smm.mapWindow + offset );
    for ( index = 0; index < chunk; index++ ) {
      if ( write ) {
        window[index] = bytes[index];
      } else {
        bytes[index] = window[index];
      }
    }
    address += chunk;
    bytes += chunk;
    size -= chunk;
  }
  return EFI_SUCCESS;
}

UINT64 physicalMask( VOID ) {
  int regs[4];
  UINT32 maxLeaf;
  UINT32 physBits;
  UINT32 cBit;

  if ( smm.physicalMask != 0 ) {
    return smm.physicalMask;
  }
  __cpuidex( regs, 0x80000000, 0 );
  maxLeaf = ( UINT32 )regs[0];
  physBits = 52;
  if ( maxLeaf >= 0x80000008U ) {
    __cpuidex( regs, 0x80000008, 0 );
    physBits = ( UINT32 )( regs[0] & 0xFF );
  }
  if ( physBits == 0 || physBits > 52 ) {
    physBits = 52;
  }
  smm.physicalMask = ( ( 1ULL << physBits ) - 1ULL ) & PAGE_MASK;
  if ( maxLeaf >= 0x8000001FU ) {
    __cpuidex( regs, 0x8000001F, 0 );
    if ( ( regs[0] & 1 ) != 0 ) {
      cBit = ( UINT32 )( regs[1] & 0x3F );
      if ( cBit < 63 ) {
        smm.physicalMask &= ~( 1ULL << cBit );
      }
    }
  }
  return smm.physicalMask;
}

UINT64 pteAddress( UINT64 entry ) {
  return entry & physicalMask();
}

EFI_STATUS readPhysical64( UINT64 address, UINT64 *value ) {
  EFI_STATUS status;

  if ( ( address & 7U ) != 0 ) {
    return copyPhysical( address, value, sizeof( *value ), 0 );
  }

  status = mapWindow( address );
  if ( EFI_ERROR( status ) ) {
    return status;
  }

  *value =
    *( volatile UINT64 * )( UINTN )( smm.mapWindow + ( address & ( PAGE_1GB_SIZE - 1ULL ) ) );
  return EFI_SUCCESS;
}

EFI_STATUS translateVirtual( UINT64 cr3, UINT64 va, UINT64 *pa ) {
  UINT64 entry;
  UINT64 base = cr3 & physicalMask();

  if ( readPhysical64( base + ( ( ( va >> 39 ) & 0x1FF ) * 8 ), &entry ) != EFI_SUCCESS ||
       ( entry & PTE_PRESENT ) == 0 ) {
    return EFI_NOT_FOUND;
  }
  base = pteAddress( entry );
  if ( readPhysical64( base + ( ( ( va >> 30 ) & 0x1FF ) * 8 ), &entry ) != EFI_SUCCESS ||
       ( entry & PTE_PRESENT ) == 0 ) {
    return EFI_NOT_FOUND;
  }
  if ( ( entry & PTE_LARGE ) != 0 ) {
    *pa = ( entry & physicalMask() & 0x000FFFFFC0000000ULL ) + ( va & 0x3FFFFFFFULL );
    return EFI_SUCCESS;
  }
  base = pteAddress( entry );
  if ( readPhysical64( base + ( ( ( va >> 21 ) & 0x1FF ) * 8 ), &entry ) != EFI_SUCCESS ||
       ( entry & PTE_PRESENT ) == 0 ) {
    return EFI_NOT_FOUND;
  }
  if ( ( entry & PTE_LARGE ) != 0 ) {
    *pa = ( entry & physicalMask() & 0x000FFFFFFFE00000ULL ) + ( va & 0x1FFFFFULL );
    return EFI_SUCCESS;
  }
  base = pteAddress( entry );
  if ( readPhysical64( base + ( ( ( va >> 12 ) & 0x1FF ) * 8 ), &entry ) != EFI_SUCCESS ||
       ( entry & PTE_PRESENT ) == 0 ) {
    return EFI_NOT_FOUND;
  }
  *pa = pteAddress( entry ) + ( va & 0xFFFULL );
  return EFI_SUCCESS;
}

EFI_STATUS copyVirtual( UINT64 cr3, UINT64 va, VOID *buffer, UINTN size, BOOLEAN write ) {
  UINT8 *bytes = ( UINT8 * )buffer;
  while ( size != 0 ) {
    UINT64 pa;
    UINTN chunk = ( UINTN )( PAGE_SIZE - ( va & ( PAGE_SIZE - 1 ) ) );
    if ( chunk > size ) {
      chunk = size;
    }
    if ( EFI_ERROR( translateVirtual( cr3, va, &pa ) ) ) {
      return EFI_NOT_FOUND;
    }
    if ( EFI_ERROR( copyPhysical( pa, bytes, chunk, write ) ) ) {
      return EFI_NOT_FOUND;
    }
    va += chunk;
    bytes += chunk;
    size -= chunk;
  }
  return EFI_SUCCESS;
}

EFI_STATUS readVirtual64( UINT64 cr3, UINT64 va, UINT64 *value ) {
  UINT64 pa;

  if ( ( va & 7U ) != 0 ) {
    return copyVirtual( cr3, va, value, sizeof( *value ), 0 );
  }

  if ( translateVirtual( cr3, va, &pa ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  return readPhysical64( pa, value );
}

EFI_STATUS readVirtual32( UINT64 cr3, UINT64 va, UINT32 *value ) {
  return copyVirtual( cr3, va, value, sizeof( *value ), 0 );
}

EFI_STATUS readVirtual16( UINT64 cr3, UINT64 va, UINT16 *value ) {
  return copyVirtual( cr3, va, value, sizeof( *value ), 0 );
}
