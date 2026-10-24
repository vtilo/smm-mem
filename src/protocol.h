#ifndef SMM_MEM_PROTOCOL_H
#define SMM_MEM_PROTOCOL_H

#include <stdint.h>
#include "api/api.h"

#define MEMORY_WMI_GUID                                                                            \
  { 0xb61c4f65, 0xd741, 0x4ac8, { 0x93, 0x75, 0x63, 0x8d, 0x76, 0xe8, 0x18, 0x29 } }

#define SW_SMI_VALUE 0xD6U
#define MAILBOX_SIZE 0x2000U
#define RESPONSE_OFFSET 0x1000U
#define REQUEST_SIZE 64U
#define RESPONSE_SIZE 64U
#define INLINE_READ_SIZE 24U
#define INLINE_WRITE_SIZE 16U
#define BUFFER_TRANSFER_SIZE 4096U
#define QUERY_INPUT_SIZE 352U
#define PROTOCOL_VERSION 2U

#define CONFIG_MAGIC 0xE2AF0A12E52A0850ULL
#define REQ_MAGIC 0x534D4D5251455132ULL
#define RESP_MAGIC 0x534D4D5253503032ULL

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

#pragma pack( push, 1 )
typedef struct {
  uint64_t Magic;
  uint64_t Sequence;
  uint64_t Address;
  uint64_t Buffer;
  uint32_t Command;
  uint32_t Pid;
  uint32_t CallerPid;
  uint32_t Size;
  uint8_t Data[INLINE_WRITE_SIZE];
} REQUEST;

typedef struct {
  uint64_t Magic;
  uint64_t Sequence;
  uint64_t Result;
  uint32_t Command;
  uint32_t Status;
  uint32_t Size;
  uint32_t Version;
  uint8_t Data[INLINE_READ_SIZE];
} RESPONSE;

typedef struct {
  uint64_t Magic;
  uint64_t MailboxPhysical;
  uint32_t MailboxSize;
  uint32_t SwSmiValue;
  uint64_t Status;
} CONFIG;
#pragma pack( pop )

typedef char RequestLayoutCheck[sizeof( REQUEST ) == REQUEST_SIZE ? 1 : -1];
typedef char ResponseLayoutCheck[sizeof( RESPONSE ) == RESPONSE_SIZE ? 1 : -1];
typedef char ConfigLayoutCheck[sizeof( CONFIG ) == 32 ? 1 : -1];

#endif