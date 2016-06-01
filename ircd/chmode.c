/*
 *  charybdis: A slightly useful ircd.
 *  chmode.c: channel mode management
 *
 * Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 * Copyright (C) 1996-2002 Hybrid Development Team
 * Copyright (C) 2002-2005 ircd-ratbox development team
 * Copyright (C) 2005-2006 charybdis development team
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
#include "channel.h"
#include "client.h"
#include "hash.h"
#include "hook.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"		/* captab */
#include "s_user.h"
#include "send.h"
#include "whowas.h"
#include "s_conf.h"		/* ConfigFileEntry, ConfigChannel */
#include "s_newconf.h"
#include "logger.h"
#include "chmode.h"
#include "s_assert.h"
#include "parse.h"

/* bitmasks for error returns, so we send once per call */
#define SM_ERR_NOTS             0x00000001	/* No TS on channel */
#define SM_ERR_NOOPS            0x00000002	/* No chan ops */
#define SM_ERR_UNKNOWN          0x00000004
#define SM_ERR_RPL_C            0x00000008
#define SM_ERR_RPL_B            0x00000010
#define SM_ERR_RPL_E            0x00000020
#define SM_ERR_NOTONCHANNEL     0x00000040	/* Not on channel */
#define SM_ERR_RPL_I            0x00000100
#define SM_ERR_RPL_D            0x00000200
#define SM_ERR_NOPRIVS          0x00000400
#define SM_ERR_RPL_Q            0x00000800
#define SM_ERR_RPL_F            0x00001000
#define SM_ERR_MLOCK            0x00002000

#define MAXMODES_SIMPLE 46 /* a-zA-Z except bqeIov */

static struct ChModeChange mode_changes[BUFSIZE];
static int mode_count;
static int mode_limit;
static int mode_limit_simple;
static int mask_pos;
static int removed_mask_pos;

char cflagsbuf[256];
char cflagsmyinfo[256];

int chmode_flags[256];

extern int h_get_channel_access;

/* OPTIMIZE ME! -- dwr */
void
construct_cflags_strings(void)
{
	int i;
        char *ptr = cflagsbuf;
	char *ptr2 = cflagsmyinfo;

        *ptr = '\0';
	*ptr2 = '\0';

	for(i = 0; i < 256; i++)
	{
		if( !(chmode_table[i].set_func == chm_ban) &&
			!(chmode_table[i].set_func == chm_forward) &&
			!(chmode_table[i].set_func == chm_throttle) &&
                        !(chmode_table[i].set_func == chm_key) &&
                        !(chmode_table[i].set_func == chm_limit) &&
                        !(chmode_table[i].set_func == chm_op) &&
                        !(chmode_table[i].set_func == chm_voice))
		{
			chmode_flags[i] = chmode_table[i].mode_type;
		}
		else
		{
			chmode_flags[i] = 0;
		}

		switch (chmode_flags[i])
		{
		    case MODE_FREETARGET:
		    case MODE_DISFORWARD:
			if(ConfigChannel.use_forward)
				*ptr++ = (char) i;
			break;
		    default:
			if(chmode_flags[i] != 0)
			{
			    *ptr++ = (char) i;
			}
		}

		/* Should we leave orphaned check here? -- dwr */
		if(!(chmode_table[i].set_func == chm_nosuch) && !(chmode_table[i].set_func == chm_orphaned))
		{
		    *ptr2++ = (char) i;
		}
	}

        *ptr++ = '\0';
	*ptr2++ = '\0';
}

/*
 * find_umode_slot
 *
 * inputs       - NONE
 * outputs      - an available cflag bitmask or
 *                0 if no cflags are available
 * side effects - NONE
 */
static unsigned int
find_cflag_slot(void)
{
	unsigned int all_cflags = 0, my_cflag = 0, i;

	for (i = 0; i < 256; i++)
		all_cflags |= chmode_flags[i];

	for (my_cflag = 1; my_cflag && (all_cflags & my_cflag);
		my_cflag <<= 1);

	return my_cflag;
}

unsigned int
cflag_add(char c_, ChannelModeFunc function)
{
	int c = (unsigned char)c_;

	if (chmode_table[c].set_func != chm_nosuch &&
			chmode_table[c].set_func != chm_orphaned)
		return 0;

	if (chmode_table[c].set_func == chm_nosuch)
		chmode_table[c].mode_type = find_cflag_slot();
	if (chmode_table[c].mode_type == 0)
		return 0;
	chmode_table[c].set_func = function;
	construct_cflags_strings();
	return chmode_table[c].mode_type;
}

void
cflag_orphan(char c_)
{
	int c = (unsigned char)c_;

	s_assert(chmode_flags[c] != 0);
	chmode_table[c].set_func = chm_orphaned;
	construct_cflags_strings();
}

int
get_channel_access(struct Client *source_p, struct Channel *chptr, struct membership *msptr, int dir, const char *modestr)
{
	hook_data_channel_approval moduledata;

	if(!MyClient(source_p))
		return CHFL_CHANOP;

	moduledata.client = source_p;
	moduledata.chptr = chptr;
	moduledata.msptr = msptr;
	moduledata.target = NULL;
	moduledata.approved = (msptr != NULL && is_chanop(msptr)) ? CHFL_CHANOP : CHFL_PEON;
	moduledata.dir = dir;
	moduledata.modestr = modestr;

	call_hook(h_get_channel_access, &moduledata);

	return moduledata.approved;
}

/* allow_mode_change()
 *
 * Checks if mlock and chanops permit a mode change.
 *
 * inputs	- client, channel, access level, errors pointer, mode char
 * outputs	- false on failure, true on success
 * side effects - error message sent on failure
 */
static bool
allow_mode_change(struct Client *source_p, struct Channel *chptr, int alevel,
		int *errors, char c)
{
	/* If this mode char is locked, don't allow local users to change it. */
	if (MyClient(source_p) && chptr->mode_lock && strchr(chptr->mode_lock, c))
	{
		if (!(*errors & SM_ERR_MLOCK))
			sendto_one_numeric(source_p,
					ERR_MLOCKRESTRICTED,
					form_str(ERR_MLOCKRESTRICTED),
					chptr->chname,
					c,
					chptr->mode_lock);
		*errors |= SM_ERR_MLOCK;
		return false;
	}
	if(alevel < CHFL_CHANOP)
	{
		if(!(*errors & SM_ERR_NOOPS))
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
				   me.name, source_p->name, chptr->chname);
		*errors |= SM_ERR_NOOPS;
		return false;
	}
	return true;
}

/* add_id()
 *
 * inputs	- client, channel, id to add, type, forward
 * outputs	- false on failure, true on success
 * side effects - given id is added to the appropriate list
 */
bool
add_id(struct Client *source_p, struct Channel *chptr, const char *banid, const char *forward,
       rb_dlink_list * list, long mode_type)
{
	struct Ban *actualBan;
	static char who[USERHOST_REPLYLEN];
	char *realban = LOCAL_COPY(banid);
	rb_dlink_node *ptr;

	/* dont let local clients overflow the banlist, or set redundant
	 * bans
	 */
	if(MyClient(source_p))
	{
		if((rb_dlink_list_length(&chptr->banlist) + rb_dlink_list_length(&chptr->exceptlist) + rb_dlink_list_length(&chptr->invexlist) + rb_dlink_list_length(&chptr->quietlist)) >= (unsigned long)(chptr->mode.mode & MODE_EXLIMIT ? ConfigChannel.max_bans_large : ConfigChannel.max_bans))
		{
			sendto_one(source_p, form_str(ERR_BANLISTFULL),
				   me.name, source_p->name, chptr->chname, realban);
			return false;
		}

		RB_DLINK_FOREACH(ptr, list->head)
		{
			actualBan = ptr->data;
			if(mask_match(actualBan->banstr, realban))
				return false;
		}
	}
	/* dont let remotes set duplicates */
	else
	{
		RB_DLINK_FOREACH(ptr, list->head)
		{
			actualBan = ptr->data;
			if(!irccmp(actualBan->banstr, realban))
				return false;
		}
	}


	if(IsPerson(source_p))
		sprintf(who, "%s!%s@%s", source_p->name, source_p->username, source_p->host);
	else
		rb_strlcpy(who, source_p->name, sizeof(who));

	actualBan = allocate_ban(realban, who, forward);
	actualBan->when = rb_current_time();

	rb_dlinkAdd(actualBan, &actualBan->node, list);

	/* invalidate the can_send() cache */
	if(mode_type == CHFL_BAN || mode_type == CHFL_QUIET || mode_type == CHFL_EXCEPTION)
		chptr->bants = rb_current_time();

	return true;
}

/* del_id()
 *
 * inputs	- channel, id to remove, type
 * outputs	- pointer to ban that was removed, if any
 * side effects - given id is removed from the appropriate list and returned
 */
struct Ban *
del_id(struct Channel *chptr, const char *banid, rb_dlink_list * list, long mode_type)
{
	rb_dlink_node *ptr;
	struct Ban *banptr;

	if(EmptyString(banid))
		return NULL;

	RB_DLINK_FOREACH(ptr, list->head)
	{
		banptr = ptr->data;

		if(irccmp(banid, banptr->banstr) == 0)
		{
			rb_dlinkDelete(&banptr->node, list);

			/* invalidate the can_send() cache */
			if(mode_type == CHFL_BAN || mode_type == CHFL_QUIET || mode_type == CHFL_EXCEPTION)
				chptr->bants = rb_current_time();

			return banptr;
		}
	}

	return NULL;
}

/* check_string()
 *
 * input	- string to check
 * output	- pointer to 'fixed' string, or "*" if empty
 * side effects - any white space found becomes \0
 */
static char *
check_string(char *s)
{
	char *str = s;
	static char splat[] = "*";
	if(!(s && *s))
		return splat;

	for(; *s; ++s)
	{
		if(IsSpace(*s))
		{
			*s = '\0';
			break;
		}
	}
	return str;
}

/* pretty_mask()
 *
 * inputs	- mask to pretty
 * outputs	- better version of the mask
 * side effects - mask is chopped to limits, and transformed:
 *                x!y@z => x!y@z
 *                y@z   => *!y@z
 *                x!y   => x!y@*
 *                x     => x!*@*
 *                z.d   => *!*@z.d
 */
static char *
pretty_mask(const char *idmask)
{
	static char mask_buf[BUFSIZE];
	int old_mask_pos;
	const char *nick, *user, *host, *forward = NULL;
	char *t, *at, *ex;
	int nl, ul, hl, fl;
	char *mask;
	size_t masklen;

	mask = LOCAL_COPY(idmask);
	mask = check_string(mask);
	collapse(mask);
	masklen = strlen(mask);

	nick = user = host = "*";
	nl = ul = hl = 1;
	fl = 0;

	if((size_t) BUFSIZE - mask_pos < masklen + 5)
		return NULL;

	old_mask_pos = mask_pos;

	if (*mask == '$')
	{
		memcpy(mask_buf + mask_pos, mask, masklen + 1);
		mask_pos += masklen + 1;
		t = mask_buf + old_mask_pos + 1;
		if (*t == '!')
			*t = '~';
		if (*t == '~')
			t++;
		*t = irctolower(*t);
		return mask_buf + old_mask_pos;
	}

	at = ex = NULL;
	if((t = memchr(mask, '@', masklen)) != NULL)
	{
		at = t;
		t++;
		if(*t != '\0')
			host = t, hl = strlen(t);

		if((t = memchr(mask, '!', at - mask)) != NULL)
		{
			ex = t;
			t++;
			if(at != t)
				user = t, ul = at - t;
			if(ex != mask)
				nick = mask, nl = ex - mask;
		}
		else
		{
			if(at != mask)
				user = mask, ul = at - mask;
		}

		if((t = memchr(host, '!', hl)) != NULL ||
				(t = memchr(host, '$', hl)) != NULL)
		{
			t++;
			if (host + hl != t)
				forward = t, fl = host + hl - t;
			hl = t - 1 - host;
		}
	}
	else if((t = memchr(mask, '!', masklen)) != NULL)
	{
		ex = t;
		t++;
		if(ex != mask)
			nick = mask, nl = ex - mask;
		if(*t != '\0')
			user = t, ul = strlen(t);
	}
	else if(memchr(mask, '.', masklen) != NULL ||
			memchr(mask, ':', masklen) != NULL)
	{
		host = mask, hl = masklen;
	}
	else
	{
		if(masklen > 0)
			nick = mask, nl = masklen;
	}

	/* truncate values to max lengths */
	if(nl > NICKLEN - 1)
		nl = NICKLEN - 1;
	if(ul > USERLEN)
		ul = USERLEN;
	if(hl > HOSTLEN)
		hl = HOSTLEN;
	if(fl > CHANNELLEN)
		fl = CHANNELLEN;

	memcpy(mask_buf + mask_pos, nick, nl), mask_pos += nl;
	mask_buf[mask_pos++] = '!';
	memcpy(mask_buf + mask_pos, user, ul), mask_pos += ul;
	mask_buf[mask_pos++] = '@';
	memcpy(mask_buf + mask_pos, host, hl), mask_pos += hl;
	if (forward) {
		mask_buf[mask_pos++] = '$';
		memcpy(mask_buf + mask_pos, forward, fl), mask_pos += fl;
	}
	mask_buf[mask_pos++] = '\0';

	return mask_buf + old_mask_pos;
}

/* check_forward()
 *
 * input	- client, channel to set mode on, target channel name
 * output	- true if forwarding should be allowed
 * side effects - numeric sent if not allowed
 */
static bool
check_forward(struct Client *source_p, struct Channel *chptr,
		const char *forward)
{
	struct Channel *targptr = NULL;
	struct membership *msptr;

	if(!check_channel_name(forward) ||
			(MyClient(source_p) && (strlen(forward) > LOC_CHANNELLEN || hash_find_resv(forward))))
	{
		sendto_one_numeric(source_p, ERR_BADCHANNAME, form_str(ERR_BADCHANNAME), forward);
		return false;
	}
	/* don't forward to inconsistent target -- jilles */
	if(chptr->chname[0] == '#' && forward[0] == '&')
	{
		sendto_one_numeric(source_p, ERR_BADCHANNAME,
				   form_str(ERR_BADCHANNAME), forward);
		return false;
	}
	if(MyClient(source_p) && (targptr = find_channel(forward)) == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), forward);
		return false;
	}
	if(MyClient(source_p) && !(targptr->mode.mode & MODE_FREETARGET))
	{
		if((msptr = find_channel_membership(targptr, source_p)) == NULL ||
			get_channel_access(source_p, targptr, msptr, MODE_QUERY, NULL) < CHFL_CHANOP)
		{
			sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
				   me.name, source_p->name, targptr->chname);
			return false;
		}
	}
	return true;
}

/* fix_key()
 *
 * input	- key to fix
 * output	- the same key, fixed
 * side effects - anything below ascii 13 is discarded, ':' discarded,
 *                high ascii is dropped to lower half of ascii table
 */
static char *
fix_key(char *arg)
{
	unsigned char *s, *t, c;

	for(s = t = (unsigned char *) arg; (c = *s); s++)
	{
		c &= 0x7f;
		if(c != ':' && c != ',' && c > ' ')
			*t++ = c;
	}

	*t = '\0';
	return arg;
}

/* fix_key_remote()
 *
 * input	- key to fix
 * ouput	- the same key, fixed
 * side effects - high ascii dropped to lower half of table,
 *                CR/LF/':' are dropped
 */
static char *
fix_key_remote(char *arg)
{
	unsigned char *s, *t, c;

	for(s = t = (unsigned char *) arg; (c = *s); s++)
	{
		c &= 0x7f;
		if((c != 0x0a) && (c != ':') && (c != ',') && (c != 0x0d) && (c != ' '))
			*t++ = c;
	}

	*t = '\0';
	return arg;
}

/* chm_*()
 *
 * The handlers for each specific mode.
 */
void
chm_nosuch(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type)
{
	if(*errors & SM_ERR_UNKNOWN)
		return;
	*errors |= SM_ERR_UNKNOWN;
	sendto_one(source_p, form_str(ERR_UNKNOWNMODE), me.name, source_p->name, c);
}

void
chm_simple(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type)
{
	if(!allow_mode_change(source_p, chptr, alevel, errors, c))
		return;

	if(MyClient(source_p) && (++mode_limit_simple > MAXMODES_SIMPLE))
		return;

	/* setting + */
	if((dir == MODE_ADD) && !(chptr->mode.mode & mode_type))
	{
		/* if +f is disabled, ignore an attempt to set +QF locally */
		if(!ConfigChannel.use_forward && MyClient(source_p) &&
				(c == 'Q' || c == 'F'))
			return;

		chptr->mode.mode |= mode_type;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count++].arg = NULL;
	}
	else if((dir == MODE_DEL) && (chptr->mode.mode & mode_type))
	{
		chptr->mode.mode &= ~mode_type;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = NULL;
	}
}

void
chm_orphaned(struct Client *source_p, struct Channel *chptr,
	   int alevel, int parc, int *parn,
	   const char **parv, int *errors, int dir, char c, long mode_type)
{
	if(MyClient(source_p))
		return;

	if((dir == MODE_ADD) && !(chptr->mode.mode & mode_type))
	{
		chptr->mode.mode |= mode_type;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count++].arg = NULL;
	}
	else if((dir == MODE_DEL) && (chptr->mode.mode & mode_type))
	{
		chptr->mode.mode &= ~mode_type;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = NULL;
	}
}

void
chm_hidden(struct Client *source_p, struct Channel *chptr,
	  int alevel, int parc, int *parn,
	  const char **parv, int *errors, int dir, char c, long mode_type)
{
	if(!IsOper(source_p) && !IsServer(source_p))
	{
		if(!(*errors & SM_ERR_NOPRIVS))
			sendto_one_numeric(source_p, ERR_NOPRIVILEGES, form_str(ERR_NOPRIVILEGES));
		*errors |= SM_ERR_NOPRIVS;
		return;
	}
	if(MyClient(source_p) && !IsOperAdmin(source_p))
	{
		if(!(*errors & SM_ERR_NOPRIVS))
			sendto_one(source_p, form_str(ERR_NOPRIVS), me.name,
					source_p->name, "admin");
		*errors |= SM_ERR_NOPRIVS;
		return;
	}

	if(MyClient(source_p) && (++mode_limit_simple > MAXMODES_SIMPLE))
		return;

	/* setting + */
	if((dir == MODE_ADD) && !(chptr->mode.mode & mode_type))
	{
		chptr->mode.mode |= mode_type;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count].mems = ONLY_OPERS;
		mode_changes[mode_count++].arg = NULL;
	}
	else if((dir == MODE_DEL) && (chptr->mode.mode & mode_type))
	{
		chptr->mode.mode &= ~mode_type;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].mems = ONLY_OPERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = NULL;
	}
}

void
chm_staff(struct Client *source_p, struct Channel *chptr,
	  int alevel, int parc, int *parn,
	  const char **parv, int *errors, int dir, char c, long mode_type)
{
	if(!IsOper(source_p) && !IsServer(source_p))
	{
		if(!(*errors & SM_ERR_NOPRIVS))
			sendto_one_numeric(source_p, ERR_NOPRIVILEGES, form_str(ERR_NOPRIVILEGES));
		*errors |= SM_ERR_NOPRIVS;
		return;
	}
	if(MyClient(source_p) && !IsOperResv(source_p))
	{
		if(!(*errors & SM_ERR_NOPRIVS))
			sendto_one(source_p, form_str(ERR_NOPRIVS), me.name,
					source_p->name, "resv");
		*errors |= SM_ERR_NOPRIVS;
		return;
	}

	if(!allow_mode_change(source_p, chptr, CHFL_CHANOP, errors, c))
		return;

	if(MyClient(source_p) && (++mode_limit_simple > MAXMODES_SIMPLE))
		return;

	/* setting + */
	if((dir == MODE_ADD) && !(chptr->mode.mode & mode_type))
	{
		chptr->mode.mode |= mode_type;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count++].arg = NULL;
	}
	else if((dir == MODE_DEL) && (chptr->mode.mode & mode_type))
	{
		chptr->mode.mode &= ~mode_type;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = NULL;
	}
}

void
chm_ban(struct Client *source_p, struct Channel *chptr,
	int alevel, int parc, int *parn,
	const char **parv, int *errors, int dir, char c, long mode_type)
{
	const char *mask, *raw_mask;
	char *forward;
	rb_dlink_list *list;
	rb_dlink_node *ptr;
	struct Ban *banptr;
	int errorval;
	const char *rpl_list_p;
	const char *rpl_endlist_p;
	int mems;

	switch (mode_type)
	{
	case CHFL_BAN:
		list = &chptr->banlist;
		errorval = SM_ERR_RPL_B;
		rpl_list_p = form_str(RPL_BANLIST);
		rpl_endlist_p = form_str(RPL_ENDOFBANLIST);
		mems = ALL_MEMBERS;
		break;

	case CHFL_EXCEPTION:
		/* if +e is disabled, allow all but +e locally */
		if(!ConfigChannel.use_except && MyClient(source_p) &&
		   ((dir == MODE_ADD) && (parc > *parn)))
			return;

		list = &chptr->exceptlist;
		errorval = SM_ERR_RPL_E;
		rpl_list_p = form_str(RPL_EXCEPTLIST);
		rpl_endlist_p = form_str(RPL_ENDOFEXCEPTLIST);

		if(ConfigChannel.use_except || (dir == MODE_DEL))
			mems = ONLY_CHANOPS;
		else
			mems = ONLY_SERVERS;
		break;

	case CHFL_INVEX:
		/* if +I is disabled, allow all but +I locally */
		if(!ConfigChannel.use_invex && MyClient(source_p) &&
		   (dir == MODE_ADD) && (parc > *parn))
			return;

		list = &chptr->invexlist;
		errorval = SM_ERR_RPL_I;
		rpl_list_p = form_str(RPL_INVITELIST);
		rpl_endlist_p = form_str(RPL_ENDOFINVITELIST);

		if(ConfigChannel.use_invex || (dir == MODE_DEL))
			mems = ONLY_CHANOPS;
		else
			mems = ONLY_SERVERS;
		break;

	case CHFL_QUIET:
		list = &chptr->quietlist;
		errorval = SM_ERR_RPL_Q;
		rpl_list_p = form_str(RPL_QUIETLIST);
		rpl_endlist_p = form_str(RPL_ENDOFQUIETLIST);
		mems = ALL_MEMBERS;
		break;

	default:
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "chm_ban() called with unknown type!");
		return;
	}

	if(dir == 0 || parc <= *parn)
	{
		if((*errors & errorval) != 0)
			return;
		*errors |= errorval;

		/* non-ops cant see +eI lists.. */
		/* note that this is still permitted if +e/+I are mlocked. */
		if(alevel < CHFL_CHANOP && mode_type != CHFL_BAN &&
				mode_type != CHFL_QUIET)
		{
			if(!(*errors & SM_ERR_NOOPS))
				sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
					   me.name, source_p->name, chptr->chname);
			*errors |= SM_ERR_NOOPS;
			return;
		}

		RB_DLINK_FOREACH(ptr, list->head)
		{
			char buf[BANLEN];
			banptr = ptr->data;
			if(banptr->forward)
				snprintf(buf, sizeof(buf), "%s$%s", banptr->banstr, banptr->forward);
			else
				rb_strlcpy(buf, banptr->banstr, sizeof(buf));

			sendto_one(source_p, rpl_list_p,
				   me.name, source_p->name, chptr->chname,
				   buf, banptr->who, banptr->when);
		}
		sendto_one(source_p, rpl_endlist_p, me.name, source_p->name, chptr->chname);
		return;
	}

	if(!allow_mode_change(source_p, chptr, alevel, errors, c))
		return;


	if(MyClient(source_p) && (++mode_limit > MAXMODEPARAMS))
		return;

	raw_mask = parv[(*parn)];
	(*parn)++;

	/* empty ban, or starts with ':' which messes up s2s, ignore it */
	if(EmptyString(raw_mask) || *raw_mask == ':')
		return;

	if(!MyClient(source_p))
	{
		if(strchr(raw_mask, ' '))
			return;

		mask = raw_mask;
	}
	else
		mask = pretty_mask(raw_mask);

	/* we'd have problems parsing this, hyb6 does it too
	 * also make sure it will always fit on a line with channel
	 * name etc.
	 */
	if(strlen(mask) > MIN(BANLEN, MODEBUFLEN - 5))
	{
		sendto_one_numeric(source_p, ERR_INVALIDBAN,
				form_str(ERR_INVALIDBAN),
				chptr->chname, c, raw_mask);
		return;
	}

	/* Look for a $ after the first character.
	 * As the first character, it marks an extban; afterwards
	 * it delimits a forward channel.
	 */
	if ((forward = strchr(mask+1, '$')) != NULL)
	{
		*forward++ = '\0';
		if (*forward == '\0')
			forward = NULL;
	}

	/* if we're adding a NEW id */
	if(dir == MODE_ADD)
	{
		if (*mask == '$' && MyClient(source_p))
		{
			if (!valid_extban(mask, source_p, chptr, mode_type))
			{
				sendto_one_numeric(source_p, ERR_INVALIDBAN,
						form_str(ERR_INVALIDBAN),
						chptr->chname, c, raw_mask);
				return;
			}
		}

		/* For compatibility, only check the forward channel from
		 * local clients. Accept any forward channel from servers.
		 */
		if(forward != NULL && MyClient(source_p))
		{
			/* For simplicity and future flexibility, do not
			 * allow '$' in forwarding targets.
			 */
			if(!ConfigChannel.use_forward ||
					strchr(forward, '$') != NULL)
			{
				sendto_one_numeric(source_p, ERR_INVALIDBAN,
						form_str(ERR_INVALIDBAN),
						chptr->chname, c, raw_mask);
				return;
			}
			/* check_forward() sends its own error message */
			if(!check_forward(source_p, chptr, forward))
				return;
			/* Forwards only make sense for bans. */
			if(mode_type != CHFL_BAN)
			{
				sendto_one_numeric(source_p, ERR_INVALIDBAN,
						form_str(ERR_INVALIDBAN),
						chptr->chname, c, raw_mask);
				return;
			}
		}

		/* dont allow local clients to overflow the banlist, dont
		 * let remote servers set duplicate bans
		 */
		if(!add_id(source_p, chptr, mask, forward, list, mode_type))
			return;

		if(forward)
			forward[-1]= '$';

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].mems = mems;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = mask;
	}
	else if(dir == MODE_DEL)
	{
		struct Ban *removed;
		static char buf[BANLEN * MAXMODEPARAMS];
		int old_removed_mask_pos = removed_mask_pos;
		if((removed = del_id(chptr, mask, list, mode_type)) == NULL)
		{
			/* mask isn't a valid ban, check raw_mask */
			if((removed = del_id(chptr, raw_mask, list, mode_type)) != NULL)
				mask = raw_mask;
		}

		if(removed && removed->forward)
			removed_mask_pos += snprintf(buf + old_removed_mask_pos, sizeof(buf), "%s$%s", removed->banstr, removed->forward) + 1;
		else
			removed_mask_pos += rb_strlcpy(buf + old_removed_mask_pos, mask, sizeof(buf)) + 1;
		if(removed)
		{
			free_ban(removed);
			removed = NULL;
		}

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].mems = mems;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = buf + old_removed_mask_pos;
	}
}

void
chm_op(struct Client *source_p, struct Channel *chptr,
       int alevel, int parc, int *parn,
       const char **parv, int *errors, int dir, char c, long mode_type)
{
	struct membership *mstptr;
	const char *opnick;
	struct Client *targ_p;

	if(!allow_mode_change(source_p, chptr, alevel, errors, c))
		return;

	if((dir == MODE_QUERY) || (parc <= *parn))
		return;

	opnick = parv[(*parn)];
	(*parn)++;

	/* empty nick */
	if(EmptyString(opnick))
	{
		sendto_one_numeric(source_p, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), "*");
		return;
	}

	if((targ_p = find_chasing(source_p, opnick, NULL)) == NULL)
	{
		return;
	}

	mstptr = find_channel_membership(chptr, targ_p);

	if(mstptr == NULL)
	{
		if(!(*errors & SM_ERR_NOTONCHANNEL) && MyClient(source_p))
			sendto_one_numeric(source_p, ERR_USERNOTINCHANNEL,
					   form_str(ERR_USERNOTINCHANNEL), opnick, chptr->chname);
		*errors |= SM_ERR_NOTONCHANNEL;
		return;
	}

	if(MyClient(source_p) && (++mode_limit > MAXMODEPARAMS))
		return;

	if(dir == MODE_ADD)
	{
		if(targ_p == source_p && mstptr->flags & CHFL_CHANOP)
			return;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = targ_p->id;
		mode_changes[mode_count++].arg = targ_p->name;

		mstptr->flags |= CHFL_CHANOP;
	}
	else
	{
		if(MyClient(source_p) && IsService(targ_p))
		{
			sendto_one(source_p, form_str(ERR_ISCHANSERVICE),
				   me.name, source_p->name, targ_p->name, chptr->chname);
			return;
		}

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = targ_p->id;
		mode_changes[mode_count++].arg = targ_p->name;

		mstptr->flags &= ~CHFL_CHANOP;
	}
}

void
chm_voice(struct Client *source_p, struct Channel *chptr,
	  int alevel, int parc, int *parn,
	  const char **parv, int *errors, int dir, char c, long mode_type)
{
	struct membership *mstptr;
	const char *opnick;
	struct Client *targ_p;

	if(!allow_mode_change(source_p, chptr, alevel, errors, c))
		return;

	if((dir == MODE_QUERY) || parc <= *parn)
		return;

	opnick = parv[(*parn)];
	(*parn)++;

	/* empty nick */
	if(EmptyString(opnick))
	{
		sendto_one_numeric(source_p, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), "*");
		return;
	}

	if((targ_p = find_chasing(source_p, opnick, NULL)) == NULL)
	{
		return;
	}

	mstptr = find_channel_membership(chptr, targ_p);

	if(mstptr == NULL)
	{
		if(!(*errors & SM_ERR_NOTONCHANNEL) && MyClient(source_p))
			sendto_one_numeric(source_p, ERR_USERNOTINCHANNEL,
					   form_str(ERR_USERNOTINCHANNEL), opnick, chptr->chname);
		*errors |= SM_ERR_NOTONCHANNEL;
		return;
	}

	if(MyClient(source_p) && (++mode_limit > MAXMODEPARAMS))
		return;

	if(dir == MODE_ADD)
	{
		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = targ_p->id;
		mode_changes[mode_count++].arg = targ_p->name;

		mstptr->flags |= CHFL_VOICE;
	}
	else
	{
		mode_changes[mode_count].letter = 'v';
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = targ_p->id;
		mode_changes[mode_count++].arg = targ_p->name;

		mstptr->flags &= ~CHFL_VOICE;
	}
}

void
chm_limit(struct Client *source_p, struct Channel *chptr,
	  int alevel, int parc, int *parn,
	  const char **parv, int *errors, int dir, char c, long mode_type)
{
	const char *lstr;
	static char limitstr[30];
	int limit;

	if(!allow_mode_change(source_p, chptr, alevel, errors, c))
		return;

	if(dir == MODE_QUERY)
		return;

	if(MyClient(source_p) && (++mode_limit_simple > MAXMODES_SIMPLE))
		return;

	if((dir == MODE_ADD) && parc > *parn)
	{
		lstr = parv[(*parn)];
		(*parn)++;

		if(EmptyString(lstr) || (limit = atoi(lstr)) <= 0)
			return;

		sprintf(limitstr, "%d", limit);

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = limitstr;

		chptr->mode.limit = limit;
	}
	else if(dir == MODE_DEL)
	{
		if(!chptr->mode.limit)
			return;

		chptr->mode.limit = 0;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = NULL;
	}
}

void
chm_throttle(struct Client *source_p, struct Channel *chptr,
	     int alevel, int parc, int *parn,
	     const char **parv, int *errors, int dir, char c, long mode_type)
{
	int joins = 0, timeslice = 0;

	if(!allow_mode_change(source_p, chptr, alevel, errors, c))
		return;

	if(dir == MODE_QUERY)
		return;

	if(MyClient(source_p) && (++mode_limit_simple > MAXMODES_SIMPLE))
		return;

	if((dir == MODE_ADD) && parc > *parn)
	{
		if (sscanf(parv[(*parn)], "%d:%d", &joins, &timeslice) < 2)
			return;

		if(joins <= 0 || timeslice <= 0)
			return;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = parv[(*parn)];

		(*parn)++;

		chptr->mode.join_num = joins;
		chptr->mode.join_time = timeslice;
	}
	else if(dir == MODE_DEL)
	{
		if(!chptr->mode.join_num)
			return;

		chptr->mode.join_num = 0;
		chptr->mode.join_time = 0;
		chptr->join_count = 0;
		chptr->join_delta = 0;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = NULL;
	}
}

void
chm_forward(struct Client *source_p, struct Channel *chptr,
	int alevel, int parc, int *parn,
	const char **parv, int *errors, int dir, char c, long mode_type)
{
	const char *forward;

	/* if +f is disabled, ignore local attempts to set it */
	if(!ConfigChannel.use_forward && MyClient(source_p) &&
	   (dir == MODE_ADD) && (parc > *parn))
		return;

	if(dir == MODE_QUERY || (dir == MODE_ADD && parc <= *parn))
	{
		if (!(*errors & SM_ERR_RPL_F))
		{
			if (*chptr->mode.forward == '\0')
				sendto_one_notice(source_p, ":%s has no forward channel", chptr->chname);
			else
				sendto_one_notice(source_p, ":%s forward channel is %s", chptr->chname, chptr->mode.forward);
			*errors |= SM_ERR_RPL_F;
		}
		return;
	}

#ifndef FORWARD_OPERONLY
	if(!allow_mode_change(source_p, chptr, alevel, errors, c))
		return;
#else
	if(!IsOper(source_p) && !IsServer(source_p))
	{
		if(!(*errors & SM_ERR_NOPRIVS))
			sendto_one_numeric(source_p, ERR_NOPRIVILEGES, form_str(ERR_NOPRIVILEGES));
		*errors |= SM_ERR_NOPRIVS;
		return;
	}
#endif

	if(MyClient(source_p) && (++mode_limit_simple > MAXMODES_SIMPLE))
		return;

	if(dir == MODE_ADD && parc > *parn)
	{
		forward = parv[(*parn)];
		(*parn)++;

		if(EmptyString(forward))
			return;

		if(!check_forward(source_p, chptr, forward))
			return;

		rb_strlcpy(chptr->mode.forward, forward, sizeof(chptr->mode.forward));

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].mems =
			ConfigChannel.use_forward ? ALL_MEMBERS : ONLY_SERVERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = forward;
	}
	else if(dir == MODE_DEL)
	{
		if(!(*chptr->mode.forward))
			return;

		*chptr->mode.forward = '\0';

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = NULL;
	}
}

void
chm_key(struct Client *source_p, struct Channel *chptr,
	int alevel, int parc, int *parn,
	const char **parv, int *errors, int dir, char c, long mode_type)
{
	char *key;

	if(!allow_mode_change(source_p, chptr, alevel, errors, c))
		return;

	if(dir == MODE_QUERY)
		return;

	if(MyClient(source_p) && (++mode_limit_simple > MAXMODES_SIMPLE))
		return;

	if((dir == MODE_ADD) && parc > *parn)
	{
		key = LOCAL_COPY(parv[(*parn)]);
		(*parn)++;

		if(MyClient(source_p))
			fix_key(key);
		else
			fix_key_remote(key);

		if(EmptyString(key))
			return;

		s_assert(key[0] != ' ');
		rb_strlcpy(chptr->mode.key, key, sizeof(chptr->mode.key));

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_ADD;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = chptr->mode.key;
	}
	else if(dir == MODE_DEL)
	{
		static char splat[] = "*";
		int i;

		if(parc > *parn)
			(*parn)++;

		if(!(*chptr->mode.key))
			return;

		/* hack time.  when we get a +k-k mode, the +k arg is
		 * chptr->mode.key, which the -k sets to \0, so hunt for a
		 * +k when we get a -k, and set the arg to splat. --anfl
		 */
		for(i = 0; i < mode_count; i++)
		{
			if(mode_changes[i].letter == 'k' && mode_changes[i].dir == MODE_ADD)
				mode_changes[i].arg = splat;
		}

		*chptr->mode.key = 0;

		mode_changes[mode_count].letter = c;
		mode_changes[mode_count].dir = MODE_DEL;
		mode_changes[mode_count].mems = ALL_MEMBERS;
		mode_changes[mode_count].id = NULL;
		mode_changes[mode_count++].arg = "*";
	}
}

/* *INDENT-OFF* */
struct ChannelMode chmode_table[256] =
{
  {chm_nosuch,  0 },			/* 0x00 */
  {chm_nosuch,  0 },			/* 0x01 */
  {chm_nosuch,  0 },			/* 0x02 */
  {chm_nosuch,  0 },			/* 0x03 */
  {chm_nosuch,  0 },			/* 0x04 */
  {chm_nosuch,  0 },			/* 0x05 */
  {chm_nosuch,  0 },			/* 0x06 */
  {chm_nosuch,  0 },			/* 0x07 */
  {chm_nosuch,  0 },			/* 0x08 */
  {chm_nosuch,  0 },			/* 0x09 */
  {chm_nosuch,  0 },			/* 0x0a */
  {chm_nosuch,  0 },			/* 0x0b */
  {chm_nosuch,  0 },			/* 0x0c */
  {chm_nosuch,  0 },			/* 0x0d */
  {chm_nosuch,  0 },			/* 0x0e */
  {chm_nosuch,  0 },			/* 0x0f */
  {chm_nosuch,  0 },			/* 0x10 */
  {chm_nosuch,  0 },			/* 0x11 */
  {chm_nosuch,  0 },			/* 0x12 */
  {chm_nosuch,  0 },			/* 0x13 */
  {chm_nosuch,  0 },			/* 0x14 */
  {chm_nosuch,  0 },			/* 0x15 */
  {chm_nosuch,  0 },			/* 0x16 */
  {chm_nosuch,  0 },			/* 0x17 */
  {chm_nosuch,  0 },			/* 0x18 */
  {chm_nosuch,  0 },			/* 0x19 */
  {chm_nosuch,  0 },			/* 0x1a */
  {chm_nosuch,  0 },			/* 0x1b */
  {chm_nosuch,  0 },			/* 0x1c */
  {chm_nosuch,  0 },			/* 0x1d */
  {chm_nosuch,  0 },			/* 0x1e */
  {chm_nosuch,  0 },			/* 0x1f */
  {chm_nosuch,  0 },			/* 0x20 */
  {chm_nosuch,  0 },			/* 0x21 */
  {chm_nosuch,  0 },			/* 0x22 */
  {chm_nosuch,  0 },			/* 0x23 */
  {chm_nosuch,  0 },			/* 0x24 */
  {chm_nosuch,  0 },			/* 0x25 */
  {chm_nosuch,  0 },			/* 0x26 */
  {chm_nosuch,  0 },			/* 0x27 */
  {chm_nosuch,  0 },			/* 0x28 */
  {chm_nosuch,  0 },			/* 0x29 */
  {chm_nosuch,  0 },			/* 0x2a */
  {chm_nosuch,  0 },			/* 0x2b */
  {chm_nosuch,  0 },			/* 0x2c */
  {chm_nosuch,  0 },			/* 0x2d */
  {chm_nosuch,  0 },			/* 0x2e */
  {chm_nosuch,  0 },			/* 0x2f */
  {chm_nosuch,  0 },			/* 0x30 */
  {chm_nosuch,  0 },			/* 0x31 */
  {chm_nosuch,  0 },			/* 0x32 */
  {chm_nosuch,  0 },			/* 0x33 */
  {chm_nosuch,  0 },			/* 0x34 */
  {chm_nosuch,  0 },			/* 0x35 */
  {chm_nosuch,  0 },			/* 0x36 */
  {chm_nosuch,  0 },			/* 0x37 */
  {chm_nosuch,  0 },			/* 0x38 */
  {chm_nosuch,  0 },			/* 0x39 */
  {chm_nosuch,  0 },			/* 0x3a */
  {chm_nosuch,  0 },			/* 0x3b */
  {chm_nosuch,  0 },			/* 0x3c */
  {chm_nosuch,  0 },			/* 0x3d */
  {chm_nosuch,  0 },			/* 0x3e */
  {chm_nosuch,  0 },			/* 0x3f */

  {chm_nosuch,	0 },			/* @ */
  {chm_nosuch,	0 },			/* A */
  {chm_nosuch,	0 },			/* B */
  {chm_nosuch,  0 },			/* C */
  {chm_nosuch,	0 },			/* D */
  {chm_nosuch,	0 },			/* E */
  {chm_simple,	MODE_FREETARGET },	/* F */
  {chm_nosuch,	0 },			/* G */
  {chm_nosuch,	0 },			/* H */
  {chm_ban,	CHFL_INVEX },           /* I */
  {chm_nosuch,	0 },			/* J */
  {chm_nosuch,	0 },			/* K */
  {chm_staff,	MODE_EXLIMIT },		/* L */
  {chm_nosuch,	0 },			/* M */
  {chm_nosuch,	0 },			/* N */
  {chm_nosuch,	0 },			/* O */
  {chm_staff,	MODE_PERMANENT },	/* P */
  {chm_simple,	MODE_DISFORWARD },	/* Q */
  {chm_nosuch,	0 },			/* R */
  {chm_nosuch,	0 },			/* S */
  {chm_nosuch,	0 },			/* T */
  {chm_nosuch,	0 },			/* U */
  {chm_nosuch,	0 },			/* V */
  {chm_nosuch,	0 },			/* W */
  {chm_nosuch,	0 },			/* X */
  {chm_nosuch,	0 },			/* Y */
  {chm_nosuch,	0 },			/* Z */
  {chm_nosuch,	0 },
  {chm_nosuch,	0 },
  {chm_nosuch,	0 },
  {chm_nosuch,	0 },
  {chm_nosuch,	0 },
  {chm_nosuch,	0 },
  {chm_nosuch,	0 },			/* a */
  {chm_ban,	CHFL_BAN },		/* b */
  {chm_nosuch,	0 },			/* c */
  {chm_nosuch,	0 },			/* d */
  {chm_ban,	CHFL_EXCEPTION },	/* e */
  {chm_forward,	0 },			/* f */
  {chm_simple,	MODE_FREEINVITE },	/* g */
  {chm_nosuch,	0 },			/* h */
  {chm_simple,	MODE_INVITEONLY },	/* i */
  {chm_throttle, 0 },			/* j */
  {chm_key,	0 },			/* k */
  {chm_limit,	0 },			/* l */
  {chm_simple,	MODE_MODERATED },	/* m */
  {chm_simple,	MODE_NOPRIVMSGS },	/* n */
  {chm_op,	0 },			/* o */
  {chm_simple,	MODE_PRIVATE },		/* p */
  {chm_ban,	CHFL_QUIET },		/* q */
  {chm_simple,  MODE_REGONLY },		/* r */
  {chm_simple,	MODE_SECRET },		/* s */
  {chm_simple,	MODE_TOPICLIMIT },	/* t */
  {chm_nosuch,	0 },			/* u */
  {chm_voice,	0 },			/* v */
  {chm_nosuch,	0 },			/* w */
  {chm_nosuch,	0 },			/* x */
  {chm_nosuch,	0 },			/* y */
  {chm_simple,	MODE_OPMODERATE },	/* z */

  {chm_nosuch,  0 },			/* 0x7b */
  {chm_nosuch,  0 },			/* 0x7c */
  {chm_nosuch,  0 },			/* 0x7d */
  {chm_nosuch,  0 },			/* 0x7e */
  {chm_nosuch,  0 },			/* 0x7f */

  {chm_nosuch,  0 },			/* 0x80 */
  {chm_nosuch,  0 },			/* 0x81 */
  {chm_nosuch,  0 },			/* 0x82 */
  {chm_nosuch,  0 },			/* 0x83 */
  {chm_nosuch,  0 },			/* 0x84 */
  {chm_nosuch,  0 },			/* 0x85 */
  {chm_nosuch,  0 },			/* 0x86 */
  {chm_nosuch,  0 },			/* 0x87 */
  {chm_nosuch,  0 },			/* 0x88 */
  {chm_nosuch,  0 },			/* 0x89 */
  {chm_nosuch,  0 },			/* 0x8a */
  {chm_nosuch,  0 },			/* 0x8b */
  {chm_nosuch,  0 },			/* 0x8c */
  {chm_nosuch,  0 },			/* 0x8d */
  {chm_nosuch,  0 },			/* 0x8e */
  {chm_nosuch,  0 },			/* 0x8f */

  {chm_nosuch,  0 },			/* 0x90 */
  {chm_nosuch,  0 },			/* 0x91 */
  {chm_nosuch,  0 },			/* 0x92 */
  {chm_nosuch,  0 },			/* 0x93 */
  {chm_nosuch,  0 },			/* 0x94 */
  {chm_nosuch,  0 },			/* 0x95 */
  {chm_nosuch,  0 },			/* 0x96 */
  {chm_nosuch,  0 },			/* 0x97 */
  {chm_nosuch,  0 },			/* 0x98 */
  {chm_nosuch,  0 },			/* 0x99 */
  {chm_nosuch,  0 },			/* 0x9a */
  {chm_nosuch,  0 },			/* 0x9b */
  {chm_nosuch,  0 },			/* 0x9c */
  {chm_nosuch,  0 },			/* 0x9d */
  {chm_nosuch,  0 },			/* 0x9e */
  {chm_nosuch,  0 },			/* 0x9f */

  {chm_nosuch,  0 },			/* 0xa0 */
  {chm_nosuch,  0 },			/* 0xa1 */
  {chm_nosuch,  0 },			/* 0xa2 */
  {chm_nosuch,  0 },			/* 0xa3 */
  {chm_nosuch,  0 },			/* 0xa4 */
  {chm_nosuch,  0 },			/* 0xa5 */
  {chm_nosuch,  0 },			/* 0xa6 */
  {chm_nosuch,  0 },			/* 0xa7 */
  {chm_nosuch,  0 },			/* 0xa8 */
  {chm_nosuch,  0 },			/* 0xa9 */
  {chm_nosuch,  0 },			/* 0xaa */
  {chm_nosuch,  0 },			/* 0xab */
  {chm_nosuch,  0 },			/* 0xac */
  {chm_nosuch,  0 },			/* 0xad */
  {chm_nosuch,  0 },			/* 0xae */
  {chm_nosuch,  0 },			/* 0xaf */

  {chm_nosuch,  0 },			/* 0xb0 */
  {chm_nosuch,  0 },			/* 0xb1 */
  {chm_nosuch,  0 },			/* 0xb2 */
  {chm_nosuch,  0 },			/* 0xb3 */
  {chm_nosuch,  0 },			/* 0xb4 */
  {chm_nosuch,  0 },			/* 0xb5 */
  {chm_nosuch,  0 },			/* 0xb6 */
  {chm_nosuch,  0 },			/* 0xb7 */
  {chm_nosuch,  0 },			/* 0xb8 */
  {chm_nosuch,  0 },			/* 0xb9 */
  {chm_nosuch,  0 },			/* 0xba */
  {chm_nosuch,  0 },			/* 0xbb */
  {chm_nosuch,  0 },			/* 0xbc */
  {chm_nosuch,  0 },			/* 0xbd */
  {chm_nosuch,  0 },			/* 0xbe */
  {chm_nosuch,  0 },			/* 0xbf */

  {chm_nosuch,  0 },			/* 0xc0 */
  {chm_nosuch,  0 },			/* 0xc1 */
  {chm_nosuch,  0 },			/* 0xc2 */
  {chm_nosuch,  0 },			/* 0xc3 */
  {chm_nosuch,  0 },			/* 0xc4 */
  {chm_nosuch,  0 },			/* 0xc5 */
  {chm_nosuch,  0 },			/* 0xc6 */
  {chm_nosuch,  0 },			/* 0xc7 */
  {chm_nosuch,  0 },			/* 0xc8 */
  {chm_nosuch,  0 },			/* 0xc9 */
  {chm_nosuch,  0 },			/* 0xca */
  {chm_nosuch,  0 },			/* 0xcb */
  {chm_nosuch,  0 },			/* 0xcc */
  {chm_nosuch,  0 },			/* 0xcd */
  {chm_nosuch,  0 },			/* 0xce */
  {chm_nosuch,  0 },			/* 0xcf */

  {chm_nosuch,  0 },			/* 0xd0 */
  {chm_nosuch,  0 },			/* 0xd1 */
  {chm_nosuch,  0 },			/* 0xd2 */
  {chm_nosuch,  0 },			/* 0xd3 */
  {chm_nosuch,  0 },			/* 0xd4 */
  {chm_nosuch,  0 },			/* 0xd5 */
  {chm_nosuch,  0 },			/* 0xd6 */
  {chm_nosuch,  0 },			/* 0xd7 */
  {chm_nosuch,  0 },			/* 0xd8 */
  {chm_nosuch,  0 },			/* 0xd9 */
  {chm_nosuch,  0 },			/* 0xda */
  {chm_nosuch,  0 },			/* 0xdb */
  {chm_nosuch,  0 },			/* 0xdc */
  {chm_nosuch,  0 },			/* 0xdd */
  {chm_nosuch,  0 },			/* 0xde */
  {chm_nosuch,  0 },			/* 0xdf */

  {chm_nosuch,  0 },			/* 0xe0 */
  {chm_nosuch,  0 },			/* 0xe1 */
  {chm_nosuch,  0 },			/* 0xe2 */
  {chm_nosuch,  0 },			/* 0xe3 */
  {chm_nosuch,  0 },			/* 0xe4 */
  {chm_nosuch,  0 },			/* 0xe5 */
  {chm_nosuch,  0 },			/* 0xe6 */
  {chm_nosuch,  0 },			/* 0xe7 */
  {chm_nosuch,  0 },			/* 0xe8 */
  {chm_nosuch,  0 },			/* 0xe9 */
  {chm_nosuch,  0 },			/* 0xea */
  {chm_nosuch,  0 },			/* 0xeb */
  {chm_nosuch,  0 },			/* 0xec */
  {chm_nosuch,  0 },			/* 0xed */
  {chm_nosuch,  0 },			/* 0xee */
  {chm_nosuch,  0 },			/* 0xef */

  {chm_nosuch,  0 },			/* 0xf0 */
  {chm_nosuch,  0 },			/* 0xf1 */
  {chm_nosuch,  0 },			/* 0xf2 */
  {chm_nosuch,  0 },			/* 0xf3 */
  {chm_nosuch,  0 },			/* 0xf4 */
  {chm_nosuch,  0 },			/* 0xf5 */
  {chm_nosuch,  0 },			/* 0xf6 */
  {chm_nosuch,  0 },			/* 0xf7 */
  {chm_nosuch,  0 },			/* 0xf8 */
  {chm_nosuch,  0 },			/* 0xf9 */
  {chm_nosuch,  0 },			/* 0xfa */
  {chm_nosuch,  0 },			/* 0xfb */
  {chm_nosuch,  0 },			/* 0xfc */
  {chm_nosuch,  0 },			/* 0xfd */
  {chm_nosuch,  0 },			/* 0xfe */
  {chm_nosuch,  0 },			/* 0xff */
};

/* *INDENT-ON* */

/* set_channel_mode()
 *
 * inputs	- client, source, channel, membership pointer, params
 * output	-
 * side effects - channel modes/memberships are changed, MODE is issued
 *
 * Extensively modified to be hotpluggable, 03/09/06 -- nenolod
 */
void
set_channel_mode(struct Client *client_p, struct Client *source_p,
		 struct Channel *chptr, struct membership *msptr, int parc, const char *parv[])
{
	static char modebuf[BUFSIZE];
	static char parabuf[BUFSIZE];
	char *mbuf;
	char *pbuf;
	int cur_len, mlen, paralen, paracount, arglen, len;
	int i, j, flags;
	int dir = MODE_QUERY;
	int parn = 1;
	int errors = 0;
	int alevel;
	const char *ml = parv[0];
	char c;
	struct Client *fakesource_p;
	int reauthorized = 0;	/* if we change from MODE_QUERY to MODE_ADD/MODE_DEL, then reauth once, ugly but it works */
	int flags_list[3] = { ALL_MEMBERS, ONLY_CHANOPS, ONLY_OPERS };

	mask_pos = 0;
	removed_mask_pos = 0;
	mode_count = 0;
	mode_limit = 0;
	mode_limit_simple = 0;

	/* Hide connecting server on netburst -- jilles */
	if (ConfigServerHide.flatten_links && IsServer(source_p) && !has_id(source_p) && !HasSentEob(source_p))
		fakesource_p = &me;
	else
		fakesource_p = source_p;

	alevel = get_channel_access(source_p, chptr, msptr, dir, reconstruct_parv(parc, parv));

	for(; (c = *ml) != 0; ml++)
	{
		switch (c)
		{
		case '+':
			dir = MODE_ADD;
			if (!reauthorized)
			{
				alevel = get_channel_access(source_p, chptr, msptr, dir, reconstruct_parv(parc, parv));
				reauthorized = 1;
			}
			break;
		case '-':
			dir = MODE_DEL;
			if (!reauthorized)
			{
				alevel = get_channel_access(source_p, chptr, msptr, dir, reconstruct_parv(parc, parv));
				reauthorized = 1;
			}
			break;
		case '=':
			dir = MODE_QUERY;
			break;
		default:
			chmode_table[(unsigned char) c].set_func(fakesource_p, chptr, alevel,
				       parc, &parn, parv,
				       &errors, dir, c,
				       chmode_table[(unsigned char) c].mode_type);
			break;
		}
	}

	/* bail out if we have nothing to do... */
	if(!mode_count)
		return;

	if(IsServer(source_p))
		mlen = sprintf(modebuf, ":%s MODE %s ", fakesource_p->name, chptr->chname);
	else
		mlen = sprintf(modebuf, ":%s!%s@%s MODE %s ",
				  source_p->name, source_p->username,
				  source_p->host, chptr->chname);

	for(j = 0; j < 3; j++)
	{
		flags = flags_list[j];
		cur_len = mlen;
		mbuf = modebuf + mlen;
		pbuf = parabuf;
		parabuf[0] = '\0';
		paracount = paralen = 0;
		dir = MODE_QUERY;

		for(i = 0; i < mode_count; i++)
		{
			if(mode_changes[i].letter == 0 || mode_changes[i].mems != flags)
				continue;

			if(mode_changes[i].arg != NULL)
			{
				arglen = strlen(mode_changes[i].arg);

				if(arglen > MODEBUFLEN - 5)
					continue;
			}
			else
				arglen = 0;

			/* if we're creeping over MAXMODEPARAMSSERV, or over
			 * bufsize (4 == +/-,modechar,two spaces) send now.
			 */
			if(mode_changes[i].arg != NULL &&
			   ((paracount == MAXMODEPARAMSSERV) ||
			    ((cur_len + paralen + arglen + 4) > (BUFSIZE - 3))))
			{
				*mbuf = '\0';

				if(cur_len > mlen)
					sendto_channel_local(flags, chptr, "%s %s", modebuf,
							     parabuf);
				else
					continue;

				paracount = paralen = 0;
				cur_len = mlen;
				mbuf = modebuf + mlen;
				pbuf = parabuf;
				parabuf[0] = '\0';
				dir = MODE_QUERY;
			}

			if(dir != mode_changes[i].dir)
			{
				*mbuf++ = (mode_changes[i].dir == MODE_ADD) ? '+' : '-';
				cur_len++;
				dir = mode_changes[i].dir;
			}

			*mbuf++ = mode_changes[i].letter;
			cur_len++;

			if(mode_changes[i].arg != NULL)
			{
				paracount++;
				len = sprintf(pbuf, "%s ", mode_changes[i].arg);
				pbuf += len;
				paralen += len;
			}
		}

		if(paralen && parabuf[paralen - 1] == ' ')
			parabuf[paralen - 1] = '\0';

		*mbuf = '\0';
		if(cur_len > mlen)
			sendto_channel_local(flags, chptr, "%s %s", modebuf, parabuf);
	}

	/* only propagate modes originating locally, or if we're hubbing */
	if(MyClient(source_p) || rb_dlink_list_length(&serv_list) > 1)
		send_cap_mode_changes(client_p, source_p, chptr, mode_changes, mode_count);
}

/* set_channel_mlock()
 *
 * inputs	- client, source, channel, params
 * output	-
 * side effects - channel mlock is changed / MLOCK is propagated
 */
void
set_channel_mlock(struct Client *client_p, struct Client *source_p,
		  struct Channel *chptr, const char *newmlock, bool propagate)
{
	rb_free(chptr->mode_lock);
	chptr->mode_lock = newmlock ? rb_strdup(newmlock) : NULL;

	if (propagate)
	{
		sendto_server(client_p, NULL, CAP_TS6 | CAP_MLOCK, NOCAPS, ":%s MLOCK %ld %s :%s",
			      source_p->id, (long) chptr->channelts, chptr->chname,
			      chptr->mode_lock ? chptr->mode_lock : "");
	}
}
