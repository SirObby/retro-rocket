#ifndef __IO_H__
#define __IO_H__

/* Output one byte to an I/O port. Privileged operation. */
static inline void outb(int port, unsigned char value)
{
	asm volatile("outb %b0, %w1" : : "a"(value), "Nd"(port));
}

/* Read a byte from an I/O port. Privileged operation. */
static inline unsigned char inb(int port)
{
	unsigned char value;
	asm volatile("inb %w1, %b0" : "=a"(value) : "Nd"(port));
	return value;
}

static inline void interrupts_on()
{
	asm volatile("sti");
}

static inline void interrupts_off()
{
	asm volatile("cli");
}

#endif
