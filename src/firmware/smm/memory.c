#include "internal.h"
#include "mapping.h"

VOID WritePte( volatile UINT64 *Entry, UINT64 Value ) {
  UINT64 Cr0;

  Cr0 = __readcr0();
  __writecr0( Cr0 & ~CR0_WP );
  *Entry = Value;
  __writecr0( Cr0 );
}

BOOLEAN IsKernelPointer( UINT64 Value ) {
  return Value >= 0xFFFF800000000000ULL;
}

BOOLEAN IsUserPointer( UINT64 Value ) {
  return Value >= 0x10000ULL && Value < 0x0000800000000000ULL;
}

EFI_STATUS InitMapWindow( VOID ) {
  SMM_ALLOCATE_PAGES AllocatePages;
  volatile UINT64 *Pml4;
  EFI_PHYSICAL_ADDRESS Memory;
  EFI_STATUS Status;
  UINT64 Cr3;
  UINT64 Mask;
  UINTN Index;

  if ( Smm.MapReady != 0 ) {
    return EFI_SUCCESS;
  }

  if ( Smm.Table == 0 || Smm.Table->SmmAllocatePages == 0 ) {
    return EFI_UNSUPPORTED;
  }

  Cr3 = __readcr3() & PAGE_MASK;
  Mask = PhysicalMask();
  Pml4 = ( volatile UINT64 * )( UINTN )( Cr3 & Mask );

  for ( Index = 1; Index < 256; Index++ ) {
    if ( ( Pml4[Index] & PTE_PRESENT ) == 0 ) {
      break;
    }
  }

  if ( Index == 256 ) {
    return EFI_OUT_OF_RESOURCES;
  }

  AllocatePages = ( SMM_ALLOCATE_PAGES )Smm.Table->SmmAllocatePages;
  Memory = 0xFFFFFFFFULL;
  Status = AllocatePages( AllocateMaxAddress, EFI_RUNTIME_SERVICES_DATA, 1, &Memory );

  if ( EFI_ERROR( Status ) ) {
    return Status;
  }

  MemoryClear( ( VOID * )( UINTN )Memory, ( UINTN )PAGE_SIZE );
  Smm.MapPdpt = Memory;
  Smm.MapWindowBase = 0;
  Smm.MapWindowBaseValid = 0;
  Smm.MapWindow = ( ( UINT64 )Index ) << 39;
  WritePte( &Pml4[Index], ( Smm.MapPdpt & Mask ) | PAGE_TABLE_FLAGS );
  __writecr3( __readcr3() );
  Smm.MapReady = 1;
  return EFI_SUCCESS;
}

EFI_STATUS MapWindow( UINT64 Address ) {
  EFI_STATUS Status;
  UINT64 Entry;
  UINT64 Base;
  UINT64 Mask;

  Status = InitMapWindow();

  if ( EFI_ERROR( Status ) ) {
    return Status;
  }

  Mask = PhysicalMask();
  Base = Address & PAGE_1GB_MASK & Mask;

  if ( Smm.MapWindowBaseValid != 0 && Smm.MapWindowBase == Base ) {
    return EFI_SUCCESS;
  }

  Entry = Base | PAGE_TABLE_FLAGS | PTE_LARGE;
  WritePte( &( ( volatile UINT64 * )( UINTN )Smm.MapPdpt )[0], Entry );
  __writecr3( __readcr3() );
  Smm.MapWindowBase = Base;
  Smm.MapWindowBaseValid = 1;
  return EFI_SUCCESS;
}

EFI_STATUS CopyPhysical( UINT64 Address, VOID *Buffer, UINTN Size, BOOLEAN Write ) {
  volatile UINT8 *Window;
  UINT8 *Bytes;
  EFI_STATUS Status;
  UINTN Chunk;
  UINTN Index;
  UINT64 Offset;

  Bytes = ( UINT8 * )Buffer;

  while ( Size != 0 ) {
    Offset = Address & ( PAGE_1GB_SIZE - 1ULL );
    Chunk = ( UINTN )( PAGE_1GB_SIZE - Offset );

    if ( Chunk > Size ) {
      Chunk = Size;
    }

    if ( IsIdentityMapped( __readcr3(), __readcr4(), PhysicalMask(), Address, Chunk, Write ) ) {
      Window = ( volatile UINT8 * )( UINTN )Address;
    } else {
      Status = MapWindow( Address );

      if ( EFI_ERROR( Status ) ) {
        return Status;
      }

      Window = ( volatile UINT8 * )( UINTN )( Smm.MapWindow + Offset );
    }

    for ( Index = 0; Index < Chunk; Index++ ) {
      if ( Write ) {
        Window[Index] = Bytes[Index];
      } else {
        Bytes[Index] = Window[Index];
      }
    }

    Address += Chunk;
    Bytes += Chunk;
    Size -= Chunk;
  }

  return EFI_SUCCESS;
}

UINT64 PhysicalMask( VOID ) {
  int Regs[4];
  UINT32 MaxLeaf;
  UINT32 PhysBits;
  UINT32 CBit;

  if ( Smm.PhysicalMask != 0 ) {
    return Smm.PhysicalMask;
  }

  __cpuidex( Regs, 0x80000000, 0 );
  MaxLeaf = ( UINT32 )Regs[0];
  PhysBits = 52;

  if ( MaxLeaf >= 0x80000008U ) {
    __cpuidex( Regs, 0x80000008, 0 );
    PhysBits = ( UINT32 )( Regs[0] & 0xFF );
  }

  if ( PhysBits == 0 || PhysBits > 52 ) {
    PhysBits = 52;
  }

  Smm.PhysicalMask = ( ( 1ULL << PhysBits ) - 1ULL ) & PAGE_MASK;

  if ( MaxLeaf >= 0x8000001FU ) {
    __cpuidex( Regs, 0x8000001F, 0 );

    if ( ( Regs[0] & 1 ) != 0 ) {
      CBit = ( UINT32 )( Regs[1] & 0x3F );

      if ( CBit < 63 ) {
        Smm.PhysicalMask &= ~( 1ULL << CBit );
      }
    }
  }

  return Smm.PhysicalMask;
}

UINT64 PteAddress( UINT64 Entry ) {
  return Entry & PhysicalMask();
}

EFI_STATUS ReadPhysical64( UINT64 Address, UINT64 *Value ) {
  EFI_STATUS Status;

  if ( ( Address & 7U ) != 0 ) {
    return CopyPhysical( Address, Value, sizeof( *Value ), 0 );
  }

  if ( IsIdentityMapped(
         __readcr3(), __readcr4(), PhysicalMask(), Address, sizeof( *Value ), 0 ) ) {
    *Value = *( volatile UINT64 * )( UINTN )Address;
    return EFI_SUCCESS;
  }

  Status = MapWindow( Address );

  if ( EFI_ERROR( Status ) ) {
    return Status;
  }

  *Value =
    *( volatile UINT64 * )( UINTN )( Smm.MapWindow + ( Address & ( PAGE_1GB_SIZE - 1ULL ) ) );
  return EFI_SUCCESS;
}

EFI_STATUS TranslateVirtual( UINT64 Cr3, UINT64 Va, UINT64 *Pa ) {
  UINT64 Entry;
  UINT64 Base;

  // Bits 63:47 must all match for 4 level paging
  if ( ( Va >> 47 ) != 0 && ( Va >> 47 ) != 0x1FFFFULL ) {
    return EFI_INVALID_PARAMETER;
  }

  Base = Cr3 & PhysicalMask();

  if ( ReadPhysical64( Base + ( ( ( Va >> 39 ) & 0x1FF ) * 8 ), &Entry ) != EFI_SUCCESS ||
       ( Entry & PTE_PRESENT ) == 0 ) {
    return EFI_NOT_FOUND;
  }

  Base = PteAddress( Entry );

  if ( ReadPhysical64( Base + ( ( ( Va >> 30 ) & 0x1FF ) * 8 ), &Entry ) != EFI_SUCCESS ||
       ( Entry & PTE_PRESENT ) == 0 ) {
    return EFI_NOT_FOUND;
  }

  if ( ( Entry & PTE_LARGE ) != 0 ) {
    *Pa = ( Entry & PhysicalMask() & 0x000FFFFFC0000000ULL ) + ( Va & 0x3FFFFFFFULL );
    return EFI_SUCCESS;
  }

  Base = PteAddress( Entry );

  if ( ReadPhysical64( Base + ( ( ( Va >> 21 ) & 0x1FF ) * 8 ), &Entry ) != EFI_SUCCESS ||
       ( Entry & PTE_PRESENT ) == 0 ) {
    return EFI_NOT_FOUND;
  }

  if ( ( Entry & PTE_LARGE ) != 0 ) {
    *Pa = ( Entry & PhysicalMask() & 0x000FFFFFFFE00000ULL ) + ( Va & 0x1FFFFFULL );
    return EFI_SUCCESS;
  }

  Base = PteAddress( Entry );

  if ( ReadPhysical64( Base + ( ( ( Va >> 12 ) & 0x1FF ) * 8 ), &Entry ) != EFI_SUCCESS ||
       ( Entry & PTE_PRESENT ) == 0 ) {
    return EFI_NOT_FOUND;
  }

  *Pa = PteAddress( Entry ) + ( Va & 0xFFFULL );
  return EFI_SUCCESS;
}

EFI_STATUS CopyVirtual( UINT64 Cr3, UINT64 Va, VOID *Buffer, UINTN Size, BOOLEAN Write ) {
  UINT8 *Bytes = ( UINT8 * )Buffer;

  while ( Size != 0 ) {
    UINT64 Pa;
    UINTN Chunk = ( UINTN )( PAGE_SIZE - ( Va & ( PAGE_SIZE - 1 ) ) );

    if ( Chunk > Size ) {
      Chunk = Size;
    }

    if ( EFI_ERROR( TranslateVirtual( Cr3, Va, &Pa ) ) ) {
      return EFI_NOT_FOUND;
    }

    if ( EFI_ERROR( CopyPhysical( Pa, Bytes, Chunk, Write ) ) ) {
      return EFI_NOT_FOUND;
    }

    Va += Chunk;
    Bytes += Chunk;
    Size -= Chunk;
  }

  return EFI_SUCCESS;
}

EFI_STATUS ReadVirtual64( UINT64 Cr3, UINT64 Va, UINT64 *Value ) {
  UINT64 Pa;

  if ( ( Va & 7U ) != 0 ) {
    return CopyVirtual( Cr3, Va, Value, sizeof( *Value ), 0 );
  }

  if ( TranslateVirtual( Cr3, Va, &Pa ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  return ReadPhysical64( Pa, Value );
}

EFI_STATUS ReadVirtual32( UINT64 Cr3, UINT64 Va, UINT32 *Value ) {
  return CopyVirtual( Cr3, Va, Value, sizeof( *Value ), 0 );
}

EFI_STATUS ReadVirtual16( UINT64 Cr3, UINT64 Va, UINT16 *Value ) {
  return CopyVirtual( Cr3, Va, Value, sizeof( *Value ), 0 );
}