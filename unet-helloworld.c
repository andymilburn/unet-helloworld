/*
 * unet-helloworld
 *
 * Example using unet sockets
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "unet-helloworld.h"

static const char usage_synopsis[] = PACKAGE " [options] <input file>";
static const char usage_short_opts[] = "hv";
static struct option const usage_long_opts[] = {
	{ "help",	      no_argument, NULL, 'h'},
	{ "version",	      no_argument, NULL, 'v'},
	{ NULL,		      no_argument, NULL, 0x0},
};
static const char * const usage_opts_help[] = {
	"\n\tPrint this help and exit",
	"\n\tPrint version and exit",
	NULL,
};

static void print_usage(const char *errmsg, const char *synopsis,
		const char *short_opts, struct option const long_opts[],
		const char * const opts_help[]) __attribute__((noreturn));
static void usage(const char *errmsg) __attribute__((noreturn));

static void print_usage(const char *errmsg, const char *synopsis,
		const char *short_opts, struct option const long_opts[],
		const char * const opts_help[])
{
	FILE *fp = errmsg ? stderr : stdout;
	const char a_arg[] = "<arg>";
	size_t a_arg_len = strlen(a_arg) + 1;
	size_t i;
	int optlen;

	fprintf(fp,
		"Usage: %s\n"
		"\n"
		"Options: -[%s]\n", synopsis, short_opts);

	/* prescan the --long opt length to auto-align */
	optlen = 0;
	for (i = 0; long_opts[i].name; ++i) {
		/* +1 is for space between --opt and help text */
		int l = strlen(long_opts[i].name) + 1;
		if (long_opts[i].has_arg == required_argument)
			l += a_arg_len;
		if (optlen < l)
			optlen = l;
	}

	for (i = 0; long_opts[i].name; ++i) {
		/* helps when adding new applets or options */
		assert(opts_help[i] != NULL);

		/* first output the short flag if it has one */
		if (long_opts[i].val > '~')
			fprintf(fp, "      ");
		else
			fprintf(fp, "  -%c, ", long_opts[i].val);

		/* then the long flag */
		if (long_opts[i].has_arg == no_argument)
			fprintf(fp, "--%-*s", optlen, long_opts[i].name);
		else
			fprintf(fp, "--%s %s%*s", long_opts[i].name, a_arg,
				(int)(optlen - strlen(long_opts[i].name) - a_arg_len), "");

		/* finally the help text */
		fprintf(fp, "%s\n", opts_help[i]);
	}

	if (errmsg) {
		fprintf(fp, "\nError: %s\n", errmsg);
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}

static void usage(const char *errmsg)
{
	print_usage(errmsg, usage_synopsis, usage_short_opts,
			usage_long_opts, usage_opts_help);
}

int main(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt_long(argc, argv, usage_short_opts,
				  usage_long_opts, NULL)) != EOF) {
		switch (opt) {
		case 'v':
			printf("Version: %s\n", PACKAGE_VERSION);
			exit(EXIT_SUCCESS);
		case 'h':
			usage(NULL);
		default:
			usage("unknown option");
		}
	}

	printf("Hello uNet World...\n");

	return 0;
}
