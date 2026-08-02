#ifndef FILE_PROTOCOL_H
#define FILE_PROTOCOL_H

#include <stdint.h>

#define FILE_IO_PORT 0x0278u
#define FILE_NAME_MAX 255u

enum file_operation {
	FILE_OPERATION_OPEN = 1,
	FILE_OPERATION_CLOSE,
	FILE_OPERATION_READ,
	FILE_OPERATION_WRITE,
	FILE_OPERATION_LSEEK
};

enum file_open_flag {
	FILE_OPEN_READ = 1,
	FILE_OPEN_WRITE = 2,
	FILE_OPEN_READ_WRITE = 4,
	FILE_OPEN_CREATE = 8,
	FILE_OPEN_APPEND = 9,
	FILE_OPEN_TRUNCATE = 16
};

enum file_seek_flag {
	FILE_SEEK_SET = 1,
	FILE_SEEK_END = 2,
	FILE_SEEK_CUR = 3
};

struct file_request {
	uint32_t operation;
	int32_t descriptor;
	uint32_t buffer;
	int32_t count;
	int32_t offset;
	int32_t flags;
};

#if defined(__cplusplus)
static_assert(sizeof(struct file_request) == 24, "Unexpected file request layout");
#else
_Static_assert(sizeof(struct file_request) == 24, "Unexpected file request layout");
#endif

#endif /* FILE_PROTOCOL_H */
