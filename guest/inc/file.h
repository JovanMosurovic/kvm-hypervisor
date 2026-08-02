#ifndef FILE_H
#define FILE_H

#include "file_protocol.h"

#define O_RD FILE_OPEN_READ
#define O_WR FILE_OPEN_WRITE
#define O_RDWR FILE_OPEN_READ_WRITE
#define O_CREATE FILE_OPEN_CREATE
#define O_APPEND FILE_OPEN_APPEND

#define SEEK_SET FILE_SEEK_SET
#define SEEK_END FILE_SEEK_END
#define SEEK_CUR FILE_SEEK_CUR

int open(const char *path, int flags);
int close(int fd);
int read(int fd, char *buf, int count);
int write(int fd, const char *buf, int count);
int lseek(int fd, const int offset, int off_flag);

#endif /* FILE_H */
