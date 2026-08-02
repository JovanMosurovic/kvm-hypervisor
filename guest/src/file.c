#include "file.h"
#include "io.h"

#include <stdint.h>

static int submit_file_request(struct file_request *request)
{
	outl(FILE_IO_PORT, (uint32_t)(uintptr_t)request);
	return (int32_t)inl(FILE_IO_PORT);
}

int open(const char *path, int flags)
{
	if (path == 0)
		return -1;

	struct file_request request = {
		.operation = FILE_OPERATION_OPEN,
		.buffer = (uint32_t)(uintptr_t)path,
		.flags = flags,
	};

	return submit_file_request(&request);
}

int close(int fd)
{
	struct file_request request = {
		.operation = FILE_OPERATION_CLOSE,
		.descriptor = fd,
	};

	return submit_file_request(&request);
}

int read(int fd, char *buf, int count)
{
	if (buf == 0 && count > 0)
		return -1;

	struct file_request request = {
		.operation = FILE_OPERATION_READ,
		.descriptor = fd,
		.buffer = (uint32_t)(uintptr_t)buf,
		.count = count,
	};

	return submit_file_request(&request);
}

int write(int fd, const char *buf, int count)
{
	if (buf == 0 && count > 0)
		return -1;

	struct file_request request = {
		.operation = FILE_OPERATION_WRITE,
		.descriptor = fd,
		.buffer = (uint32_t)(uintptr_t)buf,
		.count = count,
	};

	return submit_file_request(&request);
}

int lseek(int fd, const int offset, int off_flag)
{
	struct file_request request = {
		.operation = FILE_OPERATION_LSEEK,
		.descriptor = fd,
		.offset = offset,
		.flags = off_flag,
	};

	return submit_file_request(&request);
}

int unlink(const char *path)
{
	if (path == 0)
		return -1;

	struct file_request request = {
		.operation = FILE_OPERATION_UNLINK,
		.buffer = (uint32_t)(uintptr_t)path,
	};

	return submit_file_request(&request);
}

int ftruncate(int fd, int length)
{
	struct file_request request = {
		.operation = FILE_OPERATION_FTRUNCATE,
		.descriptor = fd,
		.offset = length,
	};

	return submit_file_request(&request);
}

int file_size(int fd)
{
	struct file_request request = {
		.operation = FILE_OPERATION_FILE_SIZE,
		.descriptor = fd,
	};

	return submit_file_request(&request);
}
