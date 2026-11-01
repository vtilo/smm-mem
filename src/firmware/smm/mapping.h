#ifndef SMM_MAPPING_H
#define SMM_MAPPING_H

#include "../common/uefi.h"

// Assumes PT pages are identity mapped in SMM
static __forceinline BOOLEAN IsIdentityMapped(
  UINT64 RootCr3, UINT64 Cr4, UINT64 Mask, UINT64 Address, UINTN Size, BOOLEAN Write ) {
  UINT64 Root = RootCr3 & Mask;
  UINT64 Last;
  UINT64 AllowedLow = ( Cr4 & ( 1ULL << 17 ) ) != 0 ? 0xFFFULL : 0x18ULL;

  if ( Size == 0 ) {
    return 1;
  }

  if ( Root == 0 || Root > 0x00007FFFFFFFF000ULL || Mask == 0 || ( Mask & 0xFFF ) != 0 ||
       ( RootCr3 & ~( Mask | AllowedLow ) ) != 0 || ( Cr4 & ( 1ULL << 5 ) ) == 0 ||
       ( Cr4 & ( ( 1ULL << 12 ) | ( 1ULL << 21 ) | ( 1ULL << 22 ) | ( 1ULL << 24 ) ) ) != 0 ||
       Address > 0x00007FFFFFFFFFFFULL || Size - 1 > 0x00007FFFFFFFFFFFULL - Address ) {
    return 0;
  }

  Last = Address + Size - 1;

  for ( ;; ) {
    UINT64 Table = Root;

    for ( int Shift = 39; Shift >= 12; Shift -= 9 ) {
      UINT64 Entry = ( ( volatile UINT64 * )( UINTN )Table )[( Address >> Shift ) & 0x1FF];
      UINT64 Base = Entry & Mask;

      if ( ( Entry & 1 ) == 0 || ( Write && ( Entry & 2 ) == 0 ) ||
           ( Entry & 0x000FFFFFFFFFF000ULL & ~Mask ) != 0 ) {
        return 0;
      }

      if ( Shift == 12 || ( ( Shift == 30 || Shift == 21 ) && ( Entry & 0x80 ) != 0 ) ) {
        UINT64 OffsetMask = ( 1ULL << Shift ) - 1;

        if ( ( Shift != 12 && ( Base & OffsetMask & ~0x1000ULL ) != 0 ) ||
             ( ( Base & ~OffsetMask ) | ( Address & OffsetMask ) ) != Address ) {
          return 0;
        }

        break;
      }

      if ( Base == 0 || Base > 0x00007FFFFFFFF000ULL || ( Shift == 39 && ( Entry & 0x80 ) != 0 ) ) {
        return 0;
      }

      Table = Base;
    }

    if ( ( Address >> 12 ) == ( Last >> 12 ) ) {
      return 1;
    }

    Address = ( Address & ~0xFFFULL ) + 0x1000;
  }
}

#endif