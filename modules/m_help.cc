/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_help.c: Provides help information to a user/operator.
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

#include <ircd/stdinc.h>
#include <ircd/client.h>
#include <ircd/ircd.h>
#include <ircd/msg.h>
#include <ircd/numeric.h>
#include <ircd/send.h>
#include <ircd/s_conf.h>
#include <ircd/logger.h>
#include <ircd/parse.h>
#include <ircd/modules.h>
#include <ircd/hash.h>
#include <ircd/cache.h>

using namespace ircd;

static const char help_desc[] =
	"Provides the help facility for commands, modes, and server concepts";

static void m_help(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void mo_help(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void mo_uhelp(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void dohelp(struct Client *, int, const char *);

struct Message help_msgtab = {
	"HELP", 0, 0, 0, 0,
	{mg_unreg, {m_help, 0}, mg_ignore, mg_ignore, mg_ignore, {mo_help, 0}}
};
struct Message uhelp_msgtab = {
	"UHELP", 0, 0, 0, 0,
	{mg_unreg, {m_help, 0}, mg_ignore, mg_ignore, mg_ignore, {mo_uhelp, 0}}
};

mapi_clist_av1 help_clist[] = { &help_msgtab, &uhelp_msgtab, NULL };

DECLARE_MODULE_AV2(help, NULL, NULL, help_clist, NULL, NULL, NULL, NULL, help_desc);

/*
 * m_help - HELP message handler
 */
static void
m_help(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	using namespace ircd::cache::help;

	dohelp(source_p, USER, parc > 1 ? parv[1] : NULL);
}

/*
 * mo_help - HELP message handler
 */
static void
mo_help(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	using namespace ircd::cache::help;

	dohelp(source_p, OPER, parc > 1 ? parv[1] : NULL);
}

/*
 * mo_uhelp - HELP message handler
 * This is used so that opers can view the user help file without deopering
 */
static void
mo_uhelp(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	using namespace ircd::cache::help;

	dohelp(source_p, USER, parc > 1 ? parv[1] : NULL);
}

static void
dohelp(Client *const source_p,
       int type,
       const char *topic)
try
{
	namespace help = cache::help;

	static const char *const ntopic("index");
	if (EmptyString(topic))
		topic = ntopic;

	std::shared_ptr<cache::file> file
	{
		type & help::OPER? help::oper.at(topic):
		type & help::USER? help::user.at(topic):
		                   nullptr
	};

	if (!file || !(flags(*file) & type))
		throw std::out_of_range(ntopic);

	auto it(begin(contents(*file)));
	if (it != end(contents(*file)))
		sendto_one(source_p, form_str(RPL_HELPSTART),
		           me.name,
		           source_p->name,
		           topic,
		           it->c_str());

	for (++it; it != end(contents(*file)); ++it)
		sendto_one(source_p, form_str(RPL_HELPTXT),
		           me.name,
		           source_p->name,
		           topic,
		           it->c_str());

	sendto_one(source_p, form_str(RPL_ENDOFHELP),
	           me.name,
	           source_p->name,
	           topic);
}
catch(const std::out_of_range &e)
{
	sendto_one(source_p, form_str(ERR_HELPNOTFOUND),
	           me.name,
	           source_p->name,
	           topic?: e.what());
}
