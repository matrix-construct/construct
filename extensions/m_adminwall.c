/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_adminwall.c: Sends a message to all admins
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2007 ircd-ratbox development team
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
 *  $Id: m_wallops.c 20702 2005-08-31 20:59:02Z leeh $
 */

#include "stdinc.h"
#include "client.h"
#include "ircd.h"
#include "match.h"
#include "numeric.h"
#include "send.h"
#include "s_user.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_serv.h"

static int mo_adminwall(struct Client *, struct Client *, int, const char **);
static int me_adminwall(struct Client *, struct Client *, int, const char **);

struct Message adminwall_msgtab = {
	"ADMINWALL", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_adminwall, 2}, {mo_adminwall, 2}}
};
                

mapi_clist_av1 adminwall_clist[] = { &adminwall_msgtab, NULL };
DECLARE_MODULE_AV1(adminwall, NULL, NULL, adminwall_clist, NULL, NULL, "$Revision: 20702 $");


/*
 * mo_adminwall (write to *all* admins currently online)
 *      parv[1] = message text
 */

static int
mo_adminwall(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])  
{ 
        if(!IsAdmin(source_p))
        {
                sendto_one(source_p, form_str(ERR_NOPRIVS),
                           me.name, source_p->name, "adminwall");
                return 0;
        }
        sendto_wallops_flags(UMODE_ADMIN, source_p, "ADMINWALL - %s", parv[1]);
        sendto_match_servs(source_p, "*", CAP_ENCAP, NOCAPS, "ENCAP * ADMINWALL :%s", parv[1]);
        return 0;
} 

static int
me_adminwall(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])  
{ 
        sendto_wallops_flags(UMODE_ADMIN, source_p, "ADMINWALL - %s", parv[1]);
        return 0;
}
