/*
 * sendload
 *
 * must be run as root to set iopl and use inb/outb
 *
 * assumes parallel port is at PORTADDRESS and Loader has been loaded on the
 * Mailstation and is running
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <err.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <machine/sysarch.h>
#include <machine/pio.h>

#include "tribble.h"

void
timeout(int sig)
{
	printf("\ntimed out, aborting\n");
	fflush(stdout);

	_exit(1);
}

void
usage(void)
{
	printf("usage: %s [-r] <file to send>\n", getprogname());
	exit(1);
}

int
main(int argc, char *argv[])
{
	FILE *pFile;
	struct stat sb;
	unsigned int sent = 0, size = 0, raw = 0;
	int ch;
	char *fn;

	while ((ch = getopt(argc, argv, "r")) != -1) {
		switch (ch) {
		case 'r':
			raw = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	if (geteuid() != 0)
		errx(1, "must be run as root");

#ifdef __OpenBSD__
#ifdef __amd64__
	if (amd64_iopl(1) != 0)
		errx(1, "amd64_iopl failed (is machdep.allowaperture=1?)");
#elif defined(__i386__)
	if (i386_iopl(1) != 0)
		errx(1, "i386_iopl failed (is machdep.allowaperture=1?)");
#endif
#endif

	fn = argv[0];
	pFile = fopen(fn, "rb");
	if (!pFile)
		err(1, "open: %s", fn);

	if (fstat(fileno(pFile), &sb) != 0)
		err(1, "fstat: %s", fn);

	/* we're never going to send huge files */
	size = (unsigned int)sb.st_size;

	signal(SIGALRM, timeout);

	printf("sending %s (%d bytes)...", fn, size);
	fflush(stdout);

	/* loader expects two bytes, the low and then high of the file size */
	if (!raw) {
		alarm(10);
		sendbyte(size & 0xff);
		sendbyte((size >> 8) & 0xff);
	}

	while (sent < size) {
		alarm(1);
		sendbyte(fgetc(pFile));

		if (sent++ == 0)
			printf("\n");

		if (sent % (raw ? 64 : 1024) == 0 || sent == size) {
			printf("\rsent: %07d/%07d", sent, size);
			fflush(stdout);
		}
	}
	fclose(pFile);

	printf("\n");

	return 0;
}
