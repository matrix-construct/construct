/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_user.c: Sends username information.
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
 *  $Id: m_user.c 3416 2007-04-15 20:18:54Z jilles $
 */

#include "stdinc.h"
#include "client.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "s_user.h"
#include "send.h"
#include "s_conf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "blacklist.h"

static int mr_user(struct Client *, struct Client *, int, const char **);

struct Message user_msgtab = {
	"USER", 0, 0, 0, MFLG_SLOW,
	{{mr_user, 5}, mg_reg, mg_ignore, mg_ignore, mg_ignore, mg_reg}
};

mapi_clist_av1 user_clist[] = { &user_msgtab, NULL };
DECLARE_MODULE_AV1(user, NULL, NULL, user_clist, NULL, NULL, "$Revision: 3416 $");

static int do_local_user(struct Client *client_p, struct Client *source_p,
			 const char *username, const char *realname);

/* mr_user()
 *      parv[1] = username (login name, account)
 *      parv[2] = client host name (ignored)
 *      parv[3] = server host name (ignored)
 *      parv[4] = users gecos
 */
static int
mr_user(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static char buf[BUFSIZE];
	char *p;

	if (strlen(client_p->id) == 3)
	{
		exit_client(client_p, client_p, client_p, "Mixing client and server protocol");
		return 0;
	}

	if((p = strchr(parv[1], '@')))
		*p = '\0';

	rb_snprintf(buf, sizeof(buf), "%s %s", parv[2], parv[3]);
	rb_free(source_p->localClient->fullcaps);
	source_p->localClient->fullcaps = rb_strdup(buf);

	do_local_user(client_p, source_p, parv[1], parv[4]);
	return 0;
}

static int
do_local_user(struct Client *client_p, struct Client *source_p,
	      const char *username, const char *realname)
{
	struct User *user;

	s_assert(NULL != source_p);
	s_assert(source_p->username != username);

	user = make_user(source_p);

	if (!(source_p->flags & FLAGS_SENTUSER))
	{
		lookup_blacklists(source_p);
		source_p->flags |= FLAGS_SENTUSER;
	}

	rb_strlcpy(source_p->info, realname, sizeof(source_p->info));

	if(!IsGotId(source_p))
	{
		/* This is in this location for a reason..If there is no identd
		 * and ping cookies are enabled..we need to have a copy of this
		 */
		rb_strlcpy(source_p->username, username, sizeof(source_p->username));
	}

	if(source_p->name[0])
	{
		/* NICK already received, now I have USER... */
		return register_local_user(client_p, source_p, username);
	}

	return 0;
}
