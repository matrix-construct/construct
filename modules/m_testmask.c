/*
 *  m_testmask.c: Shows the number of matching local and global clients
 *                for a user@host mask
 *
 *  Copyright (C) 2003 by W. Campbell
 *  Coypright (C) 2004 ircd-ratbox development team
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
 *  $Id: m_testmask.c 3161 2007-01-25 07:23:01Z nenolod $
 *
 */

/* List of ircd includes from ../include/ */
#include "stdinc.h"
#include "client.h"
#include "common.h"     /* FALSE bleah */
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

static int mo_testmask(struct Client *client_p, struct Client *source_p,
			int parc, const char *parv[]);

struct Message testmask_msgtab = {
	"TESTMASK", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_testmask, 2}}
};

mapi_clist_av1 testmask_clist[] = { &testmask_msgtab, NULL };
DECLARE_MODULE_AV1(testmask, NULL, NULL, testmask_clist, NULL, NULL, "$Revision: 3161 $");

static const char *empty_sockhost = "255.255.255.255";
static const char *spoofed_sockhost = "0";

static int
mo_testmask(struct Client *client_p, struct Client *source_p,
                        int parc, const char *parv[])
{
	struct Client *target_p;
	int lcount = 0;
	int gcount = 0;
	char *name, *username, *hostname;
	const char *sockhost;
	char *gecos = NULL;
	rb_dlink_node *ptr;

	name = LOCAL_COPY(parv[1]);
	collapse(name);

	/* username is required */
	if((hostname = strchr(name, '@')) == NULL)
	{
		sendto_one_notice(source_p, ":Invalid parameters");
		return 0;
	}

	*hostname++ = '\0';

	/* nickname is optional */
	if((username = strchr(name, '!')) == NULL)
	{
		username = name;
		name = NULL;
	}
	else
		*username++ = '\0';

	if(EmptyString(username) || EmptyString(hostname))
	{
		sendto_one_notice(source_p, ":Invalid parameters");
		return 0;
	}

	if(parc > 2 && !EmptyString(parv[2]))
	{
		gecos = LOCAL_COPY(parv[2]);
		collapse_esc(gecos);
	}

	RB_DLINK_FOREACH(ptr, global_client_list.head)
	{
		target_p = ptr->data;

		if(!IsPerson(target_p))
			continue;

		if(EmptyString(target_p->sockhost))
			sockhost = empty_sockhost;
		else if(!show_ip(source_p, target_p))
			sockhost = spoofed_sockhost;
		else
			sockhost = target_p->sockhost;

		if(match(username, target_p->username) &&
		   (match(hostname, target_p->host) ||
		    match(hostname, target_p->orighost) ||
		    match(hostname, sockhost) || match_ips(hostname, sockhost)))
		{
			if(name && !match(name, target_p->name))
				continue;

			if(gecos && !match_esc(gecos, target_p->info))
				continue;

			if(MyClient(target_p))
				lcount++;
			else
				gcount++;
		}
	}

	sendto_one(source_p, form_str(RPL_TESTMASKGECOS),
			me.name, source_p->name,
			lcount, gcount, name ? name : "*",
			username, hostname, gecos ? gecos : "*");
	return 0;
}
