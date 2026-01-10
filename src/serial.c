#include <types.h>
#include <x86.h>
#define COM1 0x3F8

void serial_init(void) {
  outb(COM1 + 1, 0x00); // Disable interrupts
  outb(COM1 + 3, 0x80); // Enable DLAB
  outb(COM1 + 0, 0x03); // Divisor low byte (38400 baud)
  outb(COM1 + 1, 0x00); // Divisor high byte
  outb(COM1 + 3, 0x03); // 8 bits, no parity, one stop bit
  outb(COM1 + 2, 0xC7); // Enable FIFO
  outb(COM1 + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

void serial_putc(char c) {
  while ((inb(COM1 + 5) & 0x20) == 0)
    ; // Wait for transmit buffer empty
  outb(COM1, c);
}
