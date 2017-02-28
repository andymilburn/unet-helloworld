/*
 * unet-helloworld
 *
 * Example using unet sockets
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
#include "unet-helloworld.h"

bool server_mode = true;
enum protocol protocol = protocol_udp;
char *endpoint = NULL;
unsigned int endpoint_index = 0;
char *bind_endpoint = NULL;
unsigned int bind_endpoint_index = 0;

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
	"\n\tProtocol one of (udp|tcp|ip-raw|unet|unet-raw) udp is default",
	"\n\tEndpoint (hostname or ip addr for udp/tcp, unet addr for unet)",
	"\n\tEndpoint index (port# for udp/tcp, message type for unet)",
	"\n\tBind endpoint (hostname or ip addr for udp/tcp, unet addr for unet)",
	"\n\tBind endpoint index (port# for udp/tcp, message type for unet)",
	"\n\tPrint this help and exit",
	"\n\tPrint version and exit",
	NULL,
};

static void usage(const char *errmsg)
{
	print_usage(errmsg, usage_synopsis, usage_short_opts,
			usage_long_opts, usage_opts_help);
}

int main(int argc, char *argv[])
{
	const char *optname;
	int s, cfd, err, opt, optidx, len, i;
	int af = 0, st = 0, sp = 0;
	char *e_txt = NULL, *be_txt = NULL;
	bool has_bind = false, has_addr = false;
	union generic_sockaddr sa, bsa, psa;
	char my_addr[256], peer_addr[256], buf[65536];
	const char *p;
	char *uatxt;
	unsigned int peer_index;
	socklen_t slen;

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
		af = AF_UNET;
		st = SOCK_DGRAM;
		sp = 0;
		has_bind = !!bind_endpoint;
		has_addr = !!endpoint;

		if (!has_bind)
			bind_endpoint = "0.0";
		if (!has_addr)
			endpoint = "0.1";

		/* no index? use an ephemeral port */
		if (!endpoint_index)
			endpoint_index = 65536 + (rand() % 65536);

		/* no index? use an ephemeral port */
		if (!bind_endpoint_index)
			bind_endpoint_index = 65536 + (rand() % 65536);

		sa.sunet.sunet_family = af;
		sa.sunet.sunet_addr.message_type = endpoint_index;
		err = unet_str_to_addr(endpoint, strlen(endpoint), &sa.sunet.sunet_addr.addr);
		if (err == -1) {
			perror("bad unet endpoint address");
			exit(EXIT_FAILURE);
		}

		bsa.sunet.sunet_family = af;
		bsa.sunet.sunet_addr.message_type = bind_endpoint_index;
		err = unet_str_to_addr(bind_endpoint, strlen(bind_endpoint), &bsa.sunet.sunet_addr.addr);
		if (err == -1) {
			perror("bad unet bind endpoint address");
			exit(EXIT_FAILURE);
		}

#if 0
		len = asprintf(&e_txt, "%s/%u", endpoint, endpoint_index);
		len = asprintf(&be_txt, "%s/%u", bind_endpoint, bind_endpoint_index);
#endif

	} else if (protocol_is_ipv4(protocol)) {
		af = AF_INET;
		if (protocol_is_dgram(protocol)) {
			st = SOCK_DGRAM;
			sp = IPPROTO_UDP;
		} else if (protocol_is_stream(protocol)){
			st = SOCK_STREAM;
			sp = IPPROTO_TCP;
		} else if (protocol_is_raw(protocol)) {
			st = SOCK_RAW;
			sp = IPPROTO_UDP;
		} else {
			errno = -EINVAL;
			perror("bad protocol type (not dgram/stream/raw)?");
			exit(EXIT_FAILURE);
		}

		has_bind = !!bind_endpoint;
		has_addr = !!endpoint;

		if (!has_bind)
			bind_endpoint = "0.0.0.0";
		if (!has_addr)
			endpoint = "127.0.0.1";

#if 0
		/* no index? use an ephemeral port */
		if (!endpoint_index && server_mode)
			endpoint_index = 49152 + (rand() % (65536 - 49152));

		/* no index? use an ephemeral port */
		if (!bind_endpoint_index)
			bind_endpoint_index = 49152 + (rand() % (65536 - 49152));

		if (endpoint_index > 65535 || bind_endpoint_index > 65535) {
			errno = EINVAL;
			perror("bad endpoint index(es) option(s)");
			exit(EXIT_FAILURE);
		}
#endif

		sa.sin.sin_family = af;
		sa.sin.sin_port = endpoint_index ? htons(endpoint_index) : 0;
		s = inet_pton(af, endpoint, &sa.sin.sin_addr);
		if (s != 1) {
			if (s == 0)
				errno = -EINVAL;
			perror("bad endpoint address");
			exit(EXIT_FAILURE);
		}

		bsa.sin.sin_family = af;
		bsa.sin.sin_port = bind_endpoint_index ? htons(bind_endpoint_index) : 0;
		s = inet_pton(af, bind_endpoint, &bsa.sin.sin_addr);
		if (s != 1) {
			if (s == 0)
				errno = -EINVAL;
			perror("bad bind endpoint address");
			exit(EXIT_FAILURE);
		}
#if 0
		len = asprintf(&e_txt, "%s:%u", endpoint, endpoint_index);
		len = asprintf(&be_txt, "%s:%u", bind_endpoint, bind_endpoint_index);
#endif

	} else
		usage("Unknown protocol type");

	printf("Hello uNet World... %s %s\n",
			server_mode ? "server" : "client",
			protocol_to_txt(protocol));

	s = socket(af, st, sp);
	if (s == -1) {
		perror("failed to create socket");
		exit(EXIT_FAILURE);
	}

	if (server_mode || has_bind) {
		err = bind(s, &bsa.sa, sizeof(bsa));
		if (err == -1) {
			perror("failed to bind\n");
			exit(EXIT_FAILURE);
		}

		/* now get sockname so that we know ports etc */
		slen = sizeof(bsa);
		memset(&bsa, 0, sizeof(bsa));
		err = getsockname(s, &bsa.sa, &slen);
		if (err == -1) {
			perror("failed to getsockname\n");
			exit(EXIT_FAILURE);
		}
		p = NULL;
		if (protocol_is_ipv4(protocol)) {
			bind_endpoint_index = ntohs(bsa.sin.sin_port);
			memset(my_addr, 0, sizeof(my_addr));
			p = inet_ntop(af, &bsa.sin.sin_addr, my_addr, sizeof(my_addr));
		} else if (protocol_is_unet(protocol)) {
			bind_endpoint_index = bsa.sunet.sunet_addr.message_type;
			uatxt = unet_addr_to_str(&bsa.sunet.sunet_addr.addr);
			if (uatxt) {
				strncpy(my_addr, uatxt, sizeof(my_addr));
				free(uatxt);
				p = my_addr;
			}
		}
		if (!p) {
			perror("failed to convert my sockaddr to ascii\n");
			exit(EXIT_FAILURE);
		}

		len = asprintf(&be_txt, "%s:%u", my_addr, bind_endpoint_index);

		printf("bound to %s\n", be_txt);
	}

	if (server_mode && (st == SOCK_STREAM || st == SOCK_SEQPACKET)) {
		err = listen(s, 5);
		if (err == -1) {
			perror("failed to listen\n");
			exit(EXIT_FAILURE);
		}
	}
	if (server_mode) {
		printf("Ctrl-C to exit\n");
		if (st == SOCK_STREAM || st == SOCK_SEQPACKET) {
			slen = sizeof(psa);
			while ((cfd = accept(s, &psa.sa, &slen)) != -1) {
				if (protocol_is_ipv4(protocol)) {
					peer_index = ntohs(psa.sin.sin_port);
					p = inet_ntop(af, &psa.sin.sin_addr,
					              peer_addr,
						      sizeof(peer_addr));
					if (!p) {
						perror("failed to inet_ntop\n");
						exit(EXIT_FAILURE);
					}
				} else {
					/* not yet */
					peer_addr[0] = '\0';
					peer_index = 0;
				}
				printf("Accepted connection from %s:%u\n",
						peer_addr, peer_index);
				while ((len = recv(cfd, buf, sizeof(buf) - 1, 0)) > 0) {

					buf[len] = '\0';
					printf("recv from %s:%u: %s\n", peer_addr, peer_index, buf);
#if 0
					len = send(cfd, buf, len, 0);
					if (len == -1) {
						perror("failed to send\n");
						exit(EXIT_FAILURE);
					}
#endif
				}
				if (len == -1) {
					perror("failed to recv\n");
					exit(EXIT_FAILURE);
				}
				if (len == 0)
					printf("Connection from %s:%u closed\n",
							peer_addr, peer_index);
				slen = sizeof(psa);
			}
			if (cfd == -1) {
				perror("failed to accept\n");
				exit(EXIT_FAILURE);
			}
		} else if (st == SOCK_DGRAM || st == SOCK_RAW) {
			slen = sizeof(psa);
			while ((len = recvfrom(s, buf, sizeof(buf) -1, 0,
					       &psa.sa, &slen)) > 0) {

				buf[len] = '\0';
				if (protocol_is_ipv4(protocol) &&
				    !protocol_is_raw(protocol)) {
					peer_index = ntohs(psa.sin.sin_port);
					p = inet_ntop(af, &psa.sin.sin_addr,
					              peer_addr,
						      sizeof(peer_addr));
					if (!p) {
						perror("failed to inet_ntop\n");
						exit(EXIT_FAILURE);
					}
				} else {
					/* not yet */
					peer_addr[0] = '\0';
					peer_index = 0;
				}

				if (!protocol_is_raw(protocol))
					printf("recv from %s:%u: %s\n", peer_addr,
							peer_index, buf);
				else {
					printf("receive raw %u bytes:\n", len);
					for (i = 0; i < len; i++) {
						if ((i % 16) == 0)
							printf("%04x", i);
						printf(" %02x", buf[i] & 0xff);
						if ((i % 16) == 15 || (i + 1) == len)
							printf("\n");
					}
				}

				slen = sizeof(psa);
			}
			if (len == -1) {
				perror("failed to recvfrom\n");
				exit(EXIT_FAILURE);
			}
		} else if (st == SOCK_RAW) {
			/* raw mode */
		}
	} else {

		err = connect(s, &sa.sa, sizeof(sa));
		if (err == -1) {
			perror("failed to connect\n");
			exit(EXIT_FAILURE);
		}

		if (!has_bind) {
			/* now get sockname so that we know ports etc */
			slen = sizeof(bsa);
			memset(&bsa, 0, sizeof(bsa));
			err = getsockname(s, &bsa.sa, &slen);
			if (err == -1) {
				perror("failed to getsockname\n");
				exit(EXIT_FAILURE);
			}
			p = NULL;
			if (protocol_is_ipv4(protocol)) {
				endpoint_index = ntohs(bsa.sin.sin_port);
				memset(my_addr, 0, sizeof(my_addr));
				p = inet_ntop(af, &bsa.sin.sin_addr, my_addr, sizeof(my_addr));
			} else if (protocol_is_unet(protocol)) {
				endpoint_index = bsa.sunet.sunet_addr.message_type;
				uatxt = unet_addr_to_str(&bsa.sunet.sunet_addr.addr);
				if (uatxt) {
					strncpy(my_addr, uatxt, sizeof(my_addr));
					free(uatxt);
					p = my_addr;
				}
			}
			if (!p) {
				perror("failed to convert my sockaddr to ascii\n");
				exit(EXIT_FAILURE);
			}

			len = asprintf(&be_txt, "%s:%u", my_addr, endpoint_index);

			printf("bound after connect to %s\n", be_txt);
		}

		/* now get peer so that we know ports etc */
		slen = sizeof(sa);
		memset(&sa, 0, sizeof(sa));
		err = getpeername(s, &sa.sa, &slen);
		if (err == -1) {
			perror("failed to getpeername\n");
			exit(EXIT_FAILURE);
		}
		p = NULL;
		if (protocol_is_ipv4(protocol)) {
			endpoint_index = ntohs(sa.sin.sin_port);
			memset(peer_addr, 0, sizeof(peer_addr));
			p = inet_ntop(af, &sa.sin.sin_addr, peer_addr, sizeof(peer_addr));
		} else if (protocol_is_unet(protocol)) {
			endpoint_index = sa.sunet.sunet_addr.message_type;
			uatxt = unet_addr_to_str(&sa.sunet.sunet_addr.addr);
			if (uatxt) {
				strncpy(peer_addr, uatxt, sizeof(peer_addr));
				free(uatxt);
				p = my_addr;
			}
		}
		if (!p) {
			perror("failed to convert peer sockaddr to ascii\n");
			exit(EXIT_FAILURE);
		}

		len = asprintf(&e_txt, "%s:%u", peer_addr, endpoint_index);

		printf("connected to %s\n", e_txt);

		while (optind < argc) {
			p = argv[optind];
			len = send(s, p, strlen(p), 0);
			if (len == -1) {
				perror("failed to send\n");
				exit(EXIT_FAILURE);
			}
			printf("send to %s: %s\n", e_txt, p);
			optind++;
		}
	}

	close(s);

	free(e_txt);
	free(be_txt);

	return 0;
}
