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
 *
 * $Id: m_identify.c 2729 2006-11-09 23:52:06Z jilles $
 */

#include "stdinc.h"
#include "client.h"
#include "common.h"
#include "ircd.h"
#include "match.h"
#include "numeric.h"
#include "s_conf.h"
#include "logger.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

#define SVS_chanserv_NICK "ChanServ"
#define SVS_nickserv_NICK "NickServ"

char *reconstruct_parv(int parc, const char *parv[]);

static int m_identify(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

struct Message identify_msgtab = {
	"IDENTIFY", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_identify, 0}, mg_ignore, mg_ignore, mg_ignore, {m_identify, 0}}
};

mapi_clist_av1 identify_clist[] = {
	&identify_msgtab,
	NULL
};

DECLARE_MODULE_AV1(identify, NULL, NULL, identify_clist, NULL, NULL, "$Revision: 2729 $");

char *reconstruct_parv(int parc, const char *parv[])
{
	static char tmpbuf[BUFSIZE]; int i;

	rb_strlcpy(tmpbuf, parv[0], BUFSIZE);
	for (i = 1; i < parc; i++)
	{
		rb_strlcat(tmpbuf, " ", BUFSIZE);
		rb_strlcat(tmpbuf, parv[i], BUFSIZE);
	}
	return tmpbuf;
}

static int m_identify(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	const char *nick;
	struct Client *target_p;

	if (parc < 2 || EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NOTEXTTOSEND), me.name, source_p->name);
		return 0;
	}

	nick = parv[1][0] == '#' ? SVS_chanserv_NICK : SVS_nickserv_NICK;
	if ((target_p = find_named_person(nick)) && IsService(target_p))
	{
		sendto_one(target_p, ":%s PRIVMSG %s :IDENTIFY %s", get_id(source_p, target_p), get_id(target_p, target_p), reconstruct_parv(parc - 1, &parv[1]));
	}
	else
	{
		sendto_one_numeric(source_p, ERR_SERVICESDOWN, form_str(ERR_SERVICESDOWN), nick);
	}
	return 0;
}
