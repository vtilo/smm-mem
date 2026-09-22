#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "Api.h"
#include "shared/protocol.h"

/* Keep Client.c usable as a single-file library with API_ONLY defined. */
#include "client/state.h"
#include "client/transport.inl"
#include "client/memory.inl"
#include "client/lookup.inl"
#include "client/dump.inl"
#include "client/main.inl"
