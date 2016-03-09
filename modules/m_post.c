/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_post.c: Exits the user if unregistered, it is a web form.
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
 */

#include "stdinc.h"
#include "client.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_conf.h"

static const char post_desc[] =
	"Ensure Web forms/proxies cannot connect by disconnecting on POST, GET, and PUT";

static void mr_dumb_proxy(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message post_msgtab = {
	"POST", 0, 0, 0, 0,
	{{mr_dumb_proxy, 0}, mg_ignore, mg_ignore, mg_ignore, mg_ignore, mg_ignore}
};
struct Message get_msgtab = {
	"GET", 0, 0, 0, 0,
	{{mr_dumb_proxy, 0}, mg_ignore, mg_ignore, mg_ignore, mg_ignore, mg_ignore}
};
struct Message put_msgtab = {
	"PUT", 0, 0, 0, 0,
	{{mr_dumb_proxy, 0}, mg_ignore, mg_ignore, mg_ignore, mg_ignore, mg_ignore}
};

mapi_clist_av1 post_clist[] = {
	&post_msgtab, &get_msgtab, &put_msgtab, NULL
};

DECLARE_MODULE_AV2(post, NULL, NULL, post_clist, NULL, NULL, NULL, NULL, post_desc);

/*
** mr_dumb_proxy
**      parv[1] = comment
*/
static void
mr_dumb_proxy(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	exit_client(client_p, source_p, source_p, "Client Exit");
}
