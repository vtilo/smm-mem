#include "internal.h"

EFI_STATUS GetProcessCr3( UINT64 Eprocess, UINT64 *Cr3 ) {
  if ( Smm.Cr3Offset == 0 ||
       ReadVirtual64( Smm.KernelCr3, Eprocess + Smm.Cr3Offset, Cr3 ) != EFI_SUCCESS || *Cr3 == 0 ) {
    return EFI_NOT_FOUND;
  }

  *Cr3 &= PAGE_MASK;
  return *Cr3 != 0 ? EFI_SUCCESS : EFI_NOT_FOUND;
}

EFI_STATUS FindPeb( UINT64 Eprocess, UINT64 Cr3, UINT64 *Peb ) {
  UINT32 Offset;
  UINT32 ListOffsets[] = { 0x10, 0x20, 0x30 };
  UINT32 Biases[] = { 0x00, 0x10, 0x20 };
  UINTN Index;

  if ( Smm.PebOffset != 0 &&
       ReadVirtual64( Smm.KernelCr3, Eprocess + Smm.PebOffset, Peb ) == EFI_SUCCESS &&
       IsUserPointer( *Peb ) ) {
    return EFI_SUCCESS;
  }

  for ( Offset = 0x100; Offset < 0x900; Offset += 8 ) {
    UINT64 Candidate;
    UINT64 Ldr;

    if ( ReadVirtual64( Smm.KernelCr3, Eprocess + Offset, &Candidate ) != EFI_SUCCESS ||
         !IsUserPointer( Candidate ) ||
         ReadVirtual64( Cr3, Candidate + 0x18, &Ldr ) != EFI_SUCCESS || !IsUserPointer( Ldr ) ) {
      continue;
    }

    for ( Index = 0; Index < sizeof( ListOffsets ) / sizeof( ListOffsets[0] ); Index++ ) {
      UINT64 Head = Ldr + ListOffsets[Index];
      UINT64 Link;
      UINT64 Base;
      UINT16 Mz;

      if ( ReadVirtual64( Cr3, Head, &Link ) == EFI_SUCCESS && IsUserPointer( Link ) &&
           Link != Head &&
           ReadVirtual64( Cr3, Link + 0x30 - Biases[Index], &Base ) == EFI_SUCCESS &&
           IsUserPointer( Base ) && ReadVirtual16( Cr3, Base, &Mz ) == EFI_SUCCESS &&
           Mz == 0x5A4D ) {
        Smm.PebOffset = Offset;
        *Peb = Candidate;
        return EFI_SUCCESS;
      }
    }
  }

  return EFI_NOT_FOUND;
}

EFI_STATUS FindImageBase( UINT64 Eprocess, UINT64 Cr3, UINT64 *Base ) {
  UINT64 Peb;
  UINT64 Ldr;
  UINT32 ListOffsets[] = { 0x10, 0x20, 0x30 };
  UINT32 Biases[] = { 0x00, 0x10, 0x20 };
  UINTN Index;

  if ( FindPeb( Eprocess, Cr3, &Peb ) == EFI_SUCCESS &&
       ReadVirtual64( Cr3, Peb + 0x18, &Ldr ) == EFI_SUCCESS && IsUserPointer( Ldr ) ) {
    for ( Index = 0; Index < sizeof( ListOffsets ) / sizeof( ListOffsets[0] ); Index++ ) {
      UINT64 Head = Ldr + ListOffsets[Index];
      UINT64 Link;

      if ( ReadVirtual64( Cr3, Head, &Link ) == EFI_SUCCESS && IsUserPointer( Link ) &&
           Link != Head && ReadVirtual64( Cr3, Link + 0x30 - Biases[Index], Base ) == EFI_SUCCESS &&
           IsUserPointer( *Base ) ) {
        return EFI_SUCCESS;
      }
    }
  }

  *Base = 0;
  return EFI_NOT_FOUND;
}

EFI_STATUS ReadProcessImagePath( UINT64 Eprocess, UINT64 Cr3, char *Out, UINTN OutSize ) {
  UINT64 Peb;
  UINT64 Params;

  if ( FindPeb( Eprocess, Cr3, &Peb ) != EFI_SUCCESS ||
       ReadVirtual64( Cr3, Peb + 0x20, &Params ) != EFI_SUCCESS || !IsUserPointer( Params ) ) {
    return EFI_NOT_FOUND;
  }

  return ReadUnicodeName( Cr3, Params + 0x60, Out, OutSize );
}

EFI_STATUS FillProcessInfo( UINT64 Eprocess, PROCESS_INFO *Info ) {
  UINT64 Pid;
  UINT64 Cr3;
  char ImagePath[260];

  MemoryClear( Info, sizeof( *Info ) );

  if ( ReadVirtual64( Smm.KernelCr3, Eprocess + Smm.PidOffset, &Pid ) != EFI_SUCCESS ||
       GetProcessCr3( Eprocess, &Cr3 ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  Info->Pid = ( UINT32 )Pid;
  Info->Eprocess = Eprocess;
  Info->Cr3 = Cr3;

  if ( Smm.NameOffset != 0 ) {
    CopyVirtual(
      Smm.KernelCr3, Eprocess + Smm.NameOffset, Info->Name, sizeof( Info->Name ) - 1, 0 );
    Info->Name[sizeof( Info->Name ) - 1] = 0;
  }

  FindImageBase( Eprocess, Cr3, &Info->ImageBase );
  MemoryClear( ImagePath, sizeof( ImagePath ) );

  if ( ReadProcessImagePath( Eprocess, Cr3, ImagePath, sizeof( ImagePath ) ) == EFI_SUCCESS ) {
    CopyText( Info->Name, sizeof( Info->Name ), BaseName( ImagePath ) );
  }

  return EFI_SUCCESS;
}

static BOOLEAN ReadProcessHint( const ProcessHint *Hint, UINT32 Pid, ProcessContext *Process ) {
  struct {
    UINT64 Pid, Next, Previous;
  } Links;
  UINT64 Entry;
  UINT64 Neighbor;

  if ( Hint->Pid != Pid || !IsKernelPointer( Hint->Eprocess ) ) {
    return 0;
  }

  Entry = Hint->Eprocess + Smm.LinksOffset;

  if ( CopyVirtual( Smm.KernelCr3, Hint->Eprocess + Smm.PidOffset, &Links, sizeof( Links ), 0 ) !=
         EFI_SUCCESS ||
       ( UINT32 )Links.Pid != Pid || !IsKernelPointer( Links.Next ) ||
       !IsKernelPointer( Links.Previous ) || Links.Next == Entry || Links.Previous == Entry ||
       ReadVirtual64( Smm.KernelCr3, Links.Next + 8, &Neighbor ) != EFI_SUCCESS ||
       Neighbor != Entry ||
       ReadVirtual64( Smm.KernelCr3, Links.Previous, &Neighbor ) != EFI_SUCCESS ||
       Neighbor != Entry || GetProcessCr3( Hint->Eprocess, &Process->Cr3 ) != EFI_SUCCESS ) {
    return 0;
  }

  Process->Eprocess = Hint->Eprocess;
  return 1;
}

EFI_STATUS ResolveProcess( UINT32 Pid, ProcessContext *Process ) {
  UINT64 Head;
  UINT64 Link;
  UINT32 Guard;

  MemoryClear( Process, sizeof( *Process ) );

  if ( ResolveProcessLayout() != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  if ( Pid == 4 ) {
    Process->Eprocess = Smm.SystemProcess;
    return GetProcessCr3( Process->Eprocess, &Process->Cr3 );
  }

  for ( UINT32 Index = 0; Index < PROCESS_HINT_COUNT; Index++ ) {
    ProcessHint *Hint = &Smm.ProcessHints[Index];

    if ( Hint->Pid != Pid ) {
      continue;
    }

    if ( ReadProcessHint( Hint, Pid, Process ) ) {
      return EFI_SUCCESS;
    }

    MemoryClear( Hint, sizeof( *Hint ) );
  }

  Head = Smm.SystemProcess + Smm.LinksOffset;

  if ( ReadVirtual64( Smm.KernelCr3, Head, &Link ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  for ( Guard = 0; Guard < 4096 && IsKernelPointer( Link ) && Link != Head; Guard++ ) {
    struct {
      UINT64 Pid, Next;
    } Current;
    UINT64 Eprocess = Link - Smm.LinksOffset;

    if ( CopyVirtual( Smm.KernelCr3, Eprocess + Smm.PidOffset, &Current, sizeof( Current ), 0 ) !=
         EFI_SUCCESS ) {
      break;
    }

    if ( ( UINT32 )Current.Pid == Pid ) {
      EFI_STATUS Status = GetProcessCr3( Eprocess, &Process->Cr3 );

      if ( Status == EFI_SUCCESS ) {
        UINT32 Index = Smm.NextProcessHint % PROCESS_HINT_COUNT;
        ProcessHint *Hint = &Smm.ProcessHints[Index];

        Process->Eprocess = Eprocess;
        Hint->Pid = Pid;
        Hint->Eprocess = Eprocess;
        Smm.NextProcessHint = ( Index + 1 ) % PROCESS_HINT_COUNT;
      }

      return Status;
    }

    Link = Current.Next;
  }

  return EFI_NOT_FOUND;
}

EFI_STATUS FindProcessPid( UINT32 Pid, PROCESS_INFO *Info ) {
  ProcessContext Process;
  EFI_STATUS Status = ResolveProcess( Pid, &Process );

  if ( EFI_ERROR( Status ) ) {
    return Status;
  }

  return FillProcessInfo( Process.Eprocess, Info );
}

EFI_STATUS FindProcessName( const char *Name, PROCESS_INFO *Info ) {
  UINT64 Head;
  UINT64 Link;
  UINT32 Guard;
  char CurrentName[NAME_SIZE];
  char ImagePath[260];
  UINT64 Cr3;

  if ( ResolveProcessLayout() != EFI_SUCCESS || Smm.NameOffset == 0 ) {
    return EFI_NOT_FOUND;
  }

  MemoryClear( CurrentName, sizeof( CurrentName ) );
  CopyVirtual(
    Smm.KernelCr3, Smm.SystemProcess + Smm.NameOffset, CurrentName, sizeof( CurrentName ) - 1, 0 );

  if ( SameName( CurrentName, Name ) ) {
    return FillProcessInfo( Smm.SystemProcess, Info );
  }

  Head = Smm.SystemProcess + Smm.LinksOffset;

  if ( ReadVirtual64( Smm.KernelCr3, Head, &Link ) != EFI_SUCCESS ) {
    return EFI_NOT_FOUND;
  }

  for ( Guard = 0; Guard < 4096 && IsKernelPointer( Link ) && Link != Head; Guard++ ) {
    UINT64 Eprocess = Link - Smm.LinksOffset;

    MemoryClear( CurrentName, sizeof( CurrentName ) );
    CopyVirtual(
      Smm.KernelCr3, Eprocess + Smm.NameOffset, CurrentName, sizeof( CurrentName ) - 1, 0 );

    if ( SameName( CurrentName, Name ) ) {
      return FillProcessInfo( Eprocess, Info );
    }

    if ( GetProcessCr3( Eprocess, &Cr3 ) == EFI_SUCCESS ) {
      MemoryClear( ImagePath, sizeof( ImagePath ) );

      if ( ReadProcessImagePath( Eprocess, Cr3, ImagePath, sizeof( ImagePath ) ) == EFI_SUCCESS &&
           SameBaseName( ImagePath, Name ) ) {
        return FillProcessInfo( Eprocess, Info );
      }
    }

    if ( ReadVirtual64( Smm.KernelCr3, Eprocess + Smm.LinksOffset, &Link ) != EFI_SUCCESS ) {
      break;
    }
  }

  return EFI_NOT_FOUND;
}