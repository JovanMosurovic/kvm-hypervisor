#include <stdint.h>

#include "file.h"
#include "io.h"

/*
	B9: verifies SEEK_END only.
	The expected final file content is "ABCDYF".
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
	const char *path = "b9data.txt";
	char buffer[16];

	int fd = open(path, O_WR | O_CREATE);
	write(fd, "ABCDEF", 6);
	close(fd);

	fd = open(path, O_RDWR);
	lseek(fd, -2, SEEK_END);
	write(fd, "Y", 1);
	close(fd);

	fd = open(path, O_RD);
	const int count = read(fd, buffer, sizeof(buffer) - 1);
	buffer[count < 0 ? 0 : count] = '\0';
	close(fd);

	serial_puts("B9: content=\"");
	serial_puts(buffer);
	serial_puts("\" (expected ABCDYF)\n");

	halt_forever();
}
