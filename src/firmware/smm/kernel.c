#include "internal.h"

static EFI_GUID SmmCpuProtocolGuid = {
  0xeb346b97, 0x975f, 0x4a9f, { 0x8b, 0x22, 0xf8, 0xe9, 0x2b, 0xb3, 0xd5, 0x69 } };

VOID LocateSmmCpu( VOID ) {
  EFI_LOCATE_PROTOCOL Locate;

  if ( Smm.Cpu == 0 ) {
    Locate = ( EFI_LOCATE_PROTOCOL )Smm.Table->SmmLocateProtocol;
    Locate( &SmmCpuProtocolGuid, 0, ( VOID ** )&Smm.Cpu );
  }
}

EFI_STATUS ReadSavedCr3( UINTN CpuIndex, UINT64 *Cr3 ) {
  LocateSmmCpu();

  if ( Smm.Cpu != 0 && Smm.Cpu->ReadSaveState != 0 &&
       Smm.Cpu->ReadSaveState( Smm.Cpu, sizeof( *Cr3 ), SAVE_STATE_CR3, CpuIndex, Cr3 ) ==
         EFI_SUCCESS &&
       *Cr3 != 0 ) {
    *Cr3 &= PAGE_MASK;
    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

BOOLEAN LooksLikeProcessList( UINT64 Eprocess, UINT32 PidOffset ) {
  UINT64 Flink;
  UINT64 Blink;
  UINT64 Back;

  if ( ReadVirtual64( Smm.KernelCr3, Eprocess + PidOffset + 8, &Flink ) != EFI_SUCCESS ||
       ReadVirtual64( Smm.KernelCr3, Eprocess + PidOffset + 16, &Blink ) != EFI_SUCCESS ||
       !IsKernelPointer( Flink ) || !IsKernelPointer( Blink ) ||
       ReadVirtual64( Smm.KernelCr3, Flink + 8, &Back ) != EFI_SUCCESS ) {
    return 0;
  }

  return Back == Eprocess + PidOffset + 8;
}

EFI_STATUS ResolveListLayout( VOID ) {
  UINT32 PidOffset;

  if ( !IsKernelPointer( Smm.SystemProcess ) ) {
    return EFI_NOT_FOUND;
  }

  for ( PidOffset = 0x20; PidOffset < 0x900; PidOffset += 8 ) {
    UINT64 Pid;

    if ( ReadVirtual64( Smm.KernelCr3, Smm.SystemProcess + PidOffset, &Pid ) == EFI_SUCCESS &&
         ( UINT32 )Pid == 4 && LooksLikeProcessList( Smm.SystemProcess, PidOffset ) ) {
      Smm.PidOffset = PidOffset;
      Smm.LinksOffset = PidOffset + 8;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

EFI_STATUS ResolveNameOffset( VOID ) {
  UINT32 Offset;

  if ( Smm.NameOffset != 0 ) {
    return EFI_SUCCESS;
  }

  for ( Offset = 0x100; Offset < 0x900; Offset++ ) {
    char Name[NAME_SIZE];

    MemoryClear( Name, sizeof( Name ) );

    if ( CopyVirtual( Smm.KernelCr3, Smm.SystemProcess + Offset, Name, sizeof( Name ) - 1, 0 ) ==
           EFI_SUCCESS &&
         SameName( Name, "System" ) ) {
      Smm.NameOffset = Offset;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

EFI_STATUS ResolveCr3Offset( VOID ) {
  UINT32 Offset = Smm.Cr3Offset;

  if ( Offset != 0 ) {
    UINT64 Cr3;
    UINT16 Mz;

    if ( ReadVirtual64( Smm.KernelCr3, Smm.SystemProcess + Offset, &Cr3 ) == EFI_SUCCESS &&
         Cr3 != 0 && ( Cr3 & ( PAGE_SIZE - 1 ) ) == 0 &&
         ReadVirtual16( Cr3, Smm.KernelBase, &Mz ) == EFI_SUCCESS && Mz == 0x5A4D ) {
      Smm.KernelCr3 = Cr3;
      Smm.KernelStable = 1;
      return EFI_SUCCESS;
    }

    Smm.Cr3Offset = 0;
  }

  for ( Offset = 0x20; Offset < 0x100; Offset += 8 ) {
    UINT64 Cr3;
    UINT16 Mz;

    if ( ReadVirtual64( Smm.KernelCr3, Smm.SystemProcess + Offset, &Cr3 ) != EFI_SUCCESS ||
         Cr3 == 0 || ( Cr3 & ( PAGE_SIZE - 1 ) ) != 0 ) {
      continue;
    }

    if ( ReadVirtual16( Cr3, Smm.KernelBase, &Mz ) == EFI_SUCCESS && Mz == 0x5A4D ) {
      Smm.Cr3Offset = Offset;
      Smm.KernelCr3 = Cr3;
      Smm.KernelStable = 1;
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

EFI_STATUS TryKernelCr3( UINT64 Cr3, UINT64 Lstar ) {
  UINT64 Start;
  UINT64 Limit;
  UINT64 Base;
  UINT64 Export;

  Smm.KernelStable = 0;
  Smm.KernelCr3 = Cr3 & PAGE_MASK;
  Start = Lstar & PAGE_2MB_MASK;
  Limit = Start > 0x10000000ULL ? Start - 0x10000000ULL : 0;

  for ( Base = Start; Base > Limit; Base -= PAGE_2MB_SIZE ) {
    UINT16 Mz;

    if ( ReadVirtual16( Smm.KernelCr3, Base, &Mz ) != EFI_SUCCESS || Mz != 0x5A4D ) {
      continue;
    }

    if ( ResolveExport( Smm.KernelCr3, Base, "PsInitialSystemProcess", &Export ) == EFI_SUCCESS ) {
      Smm.KernelBase = Base;

      if ( ReadVirtual64( Smm.KernelCr3, Export, &Smm.SystemProcess ) != EFI_SUCCESS ) {
        return EFI_NOT_FOUND;
      }

      ResolveExport( Smm.KernelCr3, Base, "PsLoadedModuleList", &Smm.LoadedModuleList );
      ( void )ResolveCr3Offset();
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

EFI_STATUS InitKernel( VOID ) {
  UINT64 Lstar;
  UINT64 Cr3;
  UINTN Cpu;

  if ( Smm.KernelBase != 0 && Smm.SystemProcess != 0 ) {
    if ( Smm.KernelStable || ResolveCr3Offset() == EFI_SUCCESS ) {
      return EFI_SUCCESS;
    }
  }

  Lstar = __readmsr( MSR_LSTAR );

  for ( Cpu = 0; Cpu < Smm.Table->NumberOfCpus; Cpu++ ) {
    if ( ReadSavedCr3( Cpu, &Cr3 ) == EFI_SUCCESS && Cr3 < 0x100000000ULL &&
         TryKernelCr3( Cr3, Lstar ) == EFI_SUCCESS ) {
      return EFI_SUCCESS;
    }
  }

  for ( Cpu = 0; Cpu < Smm.Table->NumberOfCpus; Cpu++ ) {
    if ( ReadSavedCr3( Cpu, &Cr3 ) == EFI_SUCCESS && TryKernelCr3( Cr3, Lstar ) == EFI_SUCCESS ) {
      return EFI_SUCCESS;
    }
  }

  Cr3 = __readcr3() & PAGE_MASK;
  return TryKernelCr3( Cr3, Lstar );
}

EFI_STATUS ResolveProcessLayout( VOID ) {
  if ( Smm.PidOffset != 0 && Smm.LinksOffset != 0 && Smm.Cr3Offset != 0 ) {
    return EFI_SUCCESS;
  }

  if ( InitKernel() != EFI_SUCCESS || ResolveListLayout() != EFI_SUCCESS ||
       ResolveCr3Offset() != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  ResolveNameOffset();
  return EFI_SUCCESS;
}