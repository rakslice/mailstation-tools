#include <stdio.h>
#include <sys/types.h>
#include <machine/pio.h>

#include "tribble.h"

int tribble_debug = 0;

unsigned int
havetribble(void)
{
	int tries;

	/* drop busy */
	outb(DATA, 0);

	/* wait for (inverted) strobe */
	for (tries = 0; tries < 1024; tries++)
		if ((inb(STATUS) & stbin) == 0)
			/*
			 * leave busy dropped, assume recvtribble() will deal
			 * with it
			 */
			return 1;

	/* re-raise busy */
	outb(DATA, bsyout);

	return 0;
}

int
recvtribble(void)
{
	unsigned char b;
	unsigned int tries;

	/* drop busy */
	outb(DATA, 0);

	/* wait for (inverted) strobe */
	tries = 0;
	while ((inb(STATUS) & stbin) != 0) {
		if (++tries >= 500000) {
			/* raise busy/ack */
			outb(DATA,bsyout);
			return -1;
		}
	}

	/* grab tribble */
	b = (inb(STATUS) >> 3) & tribmask;

	/* raise busy/ack */
	outb(DATA,bsyout);

	/* wait for (inverted) UNstrobe */
	tries = 0;
	while ((inb(STATUS) & stbin) == 0) {
		if (++tries >= 500000)
			return -1;
	}

	return b;
}

int
recvbyte(void)
{
	char c, t;

	if ((t = recvtribble()) == -1)
		return -1;
	c = t;

	if ((t = recvtribble()) == -1)
		return -1;
	c += (t << 3);

	if ((t = recvtribble()) == -1)
		return -1;
	c += ((t & dibmask) << 6);

	return c;
}

int
sendtribble(unsigned char b)
{
	unsigned int tries;
	int ret = 0;

	/* raise busy */
	outb(DATA, bsyout);

	/* wait for mailstation to drop busy */
	tries = 0;
	while ((inb(STATUS) & bsyin) != 0) {
		if (++tries >= 500000) {
			ret = 1;
			goto sendtribble_out;
		}
	}

	/* send tribble */
	outb(DATA, b & tribmask);

	/* strobe */
	outb(DATA, (b & tribmask) | stbout);

	/* wait for ack */
	tries = 0;
	while ((inb(STATUS) & bsyin) == 0) {
		if (++tries >= 500000) {
			ret = 1;
			goto sendtribble_out;
		}
	}

	/* unstrobe */
	outb(DATA, 0);

sendtribble_out:

	/* raise busy/ack */
	outb(DATA, bsyout);

	return ret;
}

int
sendbyte(unsigned char b)
{
	if (sendtribble(b) != 0)
		return 1;
	if (sendtribble(b >> 3) != 0)
		return 2;
	if (sendtribble(b >> 6) != 0)
		return 3;

	return 0;
}
