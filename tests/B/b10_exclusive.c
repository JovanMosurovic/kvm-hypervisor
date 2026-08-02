#include <stdint.h>

#include "file.h"
#include "io.h"

/*
	B10: verifies O_EXCL together with O_CREATE.
	Opening an existing file must fail, while a new file must be created.
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
	const int exclusiveFlags = O_WR | O_CREATE | O_EXCL;
	int fd = open("b10existing.txt", O_WR | O_CREATE);
	close(fd);

	const int existing = open("b10existing.txt", exclusiveFlags);
	const int created = open("b10new.txt", exclusiveFlags);
	close(created);

	serial_puts("B10: existing=");
	serial_put_int(existing);
	serial_puts(" (expected -1), new=");
	serial_puts(created >= 0 ? "success" : "failure");
	serial_puts("\n");

	halt_forever();
}
