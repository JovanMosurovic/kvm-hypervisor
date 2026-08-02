#include <stdint.h>

#include "file.h"
#include "io.h"

/*
	B13: verifies that file_size reports the current file length.
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

	if (value < 0) {
		serial_putc('-');
		value = -value;
	}

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
	int fd = open("b13data.txt", O_RDWR | O_CREATE | O_TRUNC);
	write(fd, "HELLO", 5);

	const int size = file_size(fd);
	close(fd);

	serial_puts("B13: size=");
	serial_put_int(size);
	serial_puts(" (expected 5)\n");

	halt_forever();
}
