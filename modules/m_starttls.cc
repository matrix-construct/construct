/*
 * Copyright (c) 2012 William Pitcock <nenolod@dereferenced.org>.
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

using namespace ircd;

static const char starttls_desc[] = "Provides the tls CAP and STARTTLS command";

static void mr_starttls(struct MsgBuf *, client::client &, client::client &, int, const char **);

struct Message starttls_msgtab = {
	"STARTTLS", 0, 0, 0, 0,
	{{mr_starttls, 0}, mg_ignore, mg_ignore, mg_ignore, mg_ignore, mg_ignore}
};

mapi_clist_av1 starttls_clist[] = { &starttls_msgtab, NULL };

unsigned int CLICAP_TLS = 0;

mapi_cap_list_av2 starttls_cap_list[] = {
	{ MAPI_CAP_CLIENT, "tls", NULL, &CLICAP_TLS },
	{ 0, NULL, NULL, NULL }
};

DECLARE_MODULE_AV2(starttls, NULL, NULL, starttls_clist, NULL, NULL, starttls_cap_list, NULL, starttls_desc);

static void
mr_starttls(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	ssl_ctl_t *ctl;
	rb_fde_t *F[2];

	if (!MyConnect(&client))
		return;

	if (IsSSL(&client))
	{
		sendto_one_numeric(&client, ERR_STARTTLS, form_str(ERR_STARTTLS), "Nested TLS handshake not allowed");
		return;
	}

	if (!ircd_ssl_ok || !get_ssld_count())
	{
		sendto_one_numeric(&client, ERR_STARTTLS, form_str(ERR_STARTTLS), "TLS is not configured");
		return;
	}

	if (rb_socketpair(AF_UNIX, SOCK_STREAM, 0, &F[0], &F[1], "STARTTLS ssld session") == -1)
	{
		ilog_error("error creating SSL/TLS socketpair for ssld slave");
		sendto_one_numeric(&client, ERR_STARTTLS, form_str(ERR_STARTTLS), "Unable to create SSL/TLS socketpair for ssld offload slave");
		return;
	}

	s_assert(client.localClient != NULL);

	/* clear out any remaining plaintext lines */
	rb_linebuf_donebuf(&client.localClient->buf_recvq);

	sendto_one_numeric(&client, RPL_STARTTLS, form_str(RPL_STARTTLS));
	send_queued(&client);

	/* TODO: set localClient->ssl_callback and handle success/failure */

	ctl = start_ssld_accept(client.localClient->F, F[1], connid_get(&client));
	if (ctl != NULL)
	{
		client.localClient->F = F[0];
		client.localClient->ssl_ctl = ctl;
		SetSSL(&client);
	}
}
