/*
 *  ircd-ratbox: A slightly useful ircd.
 *  msg.h: A header for the message handler structure.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
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
 *  $Id: msg.h 6 2005-09-10 01:02:21Z nenolod $
 */

#ifndef INCLUDED_msg_h
#define INCLUDED_msg_h

#include "config.h"

struct Client;

/* MessageHandler */
typedef enum HandlerType
{
	UNREGISTERED_HANDLER,
	CLIENT_HANDLER,
	RCLIENT_HANDLER,
	SERVER_HANDLER,
	ENCAP_HANDLER,
	OPER_HANDLER,
	LAST_HANDLER_TYPE
}
HandlerType;

/* struct Client* client_p   - connection message originated from
 * struct Client* source_p   - source of message, may be different from client_p
 * int            parc   - parameter count
 * char*          parv[] - parameter vector
 */
typedef int (*MessageHandler) (struct Client *, struct Client *, int, const char *[]);

struct MessageEntry
{
	MessageHandler handler;
	int min_para;
};

/* Message table structure */
struct Message
{
	const char *cmd;
	unsigned int count;	/* number of times command used */
	unsigned int rcount;	/* number of times command used by server */
	unsigned long bytes;	/* bytes received for this message */
	unsigned int flags;	/* bit 0 set means that this command is allowed
				 * to be used only on the average of once per 2
				 * seconds -SRB
				 */
	/* handlers:
	 * UNREGISTERED, CLIENT, RCLIENT, SERVER, OPER, LAST
	 */
	struct MessageEntry handlers[LAST_HANDLER_TYPE];
};

#define MFLG_SLOW	0x01	/* executed roughly once per 2s */
#define MFLG_UNREG	0x02	/* available to unregistered clients */

#define MAXPARA    15

/* generic handlers */
extern int m_ignore(struct Client *, struct Client *, int, const char **);
extern int m_not_oper(struct Client *, struct Client *, int, const char **);
extern int m_registered(struct Client *, struct Client *, int, const char **);
extern int m_unregistered(struct Client *, struct Client *, int, const char **);

#define mg_ignore { m_ignore, 0 }
#define mg_not_oper { m_not_oper, 0 }
#define mg_reg { m_registered, 0 }
#define mg_unreg { m_unregistered, 0 }

/*
 * m_functions execute protocol messages on this server:
 * int m_func(struct Client* client_p, struct Client* source_p, int parc, char* parv[]);
 *
 *    client_p    is always NON-NULL, pointing to a *LOCAL* client
 *            structure (with an open socket connected!). This
 *            identifies the physical socket where the message
 *            originated (or which caused the m_function to be
 *            executed--some m_functions may call others...).
 *
 *    source_p    is the source of the message, defined by the
 *            prefix part of the message if present. If not
 *            or prefix not found, then source_p==client_p.
 *
 *            (!IsServer(client_p)) => (client_p == source_p), because
 *            prefixes are taken *only* from servers...
 *
 *            (IsServer(client_p))
 *                    (source_p == client_p) => the message didn't
 *                    have the prefix.
 *
 *                    (source_p != client_p && IsServer(source_p) means
 *                    the prefix specified servername. (?)
 *
 *                    (source_p != client_p && !IsServer(source_p) means
 *                    that message originated from a remote
 *                    user (not local).
 *
 *
 *            combining
 *
 *            (!IsServer(source_p)) means that, source_p can safely
 *            taken as defining the target structure of the
 *            message in this server.
 *
 *    *Always* true (if 'parse' and others are working correct):
 *
 *    1)      source_p->from == client_p  (note: client_p->from == client_p)
 *
 *    2)      MyConnect(source_p) <=> source_p == client_p (e.g. source_p
 *            *cannot* be a local connection, unless it's
 *            actually client_p!). [MyConnect(x) should probably
 *            be defined as (x == x->from) --msa ]
 *
 *    parc    number of variable parameter strings (if zero,
 *            parv is allowed to be NULL)
 *
 *    parv    a NULL terminated list of parameter pointers,
 *
 *                    parv[0], unused for historical reasons (formerly
 *                            sender name)
 *                    parv[1]...parv[parc-1]
 *                            pointers to additional parameters
 *                    parv[parc] == NULL, *always*
 *
 *            note:   it is guaranteed that parv[1]..parv[parc-1] are all
 *                    non-NULL pointers.
 */

#endif /* INCLUDED_msg_h */
