/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_dline.c: Bans/unbans a user.
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
 *  $Id: m_dline.c 3225 2007-03-04 23:42:55Z jilles $
 */

#include "stdinc.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "match.h"
#include "ircd.h"
#include "hostmask.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "logger.h"
#include "send.h"
#include "hash.h"
#include "s_serv.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static int mo_dline(struct Client *, struct Client *, int, const char **);
static int me_dline(struct Client *, struct Client *, int, const char **);
static int mo_undline(struct Client *, struct Client *, int, const char **);
static int me_undline(struct Client *, struct Client *, int, const char **);

struct Message dline_msgtab = {
	"DLINE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_dline, 3}, {mo_dline, 2}}
};
struct Message undline_msgtab = {
	"UNDLINE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_undline, 1}, {mo_undline, 2}}
};

mapi_clist_av1 dline_clist[] = { &dline_msgtab, &undline_msgtab, NULL };
DECLARE_MODULE_AV1(dline, NULL, NULL, dline_clist, NULL, NULL, "$Revision: 3225 $");

static int valid_comment(char *comment);
static int flush_write(struct Client *, FILE *, char *, char *);
static int remove_temp_dline(struct ConfItem *);
static int apply_dline(struct Client *, const char *, int, char *);
static int apply_undline(struct Client *, const char *);

/* mo_dline()
 * 
 *   parv[1] - dline to add
 *   parv[2] - reason
 */
static int
mo_dline(struct Client *client_p, struct Client *source_p,
	 int parc, const char *parv[])
{
	char def[] = "No Reason";
	const char *dlhost;
	char *reason = def;
	char cidr_form_host[HOSTLEN + 1];
	int tdline_time = 0;
	const char *target_server = NULL;
	int loc = 1;

	if(!IsOperK(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "kline");
		return 0;
	}

	if((tdline_time = valid_temp_time(parv[loc])) >= 0)
		loc++;

	dlhost = parv[loc];
	rb_strlcpy(cidr_form_host, dlhost, sizeof(cidr_form_host));

	loc++;

	if(parc >= loc+2 && !irccmp(parv[loc], "ON"))
 	{
		if(!IsOperRemoteBan(source_p))
		{
			sendto_one(source_p, form_str(ERR_NOPRIVS),
				me.name, source_p->name, "remoteban");
			return 0;
		}

		target_server = parv[loc+1];
		loc += 2;
	}

	if(parc >= loc + 1 && !EmptyString(parv[loc]))
		reason = LOCAL_COPY(parv[loc]);

	if(target_server != NULL)
	{
		sendto_match_servs(source_p, target_server,
				CAP_ENCAP, NOCAPS,
				"ENCAP %s DLINE %d %s :%s",
				target_server, tdline_time, dlhost, reason);

		if(!match(target_server, me.name))
			return 0;
	}

	apply_dline(source_p, dlhost, tdline_time, reason);

	check_dlines();
	return 0;
}

/* mo_undline()
 *
 *      parv[1] = dline to remove
 */
static int
mo_undline(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	const char *cidr;
	const char *target_server = NULL;

	if(!IsOperK(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "unkline");
 		return 0;
 	}
 
	cidr = parv[1];

	if(parc >= 4 && !irccmp(parv[2], "ON"))
	{
		if(!IsOperRemoteBan(source_p))
		{
			sendto_one(source_p, form_str(ERR_NOPRIVS),
				me.name, source_p->name, "remoteban");
			return 0;
		}

		target_server = parv[3];
		sendto_match_servs(source_p, target_server,
				CAP_ENCAP, NOCAPS,
				"ENCAP %s UNDLINE %s",
				target_server, cidr);

		if(!match(target_server, me.name))
			return 0;
	}

	apply_undline(source_p, cidr);

	return 0;
}

static int
me_dline(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	int tdline_time = atoi(parv[1]);
	/* Since this is coming over a server link, assume that the originating
	 * server did the relevant permission/sanity checks...
	 */

	if(!IsPerson(source_p))
		return 0;

	if(!find_shared_conf(source_p->username, source_p->host,
				source_p->servptr->name, tdline_time > 0 ? SHARED_TDLINE : SHARED_PDLINE))
		return 0;

	apply_dline(source_p, parv[2], tdline_time, LOCAL_COPY(parv[3]));

	check_dlines();
	return 0;
}

static int
me_undline(struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!IsPerson(source_p))
		return 0;

	if(!find_shared_conf(source_p->username, source_p->host,
				source_p->servptr->name, SHARED_UNDLINE))
		return 0;

	apply_undline(source_p, parv[1]);

	return 0;
}

static int
apply_dline(struct Client *source_p, const char *dlhost, int tdline_time, char *reason)
{
	struct ConfItem *aconf;
	char *oper_reason;
	char dlbuffer[IRCD_BUFSIZE];
	const char *current_date;
	struct rb_sockaddr_storage daddr;
	int t = AF_INET, ty, b;
	const char *creason;

	ty = parse_netmask(dlhost, (struct sockaddr *)&daddr, &b);
	if(ty == HM_HOST)
	{
		sendto_one(source_p, ":%s NOTICE %s :Invalid D-Line",
			   me.name, source_p->name);
		return 0;
	}
#ifdef RB_IPV6
	if(ty == HM_IPV6)
		t = AF_INET6;
	else
#endif
		t = AF_INET;

	/* This means dlines wider than /16 cannot be set remotely */
	if(IsOperAdmin(source_p))
	{
		if(b < 8)
		{
			sendto_one_notice(source_p,
				   ":For safety, bitmasks less than 8 require conf access.");
			return 0;
		}
	}
	else
	{
		if(b < 16)
		{
			sendto_one_notice(source_p,
				   ":Dline bitmasks less than 16 are for admins only.");
			return 0;
		}
	}

	if(!valid_comment(reason))
	{
		sendto_one(source_p,
			   ":%s NOTICE %s :Invalid character '\"' in comment",
			   me.name, source_p->name);
		return 0;
	}

	if(ConfigFileEntry.non_redundant_klines)
	{
		if((aconf = find_dline((struct sockaddr *)&daddr, t)) != NULL)
		{
			int bx;
			parse_netmask(aconf->host, NULL, &bx);
			if(b >= bx)
			{
				creason = aconf->passwd ? aconf->passwd : "<No Reason>";
				if(IsConfExemptKline(aconf))
					sendto_one(source_p,
						   ":%s NOTICE %s :[%s] is (E)d-lined by [%s] - %s",
						   me.name, source_p->name, dlhost, aconf->host, creason);
				else
					sendto_one(source_p,
						   ":%s NOTICE %s :[%s] already D-lined by [%s] - %s",
						   me.name, source_p->name, dlhost, aconf->host, creason);
				return 0;
			}
		}
	}

	rb_set_time();
	current_date = smalldate();

	aconf = make_conf();
	aconf->status = CONF_DLINE;
	aconf->host = rb_strdup(dlhost);

	/* Look for an oper reason */
	if((oper_reason = strchr(reason, '|')) != NULL)
	{
		*oper_reason = '\0';
		oper_reason++;

		if(!EmptyString(oper_reason))
			aconf->spasswd = rb_strdup(oper_reason);
	}

	if(tdline_time > 0)
	{
		rb_snprintf(dlbuffer, sizeof(dlbuffer), 
			 "Temporary D-line %d min. - %s (%s)",
			 (int) (tdline_time / 60), reason, current_date);
		aconf->passwd = rb_strdup(dlbuffer);
		aconf->hold = rb_current_time() + tdline_time;
		add_temp_dline(aconf);

		if(EmptyString(oper_reason))
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					     "%s added temporary %d min. D-Line for [%s] [%s]",
					     get_oper_name(source_p), tdline_time / 60,
					     aconf->host, reason);
			ilog(L_KLINE, "D %s %d %s %s",
				get_oper_name(source_p), tdline_time / 60,
				aconf->host, reason);
		}
		else
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					     "%s added temporary %d min. D-Line for [%s] [%s|%s]",
					     get_oper_name(source_p), tdline_time / 60,
					     aconf->host, reason, oper_reason);
			ilog(L_KLINE, "D %s %d %s %s|%s",
				get_oper_name(source_p), tdline_time / 60,
				aconf->host, reason, oper_reason);
		}

		sendto_one(source_p, ":%s NOTICE %s :Added temporary %d min. D-Line for [%s]",
			   me.name, source_p->name, tdline_time / 60, aconf->host);
	}
	else
	{
		rb_snprintf(dlbuffer, sizeof(dlbuffer), "%s (%s)", reason, current_date);
		aconf->passwd = rb_strdup(dlbuffer);
		add_conf_by_address(aconf->host, CONF_DLINE, NULL, NULL, aconf);
		write_confitem(DLINE_TYPE, source_p, NULL, aconf->host, reason,
			       oper_reason, current_date, 0);
	}

	return 0;
}

static int
apply_undline(struct Client *source_p, const char *cidr)
{
	FILE *in;
	FILE *out;
	char buf[BUFSIZE], buff[BUFSIZE], temppath[BUFSIZE], *p;
	const char *filename, *found_cidr;
	int pairme = NO, error_on_write = NO;
	mode_t oldumask;
	struct ConfItem *aconf;

	if(parse_netmask(cidr, NULL, NULL) == HM_HOST)
	{
		sendto_one_notice(source_p, ":Invalid D-Line");
		return 0;
	}

	rb_snprintf(temppath, sizeof(temppath), "%s.tmp", ConfigFileEntry.dlinefile);

	aconf = find_exact_conf_by_address(cidr, CONF_DLINE, NULL);
	if(aconf == NULL)
	{
		sendto_one_notice(source_p, ":No D-Line for %s", cidr);
		return 0;
	}

	rb_strlcpy(buf, aconf->host, sizeof buf);
	if(remove_temp_dline(aconf))
	{
		sendto_one(source_p,
			   ":%s NOTICE %s :Un-dlined [%s] from temporary D-lines",
			   me.name, source_p->name, buf);
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "%s has removed the temporary D-Line for: [%s]",
				     get_oper_name(source_p), buf);
		ilog(L_KLINE, "UD %s %s", get_oper_name(source_p), buf);
		return 0;
	}

	filename = get_conf_name(DLINE_TYPE);

	if((in = fopen(filename, "r")) == 0)
	{
		sendto_one(source_p, ":%s NOTICE %s :Cannot open %s", me.name, source_p->name, filename);
		return 0;
	}

	oldumask = umask(0);
	if((out = fopen(temppath, "w")) == 0)
	{
		sendto_one(source_p, ":%s NOTICE %s :Cannot open %s", me.name, source_p->name, temppath);
		fclose(in);
		umask(oldumask);
		return 0;
	}

	umask(oldumask);

	while (fgets(buf, sizeof(buf), in))
	{
		rb_strlcpy(buff, buf, sizeof(buff));

		if((p = strchr(buff, '\n')) != NULL)
			*p = '\0';

		if((*buff == '\0') || (*buff == '#'))
		{
			if(!error_on_write)
				flush_write(source_p, out, buf, temppath);
			continue;
		}

		if((found_cidr = getfield(buff)) == NULL)
		{
			if(!error_on_write)
				flush_write(source_p, out, buf, temppath);
			continue;
		}

		if(irccmp(found_cidr, aconf->host) == 0)
		{
			pairme++;
		}
		else
		{
			if(!error_on_write)
				flush_write(source_p, out, buf, temppath);
			continue;
		}
	}

	fclose(in);
	if (fclose(out))
		error_on_write = YES;

	if(error_on_write)
	{
		sendto_one(source_p,
			   ":%s NOTICE %s :Couldn't write D-line file, aborted", 
			   me.name, source_p->name);
		return 0;
	}
	else if(!pairme)
	{
		sendto_one_notice(source_p, ":Cannot find D-Line for %s in file",
				aconf->host);

		if(temppath != NULL)
			(void) unlink(temppath);

		return 0;
	}

	if (rename(temppath, filename))
	{
		sendto_one_notice(source_p, ":Couldn't rename temp file, aborted");
		return 0;
	}

	sendto_one(source_p, ":%s NOTICE %s :D-Line for [%s] is removed", me.name, source_p->name, aconf->host);
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			     "%s has removed the D-Line for: [%s]", get_oper_name(source_p), aconf->host);
	ilog(L_KLINE, "UD %s %s", get_oper_name(source_p), aconf->host);
	delete_one_address_conf(aconf->host, aconf);

	return 0;
}

/*
 * valid_comment
 * inputs	- pointer to client
 *              - pointer to comment
 * output       - 0 if no valid comment, 1 if valid
 * side effects - NONE
 */
static int
valid_comment(char *comment)
{
	if(strchr(comment, '"'))
		return 0;

	if(strlen(comment) > BANREASONLEN)
		comment[BANREASONLEN] = '\0';

	return 1;
}

/*
 * flush_write()
 *
 * inputs       - pointer to client structure of oper requesting unkline
 *              - out is the file descriptor
 *              - buf is the buffer to write
 *              - ntowrite is the expected number of character to be written
 *              - temppath is the temporary file name to be written
 * output       - YES for error on write
 *              - NO for success
 * side effects - if successful, the buf is written to output file
 *                if a write failure happesn, and the file pointed to
 *                by temppath, if its non NULL, is removed.
 *
 * The idea here is, to be as robust as possible when writing to the 
 * kline file.
 *
 * -Dianora
 */
static int
flush_write(struct Client *source_p, FILE * out, char *buf, char *temppath)
{
	int error_on_write = (fputs(buf, out) < 0) ? YES : NO;

	if(error_on_write)
	{
		sendto_one_notice(source_p, ":Unable to write to %s", temppath);
		fclose(out);
		if(temppath != NULL)
			(void) unlink(temppath);
	}
	return (error_on_write);
}

/* remove_temp_dline()
 *
 * inputs       - confitem to undline
 * outputs      -
 * side effects - tries to undline anything that matches
 */
static int
remove_temp_dline(struct ConfItem *aconf)
{
	rb_dlink_node *ptr;
	int i;

	for (i = 0; i < LAST_TEMP_TYPE; i++)
	{
		RB_DLINK_FOREACH(ptr, temp_dlines[i].head)
		{
			if (aconf == ptr->data)
			{
				rb_dlinkDestroy(ptr, &temp_dlines[i]);
				delete_one_address_conf(aconf->host, aconf);
				return YES;
			}
		}
	}

	return NO;
}
