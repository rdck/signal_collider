/*******************************************************************************
 * PLATFORM FILE IO INTERFACE
 ******************************************************************************/

#ifndef FILE_H
#define FILE_H

#include "prelude.h"
#include "arena.h"

// This does not handle cases where the file size overflows an S32. It returns
// 0 to indicate failure.
Size file_read_entire_file_to_arena(Arena* arena, const Char* path);

#endif
