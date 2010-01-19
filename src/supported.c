/*
 *  charybdis: A slightly useful ircd.
 *  supported.c: isupport (005) numeric
 *
 * Copyright (C) 2006 Jilles Tjoelker
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *  $Id: supported.c 3568 2007-09-09 18:59:08Z jilles $
 */

/* From the old supported.h which is
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 */
/*
 * - from mirc's versions.txt
 *
 *  mIRC now supports the numeric 005 tokens: CHANTYPES=# and
 *  PREFIX=(ohv)@%+ and can handle a dynamic set of channel and
 *  nick prefixes.
 *
 *  mIRC assumes that @ is supported on all networks, any mode
 *  left of @ is assumed to have at least equal power to @, and
 *  any mode right of @ has less power.
 *
 *  mIRC has internal support for @%+ modes.
 *
 *  $nick() can now handle all mode letters listed in PREFIX.
 *
 *  Also added support for CHANMODES=A,B,C,D token (not currently
 *  supported by any servers), which lists all modes supported
 *  by a channel, where:
 *
 *    A = modes that take a parameter, and add or remove nicks
 *        or addresses to a list, such as +bIe for the ban,
 *        invite, and exception lists.
 *
 *    B = modes that change channel settings, but which take
 *        a parameter when they are set and unset, such as
 *        +k key, and -k key.
 *
 *    C = modes that change channel settings, but which take
 *        a parameter only when they are set, such as +l N,
 *        and -l.
 *
 *    D = modes that change channel settings, such as +imnpst
 *        and take no parameters.
 *
 *  All unknown/unlisted modes are treated as type D.
 */
/* ELIST=[tokens]:
 *
 * M = mask search
 * N = !mask search
 * U = user count search (< >)
 * C = creation time search (C> C<)
 * T = topic search (T> T<)
 */

#include "stdinc.h"
#include "client.h"
#include "common.h"
#include "numeric.h"
#include "ircd.h"
#include "s_conf.h"
#include "supported.h"
#include "chmode.h"

rb_dlink_list isupportlist;

struct isupportitem
{
	const char *name;
	const char *(*func)(const void *);
	const void *param;
	rb_dlink_node node;
};

void
add_isupport(const char *name, const char *(*func)(const void *), const void *param)
{
	struct isupportitem *item;

	item = rb_malloc(sizeof(struct isupportitem));
	item->name = name;
	item->func = func;
	item->param = param;
	rb_dlinkAddTail(item, &item->node, &isupportlist);
}

const void *
change_isupport(const char *name, const char *(*func)(const void *), const void *param)
{
	rb_dlink_node *ptr;
	struct isupportitem *item;
	const void *oldvalue;

	RB_DLINK_FOREACH(ptr, isupportlist.head)
	{
		item = ptr->data;

		if (!strcmp(item->name, name))
		{
			oldvalue = item->param;

			// item->name = name;
			item->func = func;
			item->param = param;

			break;
		}
	}

	return oldvalue;
}

void
delete_isupport(const char *name)
{
	rb_dlink_node *ptr, *next_ptr;
	struct isupportitem *item;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, isupportlist.head)
	{
		item = ptr->data;

		if (!strcmp(item->name, name))
		{
			rb_dlinkDelete(ptr, &isupportlist);
			rb_free(item);
		}
	}
}

/* XXX caching? */
void
show_isupport(struct Client *client_p)
{
	rb_dlink_node *ptr;
	struct isupportitem *item;
	const char *value;
	char buf[512];
	int extra_space;
	unsigned int nchars, nparams;
	int l;

	extra_space = strlen(client_p->name);
	/* UID */
	if (!MyClient(client_p) && extra_space < 9)
		extra_space = 9;
	/* :<me.name> 005 <nick> <params> :are supported by this server */
	/* form_str(RPL_ISUPPORT) is %s :are supported by this server */
	extra_space += strlen(me.name) + 1 + strlen(form_str(RPL_ISUPPORT));

	nchars = extra_space, nparams = 0, buf[0] = '\0';
	RB_DLINK_FOREACH(ptr, isupportlist.head)
	{
		item = ptr->data;
		value = (*item->func)(item->param);
		if (value == NULL)
			continue;
		l = strlen(item->name) + (EmptyString(value) ? 0 : 1 + strlen(value));
		if (nchars + l + (nparams > 0) >= sizeof buf || nparams + 1 > 12)
		{
			sendto_one_numeric(client_p, RPL_ISUPPORT, form_str(RPL_ISUPPORT), buf);
			nchars = extra_space, nparams = 0, buf[0] = '\0';
		}
		if (nparams > 0)
			rb_strlcat(buf, " ", sizeof buf), nchars++;
		rb_strlcat(buf, item->name, sizeof buf);
		if (!EmptyString(value))
		{
			rb_strlcat(buf, "=", sizeof buf);
			rb_strlcat(buf, value, sizeof buf);
		}
		nchars += l;
		nparams++;
	}
	if (nparams > 0)
		sendto_one_numeric(client_p, RPL_ISUPPORT, form_str(RPL_ISUPPORT), buf);
}

const char *
isupport_intptr(const void *ptr)
{
	static char buf[15];
	rb_snprintf(buf, sizeof buf, "%d", *(const int *)ptr);
	return buf;
}

const char *
isupport_boolean(const void *ptr)
{

	return *(const int *)ptr ? "" : NULL;
}

const char *
isupport_string(const void *ptr)
{

	return (const char *)ptr;
}

const char *
isupport_stringptr(const void *ptr)
{
	return *(char * const *)ptr;	
}

static const char *
isupport_chanmodes(const void *ptr)
{
	static char result[80];

	rb_snprintf(result, sizeof result, "%s%sbq,k,%slj,%s",
			ConfigChannel.use_except ? "e" : "",
			ConfigChannel.use_invex ? "I" : "",
			ConfigChannel.use_forward ? "f" : "",
			cflagsbuf);
	return result;
}

static const char *
isupport_chanlimit(const void *ptr)
{
	static char result[30];

	rb_snprintf(result, sizeof result, "&#:%i", ConfigChannel.max_chans_per_user);
	return result;
}

static const char *
isupport_maxlist(const void *ptr)
{
	static char result[30];

	rb_snprintf(result, sizeof result, "bq%s%s:%i",
			ConfigChannel.use_except ? "e" : "",
			ConfigChannel.use_invex ? "I" : "",
			ConfigChannel.max_bans);
	return result;
}

static const char *
isupport_targmax(const void *ptr)
{
	static char result[200];

	rb_snprintf(result, sizeof result, "NAMES:1,LIST:1,KICK:1,WHOIS:1,PRIVMSG:%d,NOTICE:%d,ACCEPT:,MONITOR:",
			ConfigFileEntry.max_targets,
			ConfigFileEntry.max_targets);
	return result;
}

static const char *
isupport_extban(const void *ptr)
{
	const char *p;
	static char result[200];

	p = get_extban_string();
	if (EmptyString(p))
		return NULL;
	rb_snprintf(result, sizeof result, "$,%s", p);
	return result;
}

void
init_isupport(void)
{
	static int maxmodes = MAXMODEPARAMS;
	static int nicklen = NICKLEN-1;
	static int channellen = LOC_CHANNELLEN;
	static int topiclen = TOPICLEN;

	add_isupport("CHANTYPES", isupport_string, "&#");
	add_isupport("EXCEPTS", isupport_boolean, &ConfigChannel.use_except);
	add_isupport("INVEX", isupport_boolean, &ConfigChannel.use_invex);
	add_isupport("CHANMODES", isupport_chanmodes, NULL);
	add_isupport("CHANLIMIT", isupport_chanlimit, NULL);
	add_isupport("PREFIX", isupport_string, "(ov)@+");
	add_isupport("MAXLIST", isupport_maxlist, NULL);
	add_isupport("MODES", isupport_intptr, &maxmodes);
	add_isupport("NETWORK", isupport_stringptr, &ServerInfo.network_name);
	add_isupport("KNOCK", isupport_boolean, &ConfigChannel.use_knock);
	add_isupport("STATUSMSG", isupport_string, "@+");
	add_isupport("CALLERID", isupport_string, "g");
	add_isupport("SAFELIST", isupport_string, "");
	add_isupport("ELIST", isupport_string, "U");
	add_isupport("CASEMAPPING", isupport_string, "rfc1459");
	add_isupport("CHARSET", isupport_string, "ascii");
	add_isupport("NICKLEN", isupport_intptr, &nicklen);
	add_isupport("CHANNELLEN", isupport_intptr, &channellen);
	add_isupport("TOPICLEN", isupport_intptr, &topiclen);
	add_isupport("ETRACE", isupport_string, "");
	add_isupport("CPRIVMSG", isupport_string, "");
	add_isupport("CNOTICE", isupport_string, "");
	add_isupport("DEAF", isupport_string, "D");
	add_isupport("MONITOR", isupport_intptr, &ConfigFileEntry.max_monitor);
	add_isupport("FNC", isupport_string, "");
	add_isupport("TARGMAX", isupport_targmax, NULL);
	add_isupport("EXTBAN", isupport_extban, NULL);
	add_isupport("WHOX", isupport_string, "");
	add_isupport("CLIENTVER", isupport_string, "3.0");
}
