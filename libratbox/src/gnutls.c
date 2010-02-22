/*
 *  libratbox: a library used by ircd-ratbox and other things
 *  gnutls.c: gnutls related code
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
 *  $Id: gnutls.c 26296 2008-12-13 03:36:00Z androsyn $
 */

#include <libratbox_config.h>
#include <ratbox_lib.h>
#include <commio-int.h>
#include <commio-ssl.h>
#ifdef HAVE_GNUTLS

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gcrypt.h>

static gnutls_certificate_credentials x509;
static gnutls_dh_params dh_params;



#define SSL_P(x) *((gnutls_session_t *)F->ssl)

void
rb_ssl_shutdown(rb_fde_t *F)
{
	int i;
	if(F == NULL || F->ssl == NULL)
		return;
	for(i = 0; i < 4; i++)
	{
		if(gnutls_bye(SSL_P(F), GNUTLS_SHUT_RDWR) == GNUTLS_E_SUCCESS)
			break;
	}
	gnutls_deinit(SSL_P(F));
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
do_ssl_handshake(rb_fde_t *F, PF * callback)
{
	int ret;
	int flags;

	ret = gnutls_handshake(SSL_P(F));
	if(ret < 0)
	{
		if((ret == GNUTLS_E_INTERRUPTED && rb_ignore_errno(errno)) || ret == GNUTLS_E_AGAIN)
		{
			if(gnutls_record_get_direction(SSL_P(F)) == 0)
				flags = RB_SELECT_READ;
			else
				flags = RB_SELECT_WRITE;
			rb_setselect(F, flags, callback, NULL);
			return 0;
		}
		F->ssl_errno = ret;
		return -1;
	}
	return 1;		/* handshake is finished..go about life */
}

static void
rb_ssl_tryaccept(rb_fde_t *F, void *data)
{
	int ret;
	struct acceptdata *ad;

	lrb_assert(F->accept != NULL);

	ret = do_ssl_handshake(F, rb_ssl_tryaccept);

	/* do_ssl_handshake does the rb_setselect */
	if(ret == 0)
		return;

	ad = F->accept;
	F->accept = NULL;
	rb_settimeout(F, 0, NULL, NULL);
	rb_setselect(F, RB_SELECT_READ | RB_SELECT_WRITE, NULL, NULL);
	
	if(ret > 0)
		ad->callback(F, RB_OK, (struct sockaddr *)&ad->S, ad->addrlen, ad->data);
	else
		ad->callback(F, RB_ERROR_SSL, NULL, 0, ad->data);

	rb_free(ad);
}

void
rb_ssl_start_accepted(rb_fde_t *new_F, ACCB * cb, void *data, int timeout)
{
	gnutls_session_t *ssl;
	new_F->type |= RB_FD_SSL;
	ssl = new_F->ssl = rb_malloc(sizeof(gnutls_session_t));
	new_F->accept = rb_malloc(sizeof(struct acceptdata));

	new_F->accept->callback = cb;
	new_F->accept->data = data;
	rb_settimeout(new_F, timeout, rb_ssl_timeout, NULL);

	new_F->accept->addrlen = 0;

	gnutls_init(ssl, GNUTLS_SERVER);
	gnutls_set_default_priority(*ssl);
	gnutls_credentials_set(*ssl, GNUTLS_CRD_CERTIFICATE, x509);
	gnutls_dh_set_prime_bits(*ssl, 1024);
	gnutls_transport_set_ptr(*ssl, (gnutls_transport_ptr_t) (long int)new_F->fd);
	gnutls_certificate_server_set_request(*ssl, GNUTLS_CERT_REQUEST);
	if(do_ssl_handshake(new_F, rb_ssl_tryaccept))
	{
		struct acceptdata *ad = new_F->accept;
		new_F->accept = NULL;
		ad->callback(new_F, RB_OK, (struct sockaddr *)&ad->S, ad->addrlen, ad->data);
		rb_free(ad);
	}

}




void
rb_ssl_accept_setup(rb_fde_t *F, rb_fde_t *new_F, struct sockaddr *st, int addrlen)
{
	new_F->type |= RB_FD_SSL;
	new_F->ssl = rb_malloc(sizeof(gnutls_session_t));
	new_F->accept = rb_malloc(sizeof(struct acceptdata));

	new_F->accept->callback = F->accept->callback;
	new_F->accept->data = F->accept->data;
	rb_settimeout(new_F, 10, rb_ssl_timeout, NULL);
	memcpy(&new_F->accept->S, st, addrlen);
	new_F->accept->addrlen = addrlen;

	gnutls_init((gnutls_session_t *) new_F->ssl, GNUTLS_SERVER);
	gnutls_set_default_priority(SSL_P(new_F));
	gnutls_credentials_set(SSL_P(new_F), GNUTLS_CRD_CERTIFICATE, x509);
	gnutls_dh_set_prime_bits(SSL_P(new_F), 1024);
	gnutls_transport_set_ptr(SSL_P(new_F), (gnutls_transport_ptr_t) (long int)rb_get_fd(new_F));
	gnutls_certificate_server_set_request(SSL_P(new_F), GNUTLS_CERT_REQUEST);
	if(do_ssl_handshake(F, rb_ssl_tryaccept))
	{
		struct acceptdata *ad = F->accept;
		F->accept = NULL;
		ad->callback(F, RB_OK, (struct sockaddr *)&ad->S, ad->addrlen, ad->data);
		rb_free(ad);
	}
}




static ssize_t
rb_ssl_read_or_write(int r_or_w, rb_fde_t *F, void *rbuf, const void *wbuf, size_t count)
{
	ssize_t ret;
	gnutls_session_t *ssl = F->ssl;

	if(r_or_w == 0)
		ret = gnutls_record_recv(*ssl, rbuf, count);
	else
		ret = gnutls_record_send(*ssl, wbuf, count);

	if(ret < 0)
	{
		switch (ret)
		{
		case GNUTLS_E_AGAIN:
		case GNUTLS_E_INTERRUPTED:
			if(rb_ignore_errno(errno))
			{
				if(gnutls_record_get_direction(*ssl) == 0)
					return RB_RW_SSL_NEED_READ;
				else
					return RB_RW_SSL_NEED_WRITE;
				break;
			}
		default:
			F->ssl_errno = ret;
			errno = EIO;
			return RB_RW_IO_ERROR;
		}
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

static void
rb_gcry_random_seed(void *unused)
{
	gcry_fast_random_poll();
}

int
rb_init_ssl(void)
{
	gnutls_global_init();

	if(gnutls_certificate_allocate_credentials(&x509) != GNUTLS_E_SUCCESS)
	{
		rb_lib_log("rb_init_ssl: Unable to allocate SSL/TLS certificate credentials");
		return 0;
	}
	rb_event_addish("rb_gcry_random_seed", rb_gcry_random_seed, NULL, 300);
	return 1;
}

static void
rb_free_datum_t(gnutls_datum_t * d)
{
	rb_free(d->data);
	rb_free(d);
}

static gnutls_datum_t *
rb_load_file_into_datum_t(const char *file)
{
	FILE *f;
	gnutls_datum_t *datum;
	struct stat fileinfo;
	if((f = fopen(file, "r")) == NULL)
		return NULL;
	if(fstat(fileno(f), &fileinfo))
		return NULL;

	datum = rb_malloc(sizeof(gnutls_datum_t));

	if(fileinfo.st_size > 131072)	/* deal with retards */
		datum->size = 131072;
	else
		datum->size = fileinfo.st_size;

	datum->data = rb_malloc(datum->size + 1);
	fread(datum->data, datum->size, 1, f);
	fclose(f);
	return datum;
}

int
rb_setup_ssl_server(const char *cert, const char *keyfile, const char *dhfile)
{
	int ret;
	gnutls_datum_t *d_cert, *d_key;
	if(cert == NULL)
	{
		rb_lib_log("rb_setup_ssl_server: No certificate file");
		return 0;
	}

	if((d_cert = rb_load_file_into_datum_t(cert)) == NULL)
	{
		rb_lib_log("rb_setup_ssl_server: Error loading certificate: %s", strerror(errno));
		return 0;
	}

	if((d_key = rb_load_file_into_datum_t(keyfile)) == NULL)
	{
		rb_lib_log("rb_setup_ssl_server: Error loading key: %s", strerror(errno));
		return 0;
	}


	if((ret =
	    gnutls_certificate_set_x509_key_mem(x509, d_cert, d_key,
						GNUTLS_X509_FMT_PEM)) != GNUTLS_E_SUCCESS)
	{
		rb_lib_log("rb_setup_ssl_server: Error loading certificate or key file: %s",
			   gnutls_strerror(ret));
		return 0;
	}
	rb_free_datum_t(d_cert);
	rb_free_datum_t(d_key);

	if(dhfile != NULL)
	{
		if(gnutls_dh_params_init(&dh_params) == GNUTLS_E_SUCCESS)
		{
			gnutls_datum_t *data;
			int xret;
			data = rb_load_file_into_datum_t(dhfile);
			if(data != NULL)
			{
				xret = gnutls_dh_params_import_pkcs3(dh_params, data,
								     GNUTLS_X509_FMT_PEM);
				if(xret < 0)
					rb_lib_log
						("rb_setup_ssl_server: Error parsing DH file: %s\n",
						 gnutls_strerror(xret));
				rb_free_datum_t(data);
			}
			gnutls_certificate_set_dh_params(x509, dh_params);
		}
		else
			rb_lib_log("rb_setup_ssl_server: Unable to setup DH parameters");
	}
	return 1;
}

int
rb_ssl_listen(rb_fde_t *F, int backlog)
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
	int ret;

	ret = do_ssl_handshake(F, rb_ssl_tryconn_cb);

	switch (ret)
	{
	case -1:
		rb_ssl_connect_realcb(F, RB_ERROR_SSL, sconn);
		break;
	case 0:
		/* do_ssl_handshake does the rb_setselect stuff */
		return;
	default:
		break;


	}
	rb_ssl_connect_realcb(F, RB_OK, sconn);
}

static void
rb_ssl_tryconn(rb_fde_t *F, int status, void *data)
{
	struct ssl_connect *sconn = data;
	if(status != RB_OK)
	{
		rb_ssl_connect_realcb(F, status, sconn);
		return;
	}

	F->type |= RB_FD_SSL;


	rb_settimeout(F, sconn->timeout, rb_ssl_tryconn_timeout_cb, sconn);
	F->ssl = rb_malloc(sizeof(gnutls_session_t));
	gnutls_init(F->ssl, GNUTLS_CLIENT);
	gnutls_set_default_priority(SSL_P(F));
	gnutls_dh_set_prime_bits(SSL_P(F), 1024);
	gnutls_transport_set_ptr(SSL_P(F), (gnutls_transport_ptr_t) (long int)F->fd);

	if(do_ssl_handshake(F, rb_ssl_tryconn_cb))
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
	F->ssl = rb_malloc(sizeof(gnutls_session_t));

	gnutls_init(F->ssl, GNUTLS_CLIENT);
	gnutls_set_default_priority(SSL_P(F));
	gnutls_dh_set_prime_bits(SSL_P(F), 1024);
	gnutls_transport_set_ptr(SSL_P(F), (gnutls_transport_ptr_t) (long int)F->fd);

	rb_settimeout(F, sconn->timeout, rb_ssl_tryconn_timeout_cb, sconn);

	if(do_ssl_handshake(F, rb_ssl_tryconn_cb))
	{
		rb_ssl_connect_realcb(F, RB_OK, sconn);
	}
}

int
rb_init_prng(const char *path, prng_seed_t seed_type)
{
	gcry_fast_random_poll();
	return 1;
}

int
rb_get_random(void *buf, size_t length)
{
	gcry_randomize(buf, length, GCRY_STRONG_RANDOM);
	return 1;
}

int
rb_get_pseudo_random(void *buf, size_t length)
{
	gcry_randomize(buf, length, GCRY_WEAK_RANDOM);
	return 1;
}

const char *
rb_get_ssl_strerror(rb_fde_t *F)
{
	return gnutls_strerror(F->ssl_errno);
}

int
rb_get_ssl_certfp(rb_fde_t *F, uint8_t certfp[RB_SSL_CERTFP_LEN])
{
	gnutls_x509_crt_t cert;
	unsigned int cert_list_size;
	const gnutls_datum_t *cert_list;
	uint8_t digest[RB_SSL_CERTFP_LEN * 2];
	size_t digest_size;

	if (gnutls_certificate_type_get(SSL_P(F)) != GNUTLS_CRT_X509)
		return 0;

	if (gnutls_x509_crt_init(&cert) < 0)
		return 0;

	cert_list_size = 0;
	cert_list = gnutls_certificate_get_peers(SSL_P(F), &cert_list_size);
	if (cert_list == NULL)
	{
		gnutls_x509_crt_deinit(cert);
		return 0;
	}

	if (gnutls_x509_crt_import(cert, &cert_list[0], GNUTLS_X509_FMT_DER) < 0)
	{
		gnutls_x509_crt_deinit(cert);
		return 0;
	}

	if (gnutls_x509_crt_get_fingerprint(cert, GNUTLS_DIG_SHA1, digest, &digest_size) < 0)
	{
		gnutls_x509_crt_deinit(cert);
		return 0;
	}

	memcpy(certfp, digest, RB_SSL_CERTFP_LEN);

	gnutls_x509_crt_deinit(cert);
	return 1;
}

int
rb_supports_ssl(void)
{
	return 1;
}

void
rb_get_ssl_info(char *buf, size_t len)
{
	rb_snprintf(buf, len, "GNUTLS: compiled (%s), library(%s)", 
		    LIBGNUTLS_VERSION, gnutls_check_version(NULL));
}
  
        
#endif /* HAVE_GNUTLS */
