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

#ifdef __linux__
#include <sys/io.h>
#else
#include <machine/sysarch.h>
#include <machine/pio.h>
#endif

#include "tribble.h"
#include "progname.h"

void
usage(void)
{
	printf("usage: %s [-dr] <file to send>\n", getprogname());
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
		case 'd':
			tribble_debug = 1;
			break;
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

#ifdef __linux__
        ioperm(DATA,1,1);
        ioperm(STATUS,1,1);
        ioperm(CONTROL,1,1);
#endif

	fn = argv[0];
	pFile = fopen(fn, "rb");
	if (!pFile)
		err(1, "open: %s", fn);

	if (fstat(fileno(pFile), &sb) != 0)
		err(1, "fstat: %s", fn);

	/* we're never going to send huge files */
	size = (unsigned int)sb.st_size;

	printf("sending %s (%d bytes)...", fn, size);
	fflush(stdout);

	/* loader expects two bytes, the low and then high of the file size */
	if (!raw) {
		if (sendbyte(size & 0xff) != 0)
			errx(1, "sendbyte failed");
		if (sendbyte((size >> 8) & 0xff) != 0)
			errx(1, "sendbyte failed");
	}

	while (sent < size) {
		if (sendbyte(fgetc(pFile)) != 0)
			errx(1, "sendbyte failed at %d/%d", sent, size);

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
