#include "internal.h"

EFI_STATUS getProcessCr3( UINT64 eprocess, UINT64 *cr3 ) {
  if ( smm.cr3Offset == 0 ||
       readVirtual64( smm.kernelCr3, eprocess + smm.cr3Offset, cr3 ) != EFI_SUCCESS || *cr3 == 0 ) {
    return EFI_NOT_FOUND;
  }
  *cr3 &= PAGE_MASK;
  return *cr3 != 0 ? EFI_SUCCESS : EFI_NOT_FOUND;
}

EFI_STATUS findPeb( UINT64 eprocess, UINT64 cr3, UINT64 *peb ) {
  UINT32 offset;
  UINT32 listOffsets[] = { 0x10, 0x20, 0x30 };
  UINT32 biases[] = { 0x00, 0x10, 0x20 };
  UINTN index;

  if ( smm.pebOffset != 0 &&
       readVirtual64( smm.kernelCr3, eprocess + smm.pebOffset, peb ) == EFI_SUCCESS &&
       isUserPointer( *peb ) ) {
    return EFI_SUCCESS;
  }
  for ( offset = 0x100; offset < 0x900; offset += 8 ) {
    UINT64 candidate;
    UINT64 ldr;
    if ( readVirtual64( smm.kernelCr3, eprocess + offset, &candidate ) != EFI_SUCCESS ||
         !isUserPointer( candidate ) ||
         readVirtual64( cr3, candidate + 0x18, &ldr ) != EFI_SUCCESS || !isUserPointer( ldr ) ) {
      continue;
    }
    for ( index = 0; index < sizeof( listOffsets ) / sizeof( listOffsets[0] ); index++ ) {
      UINT64 head = ldr + listOffsets[index];
      UINT64 link;
      UINT64 base;
      UINT16 mz;
      if ( readVirtual64( cr3, head, &link ) == EFI_SUCCESS && isUserPointer( link ) &&
           link != head &&
           readVirtual64( cr3, link + 0x30 - biases[index], &base ) == EFI_SUCCESS &&
           isUserPointer( base ) && readVirtual16( cr3, base, &mz ) == EFI_SUCCESS &&
           mz == 0x5A4D ) {
        smm.pebOffset = offset;
        *peb = candidate;
        return EFI_SUCCESS;
      }
    }
  }
  return EFI_NOT_FOUND;
}

EFI_STATUS findImageBase( UINT64 eprocess, UINT64 cr3, UINT64 *base ) {
  UINT64 peb;
  UINT64 ldr;
  UINT32 listOffsets[] = { 0x10, 0x20, 0x30 };
  UINT32 biases[] = { 0x00, 0x10, 0x20 };
  UINTN index;
  if ( findPeb( eprocess, cr3, &peb ) == EFI_SUCCESS &&
       readVirtual64( cr3, peb + 0x18, &ldr ) == EFI_SUCCESS && isUserPointer( ldr ) ) {
    for ( index = 0; index < sizeof( listOffsets ) / sizeof( listOffsets[0] ); index++ ) {
      UINT64 head = ldr + listOffsets[index];
      UINT64 link;
      if ( readVirtual64( cr3, head, &link ) == EFI_SUCCESS && isUserPointer( link ) &&
           link != head && readVirtual64( cr3, link + 0x30 - biases[index], base ) == EFI_SUCCESS &&
           isUserPointer( *base ) ) {
        return EFI_SUCCESS;
      }
    }
  }
  *base = 0;
  return EFI_NOT_FOUND;
}

EFI_STATUS readProcessImagePath( UINT64 eprocess, UINT64 cr3, char *out, UINTN outSize ) {
  UINT64 peb;
  UINT64 params;

  if ( findPeb( eprocess, cr3, &peb ) != EFI_SUCCESS ||
       readVirtual64( cr3, peb + 0x20, &params ) != EFI_SUCCESS || !isUserPointer( params ) ) {
    return EFI_NOT_FOUND;
  }
  return readUnicodeName( cr3, params + 0x60, out, outSize );
}

EFI_STATUS fillProcessInfo( UINT64 eprocess, PROCESS_INFO *info ) {
  UINT64 pid;
  UINT64 cr3;
  char imagePath[260];

  memoryClear( info, sizeof( *info ) );
  if ( readVirtual64( smm.kernelCr3, eprocess + smm.pidOffset, &pid ) != EFI_SUCCESS ||
       getProcessCr3( eprocess, &cr3 ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }
  info->Pid = ( UINT32 )pid;
  info->Eprocess = eprocess;
  info->Cr3 = cr3;
  if ( smm.nameOffset != 0 ) {
    copyVirtual(
      smm.kernelCr3, eprocess + smm.nameOffset, info->Name, sizeof( info->Name ) - 1, 0 );
    info->Name[sizeof( info->Name ) - 1] = 0;
  }
  findImageBase( eprocess, cr3, &info->ImageBase );
  memoryClear( imagePath, sizeof( imagePath ) );
  if ( readProcessImagePath( eprocess, cr3, imagePath, sizeof( imagePath ) ) == EFI_SUCCESS ) {
    copyText( info->Name, sizeof( info->Name ), baseName( imagePath ) );
  }
  return EFI_SUCCESS;
}

static BOOLEAN readProcessHint( const ProcessHint *hint, UINT32 pid, ProcessContext *process ) {
  struct {
    UINT64 pid, next, previous;
  } links;
  UINT64 entry;
  UINT64 neighbor;

  if ( hint->pid != pid || !isKernelPointer( hint->eprocess ) ) {
    return 0;
  }
  entry = hint->eprocess + smm.linksOffset;
  if ( copyVirtual( smm.kernelCr3, hint->eprocess + smm.pidOffset, &links, sizeof( links ), 0 ) !=
         EFI_SUCCESS ||
       ( UINT32 )links.pid != pid || !isKernelPointer( links.next ) ||
       !isKernelPointer( links.previous ) || links.next == entry || links.previous == entry ||
       readVirtual64( smm.kernelCr3, links.next + 8, &neighbor ) != EFI_SUCCESS ||
       neighbor != entry ||
       readVirtual64( smm.kernelCr3, links.previous, &neighbor ) != EFI_SUCCESS ||
       neighbor != entry || getProcessCr3( hint->eprocess, &process->cr3 ) != EFI_SUCCESS ) {
    return 0;
  }

  process->eprocess = hint->eprocess;
  return 1;
}

EFI_STATUS resolveProcess( UINT32 pid, ProcessContext *process ) {
  UINT64 head;
  UINT64 link;
  UINT32 guard;

  memoryClear( process, sizeof( *process ) );
  if ( resolveProcessLayout() != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }
  if ( pid == 4 ) {
    process->eprocess = smm.systemProcess;
    return getProcessCr3( process->eprocess, &process->cr3 );
  }
  for ( UINT32 index = 0; index < PROCESS_HINT_COUNT; index++ ) {
    ProcessHint *hint = &smm.processHints[index];

    if ( hint->pid != pid ) {
      continue;
    }
    if ( readProcessHint( hint, pid, process ) ) {
      return EFI_SUCCESS;
    }

    memoryClear( hint, sizeof( *hint ) );
  }
  head = smm.systemProcess + smm.linksOffset;
  if ( readVirtual64( smm.kernelCr3, head, &link ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  for ( guard = 0; guard < 4096 && isKernelPointer( link ) && link != head; guard++ ) {
    struct {
      UINT64 pid, next;
    } current;
    UINT64 eprocess = link - smm.linksOffset;

    if ( copyVirtual( smm.kernelCr3, eprocess + smm.pidOffset, &current, sizeof( current ), 0 ) !=
         EFI_SUCCESS ) {
      break;
    }
    if ( ( UINT32 )current.pid == pid ) {
      EFI_STATUS status = getProcessCr3( eprocess, &process->cr3 );

      if ( status == EFI_SUCCESS ) {
        UINT32 index = smm.nextProcessHint % PROCESS_HINT_COUNT;
        ProcessHint *hint = &smm.processHints[index];

        process->eprocess = eprocess;
        hint->pid = pid;
        hint->eprocess = eprocess;
        smm.nextProcessHint = ( index + 1 ) % PROCESS_HINT_COUNT;
      }
      return status;
    }
    link = current.next;
  }

  return EFI_NOT_FOUND;
}

EFI_STATUS findProcessPid( UINT32 pid, PROCESS_INFO *info ) {
  ProcessContext process;
  EFI_STATUS status = resolveProcess( pid, &process );

  if ( EFI_ERROR( status ) ) {
    return status;
  }
  return fillProcessInfo( process.eprocess, info );
}

EFI_STATUS findProcessName( const char *name, PROCESS_INFO *info ) {
  UINT64 head;
  UINT64 link;
  UINT32 guard;
  char currentName[NAME_SIZE];
  char imagePath[260];
  UINT64 cr3;

  if ( resolveProcessLayout() != EFI_SUCCESS || smm.nameOffset == 0 ) {
    return EFI_NOT_FOUND;
  }
  memoryClear( currentName, sizeof( currentName ) );
  copyVirtual(
    smm.kernelCr3, smm.systemProcess + smm.nameOffset, currentName, sizeof( currentName ) - 1, 0 );
  if ( sameName( currentName, name ) ) {
    return fillProcessInfo( smm.systemProcess, info );
  }
  head = smm.systemProcess + smm.linksOffset;
  if ( readVirtual64( smm.kernelCr3, head, &link ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }
  for ( guard = 0; guard < 4096 && isKernelPointer( link ) && link != head; guard++ ) {
    UINT64 eprocess = link - smm.linksOffset;
    memoryClear( currentName, sizeof( currentName ) );
    copyVirtual(
      smm.kernelCr3, eprocess + smm.nameOffset, currentName, sizeof( currentName ) - 1, 0 );
    if ( sameName( currentName, name ) ) {
      return fillProcessInfo( eprocess, info );
    }
    if ( getProcessCr3( eprocess, &cr3 ) == EFI_SUCCESS ) {
      memoryClear( imagePath, sizeof( imagePath ) );
      if ( readProcessImagePath( eprocess, cr3, imagePath, sizeof( imagePath ) ) == EFI_SUCCESS &&
           sameBaseName( imagePath, name ) ) {
        return fillProcessInfo( eprocess, info );
      }
    }
    if ( readVirtual64( smm.kernelCr3, eprocess + smm.linksOffset, &link ) != EFI_SUCCESS ) {
      break;
    }
  }
  return EFI_NOT_FOUND;
}
