#include "internal.h"

VOID CopyText( char *Out, UINTN OutSize, const char *Text ) {
  UINTN Index;

  if ( OutSize == 0 ) {
    return;
  }

  for ( Index = 0; Index + 1 < OutSize && Text[Index] != 0; Index++ ) {
    Out[Index] = Text[Index];
  }

  Out[Index] = 0;
}

char LowerChar( char Ch ) {
  if ( Ch >= 'A' && Ch <= 'Z' ) {
    return ( char )( Ch + ( 'a' - 'A' ) );
  }

  return Ch;
}

BOOLEAN SameName( const char *A, const char *B ) {
  while ( *A != 0 && *B != 0 ) {
    if ( LowerChar( *A++ ) != LowerChar( *B++ ) ) {
      return 0;
    }
  }

  return *A == 0 && *B == 0;
}

const char *BaseName( const char *Path ) {
  const char *Name = Path;

  while ( *Path != 0 ) {
    if ( *Path == '\\' || *Path == '/' ) {
      Name = Path + 1;
    }

    Path++;
  }

  return Name;
}

BOOLEAN SameBaseName( const char *Path, const char *Name ) {
  return SameName( BaseName( Path ), Name );
}

EFI_STATUS ReadAscii( UINT64 Cr3, UINT64 Va, char *Out, UINTN OutSize ) {
  UINTN Done = 0;

  if ( OutSize == 0 ) {
    return EFI_INVALID_PARAMETER;
  }

  while ( Done + 1 < OutSize ) {
    UINTN Count = OutSize - Done - 1;
    UINTN PageLeft = ( UINTN )( PAGE_SIZE - ( ( Va + Done ) & ( PAGE_SIZE - 1 ) ) );
    UINTN I;

    if ( Count > PageLeft ) {
      Count = PageLeft;
    }

    if ( Count > 64 ) {
      Count = 64;
    }

    if ( CopyVirtual( Cr3, Va + Done, Out + Done, Count, 0 ) != EFI_SUCCESS ) {
      return EFI_NOT_FOUND;
    }

    for ( I = 0; I < Count; I++ ) {
      if ( Out[Done + I] == 0 ) {
        return EFI_SUCCESS;
      }
    }

    Done += Count;
  }

  Out[Done] = 0;
  return EFI_SUCCESS;
}

EFI_STATUS ReadUnicodeName( UINT64 Cr3, UINT64 StringAddress, char *Out, UINTN OutSize ) {
  struct {
    UINT16 Length;
    UINT16 MaximumLength;
    UINT32 Padding;
    UINT64 Buffer;
  } String;
  UINTN Count;
  UINTN Done = 0;

  if ( OutSize == 0 ||
       CopyVirtual( Cr3, StringAddress, &String, sizeof( String ), 0 ) != EFI_SUCCESS ||
       String.Buffer == 0 ) {
    return EFI_NOT_FOUND;
  }

  Count = String.Length / 2;

  if ( Count + 1 > OutSize ) {
    Count = OutSize - 1;
  }

  while ( Done < Count ) {
    UINT16 Chars[32];
    UINTN Chunk = Count - Done;
    UINTN I;

    if ( Chunk > 32 ) {
      Chunk = 32;
    }

    if ( CopyVirtual( Cr3, String.Buffer + Done * 2, Chars, Chunk * 2, 0 ) != EFI_SUCCESS ) {
      return EFI_NOT_FOUND;
    }

    for ( I = 0; I < Chunk; I++ ) {
      Out[Done + I] = ( char )( Chars[I] & 0xFF );
    }

    Done += Chunk;
  }

  Out[Done] = 0;
  return EFI_SUCCESS;
}