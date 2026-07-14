#ifndef IO_H
#define IO_H

#include <stdint.h>

/*
 * IN and OUT transfer data through AL/EAX. Constraint "a" selects that register,
 * while "Nd" uses an immediate port number when possible and DX otherwise.
 */
static inline void outb(uint16_t port, uint8_t value)
{
	asm volatile(
		"outb %0, %1"
		:
		: "a"(value), "Nd"(port)
		: "memory"
	);
}

static inline uint8_t inb(uint16_t port)
{
	uint8_t value;

	asm volatile(
		"inb %1, %0"
		: "=a"(value)
		: "Nd"(port)
		: "memory"
	);

	return value;
}

// File system 4
static inline void outl(uint16_t port, uint32_t value)
{
	asm volatile(
		"outl %0, %1"
		:
		: "a"(value), "Nd"(port)
		: "memory"
	);
}

static inline uint32_t inl(uint16_t port)
{
	uint32_t value;

	asm volatile(
		"inl %1, %0"
		: "=a"(value)
		: "Nd"(port)
		: "memory"
	);

	return value;
}

#endif /* IO_H */
