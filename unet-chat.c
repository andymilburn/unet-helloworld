/*
 * unet-chat
 *
 * Simple chat using unet sockets
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
#include <unistd.h>

#include "unet-common.h"

bool server_mode = true;
const char *server_id = "app.chat";
uint32_t message_type = 1200;	/* hardcoded */

static const char usage_synopsis[] = "unet-chat [options] <server-address>";
static const char usage_short_opts[] = "m:i:hv";
static struct option const usage_long_opts[] = {
	{ "mt",			required_argument, NULL, 'm'},
	{ "id",			required_argument, NULL, 'i'},
	{ "help",		no_argument,       NULL, 'h'},
	{ "version",		no_argument,       NULL, 'v'},
	{ NULL,			no_argument,       NULL, 0x0},
};

static const char * const usage_opts_help[] = {
	"\n\tMessage type (default is 1200)",
	"\n\tApplication id (default is app.chat)",
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
	int s, err, opt, optidx, len;
	struct sockaddr_unet server_sa, peer_sa, self_sa, in_sa;
	char *server_ua_txt = NULL, *peer_ua_txt = NULL, *self_ua_txt = NULL, *p;
	socklen_t slen;
	fd_set rfds;
	bool connected = false;
	char line[256], buf[65536];

	while ((opt = getopt_long(argc, argv, usage_short_opts,
				  usage_long_opts, &optidx)) != EOF) {
		switch (opt) {
		case 'm':
			message_type = atoi(optarg);
			break;
		case 'i':
			server_id = optarg;
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

	if (optind < argc)
		server_mode = false;

	memset(&server_sa, 0, sizeof(server_sa));
	server_sa.sunet_family = AF_UNET;
	server_sa.sunet_addr.message_type = message_type;
	err = unet_str_to_addr(server_id, strlen(server_id), &server_sa.sunet_addr.addr);
	if (err == -1) {
		fprintf(stderr, "bad server id (%s) provided (%d:%s)\n",
				server_id, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	s = socket(AF_UNET, SOCK_DGRAM, 0);
	if (s == -1) {
		perror("Failed to open unet socket (is unet enabled in your kernel?)");
		exit(EXIT_FAILURE);
	}

	if (server_mode) {

		server_ua_txt = unet_addr_to_str(&server_sa.sunet_addr.addr);
		if (!server_ua_txt) {
			perror("failed on unet_addr_to_str()");
			exit(EXIT_FAILURE);
		}
		printf("server binding to '%s'\n", server_ua_txt);

		free(server_ua_txt);

		server_ua_txt = NULL;

		err = bind(s, (struct sockaddr *)&server_sa, sizeof(server_sa));
		if (err == -1) {
			fprintf(stderr, "failed to bind using %s server_id (%d:%s)\n",
					server_id, errno, strerror(errno));
			exit(EXIT_FAILURE);
		}

		connected = false;
	} else {

		len = asprintf(&server_ua_txt, "%s:%s", argv[optind], server_id);

		server_sa.sunet_family = AF_UNET;
		server_sa.sunet_addr.message_type = message_type;
		err = unet_str_to_addr(server_ua_txt, strlen(server_ua_txt), &server_sa.sunet_addr.addr);
		if (err == -1) {
			fprintf(stderr, "bad full server address (%s) provided (%d:%s)\n",
					server_ua_txt, errno, strerror(errno));
			exit(EXIT_FAILURE);
		}

		err = connect(s, (struct sockaddr *)&server_sa, sizeof(server_sa));
		if (err == -1) {
			fprintf(stderr, "failed to connect to full server address (%s) (%d:%s)\n",
					server_ua_txt, errno, strerror(errno));
			exit(EXIT_FAILURE);
		}

		/* now get sockname to get the full address */
		memset(&peer_sa, 0, sizeof(peer_sa));
		slen = sizeof(peer_sa);
		err = getpeername(s,(struct sockaddr *)&peer_sa, &slen);
		if (err == -1) {
			perror("failed on getpeername()");
			exit(EXIT_FAILURE);
		}

		peer_ua_txt = unet_addr_to_str(&peer_sa.sunet_addr.addr);
		if (!peer_ua_txt) {
			perror("failed on unet_addr_to_str()");
			exit(EXIT_FAILURE);
		}

		connected = true;
	}

	/* now get sockname to get the full address */
	memset(&self_sa, 0, sizeof(self_sa));
	slen = sizeof(self_sa);
	err = getsockname(s, (struct sockaddr *)&self_sa, &slen);
	if (err == -1) {
		perror("failed on getsockname()");
		exit(EXIT_FAILURE);
	}

	self_ua_txt = unet_addr_to_str(&self_sa.sunet_addr.addr);
	if (!self_ua_txt) {
		perror("failed on unet_addr_to_str()");
		exit(EXIT_FAILURE);
	}

	printf("Welcome to unet-chat; %s '%s'\n",
			server_mode ? "listening for clients in" : "using server",
			server_mode ? self_ua_txt : server_ua_txt);
	printf("\r%s > ", self_ua_txt);
	fflush(stdout);

	FD_ZERO(&rfds);
	for (;;) {
		FD_SET(STDIN_FILENO, &rfds);
		FD_SET(s, &rfds);

		err = select(s + 1, &rfds, NULL, NULL, NULL);
		if (err == -1) {
			perror("select() failed");
			exit(EXIT_FAILURE);
		}
		/* no data (probably EAGAIN) */
		if (err == 0)
			continue;

		/* line read */
		if (FD_ISSET(STDIN_FILENO, &rfds)) {
			p = fgets(line, sizeof(line) - 1, stdin);
			if (p) {
				line[sizeof(line) - 1] = '\0';
				len = strlen(line);
				while (len > 0 && line[len-1] == '\n')
					len--;
				line[len] = '\0';

				if (!connected)
					continue;

				len = send(s, p, strlen(p), 0);
				if (len == -1) {
					perror("failed to send\n");
					exit(EXIT_FAILURE);
				}
			}

			printf("%s > ", self_ua_txt);
			fflush(stdout);

		} else if (FD_ISSET(s, &rfds)) {
			/* first server packet */

			slen = sizeof(in_sa);
			len = recvfrom(s, buf, sizeof(buf) - 1, 0,
					       (struct sockaddr *)&in_sa, &slen);
			if (len > 0) {
				buf[len] = '\0';

				slen = sizeof(in_sa);

				if (!connected) {
					memcpy(&peer_sa, &in_sa, sizeof(in_sa));

					peer_ua_txt = unet_addr_to_str(&peer_sa.sunet_addr.addr);
					if (!peer_ua_txt) {
						perror("failed on unet_addr_to_str()");
						exit(EXIT_FAILURE);
					}

					err = connect(s, (struct sockaddr *)&peer_sa, sizeof(peer_sa));
					if (err == -1) {
						fprintf(stderr, "failed to connect to peer address (%s) (%d:%s)\n",
								peer_ua_txt, errno, strerror(errno));
						exit(EXIT_FAILURE);
					}

					fprintf(stderr, "\nconnection from (%s)\n", peer_ua_txt);

					connected = true;
				}

				/* do no allow more than one connection */
				if (!unet_addr_eq(&peer_sa.sunet_addr.addr, &in_sa.sunet_addr.addr))
					continue;

				printf("\r%*s\r%s> %s\n", 80, "", peer_ua_txt, buf);

				printf("%s > ", self_ua_txt);
				fflush(stdout);
			}
		}
	}

	close(s);

	if (server_ua_txt)
		free(server_ua_txt);
	if (peer_ua_txt)
		free(peer_ua_txt);
	if (self_ua_txt)
		free(self_ua_txt);

	return 0;
}
