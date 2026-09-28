#include "internal.h"

EFI_STATUS readLdrEntry(
  UINT64 cr3, UINT64 link, UINT32 bias, char *name, UINTN nameSize, UINT64 *base, UINT32 *size ) {
  if ( readVirtual64( cr3, link + 0x30 - bias, base ) != EFI_SUCCESS ||
       readVirtual32( cr3, link + 0x40 - bias, size ) != EFI_SUCCESS || !isUserPointer( *base ) ||
       readUnicodeName( cr3, link + 0x58 - bias, name, nameSize ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }
  return EFI_SUCCESS;
}

EFI_STATUS findUserModule( UINT32 pid, const char *name, MODULE_INFO *module ) {
  ProcessContext process;
  UINT64 peb;
  UINT64 ldr;
  UINT32 listOffsets[] = { 0x10, 0x20, 0x30 };
  UINT32 biases[] = { 0x00, 0x10, 0x20 };
  UINT32 guard;
  UINTN index;

  if ( resolveProcess( pid, &process ) != EFI_SUCCESS ||
       findPeb( process.eprocess, process.cr3, &peb ) != EFI_SUCCESS ||
       readVirtual64( process.cr3, peb + 0x18, &ldr ) != EFI_SUCCESS || !isUserPointer( ldr ) ) {
    return EFI_NOT_FOUND;
  }
  for ( index = 0; index < sizeof( listOffsets ) / sizeof( listOffsets[0] ); index++ ) {
    UINT64 head = ldr + listOffsets[index];
    UINT64 link;
    if ( readVirtual64( process.cr3, head, &link ) != EFI_SUCCESS ) {
      continue;
    }
    for ( guard = 0; guard < 1024 && isUserPointer( link ) && link != head; guard++ ) {
      char moduleName[NAME_SIZE];
      UINT64 base;
      UINT32 size;
      memoryClear( moduleName, sizeof( moduleName ) );
      if ( readLdrEntry(
             process.cr3, link, biases[index], moduleName, sizeof( moduleName ), &base, &size ) ==
             EFI_SUCCESS &&
           ( name[0] == 0 || sameName( moduleName, name ) ) ) {
        memoryClear( module, sizeof( *module ) );
        module->Pid = pid;
        module->Base = base;
        module->Size = size;
        module->Cr3 = process.cr3;
        copyText( module->Name, sizeof( module->Name ), moduleName );
        return EFI_SUCCESS;
      }
      if ( readVirtual64( process.cr3, link, &link ) != EFI_SUCCESS ) {
        break;
      }
    }
  }
  return EFI_NOT_FOUND;
}

EFI_STATUS findKernelModule( const char *name, MODULE_INFO *module ) {
  UINT64 head;
  UINT64 link;
  UINT32 guard;

  if ( initKernel() != EFI_SUCCESS || smm.loadedModuleList == 0 ) {
    return EFI_NOT_FOUND;
  }
  head = smm.loadedModuleList;
  if ( readVirtual64( smm.kernelCr3, head, &link ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }
  for ( guard = 0; guard < 1024 && isKernelPointer( link ) && link != head; guard++ ) {
    char moduleName[NAME_SIZE];
    UINT64 base;
    UINT32 size;
    if ( readVirtual64( smm.kernelCr3, link + 0x30, &base ) == EFI_SUCCESS &&
         readVirtual32( smm.kernelCr3, link + 0x40, &size ) == EFI_SUCCESS &&
         readUnicodeName( smm.kernelCr3, link + 0x58, moduleName, sizeof( moduleName ) ) ==
           EFI_SUCCESS &&
         ( name[0] == 0 || sameName( moduleName, name ) ) ) {
      memoryClear( module, sizeof( *module ) );
      module->Pid = 4;
      module->Base = base;
      module->Size = size;
      module->Cr3 = smm.kernelCr3;
      copyText( module->Name, sizeof( module->Name ), moduleName );
      return EFI_SUCCESS;
    }
    if ( readVirtual64( smm.kernelCr3, link, &link ) != EFI_SUCCESS ) {
      break;
    }
  }
  return EFI_NOT_FOUND;
}
