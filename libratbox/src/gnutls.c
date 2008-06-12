/*
 *  libratbox: a library used by ircd-ratbox and other things
 *  gnutls.c: gnutls related code
 *
 *  Copyright (C) 2007-2008 ircd-ratbox development team
 *  Copyright (C) 2007-2008 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2008 William Pitcock <nenolod@nenolod.net>
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

#ifdef HAVE_GNUTLS

#include <commio-int.h>
#include <commio-ssl.h>
#include <gnutls/gnutls.h>

static gnutls_certificate_credentials_t x509_cred;
static gnutls_dh_params_t dh_params;

void
rb_ssl_shutdown(rb_fde_t * F)
{
	if(F == NULL || F->ssl == NULL)
		return;

	gnutls_bye((gnutls_session_t) F->ssl, GNUTLS_SHUT_RDWR);
	gnutls_deinit((gnutls_session_t) F->ssl);
}

static void
rb_ssl_timeout(rb_fde_t * F, void *notused)
{
	lrb_assert(F->accept != NULL);
	F->accept->callback(F, RB_ERR_TIMEOUT, NULL, 0, F->accept->data);
}

static void
rb_ssl_tryaccept(rb_fde_t * F, void *data)
{
	int ssl_err;
	lrb_assert(F->accept != NULL);
	int flags;
	struct acceptdata *ad;

	if((ssl_err = gnutls_handshake((gnutls_session_t) F->ssl)) != 0)
	{
		switch (ssl_err)
		{
		case GNUTLS_E_INTERRUPTED:
			if(rb_ignore_errno(errno))
		case GNUTLS_E_AGAIN:
			{
				if(gnutls_record_get_direction((gnutls_session_t) F->ssl))
					flags = RB_SELECT_WRITE;
				else
					flags = RB_SELECT_READ;

				F->ssl_errno = ssl_err;
				rb_setselect(F, flags, rb_ssl_tryaccept, NULL);
				return;
			}
			break;
		default:
			F->ssl_errno = ssl_err;
			F->accept->callback(F, RB_ERROR_SSL, NULL, 0, F->accept->data);
			break;
		}
		return;
	}
	rb_settimeout(F, 0, NULL, NULL);
	rb_setselect(F, RB_SELECT_READ | RB_SELECT_WRITE, NULL, NULL);
	
	ad = F->accept;
	F->accept = NULL;
	ad->callback(F, RB_OK, (struct sockaddr *) &ad->S, ad->addrlen,
			    ad->data);
	rb_free(ad);
}

void
rb_ssl_start_accepted(rb_fde_t * new_F, ACCB * cb, void *data, int timeout)
{
	gnutls_session_t sess;
	int ssl_err;

	new_F->type |= RB_FD_SSL;

	gnutls_init(&sess, GNUTLS_SERVER);
	gnutls_set_default_priority(sess);
	gnutls_credentials_set(sess, GNUTLS_CRD_CERTIFICATE, x509_cred);
	gnutls_dh_set_prime_bits(sess, 1024);
	gnutls_certificate_server_set_request(sess, GNUTLS_CERT_REQUEST);

	new_F->ssl = sess;

	new_F->accept = rb_malloc(sizeof(struct acceptdata));

	new_F->accept->callback = cb;
	new_F->accept->data = data;
	rb_settimeout(new_F, timeout, rb_ssl_timeout, NULL);

	new_F->accept->addrlen = 0;

	gnutls_transport_set_ptr((gnutls_session_t) new_F->ssl, (gnutls_transport_ptr_t) rb_get_fd(new_F));

	if((ssl_err = gnutls_handshake((gnutls_session_t) new_F->ssl)) != 0)
	{
		switch(ssl_err)
		{
		case GNUTLS_E_INTERRUPTED:
			if(rb_ignore_errno(errno))
		case GNUTLS_E_AGAIN:
			{
				int flags;

				if(gnutls_record_get_direction((gnutls_session_t) new_F->ssl))
					flags = RB_SELECT_WRITE;
				else
					flags = RB_SELECT_READ;

				new_F->ssl_errno = ssl_err;
				rb_setselect(new_F, flags, rb_ssl_tryaccept, NULL);
				return;
			}
			break;
		default:
			new_F->ssl_errno = ssl_err;
			new_F->accept->callback(new_F, RB_ERROR_SSL, NULL, 0, new_F->accept->data);
			return;
		}
	}
	else
	{
		struct acceptdata *ad;

		rb_settimeout(new_F, 0, NULL, NULL);
		rb_setselect(new_F, RB_SELECT_READ | RB_SELECT_WRITE, NULL, NULL);
	
		ad = new_F->accept;
		new_F->accept = NULL;
		ad->callback(new_F, RB_OK, (struct sockaddr *) &ad->S, ad->addrlen,
				    ad->data);
		rb_free(ad);
	}
}

void
rb_ssl_accept_setup(rb_fde_t * F, int new_fd, struct sockaddr *st, int addrlen)
{
	gnutls_session_t sess;
	rb_fde_t *new_F;
	int ssl_err;

	new_F = rb_find_fd(new_fd);

	gnutls_init(&sess, GNUTLS_SERVER);
	gnutls_set_default_priority(sess);
	gnutls_credentials_set(sess, GNUTLS_CRD_CERTIFICATE, x509_cred);
	gnutls_dh_set_prime_bits(sess, 1024);
	gnutls_certificate_server_set_request(sess, GNUTLS_CERT_REQUEST);

	new_F->type |= RB_FD_SSL;
	new_F->accept = rb_malloc(sizeof(struct acceptdata));

	new_F->accept->callback = F->accept->callback;
	new_F->accept->data = F->accept->data;
	rb_settimeout(new_F, 10, rb_ssl_timeout, NULL);
	memcpy(&new_F->accept->S, st, addrlen);
	new_F->accept->addrlen = addrlen;

	gnutls_transport_set_ptr((gnutls_session_t) new_F->ssl, (gnutls_transport_ptr_t) rb_get_fd(new_F));
	if((ssl_err = gnutls_handshake((gnutls_session_t) new_F->ssl)) != 0)
	{
		switch(ssl_err)
		{
		case GNUTLS_E_INTERRUPTED:
			if(rb_ignore_errno(errno))
		case GNUTLS_E_AGAIN:
			{
				int flags;

				if(gnutls_record_get_direction((gnutls_session_t) new_F->ssl))
					flags = RB_SELECT_WRITE;
				else
					flags = RB_SELECT_READ;

				new_F->ssl_errno = ssl_err;
				rb_setselect(new_F, flags, rb_ssl_tryaccept, NULL);
				return;
			}
			break;
		default:
			new_F->ssl_errno = ssl_err;
			new_F->accept->callback(new_F, RB_ERROR_SSL, NULL, 0, new_F->accept->data);
			return;
		}
	}
	else
	{
		struct acceptdata *ad;

		rb_settimeout(new_F, 0, NULL, NULL);
		rb_setselect(new_F, RB_SELECT_READ | RB_SELECT_WRITE, NULL, NULL);
	
		ad = new_F->accept;
		new_F->accept = NULL;
		ad->callback(new_F, RB_OK, (struct sockaddr *) &ad->S, ad->addrlen,
				    ad->data);
		rb_free(ad);
	}
}

static ssize_t
rb_ssl_read_or_write(int r_or_w, rb_fde_t * F, void *rbuf, const void *wbuf, size_t count)
{
	ssize_t ret;
	unsigned long err;
	gnutls_session_t ssl = F->ssl;

	if(r_or_w == 0)
		ret = gnutls_record_recv(ssl, rbuf, count);
	else
		ret = gnutls_record_send(ssl, wbuf, count);

	if(ret < 0)
	{
		switch (ret)
		{
		case GNUTLS_E_AGAIN:
			errno = EAGAIN;
			if (gnutls_record_get_direction(ssl))
				return RB_RW_SSL_NEED_WRITE;
			else
				return RB_RW_SSL_NEED_READ;
		case GNUTLS_E_INTERRUPTED:
			err = ret;
			if(err == 0)
			{
				F->ssl_errno = 0;
				return RB_RW_IO_ERROR;
			}
			break;
		default:
			err = ret;
			break;
		}
		F->ssl_errno = err;
		if(err > 0)
		{
			errno = EIO;	/* not great but... */
			return RB_RW_SSL_ERROR;
		}
		return RB_RW_IO_ERROR;
	}
	return ret;
}

ssize_t
rb_ssl_read(rb_fde_t * F, void *buf, size_t count)
{
	return rb_ssl_read_or_write(0, F, buf, NULL, count);
}

ssize_t
rb_ssl_write(rb_fde_t * F, const void *buf, size_t count)
{
	return rb_ssl_read_or_write(1, F, NULL, buf, count);
}

int
rb_init_ssl(void)
{
	int ret = 1, g_ret;

	gnutls_global_init();

	gnutls_certificate_allocate_credentials(&x509_cred);
	gnutls_dh_params_init(&dh_params);

	if((g_ret = gnutls_dh_params_generate2(dh_params, 1024)) < 0)
	{
		rb_lib_log("rb_init_gnutls: Failed to generate GNUTLS DH params: %s", gnutls_strerror(g_ret));
		ret = 0;
	}

	gnutls_certificate_set_dh_params(x509_cred, dh_params);

	return ret;
}

int
rb_setup_ssl_server(const char *cert, const char *keyfile, const char *dhfile)
{
	int ret = 0;

	if((ret = gnutls_certificate_set_x509_key_file(x509_cred, cert, keyfile, GNUTLS_X509_FMT_PEM)) < 0)
	{
		rb_lib_log("rb_setup_ssl_server: Setting x509 keys up failed: %s", gnutls_strerror(ret));
		return 0;
	}

	return 1;
}

int
rb_ssl_listen(rb_fde_t * F, int backlog)
{
	F->type = RB_FD_SOCKET | RB_FD_LISTEN | RB_FD_SSL;
	return listen(F->fd, backlog);
}

struct ssl_connect
{
	CNCB *callback;
	void *data;
	int timeout;
};

static void
rb_ssl_connect_realcb(rb_fde_t * F, int status, struct ssl_connect *sconn)
{
	F->connect->callback = sconn->callback;
	F->connect->data = sconn->data;
	rb_free(sconn);
	rb_connect_callback(F, status);
}

static void
rb_ssl_tryconn_timeout_cb(rb_fde_t * F, void *data)
{
	rb_ssl_connect_realcb(F, RB_ERR_TIMEOUT, data);
}

static void
rb_ssl_tryconn_cb(rb_fde_t * F, void *data)
{
	struct ssl_connect *sconn = data;
	int ssl_err;

	if((ssl_err = gnutls_handshake((gnutls_session_t) F->ssl)) != 0)
	{
		switch (ssl_err)
		{
		case GNUTLS_E_INTERRUPTED:
			if(rb_ignore_errno(errno))
		case GNUTLS_E_AGAIN:
				{
					F->ssl_errno = ssl_err;
					rb_setselect(F, RB_SELECT_READ | RB_SELECT_WRITE,
						     rb_ssl_tryconn_cb, sconn);
					return;
				}
		default:
			F->ssl_errno = ssl_err;
			rb_ssl_connect_realcb(F, RB_ERROR_SSL, sconn);
			return;
		}
	}
	else
	{
		rb_ssl_connect_realcb(F, RB_OK, sconn);
	}
}

static void
rb_ssl_tryconn(rb_fde_t * F, int status, void *data)
{
	gnutls_session_t sess;
	struct ssl_connect *sconn = data;
	int ssl_err;

	if(status != RB_OK)
	{
		rb_ssl_connect_realcb(F, status, sconn);
		return;
	}

	F->type |= RB_FD_SSL;

	gnutls_init(&sess, GNUTLS_CLIENT);
	gnutls_set_default_priority(sess);
	gnutls_credentials_set(sess, GNUTLS_CRD_CERTIFICATE, x509_cred);
	gnutls_dh_set_prime_bits(sess, 1024);
	gnutls_transport_set_ptr(sess, (gnutls_transport_ptr_t) F->fd);

	F->ssl = sess;

	rb_settimeout(F, sconn->timeout, rb_ssl_tryconn_timeout_cb, sconn);
	if((ssl_err = gnutls_handshake((gnutls_session_t) F->ssl)) != 0)
	{
		switch (ssl_err)
		{
		case GNUTLS_E_INTERRUPTED:
			if(rb_ignore_errno(errno))
		case GNUTLS_E_AGAIN:
				{
					F->ssl_errno = ssl_err;
					rb_setselect(F, RB_SELECT_READ | RB_SELECT_WRITE,
						     rb_ssl_tryconn_cb, sconn);
					return;
				}
		default:
			F->ssl_errno = ssl_err;
			rb_ssl_connect_realcb(F, RB_ERROR_SSL, sconn);
			return;
		}
	}
	else
	{
		rb_ssl_connect_realcb(F, RB_OK, sconn);
	}
}

void
rb_connect_tcp_ssl(rb_fde_t * F, struct sockaddr *dest,
		   struct sockaddr *clocal, int socklen, CNCB * callback, void *data, int timeout)
{
	struct ssl_connect *sconn;
	if(F == NULL)
		return;

	sconn = rb_malloc(sizeof(struct ssl_connect));
	sconn->data = data;
	sconn->callback = callback;
	sconn->timeout = timeout;
	rb_connect_tcp(F, dest, clocal, socklen, rb_ssl_tryconn, sconn, timeout);
}

void
rb_ssl_start_connected(rb_fde_t * F, CNCB * callback, void *data, int timeout)
{
	gnutls_session_t sess;
	struct ssl_connect *sconn;
	int ssl_err;
	if(F == NULL)
		return;

	sconn = rb_malloc(sizeof(struct ssl_connect));
	sconn->data = data;
	sconn->callback = callback;
	sconn->timeout = timeout;
	F->connect = rb_malloc(sizeof(struct conndata));
	F->connect->callback = callback;
	F->connect->data = data;
	F->type |= RB_FD_SSL;

	gnutls_init(&sess, GNUTLS_CLIENT);
	gnutls_set_default_priority(sess);
	gnutls_credentials_set(sess, GNUTLS_CRD_CERTIFICATE, x509_cred);
	gnutls_dh_set_prime_bits(sess, 1024);
	gnutls_transport_set_ptr(sess, (gnutls_transport_ptr_t) F->fd);

	F->ssl = sess;
        
	rb_settimeout(F, sconn->timeout, rb_ssl_tryconn_timeout_cb, sconn);
	if((ssl_err = gnutls_handshake((gnutls_session_t) F->ssl)) != 0)
	{
		switch (ssl_err)
		{
		case GNUTLS_E_INTERRUPTED:
			if(rb_ignore_errno(errno))
		case GNUTLS_E_AGAIN:
				{
					F->ssl_errno = ssl_err;
					rb_setselect(F, RB_SELECT_READ | RB_SELECT_WRITE,
						     rb_ssl_tryconn_cb, sconn);
					return;
				}
		default:
			F->ssl_errno = ssl_err;
			rb_ssl_connect_realcb(F, RB_ERROR_SSL, sconn);
			return;
		}
	}
	else
	{
		rb_ssl_connect_realcb(F, RB_OK, sconn);
	}
}

/* XXX: implement me */
int
rb_init_prng(const char *path, prng_seed_t seed_type)
{
	return -1;
}

int
rb_get_random(void *buf, size_t length)
{
	return -1;
}


const char *
rb_get_ssl_strerror(rb_fde_t * F)
{
	return gnutls_strerror(F->ssl_errno);
}

int
rb_supports_ssl(void)
{
	return 1;
}

#endif /* HAVE_GNUTLS */
