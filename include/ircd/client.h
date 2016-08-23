/*
 *  charybdis: A useful ircd.
 *  client.h: The ircd client header.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *  Copyright (C) 2005 William Pitcock and Jilles Tjoelker
 *  Copyright (C) 2005-2016 Charybdis Development Team
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
 */

#pragma once
#define HAVE_IRCD_CLIENT_H

#include "client_user.h"
#include "client_serv.h"
#include "client_mode.h"

#ifdef __cplusplus

namespace ircd   {
namespace client {

enum class status : uint8_t
{
	CONNECTING     = 0x01,
	HANDSHAKE      = 0x02,
	ME             = 0x04,
	UNKNOWN        = 0x08,
	REJECT         = 0x10,
	SERVER         = 0x20,
	CLIENT         = 0x40,
};

enum flags
{
	PINGSENT       = 0x00000001, // Unreplied ping sent
	DEAD           = 0x00000002, // Local socket is dead--Exiting soon
	KILLED         = 0x00000004, // Prevents "QUIT" from being sent for this
	SENTUSER       = 0x00000008, // Client sent a USER command.
	CLICAP         = 0x00000010, // In CAP negotiation, wait for CAP END
	CLOSING        = 0x00000020, // set when closing to suppress errors
	PING_COOKIE    = 0x00000040, // has sent ping cookie
	GOTID          = 0x00000080, // successful ident lookup achieved
	FLOODDONE      = 0x00000100, // flood grace period over / reported
	NORMALEX       = 0x00000200, // Client exited normally
	MARK           = 0x00000400, // marked client
	HIDDEN         = 0x00000800, // hidden server
	EOB            = 0x00001000, // EOB
	MYCONNECT      = 0x00002000, // MyConnect
	IOERROR        = 0x00004000, // IO error
	SERVICE        = 0x00008000, // network service
	TGCHANGE       = 0x00010000, // we're allowed to clear something
	DYNSPOOF       = 0x00020000, // dynamic spoof, only opers see ip
	TGEXCESSIVE    = 0x00040000, // whether the client has attemped to change targets excessively fast
	CLICAP_DATA    = 0x00080000, // requested CAP LS 302
	EXTENDCHANS    = 0x00100000,
	EXEMPTRESV     = 0x00200000,
	EXEMPTKLINE    = 0x00400000,
	EXEMPTFLOOD    = 0x00800000,
	IP_SPOOFING    = 0x01000000,
	EXEMPTSPAMBOT  = 0x02000000,
	EXEMPTSHIDE    = 0x04000000,
	EXEMPTJUPE     = 0x08000000,
};

// we store ipv6 ips for remote clients, so this needs to be v6 always
constexpr auto PASSWDLEN = 128;
constexpr auto CIPHERKEYLEN = 64; // 512bit
constexpr auto CLIENT_BUFSIZE = 512; // must be at least 512 bytes
constexpr auto IDLEN = 10;

struct client
{
	rb_dlink_node node;
	serv::list::iterator lnode;
	std::shared_ptr<user::user> user;   // ...defined, if this is a user
	std::shared_ptr<serv::serv> serv;   // ...defined, if this is a server
	struct client *servptr;	/* Points to server this Client is on */
	struct client *from;	/* == self, if Local Client, *NEVER* NULL! */

	rb_dlink_list whowas_clist;

	time_t tsinfo;		/* TS on the nick, SVINFO on server */
	mode::mode mode;
	uint64_t flags;		/* client flags */

	unsigned int snomask;	/* server notice mask */

	int hopcount;		/* number of servers to this 0 = local */
	enum status status;     // Client type
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
	rb_dlink_list on_allow_list;

	time_t first_received_message_time;
	int received_number_of_privmsgs;
	int flood_noticed;

	struct LocalUser *localClient;
	struct PreClient *preClient;

	time_t large_ctcp_sent; /* ctcp to large group sent, relax flood checks */
	char *certfp; /* client certificate fingerprint */

	client();
	client(const client &) = delete;
	client &operator=(const client &) = delete;
	~client() noexcept;
};

struct ZipStats
{
	unsigned long long in;
	unsigned long long in_wire;
	unsigned long long out;
	unsigned long long out_wire;
	double in_ratio;
	double out_ratio;
};

typedef int SSL_OPEN_CB(client *, int status);

struct LocalUser
{
	rb_dlink_node tnode;	/* This is the node for the local list type the client is on */
	rb_dlink_list connids;	/* This is the list of connids to free */

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
	 *
	 * We have modern conveniences. Let's use uint32_t. --Elizafox
	 */
	uint32_t sendM;		/* Statistics: protocol messages send */
	uint32_t sendK;		/* Statistics: total k-bytes send */
	uint32_t receiveM;	/* Statistics: protocol messages received */
	uint32_t receiveK;	/* Statistics: total k-bytes received */
	uint16_t sendB;		/* counters to count upto 1-k lots of bytes */
	uint16_t receiveB;	/* sent and received. */
	struct Listener *listener;	/* listener accepted from */
	struct ConfItem *att_conf;	/* attached conf */
	struct server_conf *att_sconf;

	struct rb_sockaddr_storage ip;
	time_t last_nick_change;
	int number_of_nick_changes;

	/*
	 * XXX - there is no reason to save this, it should be checked when it's
	 * received and not stored, this is not used after registration
	 *
	 * agreed. lets get rid of it someday! --nenolod
	 */
	char *passwd;
	char *auth_user;
	char *opername; /* name of operator{} block being used or tried (challenge) */
	char *challenge;
	char *fullcaps;
	char *cipher_string;

	int caps;		/* capabilities bit-field */
	rb_fde_t *F;		/* >= 0, for local clients */

	/* time challenge response is valid for */
	time_t chal_time;

	time_t next_away;	/* Don't allow next away before... */
	time_t last;

	/* clients allowed to talk through +g */
	rb_dlink_list allow_list;

	/* nicknames theyre monitoring */
	rb_dlink_list monitor_list;

	/*
	 * Anti-flood stuff. We track how many messages were parsed and how
	 * many we were allowed in the current second, and apply a simple decay
	 * to avoid flooding.
	 *   -- adrian
	 */
	int sent_parsed;	/* how many messages we've parsed in this second */
	time_t last_knock;	/* time of last knock */
	uint32_t random_ping;

	/* target change stuff */
	/* targets we're aware of (fnv32(use_id(target_p))):
	 * 0..TGCHANGE_NUM-1 regular slots
	 * TGCHANGE_NUM..TGCHANGE_NUM+TGCHANGE_REPLY-1 reply slots
	 */
	uint32_t targets[tgchange::NUM + tgchange::REPLY];
	unsigned int targets_free;	/* free targets */
	time_t target_last;		/* last time we cleared a slot */

	/* ratelimit items */
	time_t ratelimit;
	unsigned int join_who_credits;

	struct ListClient *safelist_data;

	char *mangledhost; /* non-NULL if host mangling module loaded and
			      applicable to this client */

	struct _ssl_ctl *ssl_ctl;		/* which ssl daemon we're associate with */
	struct _ssl_ctl *z_ctl;			/* second ctl for ssl+zlib */
	struct ws_ctl *ws_ctl;			/* ctl for wsockd */
	SSL_OPEN_CB *ssl_callback;		/* ssl connection is now open */
	uint32_t localflags;
	struct ZipStats *zipstats;		/* zipstats */
	struct ev_entry *event;			/* used for associated events */

	struct PrivilegeSet *privset;		/* privset... */

	char sasl_agent[IDLEN];
	unsigned char sasl_out;
	unsigned char sasl_complete;

	unsigned int sasl_messages;
	unsigned int sasl_failures;
	time_t sasl_next_retry;
};


#define AUTHC_F_DEFERRED 0x01
#define AUTHC_F_COMPLETE 0x02

struct AuthClient
{
	uint32_t cid;	/* authd id */
	time_t timeout;	/* When to terminate authd query */
	bool accepted;	/* did authd accept us? */
	char cause;	/* rejection cause */
	char *data;	/* reason data */
	char *reason;	/* reason we were rejected */
	int flags;
};

struct PreClient
{
	char spoofnick[NICKLEN + 1];
	char spoofuser[USERLEN + 1];
	char spoofhost[HOSTLEN + 1];

	struct AuthClient auth;

	struct rb_sockaddr_storage lip; /* address of our side of the connection */
};

struct ListClient
{
	char *chname;
	unsigned int users_min, users_max;
	time_t created_min, created_max, topic_min, topic_max;
	int operspy;
};


inline bool
is(const client &client, const mode::mode &mode)
{
	return is(client.mode, mode);
}

inline void
set(client &client, const mode::mode &mode)
{
	return set(client.mode, mode);
}

inline void
clear(client &client, const mode::mode &mode)
{
	return clear(client.mode, mode);
}

inline void
set_got_id(client &client)
{
	client.flags |= flags::GOTID;
}

inline bool
is_got_id(const client &client)
{
	return (client.flags & flags::GOTID) != 0;
}


inline bool
is_exempt_kline(const client &client)
{
	return client.flags & flags::EXEMPTKLINE;
}

inline void
set_exempt_kline(client &client)
{
	client.flags |= flags::EXEMPTKLINE;
}

inline bool
is_exempt_flood(const client &client)
{
	return client.flags & flags::EXEMPTFLOOD;
}

inline void
set_exempt_flood(client &client)
{
	client.flags |= flags::EXEMPTFLOOD;
}

inline bool
is_exempt_spambot(const client &client)
{
	return client.flags & flags::EXEMPTSPAMBOT;
}

inline void
set_exempt_spambot(client &client)
{
	client.flags |= flags::EXEMPTSPAMBOT;
}

inline bool
is_exempt_shide(const client &client)
{
	return client.flags & flags::EXEMPTSHIDE;
}

inline void
set_exempt_shide(client &client)
{
	client.flags |= flags::EXEMPTSHIDE;
}

inline bool
is_exempt_jupe(const client &client)
{
	return client.flags & flags::EXEMPTJUPE;
}

inline void
set_exempt_jupe(client &client)
{
	client.flags |= flags::EXEMPTJUPE;
}

inline bool
is_exempt_resv(const client &client)
{
	return client.flags & flags::EXEMPTRESV;
}

inline void
set_exempt_resv(client &client)
{
	client.flags |= flags::EXEMPTRESV;
}

inline bool
is_ip_spoof(const client &client)
{
	return client.flags & flags::IP_SPOOFING;
}

inline void
set_ip_spoof(client &client)
{
	client.flags |= flags::IP_SPOOFING;
}

inline bool
is_extend_chans(const client &client)
{
	return client.flags & flags::EXTENDCHANS;
}

inline void
set_extend_chans(client &client)
{
	client.flags |= flags::EXTENDCHANS;
}

inline bool
is_client(const client &client)
{
	return client.status == status::CLIENT;
}

inline bool
is_registered_user(const client &client)
{
	return client.status == status::CLIENT;
}

inline bool
is_registered(const client &client)
{
	return client.status > status::UNKNOWN && client.status != status::REJECT;
}

inline bool
is_connecting(const client &client)
{
	return client.status == status::CONNECTING;
}

inline bool
is_handshake(const client &client)
{
	return client.status == status::HANDSHAKE;
}

inline bool
is_me(const client &client)
{
	return client.status == status::ME;
}

inline bool
is_unknown(const client &client)
{
	return client.status == status::UNKNOWN;
}

inline bool
is_server(const client &client)
{
	return client.status == status::SERVER;
}

inline bool
is_reject(const client &client)
{
	return client.status == status::REJECT;
}

inline bool
is_any_server(const client &client)
{
	return is_server(client) || is_handshake(client) || is_connecting(client);
}

inline void
set_reject(client &client)
{
	client.status = status::REJECT;
	client.handler = UNREGISTERED_HANDLER;
}

inline void
set_connecting(client &client)
{
	client.status = status::CONNECTING;
	client.handler = UNREGISTERED_HANDLER;
}

inline void
set_handshake(client &client)
{
	client.status = status::HANDSHAKE;
	client.handler = UNREGISTERED_HANDLER;
}

inline void
set_me(client &client)
{
	client.status = status::ME;
	client.handler = UNREGISTERED_HANDLER;
}

inline void
set_unknown(client &client)
{
	client.status = status::UNKNOWN;
	client.handler = UNREGISTERED_HANDLER;
}

inline void
set_server(client &client)
{
	client.status = status::SERVER;
	client.handler = SERVER_HANDLER;
}

bool is_oper(const client &);

inline void
set_client(client &client)
{
	client.status = status::CLIENT;
	client.handler = is_oper(client)? OPER_HANDLER : CLIENT_HANDLER;
}

inline void
set_remote_client(client &client)
{
	client.status = status::CLIENT;
	client.handler = RCLIENT_HANDLER;
}

inline bool
parse_as_client(const client &client)
{
	return bool(client.status & (status::UNKNOWN | status::CLIENT));
}

inline bool
parse_as_server(const client &client)
{
	return bool(client.status & (status::CONNECTING | status::HANDSHAKE | status::SERVER));
}

/*
 * ts stuff
 */
#define TS_CURRENT	6
#define TS_MIN          6

#define TS_DOESTS       0x10000000
#define DoesTS(x)       ((x)->tsinfo & TS_DOESTS)

/* if target is TS6, use id if it has one, else name */

inline bool
has_id(const client &client)
{
	return client.id[0] != '\0';
}

inline bool
has_id(const client *const &client)
{
	return has_id(*client);
}

inline const char *
use_id(const client &client)
{
	return has_id(client)? client.id : client.name;
}

inline const char *
use_id(const client *const &client)
{
	return use_id(*client);
}

bool is_server(const client &client);
inline char *get_id(const client *const &source, const client *const &target)
{
	return const_cast<char *>(is_server(*target->from) && has_id(target->from) ? use_id(source) : (source)->name);
}

inline auto get_id(const client &source, const client &target)
{
	return get_id(&source, &target);
}



/* flags for local clients, this needs stuff moved from above to here at some point */
#define LFLAGS_SSL		0x00000001
#define LFLAGS_FLUSH		0x00000002


inline bool
is_person(const client &client)
{
	return is_client(client) && client.user;
}

inline bool
my_connect(const client &client)
{
	return client.flags & flags::MYCONNECT;
}

inline void
set_my_connect(client &client)
{
	client.flags |= flags::MYCONNECT;
}

inline void
clear_my_connect(client &client)
{
	client.flags &= ~flags::MYCONNECT;
}

inline bool
my(const client &client)
{
	return my_connect(client) && is_client(client);
}

inline bool
my_oper(const client &client)
{
	return my_connect(client) && is_oper(client);
}

inline bool
is_oper(const client &client)
{
	return is(client, mode::OPER);
}

inline void
set_oper(client &client)
{
	set(client, mode::OPER);
	if (my(client))
		client.handler = OPER_HANDLER;
}

inline void
clear_oper(client &client)
{
	clear(client, mode::OPER);
	clear(client, mode::ADMIN);
	if (my(client) && !is_server(client))
		client.handler = CLIENT_HANDLER;
}

inline void
set_mark(client &client)
{
	client.flags |= flags::MARK;
}

inline void
clear_mark(client &client)
{
	client.flags &= ~flags::MARK;
}

inline bool
is_marked(const client &client)
{
	return client.flags & flags::MARK;
}

inline void
set_hidden(client &client)
{
	client.flags |= flags::HIDDEN;
}

inline void
clear_hidden(client &client)
{
	client.flags &= ~flags::HIDDEN;
}

inline bool
is_hidden(const client &client)
{
	return client.flags & flags::HIDDEN;
}

inline void
clear_eob(client &client)
{
	client.flags &= ~flags::EOB;
}

inline void
set_eob(client &client)
{
	client.flags |= flags::EOB;
}

inline bool
has_sent_eob(const client &client)
{
	return client.flags & flags::EOB;
}

inline void
set_dead(client &client)
{
	client.flags |= flags::DEAD;
}

inline bool
is_dead(const client &client)
{
	return client.flags &  flags::DEAD;
}

inline void
set_closing(client &client)
{
	client.flags |= flags::CLOSING;
}

inline bool
is_closing(const client &client)
{
	return client.flags & flags::CLOSING;
}

inline void
set_io_error(client &client)
{
	client.flags |= flags::IOERROR;
}

inline bool
is_io_error(const client &client)
{
	return client.flags & flags::IOERROR;
}

inline bool
is_any_dead(const client &client)
{
	return is_io_error(client) || is_dead(client) || is_closing(client);
}

inline void
set_tg_change(client &client)
{
	client.flags |= flags::TGCHANGE;
}

inline void
clear_tg_change(client &client)
{
	client.flags &= ~flags::TGCHANGE;
}

inline bool
is_tg_change(const client &client)
{
	return client.flags & flags::TGCHANGE;
}

inline void
set_dyn_spoof(client &client)
{
	client.flags |= flags::DYNSPOOF;
}

inline void
clear_dyn_spoof(client &client)
{
	client.flags &= ~flags::DYNSPOOF;
}

inline bool
is_dyn_spoof(const client &client)
{
	return client.flags & flags::DYNSPOOF;
}

inline void
set_tg_excessive(client &client)
{
	client.flags|= flags::TGEXCESSIVE;
}

inline void
clear_tg_excessive(client &client)
{
	client.flags &= ~flags::TGEXCESSIVE;
}

inline bool
is_tg_excessive(const client &client)
{
	return client.flags & flags::TGEXCESSIVE;
}

inline void
set_flood_done(client &client)
{
	client.flags |= flags::FLOODDONE;
}

inline bool
is_flood_done(const client &client)
{
	return client.flags & flags::FLOODDONE;
}

inline bool
operator==(const client &a, const client &b)
{
	return &a == &b;
}


/* local flags */

#define IsSSL(x)		((x)->localClient->localflags & LFLAGS_SSL)
#define SetSSL(x)		((x)->localClient->localflags |= LFLAGS_SSL)
#define ClearSSL(x)		((x)->localClient->localflags &= ~LFLAGS_SSL)

#define IsFlush(x)		((x)->localClient->localflags & LFLAGS_FLUSH)
#define SetFlush(x)		((x)->localClient->localflags |= LFLAGS_FLUSH)
#define ClearFlush(x)		((x)->localClient->localflags &= ~LFLAGS_FLUSH)

/*
 * definitions for get_client_name
 */
#define HIDE_IP 0
#define SHOW_IP 1
#define MASK_IP 2

extern void check_banned_lines(void);
extern void check_klines_event(void *unused);
extern void check_klines(void);
extern void check_dlines(void);
extern void check_xlines(void);
extern void resv_nick_fnc(const char *mask, const char *reason, int temp_time);

extern const char *get_client_name(client *client, int show_ip);
extern const char *log_client_name(client *, int);
extern int is_remote_connect(client *);
void init(void);
extern client *make_client(client *from);
extern void free_pre_client(client *client);
extern void free_client(client *client);

extern int exit_client(client *, client *, client *, const char *);

extern void error_exit_client(client *, int);

extern void count_local_client_memory(size_t * count, size_t * memory);
extern void count_remote_client_memory(size_t * count, size_t * memory);

extern int clean_nick(const char *, int loc_client);

extern client *find_chasing(client *, const char *, int *);
extern client *find_person(const char *);

extern client *find_named_person(const char *);
client *find_named_person(const std::string &);

extern client *next_client(client *, const char *);

#define accept_message(s, t) ((s) == (t) || (rb_dlinkFind((s), &((t)->localClient->allow_list))))
extern void del_all_accepts(client *client_p);

extern void dead_link(client *client_p, int sendqex);
extern int show_ip(client *source_p, client *target_p);
extern int show_ip_conf(struct ConfItem *aconf, client *source_p);
extern int show_ip_whowas(struct Whowas *whowas, client *source_p);

extern void close_connection(client *);
extern void init_uid(void);
extern char *generate_uid(void);

uint32_t connid_get(client *client_p);
void connid_put(uint32_t id);
void client_release_connids(client *client_p);

user::user &make_user(client &, const std::string &login = {});
serv::serv &make_serv(client &);

}      // namespace client

using client::ZipStats;
using client::LocalUser;
using client::ListClient;

const client::user::user &user(const client::client &client);
client::user::user &user(client::client &client);

const client::serv::serv &serv(const client::client &client);
client::serv::serv &serv(client::client &client);

}      // namespace ircd
#endif // __cplusplus
