/*
 * m_certfp.c: propagates client certificate fingerprint information
 *
 * Copyright (C) 2010 Jilles Tjoelker
 * Copyright (C) 2010 charybdis development team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
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

static const char certfp_desc[] =
	"Provides the CERTFP facility used by servers to set certificate fingerprints";

static void me_certfp(struct MsgBuf *, client::client &, client::client &, int, const char **);

struct Message certfp_msgtab = {
	"CERTFP", 0, 0, 0, 0,
	{mg_unreg, mg_ignore, mg_ignore, mg_ignore, {me_certfp, 2}, mg_ignore}
};

mapi_clist_av1 certfp_clist[] = { &certfp_msgtab, NULL };

DECLARE_MODULE_AV2(certfp, NULL, NULL, certfp_clist, NULL, NULL, NULL, NULL, certfp_desc);

/*
** me_certfp
**      parv[1] = certfp string
*/
static void
me_certfp(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	if (!is_person(source))
		return;

	rb_free(source.certfp);
	source.certfp = NULL;
	if (!EmptyString(parv[1]))
		source.certfp = rb_strdup(parv[1]);
}
