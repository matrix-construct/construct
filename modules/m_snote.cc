/*
 *  charybdis: an advanced Internet Relay Chat Daemon(ircd).
 *  m_snote.c: Server notice listener
 *
 *  Copyright (c) 2006 William Pitcock <nenolod -at- nenolod.net>
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1.Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  2.Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  3.The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 *  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

using namespace ircd;

static const char snote_desc[] = "Provides server notices via the SNOTE command";

static void me_snote(struct MsgBuf *, client::client &, client::client &, int, const char **);

struct Message snote_msgtab = {
	"SNOTE", 0, 0, 0, 0,
	{mg_ignore, mg_not_oper, mg_ignore, mg_ignore, {me_snote, 3}, mg_ignore}
};

mapi_clist_av1 snote_clist[] = { &snote_msgtab, NULL };

DECLARE_MODULE_AV2(snote, NULL, NULL, snote_clist, NULL, NULL, NULL, NULL, snote_desc);

/*
 * me_snote
 *      parv[1] = snomask letter
 *	parv[2] = message
 */
static void
me_snote(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc,
	const char *parv[])
{
	/* if there's more than just two params, this is a protocol
	 * violation, but it seems stupid to drop servers over it,
	 * shit happens afterall -nenolod
	 */
	if (parc > 3)
		return;
	if (!is_server(source))
		return;

	sendto_realops_snomask_from(snomask_modes[(unsigned char) *parv[1]],
		L_ALL, &source, "%s", parv[2]);
}
