#ifndef SMM_MEM_PROTOCOL_H
#define SMM_MEM_PROTOCOL_H

#include <stdint.h>
#include "api/api.h"

#define MEMORY_WMI_GUID                                                                            \
  { 0xa0c9f8de, 0x0b71, 0x42a8, { 0xb9, 0x67, 0xe5, 0x38, 0xea, 0xcb, 0x6f, 0x21 } }
#define FAST_MEMORY_WMI_GUID                                                                       \
  { 0xb61c4f65, 0xd741, 0x4ac8, { 0x93, 0x75, 0x63, 0x8d, 0x76, 0xe8, 0x18, 0x29 } }

#define SW_SMI_VALUE 0xD6U
#define MAILBOX_SIZE 0x2000U
#define REQUEST_SIZE 4096U
#define RESPONSE_OFFSET 0x1000U
#define RESPONSE_SIZE 512U
#define RESPONSE_DATA_SIZE 352U

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
#define READ_VIRT_BUFFER 12U
#define WRITE_VIRT_BUFFER 13U
#define READ_PHYS_BUFFER 14U
#define WRITE_PHYS_BUFFER 15U
#define STATUS_OK 0U

#define FAST_REQUEST_SIZE 64U
#define FAST_RESPONSE_SIZE 64U
#define FAST_INLINE_READ_SIZE 28U
#define FAST_INLINE_WRITE_SIZE 16U
#define BUFFER_TRANSFER_SIZE 4096U
#define FEATURE_BUFFER_TRANSFER 1U
#define FAST_CAPABILITY 0x534D4D4600010001ULL
#define FAST_CONFIG_MAGIC 0x534D4D4346470001ULL

#pragma pack( push, 1 )
typedef struct {
  uint64_t Magic;
  uint32_t Command;
  uint32_t DataSize;
  uint64_t Sequence;
  uint64_t Arg1;
  uint64_t Arg2;
  uint64_t Arg3;
  uint8_t Data[1];
} REQUEST;

typedef struct {
  uint64_t Magic;
  uint32_t Status;
  uint32_t Command;
  uint32_t DataSize;
  uint64_t Sequence;
  uint64_t Result;
  uint8_t Data[RESPONSE_DATA_SIZE];
} RESPONSE;

typedef struct {
  uint64_t Magic;
  uint64_t MailboxPhysical;
  uint32_t MailboxSize;
  uint32_t SwSmiValue;
  uint64_t Status;
} CONFIG;

typedef struct {
  CONFIG Base;
  uint64_t ExtensionMagic;
  uint32_t Features;
  uint32_t Reserved;
} CONFIG_EXTENDED;

typedef struct {
  uint32_t Pid;
  uint64_t Address;
} TRANSFER_BUFFER;
#pragma pack( pop )

typedef char requestLayoutCheck[sizeof( REQUEST ) == 49 ? 1 : -1];
typedef char responseLayoutCheck[sizeof( RESPONSE ) == 388 ? 1 : -1];
typedef char configLayoutCheck[sizeof( CONFIG ) == 32 ? 1 : -1];
typedef char configExtendedLayoutCheck[sizeof( CONFIG_EXTENDED ) == 48 ? 1 : -1];
typedef char transferBufferLayoutCheck[sizeof( TRANSFER_BUFFER ) == 12 ? 1 : -1];

#endif
