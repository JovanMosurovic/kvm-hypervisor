#include "descriptors.h"
#include "file.h"
#include "interrupts.h"
#include "io.h"

static struct gdt_entry gdt[3];

static void serial_write(const char *message)
{
	for (; *message; ++message)
		outb(0xE9, *message);
}

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void)
{
	struct dt_ptr p;

	gdt[0] = (struct gdt_entry){ 0 };
	gdt[1] = (struct gdt_entry){  /* 64-bit code, selector 0x08: P=1, DPL=0, S=1, type=0xA, L=1, G=1 */
		.limit_low   = 0xFFFF,
		.access      = 0x9A,
		.flags_limit = 0xAF,
	};
	gdt[2] = (struct gdt_entry){  /* 64-bit data, selector 0x10: P=1, DPL=0, S=1, type=0x2, D/B=1, G=1 */
		.limit_low   = 0xFFFF,
		.access      = 0x92,
		.flags_limit = 0xCF,
	};

	p.limit = sizeof(gdt) - 1;
	p.base  = (uint64_t)(uintptr_t)gdt;
	asm volatile("lgdt %0" : : "m"(p) : "memory");

	/* Reload CS to 0x08 and continue at label 1 */
	asm volatile(
		"pushq $0x08\n\t"
		"lea 1f(%%rip), %%rax\n\t"
		"pushq %%rax\n\t"
		"lretq\n\t"
		"1:\n\t"
		::: "rax", "memory"
	);

	/* Reload data segment selectors to 0x10 */
	asm volatile(
		"movl $0x10, %%eax\n\t"
		"movw %%ax, %%ds\n\t"
		"movw %%ax, %%es\n\t"
		"movw %%ax, %%ss\n\t"
		::: "eax", "memory"
	);

	init_idt();

	asm volatile("sti");

	serial_write("Hello, world!\n");
	serial_write("Enter one character: ");

	uint8_t input = inb(0xE9);

	serial_write("Guest received: ");

	outb(0xE9, input);
	outb(0xE9, '\n');

	const char message[] = "File I/O works!";
	char buffer[sizeof(message)] = { 0 };
	int fd = open("guest.txt", O_RDWR | O_CREATE);
	int bytes_written = -1;
	int seek_result = -1;
	int bytes_read = -1;

	if (fd >= 0)
		bytes_written = write(fd, message, sizeof(message) - 1);

	if (bytes_written == sizeof(message) - 1)
		seek_result = lseek(fd, 0, SEEK_SET);

	if (seek_result == 0)
		bytes_read = read(fd, buffer, sizeof(buffer) - 1);

	int close_result = fd >= 0 ? close(fd) : -1;

	if (bytes_written != sizeof(message) - 1 || seek_result != 0 ||
	    bytes_read != sizeof(message) - 1 || close_result < 0) {
		serial_write("File I/O failed!\n");
	} else {
		serial_write("File content: ");
		serial_write(buffer);
		outb(0xE9, '\n');
	}

	for (;;)
		asm volatile("hlt");
}
