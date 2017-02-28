#ifndef UNET_COMMON_H
#define UNET_COMMON_H

#include <stdbool.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "unet.h"

enum protocol {
	protocol_unknown,
	protocol_udp,
	protocol_tcp,
	protocol_ip_raw,
	protocol_unet,
	protocol_unet_raw,
};

static inline bool protocol_is_ipv4(enum protocol p)
{
	return p >= protocol_udp && p <= protocol_ip_raw;
}

static inline bool protocol_is_unet(enum protocol p)
{
	return p >= protocol_unet && p <= protocol_unet_raw;
}

static inline bool protocol_is_raw(enum protocol p)
{
	return p == protocol_ip_raw || p == protocol_unet_raw;
}

static inline bool protocol_is_dgram(enum protocol p)
{
	return p == protocol_udp || p == protocol_unet;
}

static inline bool protocol_is_stream(enum protocol p)
{
	return p == protocol_tcp;
}

const char *protocol_to_txt(enum protocol p);
enum protocol txt_to_protocol(const char *txt);

union generic_sockaddr {
	struct sockaddr sa;
	struct sockaddr_in sin;
	struct sockaddr_unet sunet;
	struct sockaddr_storage sas;
};

void print_usage(const char *errmsg, const char *synopsis,
		const char *short_opts, struct option const long_opts[],
		const char * const opts_help[]) __attribute__((noreturn));

#endif
