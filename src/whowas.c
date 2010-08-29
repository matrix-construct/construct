/*
 *  ircd-ratbox: A slightly useful ircd.
 *  whowas.c: WHOWAS user cache.
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
 *  $Id: whowas.c 1717 2006-07-04 14:41:11Z jilles $
 */

#include "stdinc.h"

#include "whowas.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "match.h"
#include "ircd.h"
#include "ircd_defs.h"
#include "numeric.h"
#include "s_serv.h"
#include "s_user.h"
#include "send.h"
#include "s_conf.h"
#include "scache.h"

/* internally defined function */
static void add_whowas_to_clist(struct Whowas **, struct Whowas *);
static void del_whowas_from_clist(struct Whowas **, struct Whowas *);
static void add_whowas_to_list(struct Whowas **, struct Whowas *);
static void del_whowas_from_list(struct Whowas **, struct Whowas *);

struct Whowas WHOWAS[NICKNAMEHISTORYLENGTH];
struct Whowas *WHOWASHASH[WW_MAX];

static int whowas_next = 0;

unsigned int hash_whowas_name(const char *name)
{
	return fnv_hash_upper((const unsigned char *) name, WW_MAX_BITS);
}

void add_history(struct Client *client_p, int online)
{
	struct Whowas *who = &WHOWAS[whowas_next];

	s_assert(NULL != client_p);

	if(client_p == NULL)
		return;

	if(who->hashv != -1)
	{
		if(who->online)
			del_whowas_from_clist(&(who->online->whowas), who);
		del_whowas_from_list(&WHOWASHASH[who->hashv], who);
	}
	who->hashv = hash_whowas_name(client_p->name);
	who->logoff = rb_current_time();
	/*
	 * NOTE: strcpy ok here, the sizes in the client struct MUST
	 * match the sizes in the whowas struct
	 */
	rb_strlcpy(who->name, client_p->name, sizeof(who->name));
	strcpy(who->username, client_p->username);
	strcpy(who->hostname, client_p->host);
	strcpy(who->realname, client_p->info);
	strcpy(who->suser, client_p->user->suser);
	if (!EmptyString(client_p->sockhost) && strcmp(client_p->sockhost, "0") && show_ip(NULL, client_p))
		strcpy(who->sockhost, client_p->sockhost);
	else
		who->sockhost[0] = '\0';

	who->servername = scache_get_name(client_p->servptr->serv->nameinfo);

	if(online)
	{
		who->online = client_p;
		add_whowas_to_clist(&(client_p->whowas), who);
	}
	else
		who->online = NULL;
	add_whowas_to_list(&WHOWASHASH[who->hashv], who);
	whowas_next++;
	if(whowas_next == NICKNAMEHISTORYLENGTH)
		whowas_next = 0;
}

void off_history(struct Client *client_p)
{
	struct Whowas *temp, *next;

	for (temp = client_p->whowas; temp; temp = next)
	{
		next = temp->cnext;
		temp->online = NULL;
		del_whowas_from_clist(&(client_p->whowas), temp);
	}
}

struct Client *get_history(const char *nick, time_t timelimit)
{
	struct Whowas *temp;
	int blah;

	timelimit = rb_current_time() - timelimit;
	blah = hash_whowas_name(nick);
	temp = WHOWASHASH[blah];
	for (; temp; temp = temp->next)
	{
		if(irccmp(nick, temp->name))
			continue;
		if(temp->logoff < timelimit)
			continue;
		return temp->online;
	}
	return NULL;
}

void count_whowas_memory(size_t * wwu, size_t * wwum)
{
	*wwu = NICKNAMEHISTORYLENGTH;
	*wwum = NICKNAMEHISTORYLENGTH * sizeof(struct Whowas);
}

void
initwhowas()
{
	int i;

	for (i = 0; i < NICKNAMEHISTORYLENGTH; i++)
	{
		memset((void *) &WHOWAS[i], 0, sizeof(struct Whowas));
		WHOWAS[i].hashv = -1;
	}
	for (i = 0; i < WW_MAX; i++)
		WHOWASHASH[i] = NULL;
}


static void
add_whowas_to_clist(struct Whowas **bucket, struct Whowas *whowas)
{
	whowas->cprev = NULL;
	if((whowas->cnext = *bucket) != NULL)
		whowas->cnext->cprev = whowas;
	*bucket = whowas;
}

static void
del_whowas_from_clist(struct Whowas **bucket, struct Whowas *whowas)
{
	if(whowas->cprev)
		whowas->cprev->cnext = whowas->cnext;
	else
		*bucket = whowas->cnext;
	if(whowas->cnext)
		whowas->cnext->cprev = whowas->cprev;
}

static void
add_whowas_to_list(struct Whowas **bucket, struct Whowas *whowas)
{
	whowas->prev = NULL;
	if((whowas->next = *bucket) != NULL)
		whowas->next->prev = whowas;
	*bucket = whowas;
}

static void
del_whowas_from_list(struct Whowas **bucket, struct Whowas *whowas)
{
	if(whowas->prev)
		whowas->prev->next = whowas->next;
	else
		*bucket = whowas->next;
	if(whowas->next)
		whowas->next->prev = whowas->prev;
}
