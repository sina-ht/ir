/* -*- c-basic-offset: 8 -*-
 * ir-recv.c -- IR receiver (VS1838B) sample
 * Copyright (C) 2014 Hiroshi Takekawa
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * ATTENTION: GPL version 2 only. You cannot apply any later version.
 * This situation may change in the future.
 */

#include <stdio.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#define BAUD_RATE 9600UL
#define IR_IN 8

static void set_direction(int pin, int d)
{
	if (pin >= 0 && pin < 8) {
		if (d)
			DDRD |= (1 << pin);
		else
			DDRD &= ~(1 << pin);
	} else if (pin >= 8 && pin <= 13) {
		pin -= 8;
		if (d)
			DDRB |= (1 << pin);
		else
			DDRB &= ~(1 << pin);
	}
}

static void timer_init(void)
{
	TCCR1A = 0x00;
	TCCR1B = _BV(CS10) | _BV(CS12); // clk / 1024
	TCCR1C = 0x00;
	TCNT1  = 0x00;
	TIMSK1 = 0x00;
}

static void uart_putc(unsigned char c)
{
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
}

static void uart_puts(unsigned char *p)
{
	unsigned char c;

	while ((c = *p++) != '\0') {
		uart_putc(c);
		if (c == '\n')
			uart_putc('\r');
	}
}

static int get_port(int pin)
{
	if (pin >= 0 && pin < 8) {
		return (PIND & (1 << pin)) ? 1 : 0;
	} else if (pin >= 8 && pin <= 13) {
		pin -= 8;
		return (PINB & (1 << pin)) ? 1 : 0;
	}
	return -1;
}

#define IR_RECV_BUF_SIZE 100

static void receive_ir(void)
{
	unsigned char buf[32];
	int pw;
	int irdat[IR_RECV_BUF_SIZE];
	int i = 0;

	/* Wait for pulse... */
	while (get_port(IR_IN))
		;
	TCNT1 = 0;

	/* Measure pulse */
	while (!get_port(IR_IN))
		;
	pw = TCNT1;
	TCNT1 = 0;
	/* Cut noise */
	if (pw < 50)
		return;

	irdat[i++] = pw;

	for (;;) {
		/* Measure interval */
		while (get_port(IR_IN)) {
			if (TCNT1 >= 1563) { /* 100ms@16MHz */
				int j;

				for (j = 0; j < i; j++) {
					sprintf_P((char *)buf, PSTR("%d,"), irdat[j]);
					uart_puts(buf);
				}
				strcpy_P((char *)buf, PSTR("0\n"));
				uart_puts(buf);
				return;
			}
		}

		if (i >= IR_RECV_BUF_SIZE)
			return;
		irdat[i++] = TCNT1;
		TCNT1 = 0;

		/* Measure pulse */
		while (!get_port(IR_IN))
			;
		if (i >= IR_RECV_BUF_SIZE)
			return;
		irdat[i++] = TCNT1;
		TCNT1 = 0;
	}
}

int main(int argc, char **argv)
{
	unsigned int ubrr;

	timer_init();
	set_direction(IR_IN, 0);

	ubrr = (F_CPU / (16UL * BAUD_RATE)) - 1;
	UBRR0H = (ubrr >> 8);
	UBRR0L = (ubrr & 0xff);
	UCSR0B = _BV(TXEN0) | _BV(RXEN0); /* Enable TX/RX */

	for (;;) {
		receive_ir();
	}

	return 0;
}
