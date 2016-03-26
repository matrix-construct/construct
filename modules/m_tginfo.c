/*
 * m_tginfo.c: propagates target-change status information
 *
 * Copyright (C) 2012 Keith Buck
 * Copyright (C) 2012 charybdis development team
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

#include "stdinc.h"
#include "client.h"
#include "match.h"
#include "hash.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "msg.h"
#include "modules.h"
#include "s_newconf.h" /* add_tgchange */

static const char tginfo_desc[] = "Processes target change notifications from other servers";

static void me_tginfo(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message tginfo_msgtab = {
	"TGINFO", 0, 0, 0, 0,
	{mg_unreg, mg_ignore, mg_ignore, mg_ignore, {me_tginfo, 2}, mg_ignore}
};

mapi_clist_av1 tginfo_clist[] = { &tginfo_msgtab, NULL };

DECLARE_MODULE_AV2(tginfo, NULL, NULL, tginfo_clist, NULL, NULL, NULL, NULL, tginfo_desc);

/*
** me_tginfo
**      parv[1] = 0, reserved for future use (number of remaining targets)
*/
static void
me_tginfo(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if (!IsPerson(source_p))
		return;

	int remaining = atoi(parv[1]);
	if (remaining != 0)
		return; /* not implemented */

	if (!EmptyString(source_p->sockhost) && strcmp(source_p->sockhost, "0"))
	{
		/* We can't really add the tgchange if we don't have their IP... */
		add_tgchange(source_p->sockhost);
	}

	if (!IsTGExcessive(source_p))
	{
		SetTGExcessive(source_p);
		sendto_realops_snomask_from(SNO_BOTS, L_ALL, source_p->servptr,
			"Excessive target change from %s (%s@%s)",
			source_p->name, source_p->username, source_p->orighost);
	}
}
