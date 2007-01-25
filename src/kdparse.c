/*
 *  ircd-ratbox: A slightly useful ircd.
 *  kdparse.c: Parses K and D lines.
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
 *  $Id: kdparse.c 254 2005-09-21 23:35:12Z nenolod $
 */

#include "stdinc.h"
#include "tools.h"
#include "s_log.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "hostmask.h"
#include "client.h"
#include "irc_string.h"
#include "memory.h"
#include "hash.h"

/* conf_add_fields()
 * 
 * inputs       - pointer to config item, host/pass/user/operreason fields
 * output       - NONE
 * side effects - update respective fields with pointers
 */
static void
conf_add_fields(struct ConfItem *aconf,	const char *host_field,
		const char *pass_field,	const char *user_field,
		const char *operreason_field)
{
	if(host_field != NULL)
		DupString(aconf->host, host_field);
	if(pass_field != NULL)
		DupString(aconf->passwd, pass_field);
	if(user_field != NULL)
		DupString(aconf->user, user_field);
	if(operreason_field != NULL)
		DupString(aconf->spasswd, operreason_field);
}

/*
 * parse_k_file
 * Inputs       - pointer to line to parse
 * Output       - NONE
 * Side Effects - Parse one new style K line
 */

void
parse_k_file(FILE * file)
{
	struct ConfItem *aconf;
	char *user_field = NULL;
	char *reason_field = NULL;
	char *operreason_field = NULL;
	char *host_field = NULL;
	char line[BUFSIZE];
	char *p;

	while (fgets(line, sizeof(line), file))
	{
		if((p = strchr(line, '\n')) != NULL)
			*p = '\0';

		if((*line == '\0') || (*line == '#'))
			continue;

		user_field = getfield(line);
		if(EmptyString(user_field))
			continue;

		host_field = getfield(NULL);
		if(EmptyString(host_field))
			continue;

		reason_field = getfield(NULL);
		if(EmptyString(reason_field))
			continue;

		operreason_field = getfield(NULL);

		aconf = make_conf();
		aconf->status = CONF_KILL;
		conf_add_fields(aconf, host_field, reason_field,
				user_field, operreason_field);

		if(aconf->host != NULL)
			add_conf_by_address(aconf->host, CONF_KILL, aconf->user, aconf);
	}
}

/*
 * parse_d_file
 * Inputs       - pointer to line to parse
 * Output       - NONE
 * Side Effects - Parse one new style D line
 */

void
parse_d_file(FILE * file)
{
	struct ConfItem *aconf;
	char *reason_field = NULL;
	char *host_field = NULL;
	char *operreason_field = NULL;
	char line[BUFSIZE];
	char *p;

	while (fgets(line, sizeof(line), file))
	{
		if((p = strchr(line, '\n')))
			*p = '\0';

		if((*line == '\0') || (line[0] == '#'))
			continue;

		host_field = getfield(line);
		if(EmptyString(host_field))
			continue;

		reason_field = getfield(NULL);
		if(EmptyString(reason_field))
			continue;

		operreason_field = getfield(NULL);

		aconf = make_conf();
		aconf->status = CONF_DLINE;
		conf_add_fields(aconf, host_field, reason_field, "", operreason_field);
		conf_add_d_conf(aconf);
	}
}

void
parse_x_file(FILE * file)
{
	struct ConfItem *aconf;
	char *gecos_field = NULL;
	char *reason_field = NULL;
	char line[BUFSIZE];
	char *p;

	while (fgets(line, sizeof(line), file))
	{
		if((p = strchr(line, '\n')))
			*p = '\0';

		if((*line == '\0') || (line[0] == '#'))
			continue;

		gecos_field = getfield(line);
		if(EmptyString(gecos_field))
			continue;

		/* field for xline types, which no longer exist */
		getfield(NULL);

		reason_field = getfield(NULL);
		if(EmptyString(reason_field))
			continue;

		/* sanity checking */
		if((find_xline(gecos_field, 0) != NULL) ||
		   (strchr(reason_field, ':') != NULL))
			continue;

		aconf = make_conf();
		aconf->status = CONF_XLINE;

		DupString(aconf->name, gecos_field);
		DupString(aconf->passwd, reason_field);

		dlinkAddAlloc(aconf, &xline_conf_list);
	}
}

void
parse_resv_file(FILE * file)
{
	struct ConfItem *aconf;
	char *reason_field;
	char *host_field;
	char line[BUFSIZE];
	char *p;

	while (fgets(line, sizeof(line), file))
	{
		if((p = strchr(line, '\n')))
			*p = '\0';

		if((*line == '\0') || (line[0] == '#'))
			continue;

		host_field = getfield(line);
		if(EmptyString(host_field))
			continue;

		reason_field = getfield(NULL);
		if(EmptyString(reason_field))
			continue;

		if(IsChannelName(host_field))
		{
			if(hash_find_resv(host_field))
				continue;

			aconf = make_conf();
			aconf->status = CONF_RESV_CHANNEL;
			aconf->port = 0;

			DupString(aconf->name, host_field);
			DupString(aconf->passwd, reason_field);
			add_to_resv_hash(aconf->name, aconf);
		}
		else if(clean_resv_nick(host_field))
		{
			if(find_nick_resv(host_field))
				continue;

			aconf = make_conf();
			aconf->status = CONF_RESV_NICK;
			aconf->port = 0;

			DupString(aconf->name, host_field);
			DupString(aconf->passwd, reason_field);
			dlinkAddAlloc(aconf, &resv_conf_list);
		}
	}
}

/*
 * getfield
 *
 * inputs	- input buffer
 * output	- next field
 * side effects	- field breakup for ircd.conf file.
 */
char *
getfield(char *newline)
{
	static char *line = NULL;
	char *end, *field;

	if(newline != NULL)
		line = newline;

	if(line == NULL)
		return (NULL);

	field = line;

	/* XXX make this skip to first " if present */
	if(*field == '"')
		field++;
	else
		return (NULL);	/* mal-formed field */

	end = strchr(line, ',');

	while(1)
	{
		/* no trailing , - last field */
		if(end == NULL)
		{
			end = line + strlen(line);
			line = NULL;

			if(*end == '"')
			{
				*end = '\0';
				return field;
			}
			else
				return NULL;
		}
		else
		{
			/* look for a ", to mark the end of a field.. */
			if(*(end - 1) == '"')
			{
				line = end + 1;
				end--;
				*end = '\0';
				return field;
			}

			/* search for the next ',' */
			end++;
			end = strchr(end, ',');
		}
	}

	return NULL;
}
