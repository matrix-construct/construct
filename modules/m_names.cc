/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_names.c: Shows the users who are online.
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

using namespace ircd;

static const char names_desc[] = "Provides the NAMES command to view users on a channel";

static void m_names(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message names_msgtab = {
	"NAMES", 0, 0, 0, 0,
	{mg_unreg, {m_names, 0}, mg_ignore, mg_ignore, mg_ignore, {m_names, 0}}
};

mapi_clist_av1 names_clist[] = { &names_msgtab, NULL };

DECLARE_MODULE_AV2(names, NULL, NULL, names_clist, NULL, NULL, NULL, NULL, names_desc);

static void names_global(struct Client *source_p);

/************************************************************************
 * m_names() - Added by Jto 27 Apr 1989
 ************************************************************************/

/*
 * m_names
 *      parv[1] = channel
 */
static void
m_names(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0;
	chan::chan *chptr = NULL;
	char *s;

	if(parc > 1 && !EmptyString(parv[1]))
	{
		char *p = LOCAL_COPY(parv[1]);
		if((s = strchr(p, ',')))
			*s = '\0';

		if(!chan::check_channel_name(p))
		{
			sendto_one_numeric(source_p, ERR_BADCHANNAME,
					   form_str(ERR_BADCHANNAME),
					   (unsigned char *) p);
			return;
		}

		if((chptr = find_channel(p)) != NULL)
			channel_member_names(chptr, source_p, 1);
		else
			sendto_one(source_p, form_str(RPL_ENDOFNAMES),
				   me.name, source_p->name, p);
	}
	else
	{
		if(!IsOper(source_p))
		{
			if((last_used + ConfigFileEntry.pace_wait) > rb_current_time())
			{
				sendto_one(source_p, form_str(RPL_LOAD2HI),
					   me.name, source_p->name, "NAMES");
				sendto_one(source_p, form_str(RPL_ENDOFNAMES),
					   me.name, source_p->name, "*");
				return;
			}
			else
				last_used = rb_current_time();
		}

		names_global(source_p);
		sendto_one(source_p, form_str(RPL_ENDOFNAMES),
			   me.name, source_p->name, "*");
	}
}

/*
 * names_global
 *
 * inputs       - pointer to client struct requesting names
 * output       - none
 * side effects - lists all non public non secret channels
 */
static void
names_global(struct Client *source_p)
{
	int mlen;
	int tlen;
	int cur_len;
	bool dont_show = false;
	rb_dlink_node *lp, *ptr;
	struct Client *target_p;
	chan::chan *chptr = NULL;
	chan::membership *msptr;
	char buf[BUFSIZE];
	char *t;

	/* first do all visible channels */
	RB_DLINK_FOREACH(ptr, chan::global_channel_list.head)
	{
		chptr = (chan::chan *)ptr->data;
		channel_member_names(chptr, source_p, 0);
	}
	cur_len = mlen = sprintf(buf, form_str(RPL_NAMREPLY),
				    me.name, source_p->name, "*", "*");
	t = buf + mlen;

	/* Second, do all clients in one big sweep */
	RB_DLINK_FOREACH(ptr, global_client_list.head)
	{
		target_p = (Client *)ptr->data;
		dont_show = false;

		if(!IsPerson(target_p) || IsInvisible(target_p))
			continue;

		/* we want to show -i clients that are either:
		 *   a) not on any channels
		 *   b) only on +p channels
		 *
		 * both were missed out above.  if the target is on a
		 * common channel with source, its already been shown.
		 */
		RB_DLINK_FOREACH(lp, target_p->user->channel.head)
		{
			msptr = (chan::membership *)lp->data;
			chptr = msptr->chan;

			if(is_public(chptr) || is_member(chptr, source_p) || is_secret(chptr))
			{
				dont_show = true;
				break;
			}
		}

		if(dont_show)
			continue;

		if((cur_len + NICKLEN + 2) > (BUFSIZE - 3))
		{
			sendto_one(source_p, "%s", buf);
			cur_len = mlen;
			t = buf + mlen;
		}

		tlen = sprintf(t, "%s ", target_p->name);
		cur_len += tlen;
		t += tlen;
	}

	if(cur_len > mlen)
		sendto_one(source_p, "%s", buf);
}
