/*
 * include/uapi/linux/unet.h: Header for uNet socket interface
 *
 * Copyright (c) 2016, uNet Inc.
 * All rights reserved.
 *
 * Author: Pantelis Antoniou <pantelis.antoniou@konsulko.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _UNET_H_
#define _UNET_H_

#include <linux/types.h>
#include <linux/sockios.h>
#include <linux/socket.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <sys/socket.h>

#include <string.h>
#include <stddef.h>

#define UNET_MAX_MEDIA_NAME	16
#define UNET_MAX_IF_NAME	16
#define UNET_MAX_BEARER_NAME	32
#define UNET_MAX_LINK_NAME	60

/*
 * Socket API
 */

#ifndef AF_UNET
#define AF_UNET		43
#endif

#ifndef PF_UNET
#define PF_UNET		AF_UNET
#endif

#ifndef SOL_UNET
#define SOL_UNET	282
#endif

/* we can go up to 256 bytes per id, but we run over default sockaddr size */

#define UNET_ID_MAX	(_K_SS_MAXSIZE - sizeof(__kernel_sa_family_t) * 2 - sizeof(__le32) - 4 * sizeof(__u8))

struct unet_addr {
	__u8 parent_prefix_len;	/* when both 0; short form */
	__u8 parent_id_len;
	__u8 prefix_len;
	__u8 id_len;
	__u8 addr_buffer[UNET_ID_MAX];
};

static inline int unet_addr_buffer_len(const struct  unet_addr *ua)
{
	return ua->parent_prefix_len + ua->parent_id_len +
	       ua->prefix_len + ua->id_len;
}

static inline bool unet_addr_has_parent(const struct unet_addr *ua)
{
	return ua->parent_prefix_len && ua->parent_id_len;
}

static inline bool unet_addr_is_valid(const struct unet_addr *ua)
{
	/* it must fit */
	if (unet_addr_buffer_len(ua) > UNET_ID_MAX)
		return false;

	/* if it has a parent both must exist */
	if (unet_addr_has_parent(ua) &&
	    (!ua->parent_prefix_len || !ua->parent_id_len))
		return false;

	return ua->prefix_len && ua->id_len;
}

static inline void *unet_addr_parent_prefix(struct unet_addr *ua)
{
	if (!ua->parent_prefix_len)
		return NULL;
	return ua->addr_buffer;
}

static inline void *unet_addr_parent_id(struct unet_addr *ua)
{
	__u8 *p;

	if (!ua->parent_id_len)
		return NULL;
	p = ua->addr_buffer + ua->parent_prefix_len;
	if (p < ua->addr_buffer || p >= (ua->addr_buffer + UNET_ID_MAX))
		return NULL;
	return p;
}

static inline void *unet_addr_prefix(struct unet_addr *ua)
{
	__u8 *p;

	if (!ua->prefix_len)
		return NULL;
	p = ua->addr_buffer + ua->parent_prefix_len + ua->parent_id_len;
	if (p < ua->addr_buffer || p >= (ua->addr_buffer + UNET_ID_MAX))
		return NULL;
	return p;
}

static inline void *unet_addr_id(struct unet_addr *ua)
{
	__u8 *p;

	if (!ua->id_len)
		return NULL;
	p = ua->addr_buffer + ua->parent_prefix_len +
		ua->parent_id_len + ua->prefix_len;
	if (p < ua->addr_buffer || p >= (ua->addr_buffer + UNET_ID_MAX))
		return NULL;
	return p;
}

static inline int unet_addr_fill(struct unet_addr *ua,
		const void *parent_prefix, int parent_prefix_len,
		const void *parent_id, int parent_id_len,
		const void *prefix, int prefix_len,
		const void *id, int id_len)
{
	if (parent_prefix_len < 0 || parent_id_len < 0)
		return -EINVAL;

	if (prefix_len <= 0 || id_len <= 0)
		return -EINVAL;

	if (parent_prefix_len + parent_id_len + prefix_len + id_len >= UNET_ID_MAX)
		return -EINVAL;

	if (!prefix || !id)
		return -EINVAL;

	ua->parent_prefix_len = parent_prefix_len;
	ua->parent_id_len = parent_id_len;
	ua->prefix_len = prefix_len;
	ua->id_len = id_len;

	if (parent_prefix)
		memcpy(unet_addr_parent_prefix(ua), parent_prefix,
		       parent_prefix_len);

	if (parent_id)
		memcpy(unet_addr_parent_id(ua), parent_id, parent_id_len);

	memcpy(unet_addr_prefix(ua), prefix, prefix_len);
	memcpy(unet_addr_id(ua), id, id_len);

	return 0;
}

static inline int unet_addr_eq(const struct unet_addr *ua1,
			   const struct unet_addr *ua2)
{
	if (ua1 == ua2)
		return 1;

	/* rely on no-gaps in the address structure */
	return memcmp(ua1, ua2, offsetof(struct unet_addr, addr_buffer) +
			unet_addr_buffer_len(ua1)) == 0;
}

static inline void unet_addr_copy(struct unet_addr *dst, const struct unet_addr *src)
{
	/* rely on no-gaps in the address structure */
	memcpy(dst, src, offsetof(struct unet_addr, addr_buffer) +
			unet_addr_buffer_len(src));
}

struct unet_addr_sa {
	__u32 sunet_message_type;
	struct unet_addr sunet_addr;
};

struct sockaddr_unet {
	__kernel_sa_family_t sunet_family;	/* AF_UNET */
	__kernel_sa_family_t __pad;		/* PAD to 32 bits */
	struct unet_addr_sa sunet_addr;
};

char *unet_addr_to_str(const struct unet_addr *ua);
int unet_str_to_addr(const char *str, int size, struct unet_addr *ua);
struct unet_addr *unet_str_to_addr_alloc(const char *str, int size);

#endif
