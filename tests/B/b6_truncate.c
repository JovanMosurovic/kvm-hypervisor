#include <stdint.h>

#include "file.h"
#include "io.h"

/*
	B6: verifies that O_TRUNC clears an existing file when it is opened.
	The expected file size after truncation is zero.
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

static void serial_put_int(int value)
{
	char buffer[16];
	int length = 0;

	do {
		buffer[length++] = '0' + value % 10;
		value /= 10;
	} while (value != 0);

	while (length > 0)
		serial_putc(buffer[--length]);
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
	const char *path = "b6data.txt";
	char buffer[8];

	int fd = open(path, O_WR | O_CREATE);
	write(fd, "OLD", 3);
	close(fd);

	fd = open(path, O_WR | O_TRUNC);
	close(fd);

	fd = open(path, O_RD);
	const int count = read(fd, buffer, sizeof(buffer));
	close(fd);

	serial_puts("B6: size=");
	serial_put_int(count);
	serial_puts(" (expected 0)\n");

	halt_forever();
}
