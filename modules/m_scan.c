/*
 *  charybdis: an advanced Internet Relay Chat Daemon(ircd).
 *  m_scan.c: Provides information about various targets on various topics
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
 *
 *  $Id: m_scan.c 1853 2006-08-24 18:30:52Z jilles $
 */

#include "stdinc.h"
#include "class.h"
#include "hook.h"
#include "client.h"
#include "hash.h"
#include "common.h"
#include "hash.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_user.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static int mo_scan(struct Client *, struct Client *, int, const char **);
static int scan_umodes(struct Client *, struct Client *, int, const char **);
/*static int scan_cmodes(struct Client *, struct Client *, int, const char **);*/

struct Message scan_msgtab = {
	"SCAN", 0, 0, 0, MFLG_SLOW,
	{mg_ignore, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_scan, 2}}
};

mapi_clist_av1 scan_clist[] = { &scan_msgtab, NULL };
DECLARE_MODULE_AV1(scan, NULL, NULL, scan_clist, NULL, NULL, "$Revision: 1853 $");

typedef int (*scan_handler)(struct Client *, struct Client *, int, 
	const char **);

struct scan_cmd {
	const char *name;
	int operlevel;
	scan_handler handler;
} scan_cmds[] = {
	{"UMODES", L_OPER, scan_umodes},
	{NULL, 0, NULL}
};

static const char *empty_sockhost = "255.255.255.255";
static const char *spoofed_sockhost = "0";

/*
 * m_scan
 *      parv[1] = options [or target]
 *	parv[2] = [target]
 */
static int
mo_scan(struct Client *client_p, struct Client *source_p, int parc, 
	const char *parv[])
{
	struct scan_cmd *sptr;

	for (sptr = scan_cmds; sptr->name != NULL; sptr++)
	{
		if (!irccmp(sptr->name, parv[1]))
		{
			if (sptr->operlevel == L_ADMIN &&
				!IsOperAdmin(source_p))
				return -1;
			else
				return sptr->handler(client_p, source_p, parc, parv);
		}
	}

	sendto_one_notice(source_p, ":*** %s is not an implemented SCAN target",
			  parv[1]);

	return 0;
}

static int
scan_umodes(struct Client *client_p, struct Client *source_p, int parc,
	const char *parv[])
{
	unsigned int allowed_umodes = 0, disallowed_umodes = 0;
	int what = MODE_ADD;
	int mode;
	int list_users = YES;
	int list_max = 500;
	int list_count = 0, count = 0;
	const char *mask = NULL;
	const char *c;
	struct Client *target_p;
	rb_dlink_list *target_list = &lclient_list;	/* local clients only by default */
	rb_dlink_node *tn;
	int i;
	const char *sockhost;
	char buf[512];

	if (parc < 3)
	{
		if (MyClient(source_p))
			sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
				me.name, source_p->name, "SCAN UMODES");

		return -1;
	}

	if (parv[2][0] != '+' && parv[2][0] != '-')
	{
		sendto_one_notice(source_p, ":SCAN UMODES: umodes parameter must start with '+' or '-'");
		return -1;
	}

	for (c = parv[2]; *c; c++)
	{
		switch(*c)
		{
			case '+':
				what = MODE_ADD;
				break;
			case '-':
				what = MODE_DEL;
				break;
			default:
				if ((mode = user_modes[(unsigned char) *c]) != 0)
				{
					if (what == MODE_ADD)
						allowed_umodes |= mode;
					else
						disallowed_umodes |= mode;
				}
		}
	}

	for (i = 3; i < parc; i++)
	{
		if (!irccmp(parv[i], "no-list"))
			list_users = NO;
		else if (!irccmp(parv[i], "list"))
			list_users = YES;
		else if (!irccmp(parv[i], "global"))
			target_list = &global_client_list;
		else if (i < (parc - 1))
		{
			if (!irccmp(parv[i], "list-max"))
				list_max = atoi(parv[++i]);
			else if (!irccmp(parv[i], "mask"))
				mask = parv[++i];
			else
			{
				sendto_one_notice(source_p, ":SCAN UMODES: invalid parameters");
				return -1;
			}
		}
		else
		{
			sendto_one_notice(source_p, ":SCAN UMODES: invalid parameters");
			return -1;
		}
	}
	if (target_list == &global_client_list && list_users)
	{
		if (IsOperSpy(source_p))
		{
			if (!ConfigFileEntry.operspy_dont_care_user_info)
			{
				rb_strlcpy(buf, "UMODES", sizeof buf);
				for (i = 2; i < parc; i++)
				{
					rb_strlcat(buf, " ", sizeof buf);
					rb_strlcat(buf, parv[i], sizeof buf);
				}
				report_operspy(source_p, "SCAN", buf);
			}
		}
		else
		{
			sendto_one(source_p, form_str(ERR_NOPRIVS),
				   me.name, source_p->name, "oper_spy");
			return -1;
		}
	}

	RB_DLINK_FOREACH(tn, target_list->head)
	{
		unsigned int working_umodes = 0;
		char maskbuf[BUFSIZE];

		target_p = tn->data;

		if (!IsClient(target_p))
			continue;

		if(EmptyString(target_p->sockhost))
			sockhost = empty_sockhost;
		else if(!show_ip(source_p, target_p))
			sockhost = spoofed_sockhost;
		else
			sockhost = target_p->sockhost;

		working_umodes = target_p->umodes;

		/* require that we have the allowed umodes... */
		if ((working_umodes & allowed_umodes) != allowed_umodes)
			continue;

		/* require that we have NONE of the disallowed ones */
		if ((working_umodes & disallowed_umodes) != 0)
			continue;

		if (mask != NULL)
		{
			rb_snprintf(maskbuf, BUFSIZE, "%s!%s@%s",
				target_p->name, target_p->username, target_p->host);

			if (!match(mask, maskbuf))
				continue;
		}

		if (list_users && (!list_max || (list_count < list_max)))
		{
			char modebuf[BUFSIZE];
			char *m = modebuf;

			*m++ = '+';

			for (i = 0; i < 128; i++)
			{
				if (target_p->umodes & user_modes[i])
					*m++ = (char) i;
			}

			*m++ = '\0';

			list_count++;

			sendto_one_numeric(source_p, RPL_SCANUMODES,
						form_str(RPL_SCANUMODES),
						target_p->name, target_p->username,
						target_p->host, sockhost, 
						target_p->servptr->name, modebuf,
						target_p->info);
		}
		count++;
	}

	sendto_one_numeric(source_p, RPL_SCANMATCHED,
			form_str(RPL_SCANMATCHED), count);

	return 0;
}
