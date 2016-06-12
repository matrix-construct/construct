/*
 *  librb: a library used by ircd-ratbox and other things
 *  mbedtls.c: ARM mbedTLS backend
 *
 *  Copyright (C) 2007-2008 ircd-ratbox development team
 *  Copyright (C) 2007-2008 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2015 William Pitcock <nenolod@dereferenced.org>
 *  Copyright (C) 2016 Aaron Jones <aaronmdjones@gmail.com>
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
#include <commio-int.h>
#include <commio-ssl.h>
#include <stdbool.h>

#ifdef HAVE_MBEDTLS

#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/certs.h"
#include "mbedtls/x509.h"
#include "mbedtls/ssl.h"
#include "mbedtls/net.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"
#include "mbedtls/dhm.h"
#include "mbedtls/version.h"

#include "mbedtls_embedded_data.h"

typedef struct
{
	mbedtls_x509_crt	 crt;
	mbedtls_pk_context	 key;
	mbedtls_dhm_context	 dhp;
	mbedtls_ssl_config	 server_cfg;
	mbedtls_ssl_config	 client_cfg;
	size_t			 refcount;
} rb_mbedtls_cfg_context;

typedef struct
{
	rb_mbedtls_cfg_context	*cfg;
	mbedtls_ssl_context	 ssl;
} rb_mbedtls_ssl_context;

#define SSL_C(x)  ((rb_mbedtls_ssl_context *) (x)->ssl)->cfg
#define SSL_P(x) &((rb_mbedtls_ssl_context *) (x)->ssl)->ssl

static mbedtls_ctr_drbg_context ctr_drbg_ctx;
static mbedtls_entropy_context entropy_ctx;

static mbedtls_x509_crt dummy_ca_ctx;
static rb_mbedtls_cfg_context *rb_mbedtls_cfg = NULL;

static const char *
rb_get_ssl_strerror_internal(int err)
{
	static char errbuf[512];

#ifdef MBEDTLS_ERROR_C
	char mbed_errbuf[512];
	mbedtls_strerror(err, mbed_errbuf, sizeof mbed_errbuf);
	snprintf(errbuf, sizeof errbuf, "(-0x%x) %s", -err, mbed_errbuf);
#else
	snprintf(errbuf, sizeof errbuf, "-0x%x", -err);
#endif

	return errbuf;
}

const char *
rb_get_ssl_strerror(rb_fde_t *F)
{
	return rb_get_ssl_strerror_internal(F->ssl_errno);
}

static void rb_mbedtls_cfg_incref(rb_mbedtls_cfg_context *cfg)
{
	lrb_assert(cfg->refcount > 0);

	cfg->refcount++;
}

static void rb_mbedtls_cfg_decref(rb_mbedtls_cfg_context *cfg)
{
	if(cfg == NULL)
		return;

	lrb_assert(cfg->refcount > 0);

	if((--cfg->refcount) > 0)
		return;

	mbedtls_ssl_config_free(&cfg->client_cfg);
	mbedtls_ssl_config_free(&cfg->server_cfg);
	mbedtls_dhm_free(&cfg->dhp);
	mbedtls_pk_free(&cfg->key);
	mbedtls_x509_crt_free(&cfg->crt);

	rb_free(cfg);
}

static rb_mbedtls_cfg_context *rb_mbedtls_cfg_new(void)
{
	rb_mbedtls_cfg_context *cfg;
	int ret;

	if((cfg = rb_malloc(sizeof(rb_mbedtls_cfg_context))) == NULL)
		return NULL;

	mbedtls_x509_crt_init(&cfg->crt);
	mbedtls_pk_init(&cfg->key);
	mbedtls_dhm_init(&cfg->dhp);
	mbedtls_ssl_config_init(&cfg->server_cfg);
	mbedtls_ssl_config_init(&cfg->client_cfg);

	cfg->refcount = 1;

	if((ret = mbedtls_ssl_config_defaults(&cfg->server_cfg,
	             MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
	             MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
	{
		rb_lib_log("rb_mbedtls_cfg_new: ssl_config_defaults (server): %s",
		           rb_get_ssl_strerror_internal(ret));
		rb_mbedtls_cfg_decref(cfg);
		return NULL;
	}

	if((ret = mbedtls_ssl_config_defaults(&cfg->client_cfg,
	             MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
	             MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
	{
		rb_lib_log("rb_mbedtls_cfg_new: ssl_config_defaults (client): %s",
		           rb_get_ssl_strerror_internal(ret));
		rb_mbedtls_cfg_decref(cfg);
		return NULL;
	}

	mbedtls_ssl_conf_rng(&cfg->server_cfg, mbedtls_ctr_drbg_random, &ctr_drbg_ctx);
	mbedtls_ssl_conf_rng(&cfg->client_cfg, mbedtls_ctr_drbg_random, &ctr_drbg_ctx);

	mbedtls_ssl_conf_ca_chain(&cfg->server_cfg, &dummy_ca_ctx, NULL);
	mbedtls_ssl_conf_ca_chain(&cfg->client_cfg, &dummy_ca_ctx, NULL);

	mbedtls_ssl_conf_authmode(&cfg->server_cfg, MBEDTLS_SSL_VERIFY_OPTIONAL);
	mbedtls_ssl_conf_authmode(&cfg->client_cfg, MBEDTLS_SSL_VERIFY_NONE);

	return cfg;
}

void
rb_ssl_shutdown(rb_fde_t *F)
{
	if(F == NULL || F->ssl == NULL)
		return;

	if(SSL_P(F) != NULL)
	{
		for(int i = 0; i < 4; i++)
		{
			int r = mbedtls_ssl_close_notify(SSL_P(F));
			if(r != MBEDTLS_ERR_SSL_WANT_READ && r != MBEDTLS_ERR_SSL_WANT_WRITE)
				break;
		}
		mbedtls_ssl_free(SSL_P(F));
	}

	if(SSL_C(F) != NULL)
		rb_mbedtls_cfg_decref(SSL_C(F));

	rb_free(F->ssl);
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

static int
do_ssl_handshake(rb_fde_t *F, PF * callback, void *data)
{
	int ret = mbedtls_ssl_handshake(SSL_P(F));

	if(ret == 0)
	{
		F->handshake_count++;
		return 1;
	}

	if(ret == -1 && rb_ignore_errno(errno))
		ret = MBEDTLS_ERR_SSL_WANT_READ;

	switch(ret)
	{
	case MBEDTLS_ERR_SSL_WANT_READ:
		rb_setselect(F, RB_SELECT_READ, callback, data);
		return 0;
	case MBEDTLS_ERR_SSL_WANT_WRITE:
		rb_setselect(F, RB_SELECT_WRITE, callback, data);
		return 0;
	default:
		F->ssl_errno = ret;
		return -1;
	}
}

static void
rb_ssl_tryaccept(rb_fde_t *F, void *data)
{
	lrb_assert(F->accept != NULL);

	int ret = do_ssl_handshake(F, rb_ssl_tryaccept, NULL);

	/* do_ssl_handshake does the rb_setselect */
	if(ret == 0)
		return;

	struct acceptdata *ad = F->accept;
	F->accept = NULL;
	rb_settimeout(F, 0, NULL, NULL);
	rb_setselect(F, RB_SELECT_READ | RB_SELECT_WRITE, NULL, NULL);

	if(ret > 0)
		ad->callback(F, RB_OK, (struct sockaddr *)&ad->S, ad->addrlen, ad->data);
	else
		ad->callback(F, RB_ERROR_SSL, NULL, 0, ad->data);

	rb_free(ad);
}

static int
rb_ssl_read_cb(void *opaque, unsigned char *buf, size_t size)
{
	rb_fde_t *F = opaque;

	int ret = (int) read(F->fd, buf, size);
	if(ret < 0 && rb_ignore_errno(errno))
		return MBEDTLS_ERR_SSL_WANT_READ;

	return ret;
}

static int
rb_ssl_write_cb(void *opaque, const unsigned char *buf, size_t size)
{
	rb_fde_t *F = opaque;

	int ret = (int) write(F->fd, buf, size);
	if(ret < 0 && rb_ignore_errno(errno))
		return MBEDTLS_ERR_SSL_WANT_WRITE;

	return ret;
}

static void
rb_ssl_setup_mbed_context(rb_fde_t *F, bool is_server)
{
	rb_mbedtls_ssl_context *mbed_ssl_ctx;
	mbedtls_ssl_config *mbed_config;
	int ret;

	if((mbed_ssl_ctx = rb_malloc(sizeof(rb_mbedtls_ssl_context))) == NULL)
	{
		rb_lib_log("rb_ssl_setup_mbed_context: rb_malloc: allocation failure");
		rb_close(F);
		return;
	}

	if(is_server)
		mbed_config = &rb_mbedtls_cfg->server_cfg;
	else
		mbed_config = &rb_mbedtls_cfg->client_cfg;

	mbedtls_ssl_init(&mbed_ssl_ctx->ssl);
	mbedtls_ssl_set_bio(&mbed_ssl_ctx->ssl, F, rb_ssl_write_cb, rb_ssl_read_cb, NULL);

	if((ret = mbedtls_ssl_setup(&mbed_ssl_ctx->ssl, mbed_config)) != 0)
	{
		rb_lib_log("rb_ssl_setup_mbed_context: ssl_setup: %s",
		           rb_get_ssl_strerror_internal(ret));
		mbedtls_ssl_free(&mbed_ssl_ctx->ssl);
		rb_free(mbed_ssl_ctx);
		rb_close(F);
		return;
	}

	mbed_ssl_ctx->cfg = rb_mbedtls_cfg;
	rb_mbedtls_cfg_incref(mbed_ssl_ctx->cfg);
	F->ssl = mbed_ssl_ctx;
}

void
rb_ssl_start_accepted(rb_fde_t *F, ACCB * cb, void *data, int timeout)
{
	F->type |= RB_FD_SSL;
	F->accept = rb_malloc(sizeof(struct acceptdata));

	F->accept->callback = cb;
	F->accept->data = data;
	rb_settimeout(F, timeout, rb_ssl_timeout, NULL);

	F->accept->addrlen = 0;

	rb_ssl_setup_mbed_context(F, true);
	if(do_ssl_handshake(F, rb_ssl_tryaccept, NULL))
	{
		struct acceptdata *ad = F->accept;
		F->accept = NULL;
		ad->callback(F, RB_OK, (struct sockaddr *)&ad->S, ad->addrlen, ad->data);
		rb_free(ad);
	}
}

void
rb_ssl_accept_setup(rb_fde_t *F, rb_fde_t *new_F, struct sockaddr *st, int addrlen)
{
	new_F->type |= RB_FD_SSL;
	new_F->accept = rb_malloc(sizeof(struct acceptdata));

	new_F->accept->callback = F->accept->callback;
	new_F->accept->data = F->accept->data;
	rb_settimeout(new_F, 10, rb_ssl_timeout, NULL);

	memcpy(&new_F->accept->S, st, addrlen);
	new_F->accept->addrlen = addrlen;

	rb_ssl_setup_mbed_context(new_F, true);
	if(do_ssl_handshake(F, rb_ssl_tryaccept, NULL))
	{
		struct acceptdata *ad = F->accept;
		F->accept = NULL;
		ad->callback(F, RB_OK, (struct sockaddr *)&ad->S, ad->addrlen, ad->data);
		rb_free(ad);
	}
}

static ssize_t
rb_ssl_read_or_write(bool do_read, rb_fde_t *F, void *rbuf, const void *wbuf, size_t count)
{
	ssize_t ret;

	if(do_read)
		ret = mbedtls_ssl_read(SSL_P(F), rbuf, count);
	else
		ret = mbedtls_ssl_write(SSL_P(F), wbuf, count);

	if(ret < 0)
	{
		switch(ret)
		{
		case MBEDTLS_ERR_SSL_WANT_READ:
			return RB_RW_SSL_NEED_READ;
		case MBEDTLS_ERR_SSL_WANT_WRITE:
			return RB_RW_SSL_NEED_WRITE;
		default:
			F->ssl_errno = ret;
			errno = EIO;
			return RB_RW_SSL_ERROR;
		}
	}

	return ret;
}

ssize_t
rb_ssl_read(rb_fde_t *F, void *buf, size_t count)
{
	return rb_ssl_read_or_write(true, F, buf, NULL, count);
}

ssize_t
rb_ssl_write(rb_fde_t *F, const void *buf, size_t count)
{
	return rb_ssl_read_or_write(false, F, NULL, buf, count);
}

int
rb_init_ssl(void)
{
	int ret;

	mbedtls_ctr_drbg_init(&ctr_drbg_ctx);
	mbedtls_entropy_init(&entropy_ctx);

	if((ret = mbedtls_ctr_drbg_seed(&ctr_drbg_ctx, mbedtls_entropy_func, &entropy_ctx,
	            (const unsigned char *)rb_mbedtls_personal_str, sizeof(rb_mbedtls_personal_str))) != 0)
	{
		rb_lib_log("rb_init_ssl: ctr_drbg_seed: %s",
		           rb_get_ssl_strerror_internal(ret));
		return 0;
	}

	if((ret = mbedtls_x509_crt_parse_der(&dummy_ca_ctx, rb_mbedtls_dummy_ca_certificate,
	                                     sizeof(rb_mbedtls_dummy_ca_certificate))) != 0)
	{
		rb_lib_log("rb_init_ssl: x509_crt_parse_der (Dummy CA): %s",
		           rb_get_ssl_strerror_internal(ret));
		return 0;
	}

	return 1;
}

int
rb_setup_ssl_server(const char *certfile, const char *keyfile, const char *dhfile, const char *cipher_list)
{
	rb_mbedtls_cfg_context *newcfg;
	int ret;

	if(certfile == NULL)
	{
		rb_lib_log("rb_setup_ssl_server: no certificate file specified");
		return 0;
	}

	if(keyfile == NULL)
		keyfile = certfile;

	if((newcfg = rb_mbedtls_cfg_new()) == NULL)
	{
		rb_lib_log("rb_setup_ssl_server: rb_mbedtls_cfg_new: allocation failed");
		return 0;
	}

	if((ret = mbedtls_x509_crt_parse_file(&newcfg->crt, certfile)) != 0)
	{
		rb_lib_log("rb_setup_ssl_server: x509_crt_parse_file ('%s'): %s",
		           certfile, rb_get_ssl_strerror_internal(ret));
		rb_mbedtls_cfg_decref(newcfg);
		return 0;
	}
	if((ret = mbedtls_pk_parse_keyfile(&newcfg->key, keyfile, NULL)) != 0)
	{
		rb_lib_log("rb_setup_ssl_server: pk_parse_keyfile ('%s'): %s",
		           keyfile, rb_get_ssl_strerror_internal(ret));
		rb_mbedtls_cfg_decref(newcfg);
		return 0;
	}

	/* Absense of DH parameters does not matter with mbedTLS, as it comes with its own defaults
	   Thus, clients can still use DHE- ciphersuites, just over a weaker, common DH group
	   So, we do not consider failure to parse DH parameters as fatal */
	if(dhfile == NULL)
	{
		rb_lib_log("rb_setup_ssl_server: no DH parameters file specified");
	}
	else
	{
		if((ret = mbedtls_dhm_parse_dhmfile(&newcfg->dhp, dhfile)) != 0)
		{
			rb_lib_log("rb_setup_ssl_server: dhm_parse_dhmfile ('%s'): %s",
			           dhfile, rb_get_ssl_strerror_internal(ret));
		}
		else if((ret = mbedtls_ssl_conf_dh_param_ctx(&newcfg->server_cfg, &newcfg->dhp)) != 0)
		{
			rb_lib_log("rb_setup_ssl_server: ssl_conf_dh_param_ctx: %s",
			           rb_get_ssl_strerror_internal(ret));
		}
	}

	if((ret = mbedtls_ssl_conf_own_cert(&newcfg->server_cfg, &newcfg->crt, &newcfg->key)) != 0)
	{
		rb_lib_log("rb_setup_ssl_server: ssl_conf_own_cert (server): %s",
		           rb_get_ssl_strerror_internal(ret));
		rb_mbedtls_cfg_decref(newcfg);
		return 0;
	}
	if((ret = mbedtls_ssl_conf_own_cert(&newcfg->client_cfg, &newcfg->crt, &newcfg->key)) != 0)
	{
		rb_lib_log("rb_setup_ssl_server: ssl_conf_own_cert (client): %s",
		           rb_get_ssl_strerror_internal(ret));
		rb_mbedtls_cfg_decref(newcfg);
		return 0;
	}

	/* XXX support cipher lists when added to mbedtls */

	rb_mbedtls_cfg_decref(rb_mbedtls_cfg);
	rb_mbedtls_cfg = newcfg;

	return 1;
}

int
rb_ssl_listen(rb_fde_t *F, int backlog, int defer_accept)
{
	int result = rb_listen(F, backlog, defer_accept);
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
	int ret = do_ssl_handshake(F, rb_ssl_tryconn_cb, data);

	switch(ret)
	{
	case -1:
		rb_ssl_connect_realcb(F, RB_ERROR_SSL, data);
		break;
	case 0:
		/* do_ssl_handshake does the rb_setselect stuff */
		return;
	default:
		break;
	}
	rb_ssl_connect_realcb(F, RB_OK, data);
}

static void
rb_ssl_tryconn(rb_fde_t *F, int status, void *data)
{
	if(status != RB_OK)
	{
		rb_ssl_connect_realcb(F, status, data);
		return;
	}

	F->type |= RB_FD_SSL;

	rb_ssl_setup_mbed_context(F, false);
	rb_settimeout(F, ((struct ssl_connect *)data)->timeout, rb_ssl_tryconn_timeout_cb, data);
	do_ssl_handshake(F, rb_ssl_tryconn_cb, data);
}

void
rb_connect_tcp_ssl(rb_fde_t *F, struct sockaddr *dest,
		   struct sockaddr *clocal, CNCB * callback, void *data, int timeout)
{
	if(F == NULL)
		return;

	struct ssl_connect *sconn = rb_malloc(sizeof(struct ssl_connect));
	sconn->data = data;
	sconn->callback = callback;
	sconn->timeout = timeout;

	rb_connect_tcp(F, dest, clocal, rb_ssl_tryconn, sconn, timeout);
}

void
rb_ssl_start_connected(rb_fde_t *F, CNCB * callback, void *data, int timeout)
{
	if(F == NULL)
		return;

	struct ssl_connect *sconn = rb_malloc(sizeof(struct ssl_connect));
	sconn->data = data;
	sconn->callback = callback;
	sconn->timeout = timeout;

	F->connect = rb_malloc(sizeof(struct conndata));
	F->connect->callback = callback;
	F->connect->data = data;
	F->type |= RB_FD_SSL;

	rb_ssl_setup_mbed_context(F, false);
	rb_settimeout(F, sconn->timeout, rb_ssl_tryconn_timeout_cb, sconn);
	do_ssl_handshake(F, rb_ssl_tryconn_cb, sconn);
}

int
rb_init_prng(const char *path, prng_seed_t seed_type)
{
	return 1;
}

int
rb_get_random(void *buf, size_t length)
{
	if(mbedtls_ctr_drbg_random(&ctr_drbg_ctx, buf, length))
		return 0;

	return 1;
}

static size_t
rb_make_certfp(const mbedtls_x509_crt *peer_cert, uint8_t certfp[RB_SSL_CERTFP_LEN], int method)
{
	const mbedtls_md_info_t *md_info;
	mbedtls_md_type_t md_type;
	bool spki = false;
	size_t hashlen;
	int ret;

	uint8_t der_pubkey[8192];
	void* data = peer_cert->raw.p;
	size_t datalen = peer_cert->raw.len;

	switch(method)
	{
	case RB_SSL_CERTFP_METH_CERT_SHA1:
		md_type = MBEDTLS_MD_SHA1;
		hashlen = RB_SSL_CERTFP_LEN_SHA1;
		break;

	case RB_SSL_CERTFP_METH_SPKI_SHA256:
		spki = true;
	case RB_SSL_CERTFP_METH_CERT_SHA256:
		md_type = MBEDTLS_MD_SHA256;
		hashlen = RB_SSL_CERTFP_LEN_SHA256;
		break;

	case RB_SSL_CERTFP_METH_SPKI_SHA512:
		spki = true;
	case RB_SSL_CERTFP_METH_CERT_SHA512:
		md_type = MBEDTLS_MD_SHA512;
		hashlen = RB_SSL_CERTFP_LEN_SHA512;
		break;

	default:
		return 0;
	}

	if((md_info = mbedtls_md_info_from_type(md_type)) == NULL)
		return 0;

	if(spki)
	{
		if ((ret = mbedtls_pk_write_pubkey_der((mbedtls_pk_context *)&peer_cert->pk,
		                                       der_pubkey, sizeof(der_pubkey))) < 0)
		{
			rb_lib_log("rb_get_ssl_certfp: pk_write_pubkey_der: %s",
			           rb_get_ssl_strerror_internal(ret));
			return 0;
		}
		data = der_pubkey + (sizeof(der_pubkey) - ret);
		datalen = ret;
	}

	if((ret = mbedtls_md(md_info, data, datalen, certfp)) != 0)
	{
		rb_lib_log("rb_get_ssl_certfp: mbedtls_md: %s",
		           rb_get_ssl_strerror_internal(ret));
		return 0;
	}

	return hashlen;
}

int
rb_get_ssl_certfp(rb_fde_t *F, uint8_t certfp[RB_SSL_CERTFP_LEN], int method)
{
	const mbedtls_x509_crt *peer_cert;

	if ((peer_cert = mbedtls_ssl_get_peer_cert(SSL_P(F))) == NULL)
		return 0;

	return (int) rb_make_certfp(peer_cert, certfp, method);
}

int
rb_get_ssl_certfp_file(const char *filename, uint8_t certfp[RB_SSL_CERTFP_LEN], int method)
{
	mbedtls_x509_crt cert;
	int ret;

	mbedtls_x509_crt_init(&cert);

	if ((ret = mbedtls_x509_crt_parse_file(&cert, filename)) != 0)
		return -1;

	size_t len = rb_make_certfp(&cert, certfp, method);

	mbedtls_x509_crt_free(&cert);

	return (int) len;
}

int
rb_supports_ssl(void)
{
	return 1;
}

void
rb_get_ssl_info(char *buf, size_t len)
{
	char version_str[512];
	mbedtls_version_get_string(version_str);

	snprintf(buf, len, "ARM mbedTLS: compiled (v%s), library (v%s)",
	         MBEDTLS_VERSION_STRING, version_str);
}

const char *
rb_ssl_get_cipher(rb_fde_t *F)
{
	if(F == NULL || F->ssl == NULL || SSL_P(F) == NULL)
		return NULL;
	return mbedtls_ssl_get_ciphersuite(SSL_P(F));
}

#endif /* HAVE_MBEDTLS */
