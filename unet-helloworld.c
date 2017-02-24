/*
 * unet-helloworld
 *
 * Example using unet sockets
 */
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

#include "unet-helloworld.h"

enum protocol {
	protocol_unknown,
	protocol_udp,
	protocol_tcp,
	protocol_unet
};

static inline bool protocol_is_ipv4(enum protocol p)
{
	return p == protocol_tcp || p == protocol_udp;
}

static inline bool protocol_is_unet(enum protocol p)
{
	return p == protocol_unet;
}

bool server_mode = true;
enum protocol protocol = protocol_udp;
char *endpoint = NULL;
unsigned int endpoint_index = 0;
char *bind_endpoint = NULL;
unsigned int bind_endpoint_index = 0;

static const char *protocol_txt[] = {
	[protocol_unknown]  = "*unknown*",
	[protocol_udp]  = "udp",
	[protocol_tcp]  = "tcp",
	[protocol_unet] = "unet",
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

	for (p = protocol_udp; p <= protocol_unet; p++) {
		if (!strcmp(protocol_txt[p], txt))
			return p;
	}

	return protocol_unknown;
}

static const char usage_synopsis[] = PACKAGE " [options] <input file>";
static const char usage_short_opts[] = "scp:e:i:hv";
static struct option const usage_long_opts[] = {
	{ "server",		no_argument,       NULL, 's'},
	{ "client",		no_argument,       NULL, 'c'},
	{ "protocol",		required_argument, NULL, 'p'},
	{ "endpoint",		required_argument, NULL, 'e'},
	{ "endpoint-index",	required_argument, NULL, 'i'},
	{ "bind-endpoint",	required_argument, NULL,  0 },
	{ "bind-endpoint-index",required_argument, NULL,  0 },
	{ "help",		no_argument,       NULL, 'h'},
	{ "version",		no_argument,       NULL, 'v'},
	{ NULL,			no_argument,       NULL, 0x0},
};
static const char * const usage_opts_help[] = {
	"\n\tServer mode (default)",
	"\n\tClient mode",
	"\n\tProtocol one of (udp|tcp|unet) udp is default",
	"\n\tEndpoint (hostname or ip addr for udp/tcp, unet addr for unet)",
	"\n\tEndpoint index (port# for udp/tcp, message type for unet)",
	"\n\tBind endpoint (hostname or ip addr for udp/tcp, unet addr for unet)",
	"\n\tBind endpoint index (port# for udp/tcp, message type for unet)",
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
	const char *optname;
	int s, opt, optidx;
	int af;
	struct in_addr ipv4_addr, ipv4_bindaddr;
	char *e_txt = NULL, *be_txt = NULL;

	ipv4_addr.s_addr = INADDR_ANY;
	ipv4_bindaddr.s_addr = INADDR_ANY;

	while ((opt = getopt_long(argc, argv, usage_short_opts,
				  usage_long_opts, &optidx)) != EOF) {
		switch (opt) {
		case 0:
			optname = usage_long_opts[optidx].name;
			if (!strcmp(optname, "bind-endpoint"))
				bind_endpoint = optarg;
			else if (!strcmp(optname, "bind-endpoint-index"))
				bind_endpoint_index = atoi(optarg);
			else 
				usage("unknown long option");
			break;
		case 's':
			server_mode = true;
			break;
		case 'c':
			server_mode = false;
			break;
		case 'e':
			endpoint = optarg;
			break;
		case 'i':
			endpoint_index = atoi(optarg);
			break;
		case 'p':
			protocol = txt_to_protocol(optarg);
			if (protocol == protocol_unknown)
				usage("bad protocol argument");
			break;
		case 'v':
			printf("Version: %s\n", PACKAGE_VERSION);
			exit(EXIT_SUCCESS);
		case 'h':
			usage(NULL);
		default:
			usage("unknown option");
		}
	}

	if (protocol_is_unet(protocol)) {
		printf("Doesn't work yet\n");
		exit(EXIT_SUCCESS);
	}
	if (protocol_is_ipv4(protocol)) {
		af = AF_INET;
		if (!server_mode) {
			/* client mode and no endpoint? loopback */
			if (!endpoint)
				endpoint = "127.0.0.1";
			printf("client\n");
		} else {
			/* server mode and no endpoint? use any */
			if (!endpoint)
				endpoint = "0.0.0.0";
			printf("server\n");
		}

		/* no index? use an ephemeral port */
		if (!endpoint_index)
			endpoint_index = 49152 + (rand() % (65536 - 49152));

		/* no index? use an ephemeral port */
		if (bind_endpoint && !bind_endpoint_index)
			bind_endpoint_index = 49152 + (rand() % (65536 - 49152));

		if (!bind_endpoint)
			bind_endpoint = "0.0.0.0";

		if (endpoint_index >= (1 << 16)) {
			errno = EINVAL;
			perror("bad client endpoint index option");
			exit(EXIT_FAILURE);
		}

		s = inet_pton(af, endpoint, &ipv4_addr);
		if (s != 1) {
			if (s == 0)
				errno = -EINVAL;
			perror("bad endpoint address");
			exit(EXIT_FAILURE);
		}

		s = inet_pton(af, bind_endpoint, &ipv4_bindaddr);
		if (s != 1) {
			if (s == 0)
				errno = -EINVAL;
			perror("bad endpoint address");
			exit(EXIT_FAILURE);
		}

		s = asprintf(&e_txt, "%s:%u", endpoint, endpoint_index); 
		s = asprintf(&be_txt, "%s:%u", bind_endpoint, bind_endpoint_index); 
	} else
		usage("Unknown protocol type");

	printf("Hello uNet World... %s %s ep=%s be=%s\n",
			server_mode ? "server" : "client",
			protocol_to_txt(protocol),
			e_txt  ? e_txt  : "<NULL>",
			be_txt ? be_txt : "<NULL>");
	return 0;
}
