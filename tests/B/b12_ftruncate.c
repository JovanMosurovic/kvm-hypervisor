#include <stdint.h>

#include "file.h"
#include "io.h"

/*
	B12: verifies that ftruncate changes the size of an open file.
	The expected final file content is "HE".
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
	const char *path = "b12data.txt";
	char buffer[8];

	int fd = open(path, O_RDWR | O_CREATE);
	write(fd, "HELLO", 5);
	ftruncate(fd, 2);
	lseek(fd, 0, SEEK_SET);

	const int count = read(fd, buffer, sizeof(buffer) - 1);
	buffer[count < 0 ? 0 : count] = '\0';
	close(fd);

	serial_puts("B12: content=\"");
	serial_puts(buffer);
	serial_puts("\" (expected HE)\n");

	halt_forever();
}
