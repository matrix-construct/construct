/* 
 *  libratbox: a library used by ircd-ratbox and other things
 *  nossl.c: ssl stub code
 *
 *  Copyright (C) 2007-2008 ircd-ratbox development team
 *  Copyright (C) 2007-2008 Aaron Sethman <androsyn@ratbox.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *    
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 * 
 *  $Id: commio.c 24808 2008-01-02 08:17:05Z androsyn $
 */


#include <libratbox_config.h>
#include <ratbox_lib.h>
#if !defined(HAVE_OPENSSL) && !defined(HAVE_GNUTLS)

#include "arc4random.h"

#include <commio-int.h>
#include <commio-ssl.h>

int
rb_setup_ssl_server(const char *cert, const char *keyfile, const char *dhfile)
{
	errno = ENOSYS;
	return 0;
}

int
rb_init_ssl(void)
{
	errno = ENOSYS;
	return -1;

}

int
rb_ssl_listen(rb_fde_t *F, int backlog)
{
	errno = ENOSYS;
	return -1;
}

static void
rb_stir_arc4random(void *unused)
{
	arc4random_stir();
}


int
rb_init_prng(const char *path, prng_seed_t seed_type)
{
	/* xxx this ignores the parameters above */
	arc4random_stir();
	rb_event_addish("rb_stir_arc4random", rb_stir_arc4random, NULL, 300);
	return 1;
}

int
rb_get_random(void *buf, size_t length)
{
	uint32_t rnd = 0, i;
	uint8_t *xbuf = buf;
	for(i = 0; i < length; i++)
	{
		if(i % 4 == 0)
			rnd = arc4random();
		xbuf[i] = rnd;
		rnd >>= 8;
	}
	return 1;
}

int
rb_get_pseudo_random(void *buf, size_t length)
{
	return rb_get_random(buf, length);
}


const char *
rb_get_ssl_strerror(rb_fde_t *F)
{
	static const char *nosupport = "SSL/TLS not supported";
	return nosupport;
}

int
rb_get_ssl_certfp(rb_fde_t *F, uint8_t certfp[RB_SSL_CERTFP_LEN])
{
	return 0;
}

void
rb_ssl_start_accepted(rb_fde_t *new_F, ACCB * cb, void *data, int timeout)
{
	return;
}

void
rb_ssl_start_connected(rb_fde_t *F, CNCB * callback, void *data, int timeout)
{
	return;
}

void
rb_connect_tcp_ssl(rb_fde_t *F, struct sockaddr *dest,
		   struct sockaddr *clocal, int socklen, CNCB * callback, void *data, int timeout)
{
	return;
}

int
rb_supports_ssl(void)
{
	return 0;
}

void
rb_ssl_shutdown(rb_fde_t *F)
{
	return;
}

void
rb_ssl_accept_setup(rb_fde_t *F, rb_fde_t *new_F, struct sockaddr *st, int addrlen)
{
	return;
}

ssize_t
rb_ssl_read(rb_fde_t *F, void *buf, size_t count)
{
	errno = ENOSYS;
	return -1;
}

ssize_t
rb_ssl_write(rb_fde_t *F, const void *buf, size_t count)
{
	errno = ENOSYS;
	return -1;
}

unsigned int
rb_ssl_handshake_count(rb_fde_t *F)
{
	return 0;
}

void
rb_ssl_clear_handshake_count(rb_fde_t *F)
{
	return;
}

void
rb_get_ssl_info(char *buf, size_t len)
{
        rb_snprintf(buf, len, "Not compiled with SSL support"); 
}
                                                
#endif /* !HAVE_OPENSSL */
