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

// Chunk sizes may vary and returning zero stops streaming
typedef int ( *DUMP_CALLBACK )( uint64_t Address, const void *Data, uint32_t Size, void *Context );

#ifdef __cplusplus
extern "C" {
#endif

int Init( void );
void Close( void );
int Ping( void );
int FindProcessByPid( uint32_t Pid, PROCESS_INFO *Process );
int FindProcessByName( const char *Name, PROCESS_INFO *Process );
int TranslateVirt( uint32_t Pid, uint64_t Address, uint64_t *Physical );
int ReadPhys( uint64_t Address, void *Buffer, uint32_t Size );
int WritePhys( uint64_t Address, const void *Buffer, uint32_t Size );
int ReadVirt( uint32_t Pid, uint64_t Address, void *Buffer, uint32_t Size );
int WriteVirt( uint32_t Pid, uint64_t Address, const void *Buffer, uint32_t Size );
int FindModule( const PROCESS_INFO *Process, const char *Name, MODULE_INFO *Module );
int FindKernelModule( const char *Name, MODULE_INFO *Module );
int FindExport( const MODULE_INFO *Module, const char *Name, uint64_t *Address );
int Dump( const MODULE_INFO *Module, DUMP_CALLBACK Callback, void *Context );

#ifdef __cplusplus
}
#endif

#endif