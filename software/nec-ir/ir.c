/* -*- c-basic-offset: 8 -*-
 * ir.c -- IR receiver (VS1838B) sample
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

/* Timer count for clk/1024 */
#if 0
#define T_MS(ms) ((((F_CPU / 1000UL) * (ms)) / 1024) - 1)
#define T_US(us) ((((F_CPU / 1000000UL) * (us)) / 1024) - 1)
#endif

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

static void timer_init(void)
{
	TCCR1A = 0x00;
	TCCR1B = _BV(CS10) | _BV(CS12); // clk / 1024
	TCCR1C = 0x00;
	TCNT1  = 0x00;
	TIMSK1 = 0x00;
}

static int decode_value(void)
{
	int pw;

	TCNT1 = 0;
	while (!get_port(IR_IN))
		;
	pw = TCNT1;

	if (pw >= 7) { /* T_US(560) */
		TCNT1 = 0;
		while (get_port(IR_IN))
			;
		pw = TCNT1;
		if (pw >= 25) /* T_MS(1.69)) */
			return 1;
		if (pw >= 7) /* T_US(560) */
			return 0;
	}
	/* Invalid... */
	return 0;
}

static void decode_data(int *customer_code_p, unsigned char *cmd_p)
{
	int i;
	int customer_code = 0;
	unsigned char cmd_l = 0, cmd_h = 0;

	for (i = 0; i < 16; i++)
		customer_code |= (decode_value() << i);
	for (i = 0; i < 8; i++)
		cmd_l |= (decode_value() << i);
	for (i = 0; i < 8; i++)
		cmd_h |= (decode_value() << i);
	cmd_h = ~cmd_h & 0xff;

	if (cmd_l == cmd_h) {
		*customer_code_p = customer_code;
		*cmd_p = cmd_l;
		return;
	}

	*customer_code_p = 0xff;
	*cmd_p = 0xff;
}

static void decode_ir(int *customer_code_p, unsigned char *cmd_p)
{
	int pw;

	/* Wait for pulse... */
	TCNT1 = 0;
	while (get_port(IR_IN)) {
		if (TCNT1 >= 1563) { /* T_MS(100) */
			*customer_code_p = 0;
			*cmd_p = 0;
			return;
		}
	}

	/* Measure pulse */
	TCNT1 = 0;
	while (!get_port(IR_IN))
		;
	pw = TCNT1;

	if (pw < 140) { /* T_MS(9) */
		*customer_code_p = 0;
		*cmd_p = 0;
		return;
	}

	/* Measure interval */
	TCNT1 = 0;
	while (get_port(IR_IN))
		;
	pw = TCNT1;

	if (pw >= 68) { /* T_MS(4.5) */
		/* Got leader */
		decode_data(customer_code_p, cmd_p);
		return;
	} else if (pw >= 33) { /* T_MS(2.25) */
		TCNT1 = 0;
		while (!get_port(IR_IN))
			;
		pw = TCNT1;
		if (pw >= 7) { /* T_US(560) */
			/* Repeat code */
			return;
		}
	}

	{
		unsigned char buf[32];
		sprintf_P((char *)buf, PSTR("invalid pw = %d\n"), pw);
		uart_puts(buf);
	}

	*customer_code_p = 0;
	*cmd_p = 0xff;
}

int main(int argc, char **argv)
{
	unsigned int ubrr;
	unsigned char buf[32];
	int customer_code = 0;
	unsigned char cmd = 0;

	timer_init();
	set_direction(IR_IN, 0);

	ubrr = (F_CPU / (16UL * BAUD_RATE)) - 1;
	UBRR0H = (ubrr >> 8);
	UBRR0L = (ubrr & 0xff);
	UCSR0B = _BV(TXEN0) | _BV(RXEN0); /* Enable TX/RX */

	for (;;) {
		decode_ir(&customer_code, &cmd);
		if (customer_code == 0 && cmd == 0xff) {
			strcpy_P((char *)buf, PSTR("Invalid protocol\n"));
			uart_puts(buf);
			continue;
		} else if (customer_code == 0xff && cmd == 0xff) {
			strcpy_P((char *)buf, PSTR("cmd_l and cmd_h mismatch\n"));
			uart_puts(buf);
			continue;
		} else if (customer_code == 0 && cmd == 0) {
			continue;
		}
		sprintf_P((char *)buf, PSTR("customer_code = %04x cmd = %02x\n"), customer_code, cmd);
		uart_puts(buf);
	}

	return 0;
}
