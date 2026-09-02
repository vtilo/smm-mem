#ifndef SHARED_PROTOCOL_H
#define SHARED_PROTOCOL_H

#include "uefi.h"

#define SW_SMI_VALUE 0xD6U
#define MAILBOX_SIZE 0x2000U
#define REQUEST_SIZE 4096U
#define RESPONSE_OFFSET 0x1000U
#define RESPONSE_SIZE 512U
#define RESPONSE_DATA_SIZE 352U
#define NAME_SIZE 64U

#define CONFIG_MAGIC 0xE2AF0A12E52A0850ULL
#define REQ_MAGIC 0x99D4B1698CAF040FULL
#define RESP_MAGIC 0xC0D8E93F90BBFBECULL

#define PING 1U
#define READ_PHYS 2U
#define WRITE_PHYS 3U
#define TRANSLATE_VIRT 4U
#define READ_VIRT 5U
#define WRITE_VIRT 6U
#define FIND_PROCESS_PID 7U
#define FIND_PROCESS_NAME 8U
#define FIND_MODULE 9U
#define FIND_KERNEL_MODULE 10U
#define FIND_EXPORT 11U

#define STATUS_OK 0U
#define EFI_RUNTIME_SERVICES_DATA 6U
#define SERIAL 1U
#define COM1_PORT 0x3F8U

#pragma pack(push, 1)
typedef struct {
  EFI_GUID HeaderGuid;
  UINTN MessageLength;
  UINT8 Data[1];
} EFI_SMM_COMMUNICATE_HEADER;

typedef struct {
  UINT64 Magic;
  UINT32 Command;
  UINT32 DataSize;
  UINT64 Sequence;
  UINT64 Arg1;
  UINT64 Arg2;
  UINT64 Arg3;
  UINT8 Data[1];
} REQUEST;

typedef struct {
  UINT64 Magic;
  UINT32 Status;
  UINT32 Command;
  UINT32 DataSize;
  UINT64 Sequence;
  UINT64 Result;
  UINT8 Data[RESPONSE_DATA_SIZE];
} RESPONSE;

typedef struct {
  UINT32 Pid;
  UINT32 Reserved;
  UINT64 Eprocess;
  UINT64 Cr3;
  UINT64 ImageBase;
  char Name[NAME_SIZE];
} PROCESS_INFO;

typedef struct {
  UINT32 Pid;
  UINT32 Reserved;
  UINT64 Base;
  UINT64 Size;
  UINT64 Cr3;
  char Name[NAME_SIZE];
} MODULE_INFO;

typedef struct {
  UINT64 Magic;
  UINT64 MailboxPhysical;
  UINT32 MailboxSize;
  UINT32 SwSmiValue;
  EFI_STATUS Status;
} CONFIG;
#pragma pack(pop)
#endif
