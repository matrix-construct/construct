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
 *  $Id: m_dline.c 3051 2006-12-27 00:02:32Z jilles $
 */

#include "stdinc.h"
#include "tools.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "hostmask.h"
#include "numeric.h"
#include "commio.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_log.h"
#include "send.h"
#include "hash.h"
#include "s_serv.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static int mo_dline(struct Client *, struct Client *, int, const char **);
static int mo_undline(struct Client *, struct Client *, int, const char **);

struct Message dline_msgtab = {
	"DLINE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_dline, 2}}
};
struct Message undline_msgtab = {
	"UNDLINE", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_undline, 2}}
};

mapi_clist_av1 dline_clist[] = { &dline_msgtab, &undline_msgtab, NULL };
DECLARE_MODULE_AV1(dline, NULL, NULL, dline_clist, NULL, NULL, "$Revision: 3051 $");

static int valid_comment(char *comment);
static int flush_write(struct Client *, FILE *, char *, char *);
static int remove_temp_dline(const char *);

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
	char *oper_reason;
	char *reason = def;
	struct irc_sockaddr_storage daddr;
	char cidr_form_host[HOSTLEN + 1];
	struct ConfItem *aconf;
	int bits;
	char dlbuffer[IRCD_BUFSIZE];
	const char *current_date;
	int tdline_time = 0;
	int loc = 1;

	if(!IsOperK(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "kline");
		return 0;
	}

	if((tdline_time = valid_temp_time(parv[loc])) >= 0)
		loc++;

	if(parc < loc + 1)
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			   me.name, source_p->name, "DLINE");
		return 0;
	}

	dlhost = parv[loc];
	strlcpy(cidr_form_host, dlhost, sizeof(cidr_form_host));

	if(!parse_netmask(dlhost, NULL, &bits))
	{
		sendto_one(source_p, ":%s NOTICE %s :Invalid D-Line",
			   me.name, source_p->name);
		return 0;
	}

	loc++;

	if(parc >= loc + 1)	/* host :reason */
	{
		if(!EmptyString(parv[loc]))
			reason = LOCAL_COPY(parv[loc]);

		if(!valid_comment(reason))
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :Invalid character '\"' in comment",
				   me.name, source_p->name);
			return 0;
		}
	}

	if(IsOperAdmin(source_p))
	{
		if(bits < 8)
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :For safety, bitmasks less than 8 require conf access.",
				   me.name, parv[0]);
			return 0;
		}
	}
	else
	{
		if(bits < 16)
		{
			sendto_one(source_p,
				   ":%s NOTICE %s :Dline bitmasks less than 16 are for admins only.",
				   me.name, parv[0]);
			return 0;
		}
	}

	if(ConfigFileEntry.non_redundant_klines)
	{
		const char *creason;
		int t = AF_INET, ty, b;
		ty = parse_netmask(dlhost, (struct sockaddr *)&daddr, &b);
#ifdef IPV6
        	if(ty == HM_IPV6)
                	t = AF_INET6;
                else
#endif
			t = AF_INET;
                                  		
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
						   me.name, parv[0], dlhost, aconf->host, creason);
				else
					sendto_one(source_p,
						   ":%s NOTICE %s :[%s] already D-lined by [%s] - %s",
						   me.name, parv[0], dlhost, aconf->host, creason);
				return 0;
			}
		}
	}

	set_time();
	current_date = smalldate();

	aconf = make_conf();
	aconf->status = CONF_DLINE;
	DupString(aconf->host, dlhost);

	/* Look for an oper reason */
	if((oper_reason = strchr(reason, '|')) != NULL)
	{
		*oper_reason = '\0';
		oper_reason++;

		if(!EmptyString(oper_reason))
			DupString(aconf->spasswd, oper_reason);
	}

	if(tdline_time > 0)
	{
		ircsnprintf(dlbuffer, sizeof(dlbuffer), 
			 "Temporary D-line %d min. - %s (%s)",
			 (int) (tdline_time / 60), reason, current_date);
		DupString(aconf->passwd, dlbuffer);
		aconf->hold = CurrentTime + tdline_time;
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
		ircsnprintf(dlbuffer, sizeof(dlbuffer), "%s (%s)", reason, current_date);
		DupString(aconf->passwd, dlbuffer);
		add_conf_by_address(aconf->host, CONF_DLINE, NULL, aconf);
		write_confitem(DLINE_TYPE, source_p, NULL, aconf->host, reason,
			       oper_reason, current_date, 0);
	}

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
	FILE *in;
	FILE *out;
	char buf[BUFSIZE], buff[BUFSIZE], temppath[BUFSIZE], *p;
	const char *filename, *found_cidr;
	const char *cidr;
	int pairme = NO, error_on_write = NO;
	mode_t oldumask;

	ircsnprintf(temppath, sizeof(temppath), "%s.tmp", ConfigFileEntry.dlinefile);

	if(!IsOperUnkline(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "unkline");
		return 0;
	}

	cidr = parv[1];

	if(parse_netmask(cidr, NULL, NULL) == HM_HOST)
	{
		sendto_one(source_p, ":%s NOTICE %s :Invalid D-Line",
			   me.name, source_p->name);
		return 0;
	}

	if(remove_temp_dline(cidr))
	{
		sendto_one(source_p,
			   ":%s NOTICE %s :Un-dlined [%s] from temporary D-lines",
			   me.name, parv[0], cidr);
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "%s has removed the temporary D-Line for: [%s]",
				     get_oper_name(source_p), cidr);
		ilog(L_KLINE, "UD %s %s", get_oper_name(source_p), cidr);
		return 0;
	}

	filename = get_conf_name(DLINE_TYPE);

	if((in = fopen(filename, "r")) == 0)
	{
		sendto_one(source_p, ":%s NOTICE %s :Cannot open %s", me.name, parv[0], filename);
		return 0;
	}

	oldumask = umask(0);
	if((out = fopen(temppath, "w")) == 0)
	{
		sendto_one(source_p, ":%s NOTICE %s :Cannot open %s", me.name, parv[0], temppath);
		fclose(in);
		umask(oldumask);
		return 0;
	}

	umask(oldumask);

	while (fgets(buf, sizeof(buf), in))
	{
		strlcpy(buff, buf, sizeof(buff));

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

		if(irccmp(found_cidr, cidr) == 0)
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
			   me.name, parv[0]);
		return 0;
	}
	else if(!pairme)
	{
		sendto_one(source_p, ":%s NOTICE %s :No D-Line for %s",
			   me.name, parv[0], cidr);

		if(temppath != NULL)
			(void) unlink(temppath);

		return 0;
	}

	if (rename(temppath, filename))
	{
		sendto_one_notice(source_p, ":Couldn't rename temp file, aborted");
		return 0;
	}
	rehash_bans(0);


	sendto_one(source_p, ":%s NOTICE %s :D-Line for [%s] is removed", me.name, parv[0], cidr);
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			     "%s has removed the D-Line for: [%s]", get_oper_name(source_p), cidr);
	ilog(L_KLINE, "UD %s %s", get_oper_name(source_p), cidr);

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

	if(strlen(comment) > REASONLEN)
		comment[REASONLEN] = '\0';

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
		sendto_one(source_p, ":%s NOTICE %s :Unable to write to %s",
			   me.name, source_p->name, temppath);
		fclose(out);
		if(temppath != NULL)
			(void) unlink(temppath);
	}
	return (error_on_write);
}

/* remove_temp_dline()
 *
 * inputs       - hostname to undline
 * outputs      -
 * side effects - tries to undline anything that matches
 */
static int
remove_temp_dline(const char *host)
{
	struct ConfItem *aconf;
	dlink_node *ptr;
	struct irc_sockaddr_storage addr, caddr;
	int bits, cbits;
	int i;

	parse_netmask(host, (struct sockaddr *)&addr, &bits);

	for (i = 0; i < LAST_TEMP_TYPE; i++)
	{
		DLINK_FOREACH(ptr, temp_dlines[i].head)
		{
			aconf = ptr->data;

			parse_netmask(aconf->host, (struct sockaddr *)&caddr, &cbits);

			if(comp_with_mask_sock((struct sockaddr *)&addr, (struct sockaddr *)&caddr, bits) && bits == cbits)
			{
				dlinkDestroy(ptr, &temp_dlines[i]);
				delete_one_address_conf(aconf->host, aconf);
				return YES;
			}
		}
	}

	return NO;
}
