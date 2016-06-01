/*
 *  librb: a library used by ircd-ratbox and other things
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
 */

#include <librb_config.h>
#include <rb_lib.h>

#ifdef HAVE_OPENSSL

#include <commio-int.h>
#include <commio-ssl.h>
#include <openssl/ssl.h>
#include <openssl/dh.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/opensslv.h>

/*
 * This is a mess but what can you do when the library authors
 * refuse to play ball with established conventions?
 */
#if defined(LIBRESSL_VERSION_NUMBER) && (LIBRESSL_VERSION_NUMBER >= 0x20020002L)
#  define LRB_HAVE_TLS_METHOD_API 1
#else
#  if !defined(LIBRESSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x10100000L)
#    define LRB_HAVE_TLS_METHOD_API 1
#  endif
#endif

/*
 * Use SSL_CTX_set_ecdh_auto() in OpenSSL 1.0.2 only
 * Use SSL_CTX_set1_curves_list() in OpenSSL 1.0.2 and above
 * TODO: Merge this into the block above if LibreSSL implements them
 */
#if !defined(LIBRESSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x10002000L)
#  define LRB_HAVE_TLS_SET_CURVES 1
#  if (OPENSSL_VERSION_NUMBER < 0x10100000L)
#    define LRB_HAVE_TLS_ECDH_AUTO 1
#  endif
#endif

/*
 * More LibreSSL compatibility mess
 * Used in rb_get_ssl_info() below.
 */
#if !defined(LIBRESSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x10100000L)
   /* OpenSSL 1.1.0+ */
#  define LRB_SSL_VTEXT_COMPILETIME     OPENSSL_VERSION_TEXT
#  define LRB_SSL_VTEXT_RUNTIME         OpenSSL_version(OPENSSL_VERSION)
#  define LRB_SSL_VNUM_COMPILETIME      OPENSSL_VERSION_NUMBER
#  define LRB_SSL_VNUM_RUNTIME          OpenSSL_version_num()
#  define LRB_SSL_FULL_VERSION_INFO     1
#else
/*
 * "Full version info" above means we have access to all 4 pieces of information.
 *
 * For the below, this is not the case; LibreSSL version number at runtime returns
 * the wrong version number, and OpenSSL version text at compile time does not exist.
 * Thus, we only reliably have version text at runtime, and version number at compile
 * time.
 */
#  if defined(LIBRESSL_VERSION_NUMBER) && (LIBRESSL_VERSION_NUMBER >= 0x20200000L)
     /* LibreSSL 2.2.0+ */
#    define LRB_SSL_VTEXT_RUNTIME       SSLeay_version(SSLEAY_VERSION)
#    define LRB_SSL_VNUM_COMPILETIME    LIBRESSL_VERSION_NUMBER
#  else
     /* OpenSSL < 1.1.0 or LibreSSL < 2.2.0 */
#    define LRB_SSL_VTEXT_RUNTIME       SSLeay_version(SSLEAY_VERSION)
#    define LRB_SSL_VNUM_COMPILETIME    SSLEAY_VERSION_NUMBER
#  endif
#endif

static SSL_CTX *ssl_server_ctx = NULL;
static SSL_CTX *ssl_client_ctx = NULL;
static int librb_index = -1;

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
		rb_fde_t *F = SSL_get_ex_data(ssl, librb_index);
		if(F == NULL)
			return;
		F->handshake_count++;
	}
}

static void
rb_setup_ssl_cb(rb_fde_t *F)
{
	SSL_set_ex_data(F->ssl, librb_index, (char *)F);
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
	char librb_data[] = "librb data";

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
	/*
	 * OpenSSL 1.1.0 and above automatically initialises itself with sane defaults
	 */
	SSL_library_init();
	SSL_load_error_strings();
#endif

	librb_index = SSL_get_ex_new_index(0, librb_data, NULL, NULL, NULL);

	return 1;
}

int
rb_setup_ssl_server(const char *certfile, const char *keyfile, const char *dhfile, const char *cipher_list)
{
	const char librb_ciphers[] = "kEECDH+HIGH:kEDH+HIGH:HIGH:!aNULL";

	#ifdef LRB_HAVE_TLS_SET_CURVES
	const char librb_curves[] = "P-521:P-384:P-256";
	#endif

	if(certfile == NULL)
	{
		rb_lib_log("rb_setup_ssl_server: No certificate file");
		return 0;
	}

	if(keyfile == NULL)
		keyfile = certfile;

	if(cipher_list == NULL)
		cipher_list = librb_ciphers;

	if (ssl_server_ctx)
		SSL_CTX_free(ssl_server_ctx);

	if (ssl_client_ctx)
		SSL_CTX_free(ssl_client_ctx);

	#ifdef LRB_HAVE_TLS_METHOD_API
	ssl_server_ctx = SSL_CTX_new(TLS_server_method());
	ssl_client_ctx = SSL_CTX_new(TLS_client_method());
	#else
	ssl_server_ctx = SSL_CTX_new(SSLv23_server_method());
	ssl_client_ctx = SSL_CTX_new(SSLv23_client_method());
	#endif

	if(ssl_server_ctx == NULL)
	{
		rb_lib_log("rb_init_openssl: Unable to initialize OpenSSL server context: %s",
			   get_ssl_error(ERR_get_error()));
		return 0;
	}

	if(ssl_client_ctx == NULL)
	{
		rb_lib_log("rb_init_openssl: Unable to initialize OpenSSL client context: %s",
			   get_ssl_error(ERR_get_error()));
		return 0;
	}

	#ifndef LRB_HAVE_TLS_METHOD_API
	SSL_CTX_set_options(ssl_server_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
	SSL_CTX_set_options(ssl_client_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
	#endif

	#ifdef SSL_OP_SINGLE_DH_USE
	SSL_CTX_set_options(ssl_server_ctx, SSL_OP_SINGLE_DH_USE);
	#endif

	#ifdef SSL_OP_SINGLE_ECDH_USE
	SSL_CTX_set_options(ssl_server_ctx, SSL_OP_SINGLE_ECDH_USE);
	#endif

	#ifdef SSL_OP_NO_TICKET
	SSL_CTX_set_options(ssl_server_ctx, SSL_OP_NO_TICKET);
	SSL_CTX_set_options(ssl_client_ctx, SSL_OP_NO_TICKET);
	#endif

	#ifdef SSL_OP_CIPHER_SERVER_PREFERENCE
	SSL_CTX_set_options(ssl_server_ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
	#endif

	SSL_CTX_set_verify(ssl_server_ctx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, verify_accept_all_cb);
	SSL_CTX_set_session_cache_mode(ssl_server_ctx, SSL_SESS_CACHE_OFF);

	#ifdef LRB_HAVE_TLS_SET_CURVES
	SSL_CTX_set1_curves_list(ssl_server_ctx, librb_curves);
	#endif

	#ifdef LRB_HAVE_TLS_ECDH_AUTO
	SSL_CTX_set_ecdh_auto(ssl_server_ctx, 1);
	#endif

	/*
	 * Set manual ECDHE curve on OpenSSL 1.0.0 & 1.0.1, but make sure it's actually available
	 */
	#if (OPENSSL_VERSION_NUMBER >= 0x10000000L) && (OPENSSL_VERSION_NUMBER < 0x10002000L) && !defined(OPENSSL_NO_ECDH)
	EC_KEY *key = EC_KEY_new_by_curve_name(NID_secp384r1);
	if (key) {
		SSL_CTX_set_tmp_ecdh(ssl_server_ctx, key);
		EC_KEY_free(key);
	}
	#endif

	SSL_CTX_set_cipher_list(ssl_server_ctx, cipher_list);
	SSL_CTX_set_cipher_list(ssl_client_ctx, cipher_list);

	if(!SSL_CTX_use_certificate_chain_file(ssl_server_ctx, certfile) || !SSL_CTX_use_certificate_chain_file(ssl_client_ctx, certfile))
	{
		rb_lib_log("rb_setup_ssl_server: Error loading certificate file [%s]: %s", certfile,
			   get_ssl_error(ERR_get_error()));
		return 0;
	}

	if(!SSL_CTX_use_PrivateKey_file(ssl_server_ctx, keyfile, SSL_FILETYPE_PEM) || !SSL_CTX_use_PrivateKey_file(ssl_client_ctx, keyfile, SSL_FILETYPE_PEM))
	{
		rb_lib_log("rb_setup_ssl_server: Error loading keyfile [%s]: %s", keyfile,
			   get_ssl_error(ERR_get_error()));
		return 0;
	}

	if(dhfile != NULL)
	{
		/* DH parameters aren't necessary, but they are nice..if they didn't pass one..that is their problem */
		FILE *fp = fopen(dhfile, "r");
		DH *dh = NULL;

		if(fp == NULL)
		{
			rb_lib_log("rb_setup_ssl_server: Error loading DH params file [%s]: %s",
			           dhfile, strerror(errno));
		}
		else if(PEM_read_DHparams(fp, &dh, NULL, NULL) == NULL)
		{
			rb_lib_log("rb_setup_ssl_server: Error loading DH params file [%s]: %s",
			           dhfile, get_ssl_error(ERR_get_error()));
			fclose(fp);
		}
		else
		{
			SSL_CTX_set_tmp_dh(ssl_server_ctx, dh);
			DH_free(dh);
			fclose(fp);
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
		   struct sockaddr *clocal, CNCB * callback, void *data, int timeout)
{
	struct ssl_connect *sconn;
	if(F == NULL)
		return;

	sconn = rb_malloc(sizeof(struct ssl_connect));
	sconn->data = data;
	sconn->callback = callback;
	sconn->timeout = timeout;
	rb_connect_tcp(F, dest, clocal, rb_ssl_tryconn, sconn, timeout);
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

const char *
rb_get_ssl_strerror(rb_fde_t *F)
{
	return get_ssl_error(F->ssl_errno);
}

static int
make_certfp(X509 *cert, uint8_t certfp[RB_SSL_CERTFP_LEN], int method)
{
	const ASN1_ITEM *it;
	const EVP_MD *evp;
	void *data;
	unsigned int len;

	switch(method)
	{
	case RB_SSL_CERTFP_METH_CERT_SHA1:
		it = ASN1_ITEM_rptr(X509);
		evp = EVP_sha1();
		data = cert;
		len = RB_SSL_CERTFP_LEN_SHA1;
		break;
	case RB_SSL_CERTFP_METH_CERT_SHA256:
		it = ASN1_ITEM_rptr(X509);
		evp = EVP_sha256();
		data = cert;
		len = RB_SSL_CERTFP_LEN_SHA256;
		break;
	case RB_SSL_CERTFP_METH_CERT_SHA512:
		it = ASN1_ITEM_rptr(X509);
		evp = EVP_sha512();
		data = cert;
		len = RB_SSL_CERTFP_LEN_SHA512;
		break;
	case RB_SSL_CERTFP_METH_SPKI_SHA256:
		it = ASN1_ITEM_rptr(X509_PUBKEY);
		evp = EVP_sha256();
		data = X509_get_X509_PUBKEY(cert);
		len = RB_SSL_CERTFP_LEN_SHA256;
		break;
	case RB_SSL_CERTFP_METH_SPKI_SHA512:
		it = ASN1_ITEM_rptr(X509_PUBKEY);
		evp = EVP_sha512();
		data = X509_get_X509_PUBKEY(cert);
		len = RB_SSL_CERTFP_LEN_SHA512;
		break;
	default:
		return 0;
	}

	if (ASN1_item_digest(it, evp, data, certfp, &len) != 1)
		len = 0;
	return (int) len;
}

int
rb_get_ssl_certfp(rb_fde_t *F, uint8_t certfp[RB_SSL_CERTFP_LEN], int method)
{
	int len = 0;
	X509 *cert;
	int res;

	if (F->ssl == NULL)
		return 0;

	cert = SSL_get_peer_certificate((SSL *) F->ssl);
	if(cert == NULL)
		return 0;

	res = SSL_get_verify_result((SSL *) F->ssl);
	switch(res)
	{
	case X509_V_OK:
	case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
	case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
	case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
	case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
	case X509_V_ERR_CERT_UNTRUSTED:
		len = make_certfp(cert, certfp, method);

	default:	/* to silence code inspectors */
		break;
	}

	X509_free(cert);
	return len;
}

int
rb_get_ssl_certfp_file(const char *filename, uint8_t certfp[RB_SSL_CERTFP_LEN], int method)
{
	X509 *cert;
	FILE *f = fopen(filename, "r");

	if (!f)
		return -1;

	cert = PEM_read_X509(f, NULL, NULL, NULL);
	fclose(f);

	if (cert) {
		unsigned int len = make_certfp(cert, certfp, method);
		X509_free(cert);
		return len;
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
#ifdef LRB_SSL_FULL_VERSION_INFO
	if (LRB_SSL_VNUM_RUNTIME == LRB_SSL_VNUM_COMPILETIME)
		rb_snprintf(buf, len, "OpenSSL: compiled 0x%lx, library %s",
		            LRB_SSL_VNUM_COMPILETIME, LRB_SSL_VTEXT_COMPILETIME);
	else
		rb_snprintf(buf, len, "OpenSSL: compiled (0x%lx, %s), library (0x%lx, %s)",
		            LRB_SSL_VNUM_COMPILETIME, LRB_SSL_VTEXT_COMPILETIME,
		            LRB_SSL_VNUM_RUNTIME, LRB_SSL_VTEXT_RUNTIME);
#else
	rb_snprintf(buf, len, "OpenSSL: compiled 0x%lx, library %s",
	            LRB_SSL_VNUM_COMPILETIME, LRB_SSL_VTEXT_RUNTIME);
#endif
}

const char *
rb_ssl_get_cipher(rb_fde_t *F)
{
	const SSL_CIPHER *sslciph;

	if(F == NULL || F->ssl == NULL)
		return NULL;

	if((sslciph = SSL_get_current_cipher(F->ssl)) == NULL)
		return NULL;

	return SSL_CIPHER_get_name(sslciph);
}

#endif /* HAVE_OPENSSL */
