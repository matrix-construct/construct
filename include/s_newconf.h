/*
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * s_newconf.h: code for dealing with conf stuff
 *
 * Copyright (C) 2004 Lee Hardy <lee@leeh.co.uk>
 * Copyright (C) 2004 ircd-ratbox development team
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
 * $Id: s_newconf.h 1747 2006-07-25 21:22:45Z jilles $
 */

#ifndef INCLUDED_s_newconf_h
#define INCLUDED_s_newconf_h

#include "setup.h"
#include "privilege.h"

#ifdef HAVE_LIBCRYPTO
#include <openssl/rsa.h>
#endif

struct ConfItem;

extern rb_dlink_list cluster_conf_list;
extern rb_dlink_list shared_conf_list;
extern rb_dlink_list oper_conf_list;
extern rb_dlink_list hubleaf_conf_list;
extern rb_dlink_list server_conf_list;
extern rb_dlink_list xline_conf_list;
extern rb_dlink_list resv_conf_list;
extern rb_dlink_list nd_list;
extern rb_dlink_list tgchange_list;

extern struct _rb_patricia_tree_t *tgchange_tree;

extern void init_s_newconf(void);
extern void clear_s_newconf(void);
extern void clear_s_newconf_bans(void);

typedef struct
{
	char *ip;
	time_t expiry;
	rb_patricia_node_t *pnode;
	rb_dlink_node node;
} tgchange;

void add_tgchange(const char *host);
tgchange *find_tgchange(const char *host);

/* shared/cluster/hub/leaf confs */
struct remote_conf
{
	char *username;
	char *host;
	char *server;
	int flags;
	rb_dlink_node node;
};

/* flags used in shared/cluster */
#define SHARED_TKLINE	0x0001
#define SHARED_PKLINE	0x0002
#define SHARED_UNKLINE	0x0004
#define SHARED_LOCOPS	0x0008
#define SHARED_TXLINE	0x0010
#define SHARED_PXLINE	0x0020
#define SHARED_UNXLINE	0x0040
#define SHARED_TRESV	0x0080
#define SHARED_PRESV	0x0100
#define SHARED_UNRESV	0x0200
#define SHARED_REHASH	0x0400
#define SHARED_TDLINE	0x0800
#define SHARED_PDLINE	0x1000
#define SHARED_UNDLINE	0x2000

#define SHARED_ALL	(SHARED_TKLINE | SHARED_PKLINE | SHARED_UNKLINE |\
			SHARED_PXLINE | SHARED_TXLINE | SHARED_UNXLINE |\
			SHARED_TRESV | SHARED_PRESV | SHARED_UNRESV)
#define CLUSTER_ALL	(SHARED_ALL | SHARED_LOCOPS)

/* flags used in hub/leaf */
#define CONF_HUB	0x0001
#define CONF_LEAF	0x0002

struct oper_conf
{
	char *name;
	char *username;
	char *host;
	char *passwd;
	char *certfp;

	int flags;
	int umodes;

	unsigned int snomask;

	struct PrivilegeSet *privset;

#ifdef HAVE_LIBCRYPTO
	char *rsa_pubkey_file;
	RSA *rsa_pubkey;
#endif
};

extern struct remote_conf *make_remote_conf(void);
extern void free_remote_conf(struct remote_conf *);

extern int find_shared_conf(const char *username, const char *host,
			const char *server, int flags);
extern void propagate_generic(struct Client *source_p, const char *command,
		const char *target, int cap, const char *format, ...);
extern void cluster_generic(struct Client *, const char *, int cltype,
			int cap, const char *format, ...);

#define OPER_ENCRYPTED	0x00001
#define OPER_NEEDSSL    0x80000
/* 0x400000 and above are in client.h */

#define OPER_FLAGS	0 /* no oper privs in Client.flags2/oper_conf.flags currently */

#define IsOperConfEncrypted(x)	((x)->flags & OPER_ENCRYPTED)
#define IsOperConfNeedSSL(x)	((x)->flags & OPER_NEEDSSL)

#define HasPrivilege(x, y)	((x)->localClient != NULL && (x)->localClient->privset != NULL && privilegeset_in_set((x)->localClient->privset, (y)))

#define IsOperGlobalKill(x)     (HasPrivilege((x), "oper:global_kill"))
#define IsOperLocalKill(x)      (HasPrivilege((x), "oper:local_kill"))
#define IsOperRemote(x)         (HasPrivilege((x), "oper:routing"))
#define IsOperUnkline(x)        (HasPrivilege((x), "oper:unkline"))
#define IsOperN(x)              (HasPrivilege((x), "snomask:nick_changes"))
#define IsOperK(x)              (HasPrivilege((x), "oper:kline"))
#define IsOperXline(x)          (HasPrivilege((x), "oper:xline"))
#define IsOperResv(x)           (HasPrivilege((x), "oper:resv"))
#define IsOperDie(x)            (HasPrivilege((x), "oper:die"))
#define IsOperRehash(x)         (HasPrivilege((x), "oper:rehash"))
#define IsOperHiddenAdmin(x)    (HasPrivilege((x), "oper:hidden_admin"))
#define IsOperAdmin(x)          (HasPrivilege((x), "oper:admin") || HasPrivilege((x), "oper:hidden_admin"))
#define IsOperOperwall(x)       (HasPrivilege((x), "oper:operwall"))
#define IsOperSpy(x)            (HasPrivilege((x), "oper:spy"))
#define IsOperInvis(x)          (HasPrivilege((x), "oper:hidden"))
#define IsOperRemoteBan(x)	(HasPrivilege((x), "oper:remoteban"))
#define IsOperMassNotice(x)	(HasPrivilege((x), "oper:mass_notice"))

extern struct oper_conf *make_oper_conf(void);
extern void free_oper_conf(struct oper_conf *);
extern void clear_oper_conf(void);

extern struct oper_conf *find_oper_conf(const char *username, const char *host,
					const char *locip, const char *oname);

extern const char *get_oper_privs(int flags);

struct server_conf
{
	char *name;
	char *host;
	char *passwd;
	char *spasswd;
	int port;
	int flags;
	int servers;
	time_t hold;

	int aftype;
	struct rb_sockaddr_storage my_ipnum;

	char *class_name;
	struct Class *class;
	rb_dlink_node node;
};

#define SERVER_ILLEGAL		0x0001
#define SERVER_VHOSTED		0x0002
#define SERVER_ENCRYPTED	0x0004
#define SERVER_COMPRESSED	0x0008
#define SERVER_TB		0x0010
#define SERVER_AUTOCONN		0x0020
#define SERVER_SSL		0x0040

#define ServerConfIllegal(x)	((x)->flags & SERVER_ILLEGAL)
#define ServerConfVhosted(x)	((x)->flags & SERVER_VHOSTED)
#define ServerConfEncrypted(x)	((x)->flags & SERVER_ENCRYPTED)
#define ServerConfCompressed(x)	((x)->flags & SERVER_COMPRESSED)
#define ServerConfTb(x)		((x)->flags & SERVER_TB)
#define ServerConfAutoconn(x)	((x)->flags & SERVER_AUTOCONN)
#define ServerConfSSL(x)	((x)->flags & SERVER_SSL)

extern struct server_conf *make_server_conf(void);
extern void free_server_conf(struct server_conf *);
extern void clear_server_conf(void);
extern void add_server_conf(struct server_conf *);

extern struct server_conf *find_server_conf(const char *name);

extern void attach_server_conf(struct Client *, struct server_conf *);
extern void detach_server_conf(struct Client *);
extern void set_server_conf_autoconn(struct Client *source_p, const char *name, 
					int newval);
extern void disable_server_conf_autoconn(const char *name);


extern struct ConfItem *find_xline(const char *, int);
extern struct ConfItem *find_xline_mask(const char *);
extern struct ConfItem *find_nick_resv(const char *name);
extern struct ConfItem *find_nick_resv_mask(const char *name);

extern int valid_wild_card_simple(const char *);
extern int clean_resv_nick(const char *);
time_t valid_temp_time(const char *p);

struct nd_entry
{
	char name[NICKLEN+1];
	time_t expire;
	rb_dlink_node lnode;	/* node in ll */
};

extern void add_nd_entry(const char *name);
extern void free_nd_entry(struct nd_entry *);
extern unsigned long get_nd_count(void);

#endif

