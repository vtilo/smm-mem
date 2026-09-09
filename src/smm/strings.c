#include "internal.h"

VOID copyText( char *out, UINTN outSize, const char *text ) {
  UINTN index;
  if ( outSize == 0 ) {
    return;
  }
  for ( index = 0; index + 1 < outSize && text[index] != 0; index++ ) {
    out[index] = text[index];
  }
  out[index] = 0;
}

char lowerChar( char ch ) {
  if ( ch >= 'A' && ch <= 'Z' ) {
    return ( char )( ch + ( 'a' - 'A' ) );
  }
  return ch;
}

BOOLEAN sameName( const char *a, const char *b ) {
  while ( *a != 0 && *b != 0 ) {
    if ( lowerChar( *a++ ) != lowerChar( *b++ ) ) {
      return 0;
    }
  }
  return *a == 0 && *b == 0;
}

const char *baseName( const char *path ) {
  const char *name = path;

  while ( *path != 0 ) {
    if ( *path == '\\' || *path == '/' ) {
      name = path + 1;
    }
    path++;
  }
  return name;
}

BOOLEAN sameBaseName( const char *path, const char *name ) {
  return sameName( baseName( path ), name );
}

EFI_STATUS readAscii( UINT64 cr3, UINT64 va, char *out, UINTN outSize ) {
  UINTN done = 0;

  if ( outSize == 0 ) {
    return EFI_INVALID_PARAMETER;
  }

  while ( done + 1 < outSize ) {
    UINTN count = outSize - done - 1;
    UINTN pageLeft = ( UINTN )( PAGE_SIZE - ( ( va + done ) & ( PAGE_SIZE - 1 ) ) );
    UINTN i;

    if ( count > pageLeft ) {
      count = pageLeft;
    }
    if ( count > 64 ) {
      count = 64;
    }
    if ( copyVirtual( cr3, va + done, out + done, count, 0 ) != EFI_SUCCESS ) {
      return EFI_NOT_FOUND;
    }

    for ( i = 0; i < count; i++ ) {
      if ( out[done + i] == 0 ) {
        return EFI_SUCCESS;
      }
    }

    done += count;
  }

  out[done] = 0;
  return EFI_SUCCESS;
}

EFI_STATUS readUnicodeName( UINT64 cr3, UINT64 stringAddress, char *out, UINTN outSize ) {
  struct {
    UINT16 length;
    UINT16 maximumLength;
    UINT32 padding;
    UINT64 buffer;
  } string;
  UINTN count;
  UINTN done = 0;

  if ( outSize == 0 ||
       copyVirtual( cr3, stringAddress, &string, sizeof( string ), 0 ) != EFI_SUCCESS ||
       string.buffer == 0 ) {
    return EFI_NOT_FOUND;
  }

  count = string.length / 2;
  if ( count + 1 > outSize ) {
    count = outSize - 1;
  }

  while ( done < count ) {
    UINT16 chars[32];
    UINTN chunk = count - done;
    UINTN i;

    if ( chunk > 32 ) {
      chunk = 32;
    }
    if ( copyVirtual( cr3, string.buffer + done * 2, chars, chunk * 2, 0 ) != EFI_SUCCESS ) {
      return EFI_NOT_FOUND;
    }

    for ( i = 0; i < chunk; i++ ) {
      out[done + i] = ( char )( chars[i] & 0xFF );
    }
    done += chunk;
  }

  out[done] = 0;
  return EFI_SUCCESS;
}
