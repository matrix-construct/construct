/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_pass.c: Used to send a password for a server or client{} block.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id: m_pass.c 3550 2007-08-09 06:47:26Z nenolod $
 */

#include "stdinc.h"
#include "client.h"		/* client struct */
#include "match.h"
#include "send.h"		/* sendto_one */
#include "numeric.h"		/* ERR_xxx */
#include "ircd.h"		/* me */
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_serv.h"
#include "hash.h"
#include "s_conf.h"

static int mr_pass(struct Client *, struct Client *, int, const char **);

struct Message pass_msgtab = {
	"PASS", 0, 0, 0, MFLG_SLOW | MFLG_UNREG,
	{{mr_pass, 2}, mg_reg, mg_ignore, mg_ignore, mg_ignore, mg_reg}
};

mapi_clist_av1 pass_clist[] = { &pass_msgtab, NULL };
DECLARE_MODULE_AV1(pass, NULL, NULL, pass_clist, NULL, NULL, "$Revision: 3550 $");

/*
 * m_pass() - Added Sat, 4 March 1989
 *
 *
 * mr_pass - PASS message handler
 *      parv[1] = password
 *      parv[2] = "TS" if this server supports TS.
 *      parv[3] = optional TS version field -- needed for TS6
 */
static int
mr_pass(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	char *auth_user, *pass, *buf;
	buf = LOCAL_COPY(parv[1]);
	
	if(client_p->localClient->passwd)
	{
		memset(client_p->localClient->passwd, 0,
			strlen(client_p->localClient->passwd));
		rb_free(client_p->localClient->passwd);
		client_p->localClient->passwd = NULL;
	}

	if (client_p->localClient->auth_user)
	{
		memset(client_p->localClient->auth_user, 0,
		       strlen(client_p->localClient->auth_user));
		rb_free(client_p->localClient->auth_user);
		client_p->localClient->auth_user = NULL;
	}

	if ((pass = strchr(buf, ':')) != NULL)
	{
		*pass++ = '\0'; 
		auth_user = buf; 
	}
	else
	{
		pass = buf;
		auth_user = NULL;
	}
	
	client_p->localClient->passwd = *pass ? rb_strndup(pass, PASSWDLEN) : NULL;
	
	if(auth_user && *auth_user)
		client_p->localClient->auth_user = rb_strndup(auth_user, PASSWDLEN);

	/* These are for servers only */
	if(parc > 2 && client_p->user == NULL)
	{
		/* 
		 * It looks to me as if orabidoo wanted to have more
		 * than one set of option strings possible here...
		 * i.e. ":AABBTS" as long as TS was the last two chars
		 * however, as we are now using CAPAB, I think we can
		 * safely assume if there is a ":TS" then its a TS server
		 * -Dianora
		 */
		if(irccmp(parv[2], "TS") == 0 && client_p->tsinfo == 0)
			client_p->tsinfo = TS_DOESTS;

		if(parc == 5 && atoi(parv[3]) >= 6)
		{
			/* only mark as TS6 if the SID is valid.. */
			if(IsDigit(parv[4][0]) && IsIdChar(parv[4][1]) &&
			   IsIdChar(parv[4][2]) && parv[4][3] == '\0' &&
			   EmptyString(client_p->id))
			{
				client_p->localClient->caps |= CAP_TS6;
				strcpy(client_p->id, parv[4]);
			}
		}
	}

	return 0;
}
