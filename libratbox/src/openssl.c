/*
 *  libratbox: a library used by ircd-ratbox and other things
 *  openssl.c: openssl related code
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

#ifdef HAVE_OPENSSL

#include <commio-int.h>
#include <commio-ssl.h>
#include <openssl/ssl.h>
#include <openssl/dh.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

static SSL_CTX *ssl_server_ctx;
static SSL_CTX *ssl_client_ctx;
static int libratbox_index = -1;

static unsigned long
get_last_err(void)
{
	unsigned long t_err, err = 0;
	err = ERR_get_error();
	if(err == 0)
		return 0;

	while((t_err = ERR_get_error()) > 0)
		err = t_err;

	return err;
}

void
rb_ssl_shutdown(rb_fde_t *F)
{
	int i;
	if(F == NULL || F->ssl == NULL)
		return;
	SSL_set_shutdown((SSL *) F->ssl, SSL_RECEIVED_SHUTDOWN);

	for(i = 0; i < 4; i++)
	{
		if(SSL_shutdown((SSL *) F->ssl))
			break;
	}
	get_last_err();
	SSL_free((SSL *) F->ssl);
}

unsigned int
rb_ssl_handshake_count(rb_fde_t *F)
{
	return F->handshake_count;
}

void
rb_ssl_clear_handshake_count(rb_fde_t *F)
{
	F->handshake_count = 0;
}

static void
rb_ssl_timeout(rb_fde_t *F, void *notused)
{
	lrb_assert(F->accept != NULL);
	F->accept->callback(F, RB_ERR_TIMEOUT, NULL, 0, F->accept->data);
}


static void
rb_ssl_info_callback(SSL * ssl, int where, int ret)
{
	if(where & SSL_CB_HANDSHAKE_START)
	{
		rb_fde_t *F = SSL_get_ex_data(ssl, libratbox_index);
		if(F == NULL)
			return;
		F->handshake_count++;
	}
}

static void
rb_setup_ssl_cb(rb_fde_t *F)
{
	SSL_set_ex_data(F->ssl, libratbox_index, (char *)F);
	SSL_set_info_callback((SSL *) F->ssl, (void (*)(const SSL *,int,int))rb_ssl_info_callback);
}

static void
rb_ssl_tryaccept(rb_fde_t *F, void *data)
{
	int ssl_err;
	lrb_assert(F->accept != NULL);
	int flags;
	struct acceptdata *ad;

	if(!SSL_is_init_finished((SSL *) F->ssl))
	{
		if((ssl_err = SSL_accept((SSL *) F->ssl)) <= 0)
		{
			switch (ssl_err = SSL_get_error((SSL *) F->ssl, ssl_err))
			{
			case SSL_ERROR_WANT_READ:
			case SSL_ERROR_WANT_WRITE:
				if(ssl_err == SSL_ERROR_WANT_WRITE)
					flags = RB_SELECT_WRITE;
				else
					flags = RB_SELECT_READ;
				F->ssl_errno = get_last_err();
				rb_setselect(F, flags, rb_ssl_tryaccept, NULL);
				break;
			case SSL_ERROR_SYSCALL:
				F->accept->callback(F, RB_ERROR, NULL, 0, F->accept->data);
				break;
			default:
				F->ssl_errno = get_last_err();
				F->accept->callback(F, RB_ERROR_SSL, NULL, 0, F->accept->data);
				break;
			}
			return;
		}
	}
	rb_settimeout(F, 0, NULL, NULL);
	rb_setselect(F, RB_SELECT_READ | RB_SELECT_WRITE, NULL, NULL);

	ad = F->accept;
	F->accept = NULL;
	ad->callback(F, RB_OK, (struct sockaddr *)&ad->S, ad->addrlen, ad->data);
	rb_free(ad);

}


static void
rb_ssl_accept_common(rb_fde_t *new_F)
{
	int ssl_err;
	if((ssl_err = SSL_accept((SSL *) new_F->ssl)) <= 0)
	{
		switch (ssl_err = SSL_get_error((SSL *) new_F->ssl, ssl_err))
		{
		case SSL_ERROR_SYSCALL:
			if(rb_ignore_errno(errno))
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
				{
					new_F->ssl_errno = get_last_err();
					rb_setselect(new_F, RB_SELECT_READ | RB_SELECT_WRITE,
						     rb_ssl_tryaccept, NULL);
					return;
				}
		default:
			new_F->ssl_errno = get_last_err();
			new_F->accept->callback(new_F, RB_ERROR_SSL, NULL, 0, new_F->accept->data);
			return;
		}
	}
	else
	{
		rb_ssl_tryaccept(new_F, NULL);
	}
}

void
rb_ssl_start_accepted(rb_fde_t *new_F, ACCB * cb, void *data, int timeout)
{
	new_F->type |= RB_FD_SSL;
	new_F->ssl = SSL_new(ssl_server_ctx);
	new_F->accept = rb_malloc(sizeof(struct acceptdata));

	new_F->accept->callback = cb;
	new_F->accept->data = data;
	rb_settimeout(new_F, timeout, rb_ssl_timeout, NULL);

	new_F->accept->addrlen = 0;
	SSL_set_fd((SSL *) new_F->ssl, rb_get_fd(new_F));
	rb_setup_ssl_cb(new_F);
	rb_ssl_accept_common(new_F);
}




void
rb_ssl_accept_setup(rb_fde_t *F, rb_fde_t *new_F, struct sockaddr *st, int addrlen)
{
	new_F->type |= RB_FD_SSL;
	new_F->ssl = SSL_new(ssl_server_ctx);
	new_F->accept = rb_malloc(sizeof(struct acceptdata));

	new_F->accept->callback = F->accept->callback;
	new_F->accept->data = F->accept->data;
	rb_settimeout(new_F, 10, rb_ssl_timeout, NULL);
	memcpy(&new_F->accept->S, st, addrlen);
	new_F->accept->addrlen = addrlen;

	SSL_set_fd((SSL *) new_F->ssl, rb_get_fd(new_F));
	rb_setup_ssl_cb(new_F);
	rb_ssl_accept_common(new_F);
}

static ssize_t
rb_ssl_read_or_write(int r_or_w, rb_fde_t *F, void *rbuf, const void *wbuf, size_t count)
{
	ssize_t ret;
	unsigned long err;
	SSL *ssl = F->ssl;

	if(r_or_w == 0)
		ret = (ssize_t) SSL_read(ssl, rbuf, (int)count);
	else
		ret = (ssize_t) SSL_write(ssl, wbuf, (int)count);

	if(ret < 0)
	{
		switch (SSL_get_error(ssl, ret))
		{
		case SSL_ERROR_WANT_READ:
			errno = EAGAIN;
			return RB_RW_SSL_NEED_READ;
		case SSL_ERROR_WANT_WRITE:
			errno = EAGAIN;
			return RB_RW_SSL_NEED_WRITE;
		case SSL_ERROR_ZERO_RETURN:
			return 0;
		case SSL_ERROR_SYSCALL:
			err = get_last_err();
			if(err == 0)
			{
				F->ssl_errno = 0;
				return RB_RW_IO_ERROR;
			}
			break;
		default:
			err = get_last_err();
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
rb_ssl_read(rb_fde_t *F, void *buf, size_t count)
{
	return rb_ssl_read_or_write(0, F, buf, NULL, count);
}

ssize_t
rb_ssl_write(rb_fde_t *F, const void *buf, size_t count)
{
	return rb_ssl_read_or_write(1, F, NULL, buf, count);
}

static int
verify_accept_all_cb(int preverify_ok, X509_STORE_CTX *x509_ctx)
{
	return 1;
}

static const char *
get_ssl_error(unsigned long err)
{
	static char buf[512];

	ERR_error_string_n(err, buf, sizeof buf);
	return buf;
}

int
rb_init_ssl(void)
{
	int ret = 1;
	char libratbox_data[] = "libratbox data";
	SSL_load_error_strings();
	SSL_library_init();
	libratbox_index = SSL_get_ex_new_index(0, libratbox_data, NULL, NULL, NULL);
	ssl_server_ctx = SSL_CTX_new(SSLv23_server_method());
	if(ssl_server_ctx == NULL)
	{
		rb_lib_log("rb_init_openssl: Unable to initialize OpenSSL server context: %s",
			   get_ssl_error(ERR_get_error()));
		ret = 0;
	}
	/* Disable SSLv2, make the client use our settings */
	SSL_CTX_set_options(ssl_server_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_CIPHER_SERVER_PREFERENCE
#ifdef SSL_OP_SINGLE_DH_USE
			| SSL_OP_SINGLE_DH_USE
#endif
#ifdef SSL_OP_NO_TICKET
			| SSL_OP_NO_TICKET
#endif
			);
	SSL_CTX_set_verify(ssl_server_ctx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, verify_accept_all_cb);
	SSL_CTX_set_session_cache_mode(ssl_server_ctx, SSL_SESS_CACHE_OFF);
	SSL_CTX_set_cipher_list(ssl_server_ctx, "kEECDH+HIGH:kEDH+HIGH:HIGH:!RC4:!aNULL");

	/* Set ECDHE on OpenSSL 1.00+, but make sure it's actually available because redhat are dicks
	   and bastardise their OpenSSL for stupid reasons... */
	#if (OPENSSL_VERSION_NUMBER >= 0x10000000) && defined(NID_secp384r1)
		EC_KEY *key = EC_KEY_new_by_curve_name(NID_secp384r1);
		if (key) {
			SSL_CTX_set_tmp_ecdh(ssl_server_ctx, key);
			EC_KEY_free(key);
		}
#ifdef SSL_OP_SINGLE_ECDH_USE
		SSL_CTX_set_options(ssl_server_ctx, SSL_OP_SINGLE_ECDH_USE);
#endif
	#endif

	ssl_client_ctx = SSL_CTX_new(TLSv1_client_method());

	if(ssl_client_ctx == NULL)
	{
		rb_lib_log("rb_init_openssl: Unable to initialize OpenSSL client context: %s",
			   get_ssl_error(ERR_get_error()));
		ret = 0;
	}

#ifdef SSL_OP_NO_TICKET
	SSL_CTX_set_options(ssl_client_ctx, SSL_OP_NO_TICKET);
#endif

	return ret;
}


int
rb_setup_ssl_server(const char *cert, const char *keyfile, const char *dhfile)
{
	DH *dh;
	unsigned long err;
	if(cert == NULL)
	{
		rb_lib_log("rb_setup_ssl_server: No certificate file");
		return 0;
	}
	if(!SSL_CTX_use_certificate_chain_file(ssl_server_ctx, cert) || !SSL_CTX_use_certificate_chain_file(ssl_client_ctx, cert))
	{
		err = ERR_get_error();
		rb_lib_log("rb_setup_ssl_server: Error loading certificate file [%s]: %s", cert,
			   get_ssl_error(err));
		return 0;
	}

	if(keyfile == NULL)
	{
		rb_lib_log("rb_setup_ssl_server: No key file");
		return 0;
	}


	if(!SSL_CTX_use_PrivateKey_file(ssl_server_ctx, keyfile, SSL_FILETYPE_PEM) || !SSL_CTX_use_PrivateKey_file(ssl_client_ctx, keyfile, SSL_FILETYPE_PEM))
	{
		err = ERR_get_error();
		rb_lib_log("rb_setup_ssl_server: Error loading keyfile [%s]: %s", keyfile,
			   get_ssl_error(err));
		return 0;
	}

	if(dhfile != NULL)
	{
		/* DH parameters aren't necessary, but they are nice..if they didn't pass one..that is their problem */
		BIO *bio = BIO_new_file(dhfile, "r");
		if(bio != NULL)
		{
			dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
			if(dh == NULL)
			{
				err = ERR_get_error();
				rb_lib_log
					("rb_setup_ssl_server: Error loading DH params file [%s]: %s",
					 dhfile, get_ssl_error(err));
				BIO_free(bio);
				return 0;
			}
			BIO_free(bio);
			SSL_CTX_set_tmp_dh(ssl_server_ctx, dh);
		}
		else
		{
			err = ERR_get_error();
			rb_lib_log("rb_setup_ssl_server: Error loading DH params file [%s]: %s",
				   dhfile, get_ssl_error(err));
		}
	}
	return 1;
}

int
rb_ssl_listen(rb_fde_t *F, int backlog, int defer_accept)
{
	int result;

	result = rb_listen(F, backlog, defer_accept);
	F->type = RB_FD_SOCKET | RB_FD_LISTEN | RB_FD_SSL;

	return result;
}

struct ssl_connect
{
	CNCB *callback;
	void *data;
	int timeout;
};

static void
rb_ssl_connect_realcb(rb_fde_t *F, int status, struct ssl_connect *sconn)
{
	F->connect->callback = sconn->callback;
	F->connect->data = sconn->data;
	rb_free(sconn);
	rb_connect_callback(F, status);
}

static void
rb_ssl_tryconn_timeout_cb(rb_fde_t *F, void *data)
{
	rb_ssl_connect_realcb(F, RB_ERR_TIMEOUT, data);
}

static void
rb_ssl_tryconn_cb(rb_fde_t *F, void *data)
{
	struct ssl_connect *sconn = data;
	int ssl_err;
	if(!SSL_is_init_finished((SSL *) F->ssl))
	{
		if((ssl_err = SSL_connect((SSL *) F->ssl)) <= 0)
		{
			switch (ssl_err = SSL_get_error((SSL *) F->ssl, ssl_err))
			{
			case SSL_ERROR_SYSCALL:
				if(rb_ignore_errno(errno))
			case SSL_ERROR_WANT_READ:
			case SSL_ERROR_WANT_WRITE:
					{
						F->ssl_errno = get_last_err();
						rb_setselect(F, RB_SELECT_READ | RB_SELECT_WRITE,
							     rb_ssl_tryconn_cb, sconn);
						return;
					}
			default:
				F->ssl_errno = get_last_err();
				rb_ssl_connect_realcb(F, RB_ERROR_SSL, sconn);
				return;
			}
		}
		else
		{
			rb_ssl_connect_realcb(F, RB_OK, sconn);
		}
	}
}

static void
rb_ssl_tryconn(rb_fde_t *F, int status, void *data)
{
	struct ssl_connect *sconn = data;
	int ssl_err;
	if(status != RB_OK)
	{
		rb_ssl_connect_realcb(F, status, sconn);
		return;
	}

	F->type |= RB_FD_SSL;
	F->ssl = SSL_new(ssl_client_ctx);
	SSL_set_fd((SSL *) F->ssl, F->fd);
	rb_setup_ssl_cb(F);
	rb_settimeout(F, sconn->timeout, rb_ssl_tryconn_timeout_cb, sconn);
	if((ssl_err = SSL_connect((SSL *) F->ssl)) <= 0)
	{
		switch (ssl_err = SSL_get_error((SSL *) F->ssl, ssl_err))
		{
		case SSL_ERROR_SYSCALL:
			if(rb_ignore_errno(errno))
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
				{
					F->ssl_errno = get_last_err();
					rb_setselect(F, RB_SELECT_READ | RB_SELECT_WRITE,
						     rb_ssl_tryconn_cb, sconn);
					return;
				}
		default:
			F->ssl_errno = get_last_err();
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
rb_connect_tcp_ssl(rb_fde_t *F, struct sockaddr *dest,
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
rb_ssl_start_connected(rb_fde_t *F, CNCB * callback, void *data, int timeout)
{
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
	F->ssl = SSL_new(ssl_client_ctx);

	SSL_set_fd((SSL *) F->ssl, F->fd);
	rb_setup_ssl_cb(F);
	rb_settimeout(F, sconn->timeout, rb_ssl_tryconn_timeout_cb, sconn);
	if((ssl_err = SSL_connect((SSL *) F->ssl)) <= 0)
	{
		switch (ssl_err = SSL_get_error((SSL *) F->ssl, ssl_err))
		{
		case SSL_ERROR_SYSCALL:
			if(rb_ignore_errno(errno))
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
				{
					F->ssl_errno = get_last_err();
					rb_setselect(F, RB_SELECT_READ | RB_SELECT_WRITE,
						     rb_ssl_tryconn_cb, sconn);
					return;
				}
		default:
			F->ssl_errno = get_last_err();
			rb_ssl_connect_realcb(F, RB_ERROR_SSL, sconn);
			return;
		}
	}
	else
	{
		rb_ssl_connect_realcb(F, RB_OK, sconn);
	}
}

int
rb_init_prng(const char *path, prng_seed_t seed_type)
{
	if(seed_type == RB_PRNG_DEFAULT)
	{
#ifdef _WIN32
		RAND_screen();
#endif
		return RAND_status();
	}
	if(path == NULL)
		return RAND_status();

	switch (seed_type)
	{
	case RB_PRNG_FILE:
		if(RAND_load_file(path, -1) == -1)
			return -1;
		break;
#ifdef _WIN32
	case RB_PRNGWIN32:
		RAND_screen();
		break;
#endif
	default:
		return -1;
	}

	return RAND_status();
}

int
rb_get_random(void *buf, size_t length)
{
	int ret;

	if((ret = RAND_bytes(buf, length)) == 0)
	{
		/* remove the error from the queue */
		ERR_get_error();
	}
	return ret;
}

int
rb_get_pseudo_random(void *buf, size_t length)
{
	int ret;
	ret = RAND_pseudo_bytes(buf, length);
	if(ret < 0)
		return 0;
	return 1;
}

const char *
rb_get_ssl_strerror(rb_fde_t *F)
{
	return get_ssl_error(F->ssl_errno);
}

int
rb_get_ssl_certfp(rb_fde_t *F, uint8_t certfp[RB_SSL_CERTFP_LEN])
{
	X509 *cert;
	int res;

	if (F->ssl == NULL)
		return 0;

	cert = SSL_get_peer_certificate((SSL *) F->ssl);
	if(cert != NULL)
	{
		res = SSL_get_verify_result((SSL *) F->ssl);
		if(
			res == X509_V_OK ||
			res == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN ||
			res == X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE ||
			res == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT ||
			res == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY)
		{
			unsigned int certfp_length = RB_SSL_CERTFP_LEN;
			X509_digest(cert, EVP_sha1(), certfp, &certfp_length);
			X509_free(cert);
			return 1;
		}
		X509_free(cert);
	}

	return 0;
}

int
rb_supports_ssl(void)
{
	return 1;
}

void
rb_get_ssl_info(char *buf, size_t len)
{
	rb_snprintf(buf, len, "Using SSL: %s compiled: 0x%lx, library 0x%lx",
		    SSLeay_version(SSLEAY_VERSION),
		    (long)OPENSSL_VERSION_NUMBER, SSLeay());
}


#endif /* HAVE_OPESSL */
