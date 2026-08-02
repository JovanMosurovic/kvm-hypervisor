#include <stdint.h>

#include "file.h"
#include "io.h"

/*
	B4: verifies SEEK_CUR and O_APPEND.
	The expected final file content is "ABXD".
*/

static void serial_putc(char character)
{
	outb(0xE9, (uint8_t)character);
}

static void serial_puts(const char *message)
{
	for (; *message; ++message)
		serial_putc(*message);
}

static __attribute__((noreturn)) void halt_forever(void)
{
	for (;;)
		asm volatile("hlt");
}

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void)
{
	const char *path = "b4data.txt";
	char buffer[8];

	int fd = open(path, O_WR | O_CREATE);
	write(fd, "ABC", 3);
	lseek(fd, 1, SEEK_SET);
	lseek(fd, 1, SEEK_CUR);
	write(fd, "X", 1);
	close(fd);

	fd = open(path, O_APPEND);
	write(fd, "D", 1);
	close(fd);

	fd = open(path, O_RD);
	const int count = read(fd, buffer, sizeof(buffer) - 1);
	buffer[count < 0 ? 0 : count] = '\0';
	close(fd);

	serial_puts("B4: content=\"");
	serial_puts(buffer);
	serial_puts("\" (expected ABXD)\n");

	halt_forever();
}
