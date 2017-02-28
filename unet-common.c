/*
 * unet-common
 *
 * common code
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <getopt.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "unet-common.h"

static const char *protocol_txt[] = {
	[protocol_unknown]	= "*unknown*",
	[protocol_udp]		= "udp",
	[protocol_tcp]		= "tcp",
	[protocol_ip_raw]	= "ip-raw",
	[protocol_unet]		= "unet",
	[protocol_unet_raw]	= "unet-raw",
};

const char *protocol_to_txt(enum protocol p)
{
	if (p < protocol_udp || p > protocol_unet)
		return NULL;

	return protocol_txt[p];
}

enum protocol txt_to_protocol(const char *txt)
{
	enum protocol p;

	if (!txt)
		return -1;

	for (p = protocol_udp; p <= protocol_unet_raw; p++) {
		if (!strcmp(protocol_txt[p], txt))
			return p;
	}

	return protocol_unknown;
}

void print_usage(const char *errmsg, const char *synopsis,
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
		if (!long_opts[i].val || long_opts[i].val > '~')
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

