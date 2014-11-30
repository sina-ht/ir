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
#define IR_OUT 3
#define TRIG 9
#define ECHO 10

/* Timer count for clk/1024 */
#if 0
static unsigned int to_us(unsigned int t)
{
	return t * 1024 / (F_CPU / 1000000UL);
}
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

static int set_port(int pin, int d)
{
	if (pin >= 0 && pin < 8) {
		if (d)
			PORTD |= (1 << pin);
		else
			PORTD &= ~(1 << pin);
	} else if (pin >= 8 && pin <= 13) {
		pin -= 8;
		if (d)
			PORTB |= (1 << pin);
		else
			PORTB &= ~(1 << pin);
	}

	return 0;
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

static void pwm_init(void)
{
	TCCR2A = _BV(WGM21) | _BV(WGM20);
	TCCR2B = _BV(WGM22) | _BV(CS21); // clk / 8
	TCNT2 = 0;
	OCR2A = 53; // 16000000/8/38000 = 52.63
	OCR2B = 17; // duty 1/3
}

static void pwm_set(int on)
{
	if (on) {
		TCNT2 = 0;
		TCCR2A |= _BV(COM2B1); // Enable output on Pin 3
	} else {
		TCCR2A &= ~_BV(COM2B1);
	}
}

static void send_ir(void)
{
	int i, v = 1;
	unsigned int intervals[100] = { 0 }; /* Must be replaced with the ir-recv result. */

	for (i = 0; intervals[i]; i++) {
		pwm_set(v);
		TCNT1 = 0;
		while (TCNT1 < intervals[i])
			;
		v = 1 - v;
	}
	pwm_set(0);
}

int main(int argc, char **argv)
{
	unsigned int ubrr;
	unsigned char buf[32];
	int pw;

	timer_init();
	pwm_init();
	set_direction(IR_OUT, 1);
	set_direction(TRIG, 1);
	set_direction(ECHO, 0);

	ubrr = (F_CPU / (16UL * BAUD_RATE)) - 1;
	UBRR0H = (ubrr >> 8);
	UBRR0L = (ubrr & 0xff);
	UCSR0B = _BV(TXEN0) | _BV(RXEN0); /* Enable TX/RX */

	strcpy_P((char *)buf, PSTR("IR Send with HC-SR04 started\n"));
	uart_puts(buf);
	set_port(TRIG, 0);
	_delay_us(20);

	for (;;) {
		set_port(TRIG, 1);
		_delay_us(20);
		set_port(TRIG, 0);
		/* Wait for echo */
		while (!get_port(ECHO))
			;
		/* Measure echo */
		TCNT1 = 0;
		while (get_port(ECHO))
			;
		pw = TCNT1;
		if (pw < 256) {
			/* distance = (1/(F_CPU/1024)) * pw * 34000/2 */
			sprintf_P((char *)buf, PSTR("pw = %d, distance = %d\n"), pw, pw * 136 / 125);
			uart_puts(buf);
			if (pw < 5) {
				strcpy_P((char *)buf, PSTR("Send IR\n"));
				uart_puts(buf);
				send_ir();
			}
		}
		_delay_ms(1000);
	}


	return 0;
}
