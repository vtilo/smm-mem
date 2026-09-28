#include "internal.h"

static EFI_GUID smmCpuProtocolGuid = {
  0xeb346b97, 0x975f, 0x4a9f, { 0x8b, 0x22, 0xf8, 0xe9, 0x2b, 0xb3, 0xd5, 0x69 } };

VOID locateSmmCpu( VOID ) {
  EFI_LOCATE_PROTOCOL locate;

  if ( smm.cpu == 0 ) {
    locate = ( EFI_LOCATE_PROTOCOL )smm.table->SmmLocateProtocol;
    locate( &smmCpuProtocolGuid, 0, ( VOID ** )&smm.cpu );
  }
}

EFI_STATUS readSavedCr3( UINTN cpuIndex, UINT64 *cr3 ) {
  locateSmmCpu();
  if ( smm.cpu != 0 && smm.cpu->ReadSaveState != 0 &&
       smm.cpu->ReadSaveState( smm.cpu, sizeof( *cr3 ), SAVE_STATE_CR3, cpuIndex, cr3 ) ==
         EFI_SUCCESS &&
       *cr3 != 0 ) {
    *cr3 &= PAGE_MASK;
    return EFI_SUCCESS;
  }
  return EFI_NOT_FOUND;
}

BOOLEAN looksLikeProcessList( UINT64 eprocess, UINT32 pidOffset ) {
  UINT64 flink;
  UINT64 blink;
  UINT64 back;

  if ( readVirtual64( smm.kernelCr3, eprocess + pidOffset + 8, &flink ) != EFI_SUCCESS ||
       readVirtual64( smm.kernelCr3, eprocess + pidOffset + 16, &blink ) != EFI_SUCCESS ||
       !isKernelPointer( flink ) || !isKernelPointer( blink ) ||
       readVirtual64( smm.kernelCr3, flink + 8, &back ) != EFI_SUCCESS ) {
    return 0;
  }
  return back == eprocess + pidOffset + 8;
}

EFI_STATUS resolveListLayout( VOID ) {
  UINT32 pidOffset;

  if ( !isKernelPointer( smm.systemProcess ) ) {
    return EFI_NOT_FOUND;
  }
  for ( pidOffset = 0x20; pidOffset < 0x900; pidOffset += 8 ) {
    UINT64 pid;
    if ( readVirtual64( smm.kernelCr3, smm.systemProcess + pidOffset, &pid ) == EFI_SUCCESS &&
         ( UINT32 )pid == 4 && looksLikeProcessList( smm.systemProcess, pidOffset ) ) {
      smm.pidOffset = pidOffset;
      smm.linksOffset = pidOffset + 8;
      return EFI_SUCCESS;
    }
  }
  return EFI_NOT_FOUND;
}

EFI_STATUS resolveNameOffset( VOID ) {
  UINT32 offset;

  if ( smm.nameOffset != 0 ) {
    return EFI_SUCCESS;
  }
  for ( offset = 0x100; offset < 0x900; offset++ ) {
    char name[NAME_SIZE];
    memoryClear( name, sizeof( name ) );
    if ( copyVirtual( smm.kernelCr3, smm.systemProcess + offset, name, sizeof( name ) - 1, 0 ) ==
           EFI_SUCCESS &&
         sameName( name, "System" ) ) {
      smm.nameOffset = offset;
      return EFI_SUCCESS;
    }
  }
  return EFI_NOT_FOUND;
}

EFI_STATUS resolveCr3Offset( VOID ) {
  UINT32 offset = smm.cr3Offset;

  if ( offset != 0 ) {
    UINT64 cr3;
    UINT16 mz;

    if ( readVirtual64( smm.kernelCr3, smm.systemProcess + offset, &cr3 ) == EFI_SUCCESS &&
         cr3 != 0 && ( cr3 & ( PAGE_SIZE - 1 ) ) == 0 &&
         readVirtual16( cr3, smm.kernelBase, &mz ) == EFI_SUCCESS && mz == 0x5A4D ) {
      smm.kernelCr3 = cr3;
      smm.kernelStable = 1;
      return EFI_SUCCESS;
    }

    smm.cr3Offset = 0;
  }

  for ( offset = 0x20; offset < 0x100; offset += 8 ) {
    UINT64 cr3;
    UINT16 mz;

    if ( readVirtual64( smm.kernelCr3, smm.systemProcess + offset, &cr3 ) != EFI_SUCCESS ||
         cr3 == 0 || ( cr3 & ( PAGE_SIZE - 1 ) ) != 0 ) {
      continue;
    }

    if ( readVirtual16( cr3, smm.kernelBase, &mz ) == EFI_SUCCESS && mz == 0x5A4D ) {
      smm.cr3Offset = offset;
      smm.kernelCr3 = cr3;
      smm.kernelStable = 1;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

EFI_STATUS tryKernelCr3( UINT64 cr3, UINT64 lstar ) {
  UINT64 start;
  UINT64 limit;
  UINT64 base;
  UINT64 export;

  smm.kernelStable = 0;
  smm.kernelCr3 = cr3 & PAGE_MASK;
  start = lstar & PAGE_2MB_MASK;
  limit = start > 0x10000000ULL ? start - 0x10000000ULL : 0;
  for ( base = start; base > limit; base -= PAGE_2MB_SIZE ) {
    UINT16 mz;
    if ( readVirtual16( smm.kernelCr3, base, &mz ) != EFI_SUCCESS || mz != 0x5A4D ) {
      continue;
    }
    if ( resolveExport( smm.kernelCr3, base, "PsInitialSystemProcess", &export ) == EFI_SUCCESS ) {
      smm.kernelBase = base;
      if ( readVirtual64( smm.kernelCr3, export, &smm.systemProcess ) != EFI_SUCCESS ) {
        return EFI_NOT_FOUND;
      }
      resolveExport( smm.kernelCr3, base, "PsLoadedModuleList", &smm.loadedModuleList );
      ( void )resolveCr3Offset();
      return EFI_SUCCESS;
    }
  }
  return EFI_NOT_FOUND;
}

EFI_STATUS initKernel( VOID ) {
  UINT64 lstar;
  UINT64 cr3;
  UINTN cpu;

  if ( smm.kernelBase != 0 && smm.systemProcess != 0 ) {
    if ( smm.kernelStable || resolveCr3Offset() == EFI_SUCCESS ) {
      return EFI_SUCCESS;
    }
  }
  lstar = __readmsr( MSR_LSTAR );
  for ( cpu = 0; cpu < smm.table->NumberOfCpus; cpu++ ) {
    if ( readSavedCr3( cpu, &cr3 ) == EFI_SUCCESS && cr3 < 0x100000000ULL &&
         tryKernelCr3( cr3, lstar ) == EFI_SUCCESS ) {
      return EFI_SUCCESS;
    }
  }
  for ( cpu = 0; cpu < smm.table->NumberOfCpus; cpu++ ) {
    if ( readSavedCr3( cpu, &cr3 ) == EFI_SUCCESS && tryKernelCr3( cr3, lstar ) == EFI_SUCCESS ) {
      return EFI_SUCCESS;
    }
  }
  cr3 = __readcr3() & PAGE_MASK;
  return tryKernelCr3( cr3, lstar );
}

EFI_STATUS resolveProcessLayout( VOID ) {
  if ( smm.pidOffset != 0 && smm.linksOffset != 0 && smm.cr3Offset != 0 ) {
    return EFI_SUCCESS;
  }
  if ( initKernel() != EFI_SUCCESS || resolveListLayout() != EFI_SUCCESS ||
       resolveCr3Offset() != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }
  resolveNameOffset();
  return EFI_SUCCESS;
}
