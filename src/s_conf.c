/*
 *  ircd-ratbox: A slightly useful ircd.
 *  s_conf.c: Configuration file functions.
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
 *  $Id: s_conf.c 3550 2007-08-09 06:47:26Z nenolod $
 */

#include "stdinc.h"
#include "ircd_defs.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "newconf.h"
#include "s_serv.h"
#include "s_stats.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "common.h"
#include "hash.h"
#include "match.h"
#include "ircd.h"
#include "listener.h"
#include "hostmask.h"
#include "modules.h"
#include "numeric.h"
#include "logger.h"
#include "send.h"
#include "reject.h"
#include "cache.h"
#include "blacklist.h"
#include "privilege.h"
#include "sslproc.h"
#include "bandbi.h"
#include "operhash.h"

struct config_server_hide ConfigServerHide;

extern int yyparse(void);		/* defined in y.tab.c */
extern char linebuf[];

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned int) 0xffffffff)
#endif

static rb_bh *confitem_heap = NULL;

rb_dlink_list prop_bans;

rb_dlink_list temp_klines[LAST_TEMP_TYPE];
rb_dlink_list temp_dlines[LAST_TEMP_TYPE];
rb_dlink_list service_list;

/* internally defined functions */
static void set_default_conf(void);
static void validate_conf(void);
static void read_conf(FILE *);
static void clear_out_old_conf(void);

static void expire_prop_bans(void *list);
static void expire_temp_kd(void *list);
static void reorganise_temp_kd(void *list);

FILE *conf_fbfile_in;
extern char yytext[];

static int verify_access(struct Client *client_p, const char *username);
static int attach_iline(struct Client *, struct ConfItem *);

void
init_s_conf(void)
{
	confitem_heap = rb_bh_create(sizeof(struct ConfItem), CONFITEM_HEAP_SIZE, "confitem_heap");

	rb_event_addish("expire_prop_bans", expire_prop_bans, &prop_bans, 60);

	rb_event_addish("expire_temp_klines", expire_temp_kd, &temp_klines[TEMP_MIN], 60);
	rb_event_addish("expire_temp_dlines", expire_temp_kd, &temp_dlines[TEMP_MIN], 60);

	rb_event_addish("expire_temp_klines_hour", reorganise_temp_kd,
			&temp_klines[TEMP_HOUR], 3600);
	rb_event_addish("expire_temp_dlines_hour", reorganise_temp_kd,
			&temp_dlines[TEMP_HOUR], 3600);
	rb_event_addish("expire_temp_klines_day", reorganise_temp_kd,
			&temp_klines[TEMP_DAY], 86400);
	rb_event_addish("expire_temp_dlines_day", reorganise_temp_kd,
			&temp_dlines[TEMP_DAY], 86400);
	rb_event_addish("expire_temp_klines_week", reorganise_temp_kd,
			&temp_klines[TEMP_WEEK], 604800);
	rb_event_addish("expire_temp_dlines_week", reorganise_temp_kd,
			&temp_dlines[TEMP_WEEK], 604800);
}

/*
 * make_conf
 *
 * inputs	- none
 * output	- pointer to new conf entry
 * side effects	- none
 */
struct ConfItem *
make_conf()
{
	struct ConfItem *aconf;

	aconf = rb_bh_alloc(confitem_heap);
	aconf->status = CONF_ILLEGAL;
	return (aconf);
}

/*
 * free_conf
 *
 * inputs	- pointer to conf to free
 * output	- none
 * side effects	- crucial password fields are zeroed, conf is freed
 */
void
free_conf(struct ConfItem *aconf)
{
	s_assert(aconf != NULL);
	if(aconf == NULL)
		return;

	/* security.. */
	if(aconf->passwd)
		memset(aconf->passwd, 0, strlen(aconf->passwd));
	if(aconf->spasswd)
		memset(aconf->spasswd, 0, strlen(aconf->spasswd));

	rb_free(aconf->passwd);
	rb_free(aconf->spasswd);
	rb_free(aconf->className);
	rb_free(aconf->user);
	rb_free(aconf->host);

	if(IsConfBan(aconf))
		operhash_delete(aconf->info.oper);
	else
		rb_free(aconf->info.name);

	rb_bh_free(confitem_heap, aconf);
}

/*
 * check_client
 *
 * inputs	- pointer to client
 * output	- 0 = Success
 * 		  NOT_AUTHORISED (-1) = Access denied (no I line match)
 * 		  SOCKET_ERROR   (-2) = Bad socket.
 * 		  I_LINE_FULL    (-3) = I-line is full
 *		  TOO_MANY       (-4) = Too many connections from hostname
 * 		  BANNED_CLIENT  (-5) = K-lined
 * side effects - Ordinary client access check.
 *		  Look for conf lines which have the same
 * 		  status as the flags passed.
 */
int
check_client(struct Client *client_p, struct Client *source_p, const char *username)
{
	int i;

	if((i = verify_access(source_p, username)))
	{
		ilog(L_FUSER, "Access denied: %s[%s]", 
		     source_p->name, source_p->sockhost);
	}
	
	switch (i)
	{
	case SOCKET_ERROR:
		exit_client(client_p, source_p, &me, "Socket Error");
		break;

	case TOO_MANY_LOCAL:
		/* Note that these notices are sent to opers on other
		 * servers also, so even if local opers are allowed to
		 * see the IP, we still cannot send it.
		 */
		sendto_realops_snomask(SNO_FULL, L_NETWIDE,
				"Too many local connections for %s!%s%s@%s",
				source_p->name, IsGotId(source_p) ? "" : "~",
				source_p->username,
				show_ip(NULL, source_p) && !IsIPSpoof(source_p) ? source_p->sockhost : source_p->host);

		ilog(L_FUSER, "Too many local connections from %s!%s%s@%s",
			source_p->name, IsGotId(source_p) ? "" : "~",
			source_p->username, source_p->sockhost);	

		ServerStats.is_ref++;
		exit_client(client_p, source_p, &me, "Too many host connections (local)");
		break;

	case TOO_MANY_GLOBAL:
		sendto_realops_snomask(SNO_FULL, L_NETWIDE,
				"Too many global connections for %s!%s%s@%s",
				source_p->name, IsGotId(source_p) ? "" : "~",
				source_p->username,
				show_ip(NULL, source_p) && !IsIPSpoof(source_p) ? source_p->sockhost : source_p->host);
		ilog(L_FUSER, "Too many global connections from %s!%s%s@%s",
			source_p->name, IsGotId(source_p) ? "" : "~",
			source_p->username, source_p->sockhost);

		ServerStats.is_ref++;
		exit_client(client_p, source_p, &me, "Too many host connections (global)");
		break;

	case TOO_MANY_IDENT:
		sendto_realops_snomask(SNO_FULL, L_NETWIDE,
				"Too many user connections for %s!%s%s@%s",
				source_p->name, IsGotId(source_p) ? "" : "~",
				source_p->username,
				show_ip(NULL, source_p) && !IsIPSpoof(source_p) ? source_p->sockhost : source_p->host);
		ilog(L_FUSER, "Too many user connections from %s!%s%s@%s",
			source_p->name, IsGotId(source_p) ? "" : "~",
			source_p->username, source_p->sockhost);

		ServerStats.is_ref++;
		exit_client(client_p, source_p, &me, "Too many user connections (global)");
		break;

	case I_LINE_FULL:
		sendto_realops_snomask(SNO_FULL, L_NETWIDE,
				"I-line is full for %s!%s%s@%s (%s).",
				source_p->name, IsGotId(source_p) ? "" : "~",
				source_p->username, source_p->host,
				show_ip(NULL, source_p) && !IsIPSpoof(source_p) ? source_p->sockhost : "255.255.255.255");

		ilog(L_FUSER, "Too many connections from %s!%s%s@%s.", 
			source_p->name, IsGotId(source_p) ? "" : "~",
			source_p->username, source_p->sockhost);

		ServerStats.is_ref++;
		exit_client(client_p, source_p, &me,
			    "No more connections allowed in your connection class");
		break;

	case NOT_AUTHORISED:
		{
			int port = -1;
#ifdef RB_IPV6
			if(source_p->localClient->ip.ss_family == AF_INET6)
				port = ntohs(((struct sockaddr_in6 *)&source_p->localClient->listener->addr)->sin6_port);
			else
#endif
				port = ntohs(((struct sockaddr_in *)&source_p->localClient->listener->addr)->sin_port);
			
			ServerStats.is_ref++;
			/* jdc - lists server name & port connections are on */
			/*       a purely cosmetical change */
			/* why ipaddr, and not just source_p->sockhost? --fl */
#if 0
			static char ipaddr[HOSTIPLEN];
			rb_inet_ntop_sock(&source_p->localClient->ip, ipaddr, sizeof(ipaddr));
#endif
			sendto_realops_snomask(SNO_UNAUTH, L_ALL,
					"Unauthorised client connection from "
					"%s!%s%s@%s [%s] on [%s/%u].",
					source_p->name, IsGotId(source_p) ? "" : "~",
					source_p->username, source_p->host,
					source_p->sockhost,
					source_p->localClient->listener->name, port);

			ilog(L_FUSER,
				"Unauthorised client connection from %s!%s%s@%s on [%s/%u].",
				source_p->name, IsGotId(source_p) ? "" : "~",
				source_p->username, source_p->sockhost,
				source_p->localClient->listener->name, port);
			add_reject(client_p, NULL, NULL);
			exit_client(client_p, source_p, &me,
				    "You are not authorised to use this server");
			break;
		}
	case BANNED_CLIENT:
		exit_client(client_p, client_p, &me, "*** Banned ");
		ServerStats.is_ref++;
		break;

	case 0:
	default:
		break;
	}
	return (i);
}

/*
 * verify_access
 *
 * inputs	- pointer to client to verify
 *		- pointer to proposed username
 * output	- 0 if success -'ve if not
 * side effect	- find the first (best) I line to attach.
 */
static int
verify_access(struct Client *client_p, const char *username)
{
	struct ConfItem *aconf;
	char non_ident[USERLEN + 1];

	if(IsGotId(client_p))
	{
		aconf = find_address_conf(client_p->host, client_p->sockhost, 
					client_p->username, client_p->username,
					(struct sockaddr *) &client_p->localClient->ip,
					client_p->localClient->ip.ss_family,
					client_p->localClient->auth_user);
	}
	else
	{
		rb_strlcpy(non_ident, "~", sizeof(non_ident));
		rb_strlcat(non_ident, username, sizeof(non_ident));
		aconf = find_address_conf(client_p->host, client_p->sockhost,
					non_ident, client_p->username,
					(struct sockaddr *) &client_p->localClient->ip,
					client_p->localClient->ip.ss_family,
					client_p->localClient->auth_user);
	}

	if(aconf == NULL)
		return NOT_AUTHORISED;

	if(aconf->status & CONF_CLIENT)
	{
		if(aconf->flags & CONF_FLAGS_REDIR)
		{
			sendto_one_numeric(client_p, RPL_REDIR, form_str(RPL_REDIR),
					aconf->info.name ? aconf->info.name : "", aconf->port);
			return (NOT_AUTHORISED);
		}

		/* Thanks for spoof idea amm */
		if(IsConfDoSpoofIp(aconf))
		{
			char *p;

			/* show_ip() depends on this --fl */
			SetIPSpoof(client_p);

			if(IsConfSpoofNotice(aconf))
			{
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						"%s spoofing: %s as %s",
						client_p->name,
						show_ip(NULL, client_p) ? client_p->host : aconf->info.name,
						aconf->info.name);
			}

			/* user@host spoof */
			if((p = strchr(aconf->info.name, '@')) != NULL)
			{
				char *host = p+1;
				*p = '\0';

				rb_strlcpy(client_p->username, aconf->info.name,
					sizeof(client_p->username));
				rb_strlcpy(client_p->host, host,
					sizeof(client_p->host));
				*p = '@';
			}
			else
				rb_strlcpy(client_p->host, aconf->info.name, sizeof(client_p->host));
		}
		return (attach_iline(client_p, aconf));
	}
	else if(aconf->status & CONF_KILL)
	{
		if(ConfigFileEntry.kline_with_reason)
			sendto_one(client_p,
					form_str(ERR_YOUREBANNEDCREEP),
					me.name, client_p->name,
					get_user_ban_reason(aconf));
		add_reject(client_p, aconf->user, aconf->host);
		return (BANNED_CLIENT);
	}

	return NOT_AUTHORISED;
}


/*
 * add_ip_limit
 * 
 * Returns 1 if successful 0 if not
 *
 * This checks if the user has exceed the limits for their class
 * unless of course they are exempt..
 */

static int
add_ip_limit(struct Client *client_p, struct ConfItem *aconf)
{
	rb_patricia_node_t *pnode;
	int bitlen;

	/* If the limits are 0 don't do anything.. */
	if(ConfCidrAmount(aconf) == 0
	   || (ConfCidrIpv4Bitlen(aconf) == 0 && ConfCidrIpv6Bitlen(aconf) == 0))
		return -1;

	pnode = rb_match_ip(ConfIpLimits(aconf), (struct sockaddr *)&client_p->localClient->ip);

	if(GET_SS_FAMILY(&client_p->localClient->ip) == AF_INET)
		bitlen = ConfCidrIpv4Bitlen(aconf);
	else
		bitlen = ConfCidrIpv6Bitlen(aconf);

	if(pnode == NULL)
		pnode = make_and_lookup_ip(ConfIpLimits(aconf), (struct sockaddr *)&client_p->localClient->ip, bitlen);

	s_assert(pnode != NULL);

	if(pnode != NULL)
	{
		if(((intptr_t)pnode->data) >= ConfCidrAmount(aconf) && !IsConfExemptLimits(aconf))
		{
			/* This should only happen if the limits are set to 0 */
			if((intptr_t)pnode->data == 0)
			{
				rb_patricia_remove(ConfIpLimits(aconf), pnode);
			}
			return (0);
		}

		pnode->data = (void *)(((intptr_t)pnode->data) + 1);
	}
	return 1;
}

static void
remove_ip_limit(struct Client *client_p, struct ConfItem *aconf)
{
	rb_patricia_node_t *pnode;

	/* If the limits are 0 don't do anything.. */
	if(ConfCidrAmount(aconf) == 0
	   || (ConfCidrIpv4Bitlen(aconf) == 0 && ConfCidrIpv6Bitlen(aconf) == 0))
		return;

	pnode = rb_match_ip(ConfIpLimits(aconf), (struct sockaddr *)&client_p->localClient->ip);
	if(pnode == NULL)
		return;

	pnode->data = (void *)(((intptr_t)pnode->data) - 1);
	if(((intptr_t)pnode->data) == 0)
	{
		rb_patricia_remove(ConfIpLimits(aconf), pnode);
	}

}

/*
 * attach_iline
 *
 * inputs	- client pointer
 *		- conf pointer
 * output	-
 * side effects	- do actual attach
 */
static int
attach_iline(struct Client *client_p, struct ConfItem *aconf)
{
	struct Client *target_p;
	rb_dlink_node *ptr;
	int local_count = 0;
	int global_count = 0;
	int ident_count = 0;
	int unidented;

	if(IsConfExemptLimits(aconf))
		return (attach_conf(client_p, aconf));

	unidented = !IsGotId(client_p) && !IsNoTilde(aconf) &&
		(!IsConfDoSpoofIp(aconf) || !strchr(aconf->info.name, '@'));

	/* find_hostname() returns the head of the list to search */
	RB_DLINK_FOREACH(ptr, find_hostname(client_p->host))
	{
		target_p = ptr->data;

		if(irccmp(client_p->host, target_p->orighost) != 0)
			continue;

		if(MyConnect(target_p))
			local_count++;

		global_count++;

		if(unidented)
		{
			if(*target_p->username == '~')
				ident_count++;
		}
		else if(irccmp(target_p->username, client_p->username) == 0)
			ident_count++;

		if(ConfMaxLocal(aconf) && local_count >= ConfMaxLocal(aconf))
			return (TOO_MANY_LOCAL);
		else if(ConfMaxGlobal(aconf) && global_count >= ConfMaxGlobal(aconf))
			return (TOO_MANY_GLOBAL);
		else if(ConfMaxIdent(aconf) && ident_count >= ConfMaxIdent(aconf))
			return (TOO_MANY_IDENT);
	}


	return (attach_conf(client_p, aconf));
}

/*
 * detach_conf
 *
 * inputs	- pointer to client to detach
 * output	- 0 for success, -1 for failure
 * side effects	- Disassociate configuration from the client.
 *		  Also removes a class from the list if marked for deleting.
 */
int
detach_conf(struct Client *client_p)
{
	struct ConfItem *aconf;

	aconf = client_p->localClient->att_conf;

	if(aconf != NULL)
	{
		if(ClassPtr(aconf))
		{
			remove_ip_limit(client_p, aconf);

			if(ConfCurrUsers(aconf) > 0)
				--ConfCurrUsers(aconf);

			if(ConfMaxUsers(aconf) == -1 && ConfCurrUsers(aconf) == 0)
			{
				free_class(ClassPtr(aconf));
				ClassPtr(aconf) = NULL;
			}

		}

		aconf->clients--;
		if(!aconf->clients && IsIllegal(aconf))
			free_conf(aconf);

		client_p->localClient->att_conf = NULL;
		return 0;
	}

	return -1;
}

/*
 * attach_conf
 * 
 * inputs	- client pointer
 * 		- conf pointer
 * output	-
 * side effects - Associate a specific configuration entry to a *local*
 *                client (this is the one which used in accepting the
 *                connection). Note, that this automatically changes the
 *                attachment if there was an old one...
 */
int
attach_conf(struct Client *client_p, struct ConfItem *aconf)
{
	if(IsIllegal(aconf))
		return (NOT_AUTHORISED);

	if(ClassPtr(aconf))
	{
		if(!add_ip_limit(client_p, aconf))
			return (TOO_MANY_LOCAL);
	}

	if((aconf->status & CONF_CLIENT) &&
	   ConfCurrUsers(aconf) >= ConfMaxUsers(aconf) && ConfMaxUsers(aconf) > 0)
	{
		if(!IsConfExemptLimits(aconf))
		{
			return (I_LINE_FULL);
		}
		else
		{
			sendto_one_notice(client_p, ":*** I: line is full, but you have an >I: line!");
		}

	}

	if(client_p->localClient->att_conf != NULL)
		detach_conf(client_p);

	client_p->localClient->att_conf = aconf;

	aconf->clients++;
	ConfCurrUsers(aconf)++;
	return (0);
}

/*
 * rehash
 *
 * Actual REHASH service routine. Called with sig == 0 if it has been called
 * as a result of an operator issuing this command, else assume it has been
 * called as a result of the server receiving a HUP signal.
 */
int
rehash(int sig)
{
	if(sig != 0)
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "Got signal SIGHUP, reloading ircd conf. file");
	}

	restart_resolver();
	/* don't close listeners until we know we can go ahead with the rehash */
	read_conf_files(NO);

	if(ServerInfo.description != NULL)
		rb_strlcpy(me.info, ServerInfo.description, sizeof(me.info));
	else
		rb_strlcpy(me.info, "unknown", sizeof(me.info));

	open_logfiles();
	return (0);
}

void
rehash_bans(int sig)
{
	bandb_rehash_bans();
}

/*
 * set_default_conf()
 *
 * inputs	- NONE
 * output	- NONE
 * side effects	- Set default values here.
 *		  This is called **PRIOR** to parsing the
 *		  configuration file.  If you want to do some validation
 *		  of values later, put them in validate_conf().
 */

#define YES     1
#define NO      0
#define UNSET  -1

static void
set_default_conf(void)
{
	/* ServerInfo.name is not rehashable */
	/* ServerInfo.name = ServerInfo.name; */
	ServerInfo.description = NULL;
	ServerInfo.network_name = rb_strdup(NETWORK_NAME_DEFAULT);
	ServerInfo.network_desc = rb_strdup(NETWORK_DESC_DEFAULT);

	memset(&ServerInfo.ip, 0, sizeof(ServerInfo.ip));
	ServerInfo.specific_ipv4_vhost = 0;
#ifdef RB_IPV6
	memset(&ServerInfo.ip6, 0, sizeof(ServerInfo.ip6));
	ServerInfo.specific_ipv6_vhost = 0;
#endif

	/* Don't reset hub, as that will break lazylinks */
	/* ServerInfo.hub = NO; */
	AdminInfo.name = NULL;
	AdminInfo.email = NULL;
	AdminInfo.description = NULL;

	ConfigFileEntry.default_operstring = rb_strdup("is an IRC operator");
	ConfigFileEntry.default_adminstring = rb_strdup("is a Server Administrator");
	ConfigFileEntry.servicestring = rb_strdup("is a Network Service");

	ConfigFileEntry.default_umodes = UMODE_INVISIBLE;	
	ConfigFileEntry.failed_oper_notice = YES;
	ConfigFileEntry.anti_nick_flood = NO;
	ConfigFileEntry.disable_fake_channels = NO;
	ConfigFileEntry.max_nick_time = 20;
	ConfigFileEntry.max_nick_changes = 5;
	ConfigFileEntry.max_accept = 20;
	ConfigFileEntry.max_monitor = 60;
	ConfigFileEntry.nick_delay = 900;	/* 15 minutes */
	ConfigFileEntry.target_change = YES;
	ConfigFileEntry.anti_spam_exit_message_time = 0;
	ConfigFileEntry.ts_warn_delta = TS_WARN_DELTA_DEFAULT;
	ConfigFileEntry.ts_max_delta = TS_MAX_DELTA_DEFAULT;
	ConfigFileEntry.client_exit = YES;
	ConfigFileEntry.dline_with_reason = YES;
	ConfigFileEntry.kline_with_reason = YES;
	ConfigFileEntry.kline_delay = 0;
	ConfigFileEntry.warn_no_nline = YES;
	ConfigFileEntry.non_redundant_klines = YES;
	ConfigFileEntry.stats_e_disabled = NO;
	ConfigFileEntry.stats_o_oper_only = NO;
	ConfigFileEntry.stats_k_oper_only = 1;	/* masked */
	ConfigFileEntry.stats_i_oper_only = 1;	/* masked */
	ConfigFileEntry.stats_P_oper_only = NO;
	ConfigFileEntry.stats_c_oper_only = NO;
	ConfigFileEntry.stats_y_oper_only = NO;
	ConfigFileEntry.stats_h_oper_only = NO;
	ConfigFileEntry.map_oper_only = YES;
	ConfigFileEntry.operspy_admin_only = NO;
	ConfigFileEntry.pace_wait = 10;
	ConfigFileEntry.caller_id_wait = 60;
	ConfigFileEntry.pace_wait_simple = 1;
	ConfigFileEntry.short_motd = NO;
	ConfigFileEntry.no_oper_flood = NO;
	ConfigFileEntry.fname_userlog = NULL;
	ConfigFileEntry.fname_fuserlog = NULL;
	ConfigFileEntry.fname_operlog = NULL;
	ConfigFileEntry.fname_foperlog = NULL;
	ConfigFileEntry.fname_serverlog = NULL;
	ConfigFileEntry.fname_klinelog = NULL;
	ConfigFileEntry.fname_operspylog = NULL;
	ConfigFileEntry.fname_ioerrorlog = NULL;
	ConfigFileEntry.use_egd = NO;
	ConfigFileEntry.hide_spoof_ips = YES;
	ConfigFileEntry.hide_error_messages = 1;
	ConfigFileEntry.dots_in_ident = 0;
	ConfigFileEntry.max_targets = MAX_TARGETS_DEFAULT;
	ConfigFileEntry.egdpool_path = NULL;
	ConfigFileEntry.use_whois_actually = YES;
	ConfigFileEntry.burst_away = NO;
	ConfigFileEntry.collision_fnc = YES;
	ConfigFileEntry.global_snotices = YES;
	ConfigFileEntry.operspy_dont_care_user_info = NO;
	ConfigFileEntry.use_propagated_bans = YES;

#ifdef HAVE_LIBZ
	ConfigFileEntry.compression_level = 4;
#endif

	ConfigFileEntry.oper_umodes = UMODE_LOCOPS | UMODE_SERVNOTICE |
		UMODE_OPERWALL | UMODE_WALLOP;
	ConfigFileEntry.oper_only_umodes = UMODE_SERVNOTICE;
	ConfigFileEntry.oper_snomask = SNO_GENERAL;

	ConfigChannel.use_except = YES;
	ConfigChannel.use_invex = YES;
	ConfigChannel.use_knock = YES;
	ConfigChannel.use_forward = YES;
	ConfigChannel.knock_delay = 300;
	ConfigChannel.knock_delay_channel = 60;
	ConfigChannel.max_chans_per_user = 15;
	ConfigChannel.max_bans = 25;
	ConfigChannel.max_bans_large = 500;
	ConfigChannel.only_ascii_channels = NO;
	ConfigChannel.burst_topicwho = NO;
	ConfigChannel.kick_on_split_riding = NO;

	ConfigChannel.default_split_user_count = 15000;
	ConfigChannel.default_split_server_count = 10;
	ConfigChannel.no_join_on_split = NO;
	ConfigChannel.no_create_on_split = YES;
	ConfigChannel.resv_forcepart = YES;
	ConfigChannel.channel_target_change = YES;

	ConfigServerHide.flatten_links = 0;
	ConfigServerHide.links_delay = 300;
	ConfigServerHide.hidden = 0;
	ConfigServerHide.disable_hidden = 0;

	ConfigFileEntry.min_nonwildcard = 4;
	ConfigFileEntry.min_nonwildcard_simple = 3;
	ConfigFileEntry.default_floodcount = 8;
	ConfigFileEntry.default_ident_timeout = 5;
	ConfigFileEntry.client_flood = CLIENT_FLOOD_DEFAULT;
	ConfigFileEntry.tkline_expire_notices = 0;

        ConfigFileEntry.reject_after_count = 5;
	ConfigFileEntry.reject_ban_time = 300;  
	ConfigFileEntry.reject_duration = 120;
	ConfigFileEntry.throttle_count = 4;
	ConfigFileEntry.throttle_duration = 60;

	ServerInfo.default_max_clients = MAXCONNECTIONS;

	if (!alias_dict)
		alias_dict = irc_dictionary_create(strcasecmp);
}

#undef YES
#undef NO

/*
 * read_conf() 
 *
 *
 * inputs       - file descriptor pointing to config file to use
 * output       - None
 * side effects	- Read configuration file.
 */
static void
read_conf(FILE * file)
{
	lineno = 0;

	set_default_conf();	/* Set default values prior to conf parsing */
	yyparse();		/* Load the values from the conf */
	validate_conf();	/* Check to make sure some values are still okay. */
	/* Some global values are also loaded here. */
	check_class();		/* Make sure classes are valid */
	privilegeset_delete_all_illegal();
}

static void
validate_conf(void)
{
	if(ConfigFileEntry.ts_warn_delta < TS_WARN_DELTA_MIN)
		ConfigFileEntry.ts_warn_delta = TS_WARN_DELTA_DEFAULT;

	if(ConfigFileEntry.ts_max_delta < TS_MAX_DELTA_MIN)
		ConfigFileEntry.ts_max_delta = TS_MAX_DELTA_DEFAULT;

	if(ServerInfo.network_name == NULL)
		ServerInfo.network_name = rb_strdup(NETWORK_NAME_DEFAULT);

	if(ServerInfo.network_desc == NULL)
		ServerInfo.network_desc = rb_strdup(NETWORK_DESC_DEFAULT);

	if(ServerInfo.ssld_count < 1)
		ServerInfo.ssld_count = 1;

	if(!rb_setup_ssl_server(ServerInfo.ssl_cert, ServerInfo.ssl_private_key, ServerInfo.ssl_dh_params))
	{
		ilog(L_MAIN, "WARNING: Unable to setup SSL.");
		ssl_ok = 0;
	} else {
		ssl_ok = 1;
		send_new_ssl_certs(ServerInfo.ssl_cert, ServerInfo.ssl_private_key, ServerInfo.ssl_dh_params);
	}

	if(ServerInfo.ssld_count > get_ssld_count())
	{
		int start = ServerInfo.ssld_count - get_ssld_count();
		/* start up additional ssld if needed */
		start_ssldaemon(start, ServerInfo.ssl_cert, ServerInfo.ssl_private_key, ServerInfo.ssl_dh_params);
				
	}

	if((ConfigFileEntry.client_flood < CLIENT_FLOOD_MIN) ||
	   (ConfigFileEntry.client_flood > CLIENT_FLOOD_MAX))
		ConfigFileEntry.client_flood = CLIENT_FLOOD_MAX;

	if(!split_users || !split_servers ||
	   (!ConfigChannel.no_create_on_split && !ConfigChannel.no_join_on_split))
	{
		rb_event_delete(check_splitmode_ev);
		check_splitmode_ev = NULL;
		splitmode = 0;
		splitchecking = 0;
	}
}

/* add_temp_kline()
 *
 * inputs        - pointer to struct ConfItem
 * output        - none
 * Side effects  - links in given struct ConfItem into 
 *                 temporary kline link list
 */
void
add_temp_kline(struct ConfItem *aconf)
{
	if(aconf->hold >= rb_current_time() + (10080 * 60))
	{
		rb_dlinkAddAlloc(aconf, &temp_klines[TEMP_WEEK]);
		aconf->port = TEMP_WEEK;
	}
	else if(aconf->hold >= rb_current_time() + (1440 * 60))
	{
		rb_dlinkAddAlloc(aconf, &temp_klines[TEMP_DAY]);
		aconf->port = TEMP_DAY;
	}
	else if(aconf->hold >= rb_current_time() + (60 * 60))
	{
		rb_dlinkAddAlloc(aconf, &temp_klines[TEMP_HOUR]);
		aconf->port = TEMP_HOUR;
	}
	else
	{
		rb_dlinkAddAlloc(aconf, &temp_klines[TEMP_MIN]);
		aconf->port = TEMP_MIN;
	}

	aconf->flags |= CONF_FLAGS_TEMPORARY;
	add_conf_by_address(aconf->host, CONF_KILL, aconf->user, NULL, aconf);
}

/* add_temp_dline()
 *
 * input	- pointer to struct ConfItem
 * output	- none
 * side effects - added to tdline link list and address hash
 */
void
add_temp_dline(struct ConfItem *aconf)
{
	if(aconf->hold >= rb_current_time() + (10080 * 60))
	{
		rb_dlinkAddAlloc(aconf, &temp_dlines[TEMP_WEEK]);
		aconf->port = TEMP_WEEK;
	}
	else if(aconf->hold >= rb_current_time() + (1440 * 60))
	{
		rb_dlinkAddAlloc(aconf, &temp_dlines[TEMP_DAY]);
		aconf->port = TEMP_DAY;
	}
	else if(aconf->hold >= rb_current_time() + (60 * 60))
	{
		rb_dlinkAddAlloc(aconf, &temp_dlines[TEMP_HOUR]);
		aconf->port = TEMP_HOUR;
	}
	else
	{
		rb_dlinkAddAlloc(aconf, &temp_dlines[TEMP_MIN]);
		aconf->port = TEMP_MIN;
	}

	aconf->flags |= CONF_FLAGS_TEMPORARY;
	add_conf_by_address(aconf->host, CONF_DLINE, aconf->user, NULL, aconf);
}

/* valid_wild_card()
 * 
 * input        - user buffer, host buffer
 * output       - 0 if invalid, 1 if valid
 * side effects -
 */
int
valid_wild_card(const char *luser, const char *lhost)
{
	const char *p;
	char tmpch;
	int nonwild = 0;
	int bitlen;

	/* user has no wildcards, always accept -- jilles */
	if(!strchr(luser, '?') && !strchr(luser, '*'))
		return 1;

	/* check there are enough non wildcard chars */
	p = luser;
	while((tmpch = *p++))
	{
		if(!IsKWildChar(tmpch))
		{
			/* found enough chars, return */
			if(++nonwild >= ConfigFileEntry.min_nonwildcard)
				return 1;
		}
	}

	/* try host, as user didnt contain enough */
	/* special case for cidr masks -- jilles */
	if((p = strrchr(lhost, '/')) != NULL && IsDigit(p[1]))
	{
		bitlen = atoi(p + 1);
		/* much like non-cidr for ipv6, rather arbitrary for ipv4 */
		if(bitlen > 0
		   && bitlen >=
		   (strchr(lhost, ':') ? 4 * (ConfigFileEntry.min_nonwildcard - nonwild) : 6 -
		    2 * nonwild))
			return 1;
	}
	else
	{
		p = lhost;
		while((tmpch = *p++))
		{
			if(!IsKWildChar(tmpch))
				if(++nonwild >= ConfigFileEntry.min_nonwildcard)
					return 1;
		}
	}

	return 0;
}

rb_dlink_node *
find_prop_ban(unsigned int status, const char *user, const char *host)
{
	rb_dlink_node *ptr;
	struct ConfItem *aconf;

	RB_DLINK_FOREACH(ptr, prop_bans.head)
	{
		aconf = ptr->data;

		if((aconf->status & ~CONF_ILLEGAL) == status &&
				(!user || !aconf->user ||
				 !irccmp(aconf->user, user)) &&
				!irccmp(aconf->host, host))
			return ptr;
	}
	return NULL;
}

void
deactivate_conf(struct ConfItem *aconf, rb_dlink_node *ptr)
{
	int i;

	s_assert(ptr->data == aconf);

	switch (aconf->status)
	{
		case CONF_KILL:
			if (aconf->lifetime == 0 &&
					aconf->flags & CONF_FLAGS_TEMPORARY)
				for (i = 0; i < LAST_TEMP_TYPE; i++)
					rb_dlinkFindDestroy(aconf, &temp_klines[i]);
			/* Make sure delete_one_address_conf() does not
			 * free the aconf.
			 */
			aconf->clients++;
			delete_one_address_conf(aconf->host, aconf);
			aconf->clients--;
			break;
		case CONF_DLINE:
			if (aconf->lifetime == 0 &&
					aconf->flags & CONF_FLAGS_TEMPORARY)
				for (i = 0; i < LAST_TEMP_TYPE; i++)
					rb_dlinkFindDestroy(aconf, &temp_dlines[i]);
			aconf->clients++;
			delete_one_address_conf(aconf->host, aconf);
			aconf->clients--;
			break;
		case CONF_XLINE:
			rb_dlinkFindDestroy(aconf, &xline_conf_list);
			break;
		case CONF_RESV_NICK:
			rb_dlinkFindDestroy(aconf, &resv_conf_list);
			break;
		case CONF_RESV_CHANNEL:
			del_from_resv_hash(aconf->host, aconf);
			break;
	}
	if (aconf->lifetime != 0 && rb_current_time() < aconf->lifetime)
		aconf->status |= CONF_ILLEGAL;
	else
	{
		if (aconf->lifetime != 0)
			rb_dlinkDestroy(ptr, &prop_bans);
		free_conf(aconf);
	}
}

/* Given a new ban ConfItem, look for any matching ban, update the lifetime
 * from it and delete it.
 */
void
replace_old_ban(struct ConfItem *aconf)
{
	rb_dlink_node *ptr;
	struct ConfItem *oldconf;

	ptr = find_prop_ban(aconf->status, aconf->user, aconf->host);
	if(ptr != NULL)
	{
		oldconf = ptr->data;
		/* Remember at least as long as the old one. */
		if(oldconf->lifetime > aconf->lifetime)
			aconf->lifetime = oldconf->lifetime;
		/* Force creation time to increase. */
		if(oldconf->created >= aconf->created)
			aconf->created = oldconf->created + 1;
		/* Leave at least one second of validity. */
		if(aconf->hold <= aconf->created)
			aconf->hold = aconf->created + 1;
		if(aconf->lifetime < aconf->hold)
			aconf->lifetime = aconf->hold;
		/* Tell deactivate_conf() to destroy it. */
		oldconf->lifetime = rb_current_time();
		deactivate_conf(oldconf, ptr);
	}
}

static void
expire_prop_bans(void *list)
{
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	struct ConfItem *aconf;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, ((rb_dlink_list *) list)->head)
	{
		aconf = ptr->data;

		if(aconf->lifetime <= rb_current_time() ||
				(aconf->hold <= rb_current_time() &&
				 !(aconf->status & CONF_ILLEGAL)))
		{
			/* Alert opers that a TKline expired - Hwy */
			/* XXX show what type of ban it is */
			if(ConfigFileEntry.tkline_expire_notices &&
					!(aconf->status & CONF_ILLEGAL))
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						     "Propagated ban for [%s%s%s] expired",
						     aconf->user ? aconf->user : "",
						     aconf->user ? "@" : "",
						     aconf->host ? aconf->host : "*");

			/* will destroy or mark illegal */
			deactivate_conf(aconf, ptr);
		}
	}
}

/* expire_tkline()
 *
 * inputs       - list pointer
 * 		- type
 * output       - NONE
 * side effects - expire tklines and moves them between lists
 */
static void
expire_temp_kd(void *list)
{
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	struct ConfItem *aconf;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, ((rb_dlink_list *) list)->head)
	{
		aconf = ptr->data;

		if(aconf->hold <= rb_current_time())
		{
			/* Alert opers that a TKline expired - Hwy */
			if(ConfigFileEntry.tkline_expire_notices)
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						     "Temporary K-line for [%s@%s] expired",
						     (aconf->user) ? aconf->
						     user : "*", (aconf->host) ? aconf->host : "*");

			delete_one_address_conf(aconf->host, aconf);
			rb_dlinkDestroy(ptr, list);
		}
	}
}

static void
reorganise_temp_kd(void *list)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr, *next_ptr;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, ((rb_dlink_list *) list)->head)
	{
		aconf = ptr->data;

		if(aconf->hold < (rb_current_time() + (60 * 60)))
		{
			rb_dlinkMoveNode(ptr, list, (aconf->status == CONF_KILL) ? 
					&temp_klines[TEMP_MIN] : &temp_dlines[TEMP_MIN]);
			aconf->port = TEMP_MIN;
		}
		else if(aconf->port > TEMP_HOUR)
		{
			if(aconf->hold < (rb_current_time() + (1440 * 60)))
			{
				rb_dlinkMoveNode(ptr, list, (aconf->status == CONF_KILL) ? 
						&temp_klines[TEMP_HOUR] : &temp_dlines[TEMP_HOUR]);
				aconf->port = TEMP_HOUR;
			}
			else if(aconf->port > TEMP_DAY && 
				(aconf->hold < (rb_current_time() + (10080 * 60))))
			{
				rb_dlinkMoveNode(ptr, list, (aconf->status == CONF_KILL) ? 
						&temp_klines[TEMP_DAY] : &temp_dlines[TEMP_DAY]);
				aconf->port = TEMP_DAY;
			}
		}
	}
}


/* const char* get_oper_name(struct Client *client_p)
 * Input: A client to find the active oper{} name for.
 * Output: The nick!user@host{oper} of the oper.
 *         "oper" is server name for remote opers
 * Side effects: None.
 */
char *
get_oper_name(struct Client *client_p)
{
	/* +5 for !,@,{,} and null */
	static char buffer[NICKLEN + USERLEN + HOSTLEN + HOSTLEN + 5];

	if(MyOper(client_p))
	{
		rb_snprintf(buffer, sizeof(buffer), "%s!%s@%s{%s}",
				client_p->name, client_p->username,
				client_p->host, client_p->localClient->opername);
		return buffer;
	}

	rb_snprintf(buffer, sizeof(buffer), "%s!%s@%s{%s}",
		   client_p->name, client_p->username, 
		   client_p->host, client_p->servptr->name);
	return buffer;
}

/*
 * get_printable_conf
 *
 * inputs        - struct ConfItem
 *
 * output         - name 
 *                - host
 *                - pass
 *                - user
 *                - port
 *
 * side effects        -
 * Examine the struct struct ConfItem, setting the values
 * of name, host, pass, user to values either
 * in aconf, or "<NULL>" port is set to aconf->port in all cases.
 */
void
get_printable_conf(struct ConfItem *aconf, char **name, char **host,
		   char **pass, char **user, int *port, char **classname)
{
	static char null[] = "<NULL>";
	static char zero[] = "default";

	*name = EmptyString(aconf->info.name) ? null : aconf->info.name;
	*host = EmptyString(aconf->host) ? null : aconf->host;
	*pass = EmptyString(aconf->passwd) ? null : aconf->passwd;
	*user = EmptyString(aconf->user) ? null : aconf->user;
	*classname = EmptyString(aconf->className) ? zero : aconf->className;
	*port = (int) aconf->port;
}

char *
get_user_ban_reason(struct ConfItem *aconf)
{
	static char reasonbuf[BUFSIZE];

	if (aconf->flags & CONF_FLAGS_TEMPORARY &&
			(aconf->status == CONF_KILL || aconf->status == CONF_DLINE))
		rb_snprintf(reasonbuf, sizeof reasonbuf,
				"Temporary %c-line %d min. - ",
				aconf->status == CONF_DLINE ? 'D' : 'K',
				(int)((aconf->hold - aconf->created) / 60));
	else
		reasonbuf[0] = '\0';
	if (aconf->passwd)
		rb_strlcat(reasonbuf, aconf->passwd, sizeof reasonbuf);
	else
		rb_strlcat(reasonbuf, "No Reason", sizeof reasonbuf);
	if (aconf->created)
	{
		rb_strlcat(reasonbuf, " (", sizeof reasonbuf);
		rb_strlcat(reasonbuf, smalldate(aconf->created),
				sizeof reasonbuf);
		rb_strlcat(reasonbuf, ")", sizeof reasonbuf);
	}
	return reasonbuf;
}

void
get_printable_kline(struct Client *source_p, struct ConfItem *aconf, 
		    char **host, char **reason,
		    char **user, char **oper_reason)
{
	static char null[] = "<NULL>";
	static char operreasonbuf[BUFSIZE];

	*host = EmptyString(aconf->host) ? null : aconf->host;
	*user = EmptyString(aconf->user) ? null : aconf->user;
	*reason = get_user_ban_reason(aconf);

	if(!IsOper(source_p))
		*oper_reason = NULL;
	else
	{
		rb_snprintf(operreasonbuf, sizeof operreasonbuf, "%s%s(%s)",
				EmptyString(aconf->spasswd) ? "" : aconf->spasswd,
				EmptyString(aconf->spasswd) ? "" : " ",
				aconf->info.oper);
		*oper_reason = operreasonbuf;
	}
}

/*
 * read_conf_files
 *
 * inputs       - cold start YES or NO
 * output       - none
 * side effects - read all conf files needed, ircd.conf kline.conf etc.
 */
void
read_conf_files(int cold)
{
	const char *filename;

	conf_fbfile_in = NULL;

	filename = ConfigFileEntry.configfile;

	/* We need to know the initial filename for the yyerror() to report
	   FIXME: The full path is in conffilenamebuf first time since we
	   dont know anything else

	   - Gozem 2002-07-21 
	 */
	rb_strlcpy(conffilebuf, filename, sizeof(conffilebuf));

	if((conf_fbfile_in = fopen(filename, "r")) == NULL)
	{
		if(cold)
		{
			ilog(L_MAIN, "Failed in reading configuration file %s", filename);
			exit(-1);
		}
		else
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					     "Can't open file '%s' - aborting rehash!", filename);
			return;
		}
	}

	if(!cold)
	{
		clear_out_old_conf();
	}

	read_conf(conf_fbfile_in);
	fclose(conf_fbfile_in);
}

/*
 * free an alias{} entry.
 */
static void
free_alias_cb(struct DictionaryElement *ptr, void *unused)
{
	struct alias_entry *aptr = ptr->data;

	rb_free(aptr->name);
	rb_free(aptr->target);
	rb_free(aptr);
}

/*
 * clear_out_old_conf
 *
 * inputs       - none
 * output       - none
 * side effects - Clear out the old configuration
 */
static void
clear_out_old_conf(void)
{
	struct Class *cltmp;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;

	/*
	 * don't delete the class table, rather mark all entries
	 * for deletion. The table is cleaned up by check_class. - avalon
	 */
	RB_DLINK_FOREACH(ptr, class_list.head)
	{
		cltmp = ptr->data;
		MaxUsers(cltmp) = -1;
	}

	clear_out_address_conf();
	clear_s_newconf();

	/* clean out module paths */
#ifndef STATIC_MODULES
	mod_clear_paths();
	mod_add_path(MODULE_DIR);
	mod_add_path(MODULE_DIR  "/autoload");
#endif

	/* clean out ServerInfo */
	rb_free(ServerInfo.description);
	ServerInfo.description = NULL;
	rb_free(ServerInfo.network_name);
	ServerInfo.network_name = NULL;
	rb_free(ServerInfo.network_desc);
	ServerInfo.network_desc = NULL;

	ServerInfo.ssld_count = 1;

	/* clean out AdminInfo */
	rb_free(AdminInfo.name);
	AdminInfo.name = NULL;
	rb_free(AdminInfo.email);
	AdminInfo.email = NULL;
	rb_free(AdminInfo.description);
	AdminInfo.description = NULL;

	/* operator{} and class{} blocks are freed above */
	/* clean out listeners */
	close_listeners();

	/* auth{}, quarantine{}, shared{}, connect{}, kill{}, deny{}, exempt{}
	 * and gecos{} blocks are freed above too
	 */

	/* clean out general */
	rb_free(ConfigFileEntry.kline_reason);
	ConfigFileEntry.kline_reason = NULL;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, service_list.head)
	{
		rb_free(ptr->data);
		rb_dlinkDestroy(ptr, &service_list);
	}

	/* remove any aliases... -- nenolod */
	if (alias_dict != NULL)
	{
		irc_dictionary_destroy(alias_dict, free_alias_cb, NULL);
		alias_dict = NULL;
	}

	destroy_blacklists();

	privilegeset_mark_all_illegal();

	/* OK, that should be everything... */
}


/*
 * conf_add_class_to_conf
 * inputs       - pointer to config item
 * output       - NONE
 * side effects - Add a class pointer to a conf 
 */

void
conf_add_class_to_conf(struct ConfItem *aconf)
{
	if(aconf->className == NULL)
	{
		aconf->className = rb_strdup("default");
		ClassPtr(aconf) = default_class;
		return;
	}

	ClassPtr(aconf) = find_class(aconf->className);

	if(ClassPtr(aconf) == default_class)
	{
		if(aconf->status == CONF_CLIENT)
		{
			conf_report_error(
					     "Using default class for missing class \"%s\" in auth{} for %s@%s",
					     aconf->className, aconf->user, aconf->host);
		}

		rb_free(aconf->className);
		aconf->className = rb_strdup("default");
		return;
	}

	if(ConfMaxUsers(aconf) < 0)
	{
		ClassPtr(aconf) = default_class;
		rb_free(aconf->className);
		aconf->className = rb_strdup("default");
		return;
	}
}

/*
 * conf_add_d_conf
 * inputs       - pointer to config item
 * output       - NONE
 * side effects - Add a d/D line
 */
void
conf_add_d_conf(struct ConfItem *aconf)
{
	if(aconf->host == NULL)
		return;

	aconf->user = NULL;

	/* XXX - Should 'd' ever be in the old conf? For new conf we don't
	 *       need this anyway, so I will disable it for now... -A1kmm
	 */

	if(parse_netmask(aconf->host, NULL, NULL) == HM_HOST)
	{
		ilog(L_MAIN, "Invalid Dline %s ignored", aconf->host);
		free_conf(aconf);
	}
	else
	{
		add_conf_by_address(aconf->host, CONF_DLINE, NULL, NULL, aconf);
	}
}

static char *
strip_tabs(char *dest, const char *src, size_t len)
{
	char *d = dest;

	if(dest == NULL || src == NULL)
		return NULL;

	rb_strlcpy(dest, src, len);

	while(*d)
	{
		if(*d == '\t')
			*d = ' ';
		d++;
	}
	return dest;
}

/*
 * yyerror
 *
 * inputs	- message from parser
 * output	- none
 * side effects	- message to opers and log file entry is made
 */
void
yyerror(const char *msg)
{
	char newlinebuf[BUFSIZE];

	strip_tabs(newlinebuf, linebuf, strlen(linebuf));

	ierror("\"%s\", line %d: %s at '%s'", conffilebuf, lineno + 1, msg, newlinebuf);
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "\"%s\", line %d: %s at '%s'",
			     conffilebuf, lineno + 1, msg, newlinebuf);

}

int
conf_fgets(char *lbuf, int max_size, FILE * fb)
{
	char *buff;

	if((buff = fgets(lbuf, max_size, fb)) == NULL)
		return (0);

	return (strlen(lbuf));
}

int
conf_yy_fatal_error(const char *msg)
{
	return (0);
}
