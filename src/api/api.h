#ifndef API_H
#define API_H

#include <stdint.h>

#define NAME_SIZE 64U

typedef struct {
  uint32_t Pid;
  uint32_t Reserved;
  uint64_t Eprocess;
  uint64_t Cr3;
  uint64_t ImageBase;
  char Name[NAME_SIZE];
} PROCESS_INFO;

typedef struct {
  uint32_t Pid;
  uint32_t Reserved;
  uint64_t Base;
  uint64_t Size;
  uint64_t Cr3;
  char Name[NAME_SIZE];
} MODULE_INFO;

/* Chunk sizes may vary. Return zero to stop streaming. */
typedef int ( *DUMP_CALLBACK )( uint64_t address, const void *data, uint32_t size, void *context );

#ifdef __cplusplus
extern "C" {
#endif

int Init( void );
void Close( void );
int Ping( void );
int FindProcessByPid( uint32_t pid, PROCESS_INFO *process );
int FindProcessByName( const char *name, PROCESS_INFO *process );
int TranslateVirt( uint32_t pid, uint64_t address, uint64_t *physical );
int ReadPhys( uint64_t address, void *buffer, uint32_t size );
int WritePhys( uint64_t address, const void *buffer, uint32_t size );
int ReadVirt( uint32_t pid, uint64_t address, void *buffer, uint32_t size );
int WriteVirt( uint32_t pid, uint64_t address, const void *buffer, uint32_t size );
int FindModule( const PROCESS_INFO *process, const char *name, MODULE_INFO *module );
int FindKernelModule( const char *name, MODULE_INFO *module );
int FindExport( const MODULE_INFO *module, const char *name, uint64_t *address );
int Dump( const MODULE_INFO *module, DUMP_CALLBACK callback, void *context );

#ifdef __cplusplus
}
#endif

#endif
