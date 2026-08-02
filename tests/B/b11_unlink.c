#include <stdint.h>

#include "file.h"
#include "io.h"

/*
	B11: verifies unlink for a local guest file.
	The file must not be openable after it is removed.
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
	const char *path = "b11data.txt";

	int fd = open(path, O_WR | O_CREATE);
	write(fd, "DATA", 4);
	close(fd);

	const int unlinkResult = unlink(path);
	const int reopened = open(path, O_RD);

	serial_puts("B11: unlink=");
	serial_puts(unlinkResult == 0 ? "success" : "failure");
	serial_puts(", reopen=");
	serial_puts(reopened < 0 ? "failure (expected)" : "success (unexpected)");
	serial_puts("\n");

	halt_forever();
}
