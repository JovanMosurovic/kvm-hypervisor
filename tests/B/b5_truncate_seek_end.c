#include <stdint.h>

#include "file.h"
#include "io.h"

/*
	B5: verifies O_TRUNC and SEEK_END with an offset.
	The expected final file content is "ABX".
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
	const char *path = "b5data.txt";
	char buffer[8];

	int fd = open(path, O_WR | O_CREATE);
	write(fd, "HELLO", 5);
	close(fd);

	fd = open(path, O_WR | O_TRUNC);
	write(fd, "ABC", 3);
	close(fd);

	fd = open(path, O_RDWR);
	lseek(fd, -1, SEEK_END);
	write(fd, "X", 1);
	close(fd);

	fd = open(path, O_RD);
	const int count = read(fd, buffer, sizeof(buffer) - 1);
	buffer[count < 0 ? 0 : count] = '\0';
	close(fd);

	serial_puts("B5: content=\"");
	serial_puts(buffer);
	serial_puts("\" (expected ABX)\n");

	halt_forever();
}
