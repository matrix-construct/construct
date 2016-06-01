/*
 *  authd.c: An interface to authd.
 *  (based somewhat on ircd-ratbox dns.c)
 *
 *  Copyright (C) 2005 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2005-2012 ircd-ratbox development team
 *  Copyright (C) 2016 William Pitcock <nenolod@dereferenced.org>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "stdinc.h"
#include "rb_lib.h"
#include "client.h"
#include "ircd_defs.h"
#include "parse.h"
#include "authproc.h"
#include "match.h"
#include "logger.h"
#include "s_conf.h"
#include "s_stats.h"
#include "client.h"
#include "packet.h"
#include "hash.h"
#include "send.h"
#include "numeric.h"
#include "msg.h"
#include "dns.h"

typedef void (*authd_cb_t)(int, char **);

struct authd_cb
{
	authd_cb_t fn;
	int min_parc;
};

static int start_authd(void);
static void parse_authd_reply(rb_helper * helper);
static void restart_authd_cb(rb_helper * helper);
static EVH timeout_dead_authd_clients;

static void cmd_accept_client(int parc, char **parv);
static void cmd_reject_client(int parc, char **parv);
static void cmd_dns_result(int parc, char **parv);
static void cmd_notice_client(int parc, char **parv);
static void cmd_oper_warn(int parc, char **parv);
static void cmd_stats_results(int parc, char **parv);

rb_helper *authd_helper;
static char *authd_path;

uint32_t cid;
static rb_dictionary *cid_clients;
static struct ev_entry *timeout_ev;

rb_dictionary *bl_stats;

rb_dlink_list opm_list;
struct OPMListener opm_listeners[LISTEN_LAST];

static struct authd_cb authd_cmd_tab[256] =
{
	['A'] = { cmd_accept_client,	4 },
	['E'] = { cmd_dns_result,	5 },
	['N'] = { cmd_notice_client,	3 },
	['R'] = { cmd_reject_client,	7 },
	['W'] = { cmd_oper_warn,	3 },
	['X'] = { cmd_stats_results,	3 },
	['Y'] = { cmd_stats_results,	3 },
	['Z'] = { cmd_stats_results,	3 },
};

static int
start_authd(void)
{
	char fullpath[PATH_MAX + 1];
#ifdef _WIN32
	const char *suffix = ".exe";
#else
	const char *suffix = "";
#endif
	if(authd_path == NULL)
	{
		snprintf(fullpath, sizeof(fullpath), "%s%cauthd%s", ircd_paths[IRCD_PATH_LIBEXEC], RB_PATH_SEPARATOR, suffix);

		if(access(fullpath, X_OK) == -1)
		{
			snprintf(fullpath, sizeof(fullpath), "%s%cbin%cauthd%s",
				 ConfigFileEntry.dpath, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR, suffix);
			if(access(fullpath, X_OK) == -1)
			{
				ierror("Unable to execute authd in %s or %s/bin",
					ircd_paths[IRCD_PATH_LIBEXEC], ConfigFileEntry.dpath);
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						       "Unable to execute authd in %s or %s/bin",
						       ircd_paths[IRCD_PATH_LIBEXEC], ConfigFileEntry.dpath);
				return 1;
			}

		}

		authd_path = rb_strdup(fullpath);
	}

	if(cid_clients == NULL)
		cid_clients = rb_dictionary_create("authd cid to uid mapping", rb_uint32cmp);

	if(timeout_ev == NULL)
		timeout_ev = rb_event_addish("timeout_dead_authd_clients", timeout_dead_authd_clients, NULL, 1);

	authd_helper = rb_helper_start("authd", authd_path, parse_authd_reply, restart_authd_cb);

	if(authd_helper == NULL)
	{
		ierror("Unable to start authd helper: %s", strerror(errno));
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "Unable to start authd helper: %s", strerror(errno));
		return 1;
	}

	ilog(L_MAIN, "authd helper started");
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "authd helper started");
	rb_helper_run(authd_helper);
	return 0;
}

static inline uint32_t
str_to_cid(const char *str)
{
	long lcid = strtol(str, NULL, 16);

	if(lcid > UINT32_MAX || lcid <= 0)
	{
		iwarn("authd sent us back a bad client ID: %lx", lcid);
		restart_authd();
		return 0;
	}

	return (uint32_t)lcid;
}

static inline struct Client *
cid_to_client(uint32_t ncid, bool del)
{
	struct Client *client_p;

	if(del)
		client_p = rb_dictionary_delete(cid_clients, RB_UINT_TO_POINTER(ncid));
	else
		client_p = rb_dictionary_retrieve(cid_clients, RB_UINT_TO_POINTER(ncid));

	/* If the client's not found, that's okay, it may have already gone away.
	 * --Elizafox */

	return client_p;
}

static inline struct Client *
str_cid_to_client(const char *str, bool del)
{
	uint32_t ncid = str_to_cid(str);

	if(ncid == 0)
		return NULL;

	return cid_to_client(ncid, del);
}

static void
cmd_accept_client(int parc, char **parv)
{
	struct Client *client_p;

	/* cid to uid (retrieve and delete) */
	if((client_p = str_cid_to_client(parv[1], true)) == NULL)
		return;

	authd_accept_client(client_p, parv[2], parv[3]);
}

static void
cmd_dns_result(int parc, char **parv)
{
	dns_results_callback(parv[1], parv[2], parv[3], parv[4]);
}

static void
cmd_notice_client(int parc, char **parv)
{
	struct Client *client_p;

	if((client_p = str_cid_to_client(parv[1], false)) == NULL)
		return;

	sendto_one_notice(client_p, ":%s", parv[2]);
}

static void
cmd_reject_client(int parc, char **parv)
{
	struct Client *client_p;

	/* cid to uid (retrieve and delete) */
	if((client_p = str_cid_to_client(parv[1], true)) == NULL)
		return;

	authd_reject_client(client_p, parv[3], parv[4], toupper(*parv[2]), parv[5], parv[6]);
}

static void
cmd_oper_warn(int parc, char **parv)
{
	switch(*parv[1])
	{
	case 'D':	/* Debug */
		sendto_realops_snomask(SNO_DEBUG, L_ALL, "authd debug: %s", parv[2]);
		idebug("authd: %s", parv[2]);
		break;
	case 'I':	/* Info */
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "authd info: %s", parv[2]);
		inotice("authd: %s", parv[2]);
		break;
	case 'W':	/* Warning */
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "authd WARNING: %s", parv[2]);
		iwarn("authd: %s", parv[2]);
		break;
	case 'C':	/* Critical (error) */
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "authd CRITICAL: %s", parv[2]);
		ierror("authd: %s", parv[2]);
		break;
	default:	/* idk */
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "authd sent us an unknown oper notice type (%s): %s", parv[1], parv[2]);
		ilog(L_MAIN, "authd unknown oper notice type (%s): %s", parv[1], parv[2]);
		break;
	}
}

static void
cmd_stats_results(int parc, char **parv)
{
	/* Select by type */
	switch(*parv[2])
	{
	case 'D':
		/* parv[0] conveys status */
		if(parc < 4)
		{
			iwarn("authd sent a result with wrong number of arguments: got %d", parc);
			restart_authd();
			return;
		}
		dns_stats_results_callback(parv[1], parv[0], parc - 3, (const char **)&parv[3]);
		break;
	default:
		break;
	}
}

static void
parse_authd_reply(rb_helper * helper)
{
	ssize_t len;
	int parc;
	char buf[READBUF_SIZE];
	char *parv[MAXPARA + 1];

	while((len = rb_helper_read(helper, buf, sizeof(buf))) > 0)
	{
		struct authd_cb *cmd;

		parc = rb_string_to_array(buf, parv, MAXPARA+1);
		cmd = &authd_cmd_tab[(unsigned char)*parv[0]];
		if(cmd->fn != NULL)
		{
			if(cmd->min_parc > parc)
			{
				iwarn("authd sent a result with wrong number of arguments: expected %d, got %d",
					cmd->min_parc, parc);
				restart_authd();
				continue;
			}

			cmd->fn(parc, parv);
		}
		else
		{
			iwarn("authd sent us a bad command type: %c", *parv[0]);
			restart_authd();
			continue;
		}
	}
}

void
init_authd(void)
{
	if(start_authd())
	{
		ierror("Unable to start authd helper: %s", strerror(errno));
		exit(0);
	}
}

void
configure_authd(void)
{
	/* Timeouts */
	set_authd_timeout("ident_timeout", GlobalSetOptions.ident_timeout);
	set_authd_timeout("rdns_timeout", ConfigFileEntry.connect_timeout);
	set_authd_timeout("rbl_timeout", ConfigFileEntry.connect_timeout);

	ident_check_enable(!ConfigFileEntry.disable_auth);

	/* Configure OPM */
	if(rb_dlink_list_length(&opm_list) > 0 &&
		(opm_listeners[LISTEN_IPV4].ipaddr[0] != '\0' ||
		opm_listeners[LISTEN_IPV6].ipaddr[0] != '\0'))
	{
		rb_dlink_node *ptr;

		if(opm_listeners[LISTEN_IPV4].ipaddr[0] != '\0')
			rb_helper_write(authd_helper, "O opm_listener %s %hu",
				opm_listeners[LISTEN_IPV4].ipaddr, opm_listeners[LISTEN_IPV4].port);

#ifdef RB_IPV6
		if(opm_listeners[LISTEN_IPV6].ipaddr[0] != '\0')
			rb_helper_write(authd_helper, "O opm_listener %s %hu",
				opm_listeners[LISTEN_IPV6].ipaddr, opm_listeners[LISTEN_IPV6].port);
#endif

		RB_DLINK_FOREACH(ptr, opm_list.head)
		{
			struct OPMScanner *scanner = ptr->data;
			rb_helper_write(authd_helper, "O opm_scanner %s %hu",
				scanner->type, scanner->port);
		}

		opm_check_enable(true);
	}
	else
		opm_check_enable(false);
}

static void
authd_free_client(struct Client *client_p)
{
	if(client_p == NULL || client_p->preClient == NULL)
		return;

	if(client_p->preClient->auth.cid == 0)
		return;

	if(authd_helper != NULL)
		rb_helper_write(authd_helper, "E %x", client_p->preClient->auth.cid);

	client_p->preClient->auth.accepted = true;
	client_p->preClient->auth.cid = 0;
}

static void
authd_free_client_cb(rb_dictionary_element *delem, void *unused)
{
	struct Client *client_p = delem->data;
	authd_free_client(client_p);
}

void
authd_abort_client(struct Client *client_p)
{
	rb_dictionary_delete(cid_clients, RB_UINT_TO_POINTER(client_p->preClient->auth.cid));
	authd_free_client(client_p);
}

static void
restart_authd_cb(rb_helper * helper)
{
	rb_dictionary_iter iter;
	struct Client *client_p;

	iwarn("authd: restart_authd_cb called, authd died?");
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "authd: restart_authd_cb called, authd died?");

	if(helper != NULL)
	{
		rb_helper_close(helper);
		authd_helper = NULL;
	}

	rb_dictionary_destroy(cid_clients, authd_free_client_cb, NULL);
	cid_clients = NULL;

	start_authd();
	configure_authd();
}

void
restart_authd(void)
{
	ierror("authd restarting...");
	restart_authd_cb(authd_helper);
}

void
rehash_authd(void)
{
	rb_helper_write(authd_helper, "R");
}

void
check_authd(void)
{
	if(authd_helper == NULL)
		restart_authd();
}

static inline uint32_t
generate_cid(void)
{
	if(++cid == 0)
		cid = 1;

	return cid;
}

/* Basically when this is called we begin handing off the client to authd for
 * processing. authd "owns" the client until processing is finished, or we
 * timeout from authd. authd will make a decision whether or not to accept the
 * client, but it's up to other parts of the code (for now) to decide if we're
 * gonna accept the client and ignore authd's suggestion.
 *
 * --Elizafox
 *
 * If this is an SSL connection we must defer handing off the client for
 * reading until it is open and we have the certificate fingerprint, otherwise
 * it's possible for the client to immediately send data before authd completes
 * and before the status of the connection is communicated via ssld. This data
 * could then be processed too early by read_packet().
 */
void
authd_initiate_client(struct Client *client_p, bool defer)
{
	char client_ipaddr[HOSTIPLEN+1];
	char listen_ipaddr[HOSTIPLEN+1];
	uint16_t client_port, listen_port;
	uint32_t authd_cid;

	if(client_p->preClient == NULL || client_p->preClient->auth.cid != 0)
		return;

	authd_cid = client_p->preClient->auth.cid = generate_cid();

	/* Collisions are extremely unlikely, so disregard the possibility */
	rb_dictionary_add(cid_clients, RB_UINT_TO_POINTER(authd_cid), client_p);

	/* Retrieve listener and client IP's */
	rb_inet_ntop_sock((struct sockaddr *)&client_p->preClient->lip, listen_ipaddr, sizeof(listen_ipaddr));
	rb_inet_ntop_sock((struct sockaddr *)&client_p->localClient->ip, client_ipaddr, sizeof(client_ipaddr));

	/* Retrieve listener and client ports */
	listen_port = ntohs(GET_SS_PORT(&client_p->preClient->lip));
	client_port = ntohs(GET_SS_PORT(&client_p->localClient->ip));

	if(defer)
		client_p->preClient->auth.flags |= AUTHC_F_DEFERRED;

	/* Add a bit of a fudge factor... */
	client_p->preClient->auth.timeout = rb_current_time() + ConfigFileEntry.connect_timeout + 10;

	rb_helper_write(authd_helper, "C %x %s %hu %s %hu", authd_cid, listen_ipaddr, listen_port, client_ipaddr, client_port);
}

static inline void
authd_read_client(struct Client *client_p)
{
	/*
	 * When a client has auth'ed, we want to start reading what it sends
	 * us. This is what read_packet() does.
	 *     -- adrian
	 *
	 * Above comment was originally in s_auth.c, but moved here with below code.
	 * --Elizafox
	 */
	rb_dlinkAddTail(client_p, &client_p->node, &global_client_list);
	read_packet(client_p->localClient->F, client_p);
}

/* When this is called we have a decision on client acceptance.
 *
 * After this point authd no longer "owns" the client, but if
 * it's flagged as deferred then we're still waiting for a call
 * to authd_deferred_client().
 */
static inline void
authd_decide_client(struct Client *client_p, const char *ident, const char *host, bool accept, char cause, const char *data, const char *reason)
{
	if(client_p->preClient == NULL || client_p->preClient->auth.cid == 0)
		return;

	if(*ident != '*')
	{
		rb_strlcpy(client_p->username, ident, sizeof(client_p->username));
		SetGotId(client_p);
		ServerStats.is_asuc++;
	}
	else
		ServerStats.is_abad++; /* s_auth used to do this, stay compatible */

	if(*host != '*')
		rb_strlcpy(client_p->host, host, sizeof(client_p->host));

	rb_dictionary_delete(cid_clients, RB_UINT_TO_POINTER(client_p->preClient->auth.cid));

	client_p->preClient->auth.accepted = accept;
	client_p->preClient->auth.cause = cause;
	client_p->preClient->auth.data = (data == NULL ? NULL : rb_strdup(data));
	client_p->preClient->auth.reason = (reason == NULL ? NULL : rb_strdup(reason));
	client_p->preClient->auth.cid = 0;

	client_p->preClient->auth.flags |= AUTHC_F_COMPLETE;
	if((client_p->preClient->auth.flags & AUTHC_F_DEFERRED) == 0)
		authd_read_client(client_p);
}

void
authd_deferred_client(struct Client *client_p)
{
	client_p->preClient->auth.flags &= ~AUTHC_F_DEFERRED;
	if(client_p->preClient->auth.flags & AUTHC_F_COMPLETE)
		authd_read_client(client_p);
}

/* Convenience function to accept client */
void
authd_accept_client(struct Client *client_p, const char *ident, const char *host)
{
	authd_decide_client(client_p, ident, host, true, '\0', NULL, NULL);
}

/* Convenience function to reject client */
void
authd_reject_client(struct Client *client_p, const char *ident, const char *host, char cause, const char *data, const char *reason)
{
	authd_decide_client(client_p, ident, host, false, cause, data, reason);
}

static void
timeout_dead_authd_clients(void *notused __unused)
{
	rb_dictionary_iter iter;
	struct Client *client_p;
	rb_dlink_list freelist = { NULL, NULL, 0 };
	rb_dlink_node *ptr, *nptr;

	RB_DICTIONARY_FOREACH(client_p, &iter, cid_clients)
	{
		if(client_p->preClient->auth.timeout < rb_current_time())
		{
			authd_free_client(client_p);
			rb_dlinkAddAlloc(client_p, &freelist);
		}
	}

	/* RB_DICTIONARY_FOREACH is not safe for deletion, so we do this crap */
	RB_DLINK_FOREACH_SAFE(ptr, nptr, freelist.head)
	{
		client_p = ptr->data;
		rb_dictionary_delete(cid_clients, RB_UINT_TO_POINTER(client_p->preClient->auth.cid));
	}
}

/* Send a new blacklist to authd */
void
add_blacklist(const char *host, const char *reason, uint8_t iptype, rb_dlink_list *filters)
{
	rb_dlink_node *ptr;
	struct BlacklistStats *stats = rb_malloc(sizeof(struct BlacklistStats));
	char filterbuf[BUFSIZE] = "*";
	size_t s = 0;

	if(bl_stats == NULL)
		bl_stats = rb_dictionary_create("blacklist statistics", rb_strcasecmp);

	/* Build a list of comma-separated values for authd.
	 * We don't check for validity - do it elsewhere.
	 */
	RB_DLINK_FOREACH(ptr, filters->head)
	{
		char *filter = ptr->data;
		size_t filterlen = strlen(filter) + 1;

		if(s + filterlen > sizeof(filterbuf))
			break;

		snprintf(&filterbuf[s], sizeof(filterbuf) - s, "%s,", filter);

		s += filterlen;
	}

	if(s)
		filterbuf[s - 1] = '\0';

	stats->host = rb_strdup(host);
	stats->iptype = iptype;
	stats->hits = 0;
	rb_dictionary_add(bl_stats, host, stats);

	rb_helper_write(authd_helper, "O rbl %s %hhu %s :%s", host, iptype, filterbuf, reason);
}

/* Delete a blacklist */
void
del_blacklist(const char *host)
{
	struct BlacklistStats *stats = rb_dictionary_retrieve(bl_stats, host);
	if(stats != NULL)
	{
		rb_free(stats->host);
		rb_free(stats);
		rb_dictionary_delete(bl_stats, host);
	}

	rb_helper_write(authd_helper, "O rbl_del %s", host);
}

static void
blacklist_delete(rb_dictionary_element *delem, void *unused)
{
	struct BlacklistStats *stats = delem->data;

	rb_free(stats->host);
	rb_free(stats);
}

/* Delete all the blacklists */
void
del_blacklist_all(void)
{
	if(bl_stats != NULL)
		rb_dictionary_destroy(bl_stats, blacklist_delete, NULL);
	bl_stats = NULL;

	rb_helper_write(authd_helper, "O rbl_del_all");
}

/* Adjust an authd timeout value */
bool
set_authd_timeout(const char *key, int timeout)
{
	if(timeout <= 0)
		return false;

	rb_helper_write(authd_helper, "O %s %d", key, timeout);
	return true;
}

/* Enable identd checks */
void
ident_check_enable(bool enabled)
{
	rb_helper_write(authd_helper, "O ident_enabled %d", enabled ? 1 : 0);
}

/* Create an OPM listener
 * XXX - This is a big nasty hack, but it avoids resending duplicate data when
 * configure_authd() is called.
 */
void
conf_create_opm_listener(const char *ip, uint16_t port)
{
	char ipbuf[HOSTIPLEN];
	struct OPMListener *listener;

	rb_strlcpy(ipbuf, ip, sizeof(ipbuf));
	if(ipbuf[0] == ':')
	{
		memmove(ipbuf + 1, ipbuf, sizeof(ipbuf) - 1);
		ipbuf[0] = '0';
	}

	/* I am much too lazy to use rb_inet_pton and GET_SS_FAMILY for now --Elizafox */
	listener = &opm_listeners[(strchr(ipbuf, ':') != NULL ? LISTEN_IPV6 : LISTEN_IPV4)];
	rb_strlcpy(listener->ipaddr, ipbuf, sizeof(listener->ipaddr));
	listener->port = port;
}

void
create_opm_listener(const char *ip, uint16_t port)
{
	char ipbuf[HOSTIPLEN];

	/* XXX duplicated in conf_create_opm_listener */
	rb_strlcpy(ipbuf, ip, sizeof(ipbuf));
	if(ipbuf[0] == ':')
	{
		memmove(ipbuf + 1, ipbuf, sizeof(ipbuf) - 1);
		ipbuf[0] = '0';
	}

	conf_create_opm_listener(ip, port);
	rb_helper_write(authd_helper, "O opm_listener %s %hu", ipbuf, port);
}

void
delete_opm_listener_all(void)
{
	memset(&opm_listeners, 0, sizeof(opm_listeners));
	rb_helper_write(authd_helper, "O opm_listener_del_all");
}

/* Disable all OPM scans */
void
opm_check_enable(bool enabled)
{
	rb_helper_write(authd_helper, "O opm_enabled %d", enabled ? 1 : 0);
}

/* Create an OPM proxy scanner
 * XXX - This is a big nasty hack, but it avoids resending duplicate data when
 * configure_authd() is called.
 */
void
conf_create_opm_proxy_scanner(const char *type, uint16_t port)
{
	struct OPMScanner *scanner = rb_malloc(sizeof(struct OPMScanner));

	rb_strlcpy(scanner->type, type, sizeof(scanner->type));
	scanner->port = port;
	rb_dlinkAdd(scanner, &scanner->node, &opm_list);
}

void
create_opm_proxy_scanner(const char *type, uint16_t port)
{
	conf_create_opm_proxy_scanner(type, port);
	rb_helper_write(authd_helper, "O opm_scanner %s %hu", type, port);
}

void
delete_opm_proxy_scanner(const char *type, uint16_t port)
{
	rb_dlink_node *ptr, *nptr;

	RB_DLINK_FOREACH_SAFE(ptr, nptr, opm_list.head)
	{
		struct OPMScanner *scanner = ptr->data;

		if(rb_strncasecmp(scanner->type, type, sizeof(scanner->type)) == 0 &&
			scanner->port == port)
		{
			rb_dlinkDelete(ptr, &opm_list);
			rb_free(scanner);
			break;
		}
	}

	rb_helper_write(authd_helper, "O opm_scanner_del %s %hu", type, port);
}

void
delete_opm_proxy_scanner_all(void)
{
	rb_dlink_node *ptr, *nptr;

	RB_DLINK_FOREACH_SAFE(ptr, nptr, opm_list.head)
	{
		struct OPMScanner *scanner = ptr->data;

		rb_dlinkDelete(ptr, &opm_list);
		rb_free(scanner);
	}

	rb_helper_write(authd_helper, "O opm_scanner_del_all");
}
