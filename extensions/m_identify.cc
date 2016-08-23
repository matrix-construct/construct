/*
 * m_identify.c: dalnet-style /identify that sends to nickserv or chanserv
 *
 * Copyright (C) 2006 Jilles Tjoelker
 * Copyright (C) 2006 charybdis development team
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
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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

#define SVS_chanserv_NICK "ChanServ"
#define SVS_nickserv_NICK "NickServ"

using namespace ircd;

static const char identify_desc[] = "Adds the IDENTIFY alias that forwards to NickServ or ChanServ";

static void m_identify(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[]);

struct Message identify_msgtab = {
	"IDENTIFY", 0, 0, 0, 0,
	{mg_unreg, {m_identify, 0}, mg_ignore, mg_ignore, mg_ignore, {m_identify, 0}}
};

mapi_clist_av1 identify_clist[] = {
	&identify_msgtab,
	NULL
};

DECLARE_MODULE_AV2(identify, NULL, NULL, identify_clist, NULL, NULL, NULL, NULL, identify_desc);

static void
m_identify(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	const char *nick;
	client::client *target_p;

	if (parc < 2 || EmptyString(parv[1]))
	{
		sendto_one(&source, form_str(ERR_NOTEXTTOSEND), me.name, source.name);
		return;
	}

	nick = parv[1][0] == '#' ? SVS_chanserv_NICK : SVS_nickserv_NICK;
	if ((target_p = client::find_named_person(nick)) && IsService(target_p))
	{
		sendto_one(target_p, ":%s PRIVMSG %s :IDENTIFY %s", get_id(&source, target_p), get_id(target_p, target_p), reconstruct_parv(parc - 1, &parv[1]));
	}
	else
	{
		sendto_one_numeric(&source, ERR_SERVICESDOWN, form_str(ERR_SERVICESDOWN), nick);
	}
}
