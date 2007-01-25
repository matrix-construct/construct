/*
 *  charybdis: A useful ircd.
 *  client.h: The ircd client header.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *  Copyright (C) 2005 William Pitcock and Jilles Tjoelker
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
 *  $Id: client.h 2023 2006-09-02 23:47:27Z jilles $
 */

#ifndef INCLUDED_client_h
#define INCLUDED_client_h

#include "config.h"

#if !defined(CONFIG_RATBOX_LEVEL_1)
#error Incorrect config.h for this revision of ircd.
#endif

#include "ircd_defs.h"
#include "linebuf.h"
#include "channel.h"
#include "res.h"
#include "snomask.h"
#include "irc_string.h"
#include "sprintf_irc.h"
#include "ircd.h"
#include "commio.h"

/* other structs */
struct Blacklist;

/* we store ipv6 ips for remote clients, so this needs to be v6 always */
#define HOSTIPLEN	53	/* sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255.ipv6") */
#define PASSWDLEN       128
#define CIPHERKEYLEN    64	/* 512bit */
#define CLIENT_BUFSIZE 512	/* must be at least 512 bytes */

#define IDLEN		10

/*
 * pre declare structs
 */
struct ConfItem;
struct Whowas;
struct DNSReply;
struct Listener;
struct Client;
struct User;
struct Server;
struct LocalUser;
struct AuthRequest;
struct PreClient;
struct ListClient;

/* 
 * Atheme's coding standards require that we use BSD-style user-defined types
 * for stuff. Fun! --nenolod
 */
typedef struct User user_t;
typedef struct Server server_t;
typedef struct Client client_t;
typedef struct LocalUser local_user_t;
typedef struct Listener listener_t;
typedef struct DNSReply dns_reply_t;
typedef struct Whowas whowas_entry_t;
typedef struct ConfItem conf_item_t;
typedef struct AuthRequest auth_request_t;
typedef struct PreClient pre_client_t;
typedef struct ListClient list_client_t;

/*
 * Client structures
 */
struct User
{
	dlink_list channel;	/* chain of channel pointer blocks */
	dlink_list invited;	/* chain of invite pointer blocks */
	char *away;		/* pointer to away message */
	int refcnt;		/* Number of times this block is referenced */
	const char *server;	/* pointer to scached server name */

	char suser[NICKLEN+1];
};

struct Server
{
	user_t *user;		/* who activated this connection */
	const char *up;		/* Pointer to scache name */
	const char *upid;
	char by[NICKLEN];
	dlink_list servers;
	dlink_list users;
	int caps;		/* capabilities bit-field */
	char *fullcaps;
};

struct SlinkRpl
{
	int command;
	int datalen;
	int gotdatalen;
	int readdata;
	unsigned char *data;
};

struct ZipStats
{
	unsigned long in;
	unsigned long in_wire;
	unsigned long out;
	unsigned long out_wire;
	unsigned long inK;
	unsigned long inK_wire;
	unsigned long outK;
	unsigned long outK_wire;
	double in_ratio;
	double out_ratio;
};

struct Client
{
	dlink_node node;
	dlink_node lnode;
	user_t *user;		/* ...defined, if this is a User */
	server_t *serv;		/* ...defined, if this is a server */
	client_t *servptr;	/* Points to server this Client is on */
	client_t *from;		/* == self, if Local Client, *NEVER* NULL! */

	whowas_entry_t *whowas;	/* Pointers to whowas structs */
	time_t tsinfo;		/* TS on the nick, SVINFO on server */
	unsigned int umodes;	/* opers, normal users subset */
	unsigned int flags;	/* client flags */
	unsigned int flags2;	/* ugh. overflow */

	unsigned int snomask;	/* server notice mask */

	int hopcount;		/* number of servers to this 0 = local */
	unsigned short status;	/* Client type */
	unsigned char handler;	/* Handler index */
	unsigned long serial;	/* used to enforce 1 send per nick */

	/* client->name is the unique name for a client nick or host */
	char name[HOSTLEN + 1];

	/* 
	 * client->username is the username from ident or the USER message, 
	 * If the client is idented the USER message is ignored, otherwise 
	 * the username part of the USER message is put here prefixed with a 
	 * tilde depending on the I:line, Once a client has registered, this
	 * field should be considered read-only.
	 */
	char username[USERLEN + 1];	/* client's username */

	/*
	 * client->host contains the resolved name or ip address
	 * as a string for the user, it may be fiddled with for oper spoofing etc.
	 */
	char host[HOSTLEN + 1];	/* client's hostname */
	char orighost[HOSTLEN + 1]; /* original hostname (before dynamic spoofing) */
	char sockhost[HOSTIPLEN + 1]; /* clients ip */
	char info[REALLEN + 1];	/* Free form additional client info */

	char id[IDLEN];	/* UID/SID, unique on the network */

	/* list of who has this client on their allow list, its counterpart
	 * is in LocalUser
	 */
	dlink_list on_allow_list;

	local_user_t *localClient;
	pre_client_t *preClient;
};

struct LocalUser
{
	dlink_node tnode;	/* This is the node for the local list type the client is on*/
	/*
	 * The following fields are allocated only for local clients
	 * (directly connected to *this* server with a socket.
	 */
	/* Anti flooding part, all because of lamers... */
	time_t last_join_time;	/* when this client last 
				   joined a channel */
	time_t last_leave_time;	/* when this client last 
				 * left a channel */
	int join_leave_count;	/* count of JOIN/LEAVE in less than 
				   MIN_JOIN_LEAVE_TIME seconds */
	int oper_warn_count_down;	/* warn opers of this possible 
					   spambot every time this gets to 0 */
	time_t last_caller_id_time;
	time_t first_received_message_time;
	int received_number_of_privmsgs;
	int flood_noticed;

	time_t lasttime;	/* last time we parsed something */
	time_t firsttime;	/* time client was created */

	/* Send and receive linebuf queues .. */
	buf_head_t buf_sendq;
	buf_head_t buf_recvq;
	/*
	 * we want to use unsigned int here so the sizes have a better chance of
	 * staying the same on 64 bit machines. The current trend is to use
	 * I32LP64, (32 bit ints, 64 bit longs and pointers) and since ircd
	 * will NEVER run on an operating system where ints are less than 32 bits, 
	 * it's a relatively safe bet to use ints. Since right shift operations are
	 * performed on these, it's not safe to allow them to become negative, 
	 * which is possible for long running server connections. Unsigned values 
	 * generally overflow gracefully. --Bleep
	 */
	unsigned int sendM;	/* Statistics: protocol messages send */
	unsigned int sendK;	/* Statistics: total k-bytes send */
	unsigned int receiveM;	/* Statistics: protocol messages received */
	unsigned int receiveK;	/* Statistics: total k-bytes received */
	unsigned short sendB;	/* counters to count upto 1-k lots of bytes */
	unsigned short receiveB;	/* sent and received. */
	listener_t *listener;		/* listener accepted from */
	conf_item_t *att_conf;		/* attached conf */
	struct server_conf *att_sconf;

	struct irc_sockaddr_storage ip;
	time_t last_nick_change;
	int number_of_nick_changes;

	/*
	 * XXX - there is no reason to save this, it should be checked when it's
	 * received and not stored, this is not used after registration
	 *
	 * agreed. lets get rid of it someday! --nenolod
	 */
	char *passwd;
	char *opername; /* name of operator{} block being used or tried (challenge) */
	char *challenge;
	char *fullcaps;

	int caps;		/* capabilities bit-field */
	int fd;			/* >= 0, for local clients */

	/* time challenge response is valid for */
	time_t chal_time;

	int ctrlfd;		/* For servers:
				   control fd used for sending commands
				   to servlink */

	struct SlinkRpl slinkrpl;	/* slink reply being parsed */
	unsigned char *slinkq;	/* sendq for control data */
	int slinkq_ofs;		/* ofset into slinkq */
	int slinkq_len;		/* length remaining after slinkq_ofs */

	struct ZipStats zipstats;

	time_t last_away;	/* Away since... */
	time_t last;

	/* clients allowed to talk through +g */
	dlink_list allow_list;

	/* nicknames theyre monitoring */
	dlink_list monitor_list;

	/*
	 * Anti-flood stuff. We track how many messages were parsed and how
	 * many we were allowed in the current second, and apply a simple decay
	 * to avoid flooding.
	 *   -- adrian
	 */
	int allow_read;		/* how many we're allowed to read in this second */
	int actually_read;	/* how many we've actually read in this second */
	int sent_parsed;	/* how many messages we've parsed in this second */
	time_t last_knock;	/* time of last knock */
	unsigned long random_ping;
	auth_request_t	*auth_request;

	/* target change stuff */
	void *targets[10];		/* targets were aware of */
	unsigned int targinfo[2];	/* cyclic array, no in use */
	time_t target_last;		/* last time we cleared a slot */

	list_client_t *safelist_data;

	char *mangledhost; /* non-NULL if host mangling module loaded and
			      applicable to this client */
};

struct PreClient
{
	char spoofnick[NICKLEN + 1];
	char spoofuser[USERLEN + 1];
	char spoofhost[HOSTLEN + 1];

	char sasl_agent[IDLEN];
	unsigned char sasl_out;
	unsigned char sasl_complete;

	dlink_list dnsbl_queries; /* list of struct BlacklistClient * */
	struct Blacklist *dnsbl_listed; /* first dnsbl where it's listed */
};

struct ListClient
{
	unsigned int hash_indice;
	unsigned int users_min, users_max;

	/* It would be nice to add other modifiers,
	 * but not for 1.1 --nenolod
	 */
};

struct exit_client_hook
{
	struct Client *client_p;
	char exit_message[TOPICLEN];
};

/*
 * status macros.
 */
#define STAT_CONNECTING         0x01
#define STAT_HANDSHAKE          0x02
#define STAT_ME                 0x04
#define STAT_UNKNOWN            0x08
#define STAT_REJECT		0x10
#define STAT_SERVER             0x20
#define STAT_CLIENT             0x40


#define IsRegisteredUser(x)     ((x)->status == STAT_CLIENT)
#define IsRegistered(x)         (((x)->status  > STAT_UNKNOWN) && ((x)->status != STAT_REJECT))
#define IsConnecting(x)         ((x)->status == STAT_CONNECTING)
#define IsHandshake(x)          ((x)->status == STAT_HANDSHAKE)
#define IsMe(x)                 ((x)->status == STAT_ME)
#define IsUnknown(x)            ((x)->status == STAT_UNKNOWN)
#define IsServer(x)             ((x)->status == STAT_SERVER)
#define IsClient(x)             ((x)->status == STAT_CLIENT)
#define IsReject(x)		((x)->status == STAT_REJECT)

#define IsAnyServer(x)          (IsServer(x) || IsHandshake(x) || IsConnecting(x))

#define IsOper(x)		((x)->umodes & UMODE_OPER)
#define IsAdmin(x)		((x)->umodes & UMODE_ADMIN)

#define SetReject(x)		{(x)->status = STAT_REJECT; \
				 (x)->handler = UNREGISTERED_HANDLER; }

#define SetConnecting(x)        {(x)->status = STAT_CONNECTING; \
				 (x)->handler = UNREGISTERED_HANDLER; }

#define SetHandshake(x)         {(x)->status = STAT_HANDSHAKE; \
				 (x)->handler = UNREGISTERED_HANDLER; }

#define SetMe(x)                {(x)->status = STAT_ME; \
				 (x)->handler = UNREGISTERED_HANDLER; }

#define SetUnknown(x)           {(x)->status = STAT_UNKNOWN; \
				 (x)->handler = UNREGISTERED_HANDLER; }

#define SetServer(x)            {(x)->status = STAT_SERVER; \
				 (x)->handler = SERVER_HANDLER; }

#define SetClient(x)            {(x)->status = STAT_CLIENT; \
				 (x)->handler = IsOper((x)) ? \
					OPER_HANDLER : CLIENT_HANDLER; }
#define SetRemoteClient(x)	{(x)->status = STAT_CLIENT; \
				 (x)->handler = RCLIENT_HANDLER; }

#define STAT_CLIENT_PARSE (STAT_UNKNOWN | STAT_CLIENT)
#define STAT_SERVER_PARSE (STAT_CONNECTING | STAT_HANDSHAKE | STAT_SERVER)

#define PARSE_AS_CLIENT(x)      ((x)->status & STAT_CLIENT_PARSE)
#define PARSE_AS_SERVER(x)      ((x)->status & STAT_SERVER_PARSE)


/*
 * ts stuff
 */
#define TS_CURRENT	6

#ifdef TS6_ONLY
#define TS_MIN          6
#else
#define TS_MIN          3
#endif

#define TS_DOESTS       0x10000000
#define DoesTS(x)       ((x)->tsinfo & TS_DOESTS)

#define has_id(source)	((source)->id[0] != '\0')
#define use_id(source)	((source)->id[0] != '\0' ? (source)->id : (source)->name)

/* if target is TS6, use id if it has one, else name */
#define get_id(source, target) ((IsServer(target->from) && has_id(target->from)) ? \
				use_id(source) : (source)->name)

/* housekeeping flags */

#define FLAGS_PINGSENT     0x0001	/* Unreplied ping sent */
#define FLAGS_DEAD	   0x0002	/* Local socket is dead--Exiting soon */
#define FLAGS_KILLED       0x0004	/* Prevents "QUIT" from being sent for this */
#define FLAGS_SENTUSER     0x0008	/* Client sent a USER command. */
#define FLAGS_CLOSING      0x0020	/* set when closing to suppress errors */
#define FLAGS_CHKACCESS    0x0040	/* ok to check clients access if set */
#define FLAGS_GOTID        0x0080	/* successful ident lookup achieved */
#define FLAGS_NEEDID       0x0100	/* I-lines say must use ident return */
#define FLAGS_NORMALEX     0x0400	/* Client exited normally */
#define FLAGS_SENDQEX      0x0800	/* Sendq exceeded */
#define FLAGS_SERVLINK     0x10000	/* servlink has servlink process */
#define FLAGS_MARK	   0x20000	/* marked client */
#define FLAGS_HIDDEN       0x40000	/* hidden server */
#define FLAGS_EOB          0x80000	/* EOB */
#define FLAGS_MYCONNECT	   0x100000	/* MyConnect */
#define FLAGS_IOERROR      0x200000	/* IO error */
#define FLAGS_SERVICE	   0x400000	/* network service */
#define FLAGS_TGCHANGE     0x800000	/* we're allowed to clear something */
#define FLAGS_DYNSPOOF     0x1000000	/* dynamic spoof, only opers see ip */

/* umodes, settable flags */
/* lots of this moved to snomask -- jilles */
#define UMODE_SERVNOTICE   0x0001	/* server notices */
#define UMODE_WALLOP       0x0100	/* send wallops to them */
#define UMODE_OPERWALL     0x0200	/* Operwalls */
#define UMODE_INVISIBLE    0x0400	/* makes user invisible */
#define UMODE_CALLERID     0x2000	/* block unless caller id's */
#define UMODE_LOCOPS       0x8000	/* show locops */
#define UMODE_SERVICE      0x40000
#define UMODE_DEAF	   0x80000
#define UMODE_NOFORWARD    0x400000	/* don't forward */
#define UMODE_REGONLYMSG   0x800000	/* only allow logged in users to msg */

/* user information flags, only settable by remote mode or local oper */
#define UMODE_OPER         0x100000	/* Operator */
#define UMODE_ADMIN        0x200000	/* Admin on server */

#define UMODE_ALL	   UMODE_SERVNOTICE

/* overflow flags */
/* EARLIER FLAGS ARE IN s_newconf.h */
#define FLAGS2_EXEMPTRESV	0x0080000
#define FLAGS2_EXEMPTGLINE      0x0100000
#define FLAGS2_EXEMPTKLINE      0x0200000
#define FLAGS2_EXEMPTFLOOD      0x0400000
#define FLAGS2_NOLIMIT          0x0800000
#define FLAGS2_IDLE_LINED       0x1000000
#define FLAGS2_CLICAP		0x2000000
#define FLAGS2_PING_COOKIE      0x4000000
#define FLAGS2_IP_SPOOFING      0x8000000
#define FLAGS2_FLOODDONE        0x10000000
#define FLAGS2_EXEMPTSPAMBOT	0x20000000
#define FLAGS2_EXEMPTSHIDE	0x40000000
#define FLAGS2_EXEMPTJUPE	0x80000000

#define DEFAULT_OPER_UMODES (UMODE_SERVNOTICE | UMODE_OPERWALL | \
                             UMODE_WALLOP | UMODE_LOCOPS)
#define DEFAULT_OPER_SNOMASK SNO_GENERAL

#define FLAGS_ID     (FLAGS_NEEDID | FLAGS_GOTID)

#define CLICAP_MULTI_PREFIX	0x0001
#define CLICAP_SASL		0x0002

/*
 * flags macros.
 */
#define IsPerson(x)             (IsClient(x) && (x)->user != NULL)
#define DoAccess(x)             ((x)->flags & FLAGS_CHKACCESS)
#define SetAccess(x)            ((x)->flags |= FLAGS_CHKACCESS)
#define ClearAccess(x)          ((x)->flags &= ~FLAGS_CHKACCESS)
#define HasServlink(x)          ((x)->flags &  FLAGS_SERVLINK)
#define SetServlink(x)          ((x)->flags |= FLAGS_SERVLINK)
#define MyConnect(x)		((x)->flags & FLAGS_MYCONNECT)
#define SetMyConnect(x)		((x)->flags |= FLAGS_MYCONNECT)
#define ClearMyConnect(x)	((x)->flags &= ~FLAGS_MYCONNECT)

#define MyClient(x)             (MyConnect(x) && IsClient(x))
#define SetMark(x)		((x)->flags |= FLAGS_MARK)
#define ClearMark(x)		((x)->flags &= ~FLAGS_MARK)
#define IsMarked(x)		((x)->flags & FLAGS_MARK)
#define SetHidden(x)		((x)->flags |= FLAGS_HIDDEN)
#define ClearHidden(x)		((x)->flags &= ~FLAGS_HIDDEN)
#define IsHidden(x)		((x)->flags & FLAGS_HIDDEN)
#define ClearEob(x)		((x)->flags &= ~FLAGS_EOB)
#define SetEob(x)		((x)->flags |= FLAGS_EOB)
#define HasSentEob(x)		((x)->flags & FLAGS_EOB)
#define IsDead(x)          	((x)->flags &  FLAGS_DEAD)
#define SetDead(x)         	((x)->flags |= FLAGS_DEAD)
#define IsClosing(x)		((x)->flags & FLAGS_CLOSING)
#define SetClosing(x)		((x)->flags |= FLAGS_CLOSING)
#define IsIOError(x)		((x)->flags & FLAGS_IOERROR)
#define SetIOError(x)		((x)->flags |= FLAGS_IOERROR)
#define IsAnyDead(x)		(IsIOError(x) || IsDead(x) || IsClosing(x))
#define IsTGChange(x)		((x)->flags & FLAGS_TGCHANGE)
#define SetTGChange(x)		((x)->flags |= FLAGS_TGCHANGE)
#define ClearTGChange(x)	((x)->flags &= ~FLAGS_TGCHANGE)
#define IsDynSpoof(x)		((x)->flags & FLAGS_DYNSPOOF)
#define SetDynSpoof(x)		((x)->flags |= FLAGS_DYNSPOOF)
#define ClearDynSpoof(x)	((x)->flags &= ~FLAGS_DYNSPOOF)

/* oper flags */
#define MyOper(x)               (MyConnect(x) && IsOper(x))

#define SetOper(x)              {(x)->umodes |= UMODE_OPER; \
				 if (MyClient((x))) (x)->handler = OPER_HANDLER;}

#define ClearOper(x)            {(x)->umodes &= ~(UMODE_OPER|UMODE_ADMIN); \
				 if (MyClient((x)) && !IsOper((x)) && !IsServer((x))) \
				  (x)->handler = CLIENT_HANDLER; }

#define IsPrivileged(x)         (IsOper(x) || IsServer(x))

/* umode flags */
#define IsInvisible(x)          ((x)->umodes & UMODE_INVISIBLE)
#define SetInvisible(x)         ((x)->umodes |= UMODE_INVISIBLE)
#define ClearInvisible(x)       ((x)->umodes &= ~UMODE_INVISIBLE)
#define SendWallops(x)          ((x)->umodes & UMODE_WALLOP)
#define ClearWallops(x)         ((x)->umodes &= ~UMODE_WALLOP)
#define SendLocops(x)           ((x)->umodes & UMODE_LOCOPS)
#define SendServNotice(x)       ((x)->umodes & UMODE_SERVNOTICE)
#define SendOperwall(x)         ((x)->umodes & UMODE_OPERWALL)
#define SetWallops(x)           ((x)->umodes |= UMODE_WALLOP)
#define SetCallerId(x)		((x)->umodes |= UMODE_CALLERID)
#define IsSetCallerId(x)	((x)->umodes & UMODE_CALLERID)
#define IsService(x)		((x)->umodes & UMODE_SERVICE)
#define IsDeaf(x)		((x)->umodes & UMODE_DEAF)
#define IsNoForward(x)		((x)->umodes & UMODE_NOFORWARD)
#define IsSetRegOnlyMsg(x)	((x)->umodes & UMODE_REGONLYMSG)

#define SetNeedId(x)            ((x)->flags |= FLAGS_NEEDID)
#define IsNeedId(x)             (((x)->flags & FLAGS_NEEDID) != 0)

#define SetGotId(x)             ((x)->flags |= FLAGS_GOTID)
#define IsGotId(x)              (((x)->flags & FLAGS_GOTID) != 0)

/*
 * flags2 macros.
 */
#define IsExemptKline(x)        ((x)->flags2 & FLAGS2_EXEMPTKLINE)
#define SetExemptKline(x)       ((x)->flags2 |= FLAGS2_EXEMPTKLINE)
#define IsExemptLimits(x)       ((x)->flags2 & FLAGS2_NOLIMIT)
#define SetExemptLimits(x)      ((x)->flags2 |= FLAGS2_NOLIMIT)
#define IsExemptGline(x)        ((x)->flags2 & FLAGS2_EXEMPTGLINE)
#define SetExemptGline(x)       ((x)->flags2 |= FLAGS2_EXEMPTGLINE)
#define IsExemptFlood(x)        ((x)->flags2 & FLAGS2_EXEMPTFLOOD)
#define SetExemptFlood(x)       ((x)->flags2 |= FLAGS2_EXEMPTFLOOD)
#define IsExemptSpambot(x)	((x)->flags2 & FLAGS2_EXEMPTSPAMBOT)
#define SetExemptSpambot(x)	((x)->flags2 |= FLAGS2_EXEMPTSPAMBOT)
#define IsExemptShide(x)	((x)->flags2 & FLAGS2_EXEMPTSHIDE)
#define SetExemptShide(x)	((x)->flags2 |= FLAGS2_EXEMPTSHIDE)
#define IsExemptJupe(x)		((x)->flags2 & FLAGS2_EXEMPTJUPE)
#define SetExemptJupe(x)	((x)->flags2 |= FLAGS2_EXEMPTJUPE)
#define IsExemptResv(x)		((x)->flags2 & FLAGS2_EXEMPTRESV)
#define SetExemptResv(x)	((x)->flags2 |= FLAGS2_EXEMPTRESV)
#define IsIPSpoof(x)            ((x)->flags2 & FLAGS2_IP_SPOOFING)
#define SetIPSpoof(x)           ((x)->flags2 |= FLAGS2_IP_SPOOFING)

#define SetIdlelined(x)         ((x)->flags2 |= FLAGS2_IDLE_LINED)
#define IsIdlelined(x)          ((x)->flags2 & FLAGS2_IDLE_LINED)

/* for local users: flood grace period is over
 * for servers: mentioned in networknotice.c notice
 */
#define IsFloodDone(x)          ((x)->flags2 & FLAGS2_FLOODDONE)
#define SetFloodDone(x)         ((x)->flags2 |= FLAGS2_FLOODDONE)

/*
 * definitions for get_client_name
 */
#define HIDE_IP 0
#define SHOW_IP 1
#define MASK_IP 2

extern void check_banned_lines(void);
extern void check_klines_event(void *unused);
extern void check_klines(void);
extern void check_glines(void);
extern void check_dlines(void);
extern void check_xlines(void);

extern const char *get_client_name(struct Client *client, int show_ip);
extern const char *get_server_name(struct Client *client, int show_ip);
extern const char *log_client_name(struct Client *, int);
extern int is_remote_connect(struct Client *);
extern void init_client(void);
extern client_t *make_client(struct Client *from);
extern void free_pre_client(struct Client *client);
extern void free_client(struct Client *client);

extern int exit_client(struct Client *, struct Client *, struct Client *, const char *);

extern void error_exit_client(struct Client *, int);



extern void count_local_client_memory(size_t * count, size_t * memory);
extern void count_remote_client_memory(size_t * count, size_t * memory);

extern client_t *find_chasing(struct Client *, const char *, int *);
extern client_t *find_person(const char *);
extern client_t *find_named_person(const char *);
extern client_t *next_client(struct Client *, const char *);

#define accept_message(s, t) ((s) == (t) || (dlinkFind((s), &((t)->localClient->allow_list))))
extern void del_all_accepts(struct Client *client_p);

extern void dead_link(struct Client *client_p);
extern int show_ip(struct Client *source_p, struct Client *target_p);
extern int show_ip_conf(struct ConfItem *aconf, struct Client *target_p);

extern void initUser(void);
extern void free_user(struct User *, struct Client *);
extern user_t *make_user(struct Client *);
extern server_t *make_server(struct Client *);
extern void close_connection(struct Client *);
extern void init_uid(void);
extern char *generate_uid(void);

#endif /* INCLUDED_client_h */
