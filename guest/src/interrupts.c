#include "descriptors.h"
#include "file.h"
#include "interrupts.h"
#include "io.h"
#include "shared_buffer_protocol.h"

#include <stdint.h>

static struct idt_entry idt[IDT_ENTRIES];
static volatile uint8_t transfer_finished;
static volatile uint8_t transfer_succeeded;
static uint8_t vm_mode;
static uint8_t mode_initialized;

static void serial_write(const char *message)
{
	for (; *message; ++message)
		outb(0xE9, *message);
}

static int write_shared_buffer(void)
{
	char buffer[BUFFER_SIZE + 1u];
	int fd = open("guest.txt", O_RD);
	int bytes_read = fd >= 0 ? read(fd, buffer, sizeof(buffer)) : -1;
	int close_result = fd >= 0 ? close(fd) : -1;
	uint32_t count = bytes_read >= 0 ? (uint32_t)bytes_read : 0;

	outl(SHARED_BUFFER_PORT, count);

	for (uint32_t i = 0; i < count; ++i)
		outb(SHARED_BUFFER_PORT, (uint8_t)buffer[i]);

	uint32_t written = inl(SHARED_BUFFER_STATUS_PORT);
	uint32_t expected = count > BUFFER_SIZE ? BUFFER_SIZE : count;

	return bytes_read >= 0 && close_result == 0 && written == expected;
}

static int read_shared_buffer(void)
{
	char buffer[BUFFER_SIZE];
	uint32_t count = inl(SHARED_BUFFER_PORT);

	if (count > BUFFER_SIZE) {
		outl(SHARED_BUFFER_STATUS_PORT, 0);
		return 0;
	}

	for (uint32_t i = 0; i < count; ++i)
		buffer[i] = (char)inb(SHARED_BUFFER_PORT);

	outl(SHARED_BUFFER_STATUS_PORT, count);

	int fd = open("reader.txt", O_WR | O_CREATE);
	int seek_result = fd >= 0 ? lseek(fd, 0, SEEK_SET) : -1;
	int bytes_written = seek_result == 0 ? write(fd, buffer, (int)count) : -1;
	int close_result = fd >= 0 ? close(fd) : -1;

	return bytes_written == (int)count && close_result == 0;
}

/* FPU and SIMD state is not initialized, so interrupt handlers use only general-purpose registers. */
static void __attribute__((interrupt))
irq0_handler(struct interrupt_frame *frame)
{
	(void)frame;

	if (!mode_initialized) {
		vm_mode = inb(SHARED_BUFFER_PORT);
		mode_initialized = 1;

		if (vm_mode == VM_MODE_WRITER)
			serial_write("Mode: writer\n");
		else
			serial_write("Mode: reader\n");

		return;
	}

	transfer_succeeded = vm_mode == VM_MODE_WRITER
		? write_shared_buffer()
		: read_shared_buffer();
	transfer_finished = 1;
}

static void set_idt_gate(unsigned n, void (*handler)(struct interrupt_frame *))
{
	uint64_t addr = (uint64_t)(uintptr_t)handler;
	idt[n].offset_low  = addr & 0xFFFF;
	idt[n].selector    = 0x08;  /* 64-bit code segment */
	idt[n].ist         = 0;
	idt[n].type_attr   = 0x8E;  /* P=1, DPL=0, 64-bit interrupt gate */
	idt[n].offset_mid  = (addr >> 16) & 0xFFFF;
	idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
	idt[n].reserved    = 0;
}

void init_idt(void)
{
	struct dt_ptr p;

	set_idt_gate(32, irq0_handler);

	p.limit = sizeof(idt) - 1;
	p.base  = (uint64_t)(uintptr_t)idt;
	asm volatile("lidt %0" : : "m"(p) : "memory");
}

int communication_finished(void)
{
	return transfer_finished != 0;
}

int communication_succeeded(void)
{
	return transfer_succeeded != 0;
}
