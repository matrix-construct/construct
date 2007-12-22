/*
 * charybdis: an advanced ircd
 * gnutls.c: GnuTLS support functions.
 *
 * Copyright (c) 2008 William Pitcock <nenolod -at- sacredspiral.co.uk>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef GNUTLS

#include "stdinc.h"
#include "config.h"

#include <gnutls/gnutls.h>
#include <gcrypt.h> /* for gcry_control */

#define DH_BITS 1024

static gnutls_certificate_credentials_t x509_cred;
static gnutls_priority_t priority_cache;
static gnutls_dh_params_t dh_params;

void
irc_tls_init(void)
{
	/* force use of /dev/urandom. */
	gcry_control(GCRYCTL_ENABLE_QUICK_RANDOM, 0);

	gnutls_global_init();

	gnutls_certificate_allocate_credentials(&x509_cred);
	gnutls_certificate_set_x509_trust_file(x509_cred, ETCPATH "/ca.pem", GNUTLS_X509_FMT_PEM);
	gnutls_certificate_set_x509_crl_file(x509_cred,   ETCPATH "/crl.pem", GNUTLS_X509_FMT_PEM);
	gnutls_certificate_set_x509_key_file(x509_cred,   ETCPATH "/cert.pem", ETCPATH "/key.pem", GNUTLS_X509_FMT_PEM);

	gnutls_dh_params_init(&dh_params);
	gnutls_dh_params_generate2(dh_params, DH_BITS);

	gnutls_priority_init(&priority_cache, "NORMAL", NULL);

	gnutls_certificate_set_dh_params(x509_cred, dh_params);
}

/*
 * allocates a new TLS session.
 */
gnutls_session_t
irc_tls_session_new(int fd)
{
	gnutls_session_t session;

	gnutls_init(&session, GNUTLS_SERVER);
	gnutls_priority_set(session, priority_cache);
	gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, x509_cred);

	/* request client certificate if any. */
	gnutls_certificate_server_set_request(session, GNUTLS_CERT_REQUEST);
  
	gnutls_session_enable_compatibility_mode(session);
	gnutls_transport_set_ptr(session, (void *) sd);

	return session;
}

/*
 * helper function to handshake or remove the socket from commio.
 */
int
irc_tls_handshake(int fd, gnutls_session_t session)
{
	int ret;

	ret = gnutls_handshake(session);
	if (ret < 0)
	{
		comm_close(fd);
		gnutls_deinit(session);
		return -1;
	}

	return 0;
}

#endif
