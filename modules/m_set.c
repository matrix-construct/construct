/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_set.c: Sets a server parameter.
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
 *  $Id: m_set.c 494 2006-01-15 16:08:28Z jilles $
 */

/* rewritten by jdc */

#include "stdinc.h"
#include "client.h"
#include "event.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "numeric.h"
#include "commio.h"
#include "s_serv.h"
#include "send.h"
#include "common.h"
#include "channel.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static int mo_set(struct Client *, struct Client *, int, const char **);

struct Message set_msgtab = {
	"SET", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_set, 0}}
};

mapi_clist_av1 set_clist[] = { &set_msgtab, NULL };
DECLARE_MODULE_AV1(set, NULL, NULL, set_clist, NULL, NULL, "$Revision: 494 $");

/* Structure used for the SET table itself */
struct SetStruct
{
	const char *name;
	void (*handler) ();
	int wants_char;		/* 1 if it expects (char *, [int]) */
	int wants_int;		/* 1 if it expects ([char *], int) */

	/* eg:  0, 1 == only an int arg
	 * eg:  1, 1 == char and int args */
};


static void quote_adminstring(struct Client *, const char *);
static void quote_autoconn(struct Client *, char *, int);
static void quote_autoconnall(struct Client *, int);
static void quote_floodcount(struct Client *, int);
static void quote_identtimeout(struct Client *, int);
static void quote_idletime(struct Client *, int);
static void quote_max(struct Client *, int);
static void quote_operstring(struct Client *, const char *);
static void quote_spamnum(struct Client *, int);
static void quote_spamtime(struct Client *, int);
static void quote_splitmode(struct Client *, char *);
static void quote_splitnum(struct Client *, int);
static void quote_splitusers(struct Client *, int);
static void list_quote_commands(struct Client *);


/* 
 * If this ever needs to be expanded to more than one arg of each
 * type, want_char/want_int could be the count of the arguments,
 * instead of just a boolean flag...
 *
 * -davidt
 */

static struct SetStruct set_cmd_table[] = {
	/* name               function      string arg  int arg */
	/* -------------------------------------------------------- */
	{"ADMINSTRING",	quote_adminstring,	1,	0	},
	{"AUTOCONN", 	quote_autoconn, 	1,	1	},
	{"AUTOCONNALL", quote_autoconnall, 	0,	1	},
	{"FLOODCOUNT", 	quote_floodcount, 	0,	1	},
	{"IDENTTIMEOUT", quote_identtimeout,	0,	1	},
	{"IDLETIME", 	quote_idletime, 	0,	1	},
	{"MAX", 	quote_max, 		0,	1	},
	{"MAXCLIENTS",	quote_max,		0,	1	},
	{"OPERSTRING",	quote_operstring,	1,	0	},
	{"SPAMNUM", 	quote_spamnum, 		0,	1	},
	{"SPAMTIME", 	quote_spamtime, 	0,	1	},
	{"SPLITMODE", 	quote_splitmode, 	1,	0	},
	{"SPLITNUM", 	quote_splitnum, 	0,	1	},
	{"SPLITUSERS", 	quote_splitusers, 	0,	1	},
	/* -------------------------------------------------------- */
	{(char *) 0, (void (*)()) 0, 0, 0}
};


/*
 * list_quote_commands() sends the client all the available commands.
 * Four to a line for now.
 */
static void
list_quote_commands(struct Client *source_p)
{
	int i;
	int j = 0;
	const char *names[4];

	sendto_one(source_p, ":%s NOTICE %s :Available QUOTE SET commands:",
		   me.name, source_p->name);

	names[0] = names[1] = names[2] = names[3] = "";

	for (i = 0; set_cmd_table[i].handler; i++)
	{
		names[j++] = set_cmd_table[i].name;

		if(j > 3)
		{
			sendto_one(source_p, ":%s NOTICE %s :%s %s %s %s",
				   me.name, source_p->name, names[0], names[1], names[2], names[3]);
			j = 0;
			names[0] = names[1] = names[2] = names[3] = "";
		}

	}
	if(j)
		sendto_one(source_p, ":%s NOTICE %s :%s %s %s %s",
			   me.name, source_p->name, names[0], names[1], names[2], names[3]);
}

/* SET AUTOCONN */
static void
quote_autoconn(struct Client *source_p, char *arg, int newval)
{
	set_server_conf_autoconn(source_p, arg, newval);
}

/* SET AUTOCONNALL */
static void
quote_autoconnall(struct Client *source_p, int newval)
{
	if(newval >= 0)
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s has changed AUTOCONNALL to %i",
				     source_p->name, newval);

		GlobalSetOptions.autoconn = newval;
	}
	else
	{
		sendto_one(source_p, ":%s NOTICE %s :AUTOCONNALL is currently %i",
			   me.name, source_p->name, GlobalSetOptions.autoconn);
	}
}


/* SET FLOODCOUNT */
static void
quote_floodcount(struct Client *source_p, int newval)
{
	if(newval >= 0)
	{
		GlobalSetOptions.floodcount = newval;
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "%s has changed FLOODCOUNT to %i", source_p->name,
				     GlobalSetOptions.floodcount);
	}
	else
	{
		sendto_one(source_p, ":%s NOTICE %s :FLOODCOUNT is currently %i",
			   me.name, source_p->name, GlobalSetOptions.floodcount);
	}
}

/* SET IDENTTIMEOUT */
static void
quote_identtimeout(struct Client *source_p, int newval)
{
	if(!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "admin");
		return;
	}

	if(newval > 0)
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "%s has changed IDENTTIMEOUT to %d",
				     get_oper_name(source_p), newval);
		GlobalSetOptions.ident_timeout = newval;
	}
	else
		sendto_one(source_p, ":%s NOTICE %s :IDENTTIMEOUT is currently %d",
			   me.name, source_p->name, GlobalSetOptions.ident_timeout);
}

/* SET IDLETIME */
static void
quote_idletime(struct Client *source_p, int newval)
{
	if(newval >= 0)
	{
		if(newval == 0)
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					     "%s has disabled idletime checking", source_p->name);
			GlobalSetOptions.idletime = 0;
		}
		else
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					     "%s has changed IDLETIME to %i",
					     source_p->name, newval);
			GlobalSetOptions.idletime = (newval * 60);
		}
	}
	else
	{
		sendto_one(source_p, ":%s NOTICE %s :IDLETIME is currently %i",
			   me.name, source_p->name, GlobalSetOptions.idletime / 60);
	}
}

/* SET MAX */
static void
quote_max(struct Client *source_p, int newval)
{
	if(newval > 0)
	{
		if(newval > MASTER_MAX)
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :You cannot set MAXCLIENTS to > MASTER_MAX (%d)",
				   me.name, source_p->name, MASTER_MAX);
			return;
		}

		if(newval < 32)
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :You cannot set MAXCLIENTS to < 32 (%d:%d)",
				   me.name, source_p->name, GlobalSetOptions.maxclients,
				   highest_fd);
			return;
		}

		GlobalSetOptions.maxclients = newval;

		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "%s!%s@%s set new MAXCLIENTS to %d (%lu current)",
				     source_p->name, source_p->username, source_p->host,
				     GlobalSetOptions.maxclients, 
				     dlink_list_length(&lclient_list));

		return;
	}
	else
	{
		sendto_one(source_p, ":%s NOTICE %s :Current Maxclients = %d (%lu)",
			   me.name, source_p->name, GlobalSetOptions.maxclients,
			   dlink_list_length(&lclient_list));
	}
}

/* SET OPERSTRING */
static void
quote_operstring(struct Client *source_p, const char *arg)
{
	if(EmptyString(arg))
	{
		sendto_one(source_p, ":%s NOTICE %s :OPERSTRING is currently '%s'",
			   me.name, source_p->name, GlobalSetOptions.operstring);
	}
	else
	{
		strlcpy(GlobalSetOptions.operstring, arg,
			sizeof(GlobalSetOptions.operstring));
		
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "%s has changed OPERSTRING to '%s'",
				     get_oper_name(source_p), arg);
	}
}

/* SET ADMINSTRING */
static void
quote_adminstring(struct Client *source_p, const char *arg)
{
	if(EmptyString(arg))
	{
		sendto_one(source_p, ":%s NOTICE %s :ADMINSTRING is currently '%s'",
			   me.name, source_p->name, GlobalSetOptions.adminstring);
	}
	else
	{
		strlcpy(GlobalSetOptions.adminstring, arg,
			sizeof(GlobalSetOptions.adminstring));
		
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "%s has changed ADMINSTRING to '%s'",
				     get_oper_name(source_p), arg);
	}
}

/* SET SPAMNUM */
static void
quote_spamnum(struct Client *source_p, int newval)
{
	if(newval > 0)
	{
		if(newval == 0)
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					     "%s has disabled ANTI_SPAMBOT", source_p->name);
			GlobalSetOptions.spam_num = newval;
			return;
		}
		if(newval < MIN_SPAM_NUM)
		{
			GlobalSetOptions.spam_num = MIN_SPAM_NUM;
		}
		else		/* if (newval < MIN_SPAM_NUM) */
		{
			GlobalSetOptions.spam_num = newval;
		}
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s has changed SPAMNUM to %i",
				     source_p->name, GlobalSetOptions.spam_num);
	}
	else
	{
		sendto_one(source_p, ":%s NOTICE %s :SPAMNUM is currently %i",
			   me.name, source_p->name, GlobalSetOptions.spam_num);
	}
}

/* SET SPAMTIME */
static void
quote_spamtime(struct Client *source_p, int newval)
{
	if(newval > 0)
	{
		if(newval < MIN_SPAM_TIME)
		{
			GlobalSetOptions.spam_time = MIN_SPAM_TIME;
		}
		else		/* if (newval < MIN_SPAM_TIME) */
		{
			GlobalSetOptions.spam_time = newval;
		}
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s has changed SPAMTIME to %i",
				     source_p->name, GlobalSetOptions.spam_time);
	}
	else
	{
		sendto_one(source_p, ":%s NOTICE %s :SPAMTIME is currently %i",
			   me.name, source_p->name, GlobalSetOptions.spam_time);
	}
}

/* this table is what splitmode may be set to */
static const char *splitmode_values[] = {
	"OFF",
	"ON",
	"AUTO",
	NULL
};

/* this table is what splitmode may be */
static const char *splitmode_status[] = {
	"OFF",
	"AUTO (OFF)",
	"ON",
	"AUTO (ON)",
	NULL
};

/* SET SPLITMODE */
static void
quote_splitmode(struct Client *source_p, char *charval)
{
	if(charval)
	{
		int newval;

		for (newval = 0; splitmode_values[newval]; newval++)
		{
			if(!irccmp(splitmode_values[newval], charval))
				break;
		}

		/* OFF */
		if(newval == 0)
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					     "%s is disabling splitmode", get_oper_name(source_p));

			splitmode = 0;
			splitchecking = 0;

			eventDelete(check_splitmode, NULL);
		}
		/* ON */
		else if(newval == 1)
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					     "%s is enabling and activating splitmode",
					     get_oper_name(source_p));

			splitmode = 1;
			splitchecking = 0;

			/* we might be deactivating an automatic splitmode, so pull the event */
			eventDelete(check_splitmode, NULL);
		}
		/* AUTO */
		else if(newval == 2)
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					     "%s is enabling automatic splitmode",
					     get_oper_name(source_p));

			splitchecking = 1;
			check_splitmode(NULL);
		}
	}
	else
		/* if we add splitchecking to splitmode*2 we get a unique table to 
		 * pull values back out of, splitmode can be four states - but you can
		 * only set to three, which means we cant use the same table --fl_
		 */
		sendto_one(source_p, ":%s NOTICE %s :SPLITMODE is currently %s",
			   me.name, source_p->name,
			   splitmode_status[(splitchecking + (splitmode * 2))]);
}

/* SET SPLITNUM */
static void
quote_splitnum(struct Client *source_p, int newval)
{
	if(newval >= 0)
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "%s has changed SPLITNUM to %i", source_p->name, newval);
		split_servers = newval;

		if(splitchecking)
			check_splitmode(NULL);
	}
	else
		sendto_one(source_p, ":%s NOTICE %s :SPLITNUM is currently %i",
			   me.name, source_p->name, split_servers);
}

/* SET SPLITUSERS */
static void
quote_splitusers(struct Client *source_p, int newval)
{
	if(newval >= 0)
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "%s has changed SPLITUSERS to %i", source_p->name, newval);
		split_users = newval;

		if(splitchecking)
			check_splitmode(NULL);
	}
	else
		sendto_one(source_p, ":%s NOTICE %s :SPLITUSERS is currently %i",
			   me.name, source_p->name, split_users);
}

/*
 * mo_set - SET command handler
 * set options while running
 */
static int
mo_set(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	int newval;
	int i, n;
	const char *arg = NULL;
	const char *intarg = NULL;

	if(parc > 1)
	{
		/*
		 * Go through all the commands in set_cmd_table, until one is
		 * matched.  I realize strcmp() is more intensive than a numeric
		 * lookup, but at least it's better than a big-ass switch/case
		 * statement.
		 */
		for (i = 0; set_cmd_table[i].handler; i++)
		{
			if(!irccmp(set_cmd_table[i].name, parv[1]))
			{
				/*
				 * Command found; now execute the code
				 */
				n = 2;

				if(set_cmd_table[i].wants_char)
				{
					arg = parv[n++];
				}

				if(set_cmd_table[i].wants_int)
				{
					intarg = parv[n++];
				}

				if((n - 1) > parc)
				{
					sendto_one(source_p,
						   ":%s NOTICE %s :SET %s expects (\"%s%s\") args",
						   me.name, source_p->name,
						   set_cmd_table[i].name,
						   (set_cmd_table[i].
						    wants_char ? "string, " : ""),
						   (set_cmd_table[i].
						    wants_char ? "int" : ""));
					return 0;
				}

				if(parc <= 2)
				{
					arg = NULL;
					intarg = NULL;
				}

				if(set_cmd_table[i].wants_int && (parc > 2))
				{
					if(intarg)
					{
						if(!irccmp(intarg, "yes") || !irccmp(intarg, "on"))
							newval = 1;
						else if(!irccmp(intarg, "no")
							|| !irccmp(intarg, "off"))
							newval = 0;
						else
							newval = atoi(intarg);
					}
					else
					{
						newval = -1;
					}

					if(newval < 0)
					{
						sendto_one(source_p,
							   ":%s NOTICE %s :Value less than 0 illegal for %s",
							   me.name, source_p->name,
							   set_cmd_table[i].name);

						return 0;
					}
				}
				else
					newval = -1;

				if(set_cmd_table[i].wants_char)
				{
					if(set_cmd_table[i].wants_int)
						set_cmd_table[i].handler(source_p, arg, newval);
					else
						set_cmd_table[i].handler(source_p, arg);
					return 0;
				}
				else
				{
					if(set_cmd_table[i].wants_int)
						set_cmd_table[i].handler(source_p, newval);
					else
						/* Just in case someone actually wants a
						 * set function that takes no args.. *shrug* */
						set_cmd_table[i].handler(source_p);
					return 0;
				}
			}
		}

		/*
		 * Code here will be executed when a /QUOTE SET command is not
		 * found within set_cmd_table.
		 */
		sendto_one(source_p, ":%s NOTICE %s :Variable not found.", me.name, parv[0]);
		return 0;
	}

	list_quote_commands(source_p);

	return 0;
}
