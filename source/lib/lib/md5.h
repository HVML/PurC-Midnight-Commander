/*
 * Copyright (C) 2014 Felix Fietkau <nbd@openwrt.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
 * MD5 Message-Digest Algorithm (RFC 1321).
 *
 * Homepage:
 * http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
 *
 * Author:
 * Alexander Peslyak, better known as Solar Designer <solar at openwall.com>
 *
 * This software was written by Alexander Peslyak in 2001.  No copyright is
 * claimed, and the software is hereby placed in the public domain.
 * In case this attempt to disclaim copyright and place the software in the
 * public domain is deemed null and void, then the software is
 * Copyright (c) 2001 Alexander Peslyak and it is hereby released to the
 * general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 *
 * See md5.c for more information.
 */

#ifndef _LIBHIBOX_MD5_H
#define _LIBHIBOX_MD5_H

#include <stdint.h>
#include <stddef.h>

#define MD5_DIGEST_SIZE          (16)

typedef struct md5_ctx {
	uint32_t lo, hi;
	uint32_t a, b, c, d;
	unsigned char buffer[64];
} md5_ctx_t;

#ifdef __cplusplus
extern "C" {
#endif

void md5_begin (md5_ctx_t *ctx);
void md5_hash (const void *data, size_t length, md5_ctx_t *ctx);
void md5_end (unsigned char *resbuf, md5_ctx_t *ctx);

/* \a digest should be long enough (at least 16) to store the returned digest */
void md5digest (const char *string, unsigned char *digest);
int md5sum (const char *file, unsigned char *md5_buf);

/* hex must be long enough to hold the heximal characters */
void bin2hex (const unsigned char *bin, int len, char *hex);

/* bin must be long enough to hold the bytes.
   return the number of bytes converted, <= 0 for error */
int hex2bin (const char *hex, unsigned char *bin);

#ifdef __cplusplus
}
#endif

#endif  /* _LIBHIBOX_MD5_H */
