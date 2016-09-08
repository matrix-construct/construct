/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
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
 */

#pragma once
#define HAVE_IRCD_CONF_H

#ifdef __cplusplus
namespace ircd {
namespace conf {

IRCD_EXCEPTION(ircd::error, error)
IRCD_EXCEPTION(error, already_exists)
IRCD_EXCEPTION(error, bad_topconf)
IRCD_EXCEPTION(error, not_found)
IRCD_EXCEPTION(error, bad_cast)

struct item
{
	uint8_t *ptr;
	std::type_index type;

	template<class T> item(T *ptr);
};

struct top
:cmd
{
	using items = std::initializer_list<std::pair<std::string, item>>;

	char letter;
	std::string name;
	std::unordered_map<std::string, item> map;

	// Override to provide custom type handling
	virtual void assign(item &item, std::string val) const;

	// Override to provide operations on singleton blocks
	virtual const uint8_t *get(client::client &, const std::string &key) const;
	virtual void set(client::client &, std::string key, std::string val);
	virtual void del(client::client &, const std::string &key);
	virtual void enu(client::client &, const std::string &key);

	// Override to provide operations on named blocks
	virtual const uint8_t *get(client::client &, const std::string &label, const std::string &key) const;
	virtual void set(client::client &, std::string label, std::string key, std::string val);
	virtual void del(client::client &, const std::string &label, const std::string &key);
	virtual void enu(client::client &, const std::string &label, const std::string &key);

	// Override to handle the raw line
	virtual void operator()(client::client &, line) override;

	top(const char &letter, const std::string &name, const items & = {});
	~top() noexcept;
};

namespace newconf
{
	extern topconf current;    // The latest newconf parse map after startup or rehash
	extern topconf last;       // The current is moved to last for differentiation
}

extern struct log::log log;
extern std::array<top *, 256> confs;

// Dynamic casting closure
using type_handler = std::function<void (uint8_t *const &ptr, std::string text)>;
extern std::map<std::type_index, type_handler> type_handlers;

template<class T> std::type_index make_index();
template<class T = std::string> const T &get(client::client &, const char &, const std::string &label, const std::string &key);
template<class T = std::string> const T &get(client::client &, const char &, const std::string &key);

void execute();
void parse(const std::string &path);


template<class T>
const T &
get(client::client &client,
    const char &letter,
    const std::string &key)
{
	const uint8_t &idx(letter);
	const auto &conf(confs[idx]);
	if(unlikely(!conf))
		throw not_found("conf[%c] is not registered", letter);

	return *reinterpret_cast<const T *>(conf->get(client, key));
}

template<class T>
const T &
get(client::client &client,
    const char &letter,
    const std::string &label,
    const std::string &key)
{
	const uint8_t &idx(letter);
	const auto &conf(confs[idx]);
	if(unlikely(!conf))
		throw not_found("conf[%c] is not registered", letter);

	return *reinterpret_cast<const T *>(conf->get(client, label, key));
}

template<class T>
std::type_index
make_index()
{
	return typeid(typename std::add_pointer<T>::type);
}

template<class T>
item::item(T *ptr):
ptr{reinterpret_cast<uint8_t *>(ptr)},
type{typeid(ptr)}
{
}

}      // namespace conf
}      // namespace ircd
#endif // __cplusplus















#ifdef __cplusplus
namespace ircd {
namespace conf {



#define NOT_AUTHORISED  (-1)
#define I_SOCKET_ERROR  (-2)
#define I_LINE_FULL     (-3)
#define BANNED_CLIENT   (-4)
#define TOO_MANY_LOCAL	(-6)
#define TOO_MANY_GLOBAL (-7)
#define TOO_MANY_IDENT	(-8)

#define CONF_ILLEGAL		0x80000000
#define CONF_CLIENT		0x0002
#define CONF_KILL		0x0040
#define CONF_XLINE		0x0080
#define CONF_RESV_CHANNEL	0x0100
#define CONF_RESV_NICK		0x0200
#define CONF_RESV		(CONF_RESV_CHANNEL | CONF_RESV_NICK)

#define CONF_DLINE		0x020000
#define CONF_EXEMPTDLINE	0x100000

#define IsIllegal(x)    ((x)->status & CONF_ILLEGAL)

/* aConfItem->flags */

/* Generic flags... */
#define CONF_FLAGS_TEMPORARY		0x00800000
#define CONF_FLAGS_NEED_SSL		0x00000002
#define CONF_FLAGS_MYOPER		0x00080000	/* need to rewrite info.oper on burst */
/* auth{} flags... */
#define CONF_FLAGS_NO_TILDE		0x00000004
#define CONF_FLAGS_NEED_IDENTD		0x00000008
#define CONF_FLAGS_EXEMPTKLINE		0x00000040
#define CONF_FLAGS_NOLIMIT		0x00000080
#define CONF_FLAGS_SPOOF_IP		0x00000200
#define CONF_FLAGS_SPOOF_NOTICE		0x00000400
#define CONF_FLAGS_REDIR		0x00000800
#define CONF_FLAGS_EXEMPTRESV		0x00002000	/* exempt from resvs */
#define CONF_FLAGS_EXEMPTFLOOD		0x00004000
#define CONF_FLAGS_EXEMPTSPAMBOT	0x00008000
#define CONF_FLAGS_EXEMPTSHIDE		0x00010000
#define CONF_FLAGS_EXEMPTJUPE		0x00020000	/* exempt from resv generating warnings */
#define CONF_FLAGS_NEED_SASL		0x00040000
#define CONF_FLAGS_EXTEND_CHANS		0x00080000
#define CONF_FLAGS_ENCRYPTED		0x00200000
#define CONF_FLAGS_EXEMPTDNSBL		0x04000000
#define CONF_FLAGS_EXEMPTPROXY		0x08000000


struct ConfItem
{
	unsigned int status;	/* If CONF_ILLEGAL, delete when no clients */
	unsigned int flags;
	int clients;		/* Number of *LOCAL* clients using this */
	union
	{
		char *name;	/* IRC name, nick, server name, or original u@h */
		const char *oper;
	} info;
	char *host;		/* host part of user@host */
	char *passwd;		/* doubles as kline reason *ugh* */
	char *spasswd;		/* Password to send. */
	char *user;		/* user part of user@host */
	int port;
	time_t hold;		/* Hold action until this time (calendar time) */
	time_t created;		/* Creation time (for klines etc) */
	time_t lifetime;	/* Propagated lines: remember until this time */
	char *className;	/* Name of class */
	struct Class *c_class;	/* Class of connection */
	rb_patricia_node_t *pnode;	/* Our patricia node */
};

/* Macros for struct ConfItem */
#define IsConfBan(x)		((x)->status & (CONF_KILL|CONF_XLINE|CONF_DLINE|\
						CONF_RESV_CHANNEL|CONF_RESV_NICK))

#define IsNoTilde(x)            ((x)->flags & CONF_FLAGS_NO_TILDE)
#define IsNeedIdentd(x)         ((x)->flags & CONF_FLAGS_NEED_IDENTD)
#define IsConfExemptKline(x)    ((x)->flags & CONF_FLAGS_EXEMPTKLINE)
#define IsConfExemptLimits(x)   ((x)->flags & CONF_FLAGS_NOLIMIT)
#define IsConfExemptFlood(x)    ((x)->flags & CONF_FLAGS_EXEMPTFLOOD)
#define IsConfExemptSpambot(x)	((x)->flags & CONF_FLAGS_EXEMPTSPAMBOT)
#define IsConfExemptShide(x)	((x)->flags & CONF_FLAGS_EXEMPTSHIDE)
#define IsConfExemptJupe(x)	((x)->flags & CONF_FLAGS_EXEMPTJUPE)
#define IsConfExemptResv(x)	((x)->flags & CONF_FLAGS_EXEMPTRESV)
#define IsConfDoSpoofIp(x)      ((x)->flags & CONF_FLAGS_SPOOF_IP)
#define IsConfSpoofNotice(x)    ((x)->flags & CONF_FLAGS_SPOOF_NOTICE)
#define IsConfEncrypted(x)      ((x)->flags & CONF_FLAGS_ENCRYPTED)
#define IsNeedSasl(x)		((x)->flags & CONF_FLAGS_NEED_SASL)
#define IsConfExemptDNSBL(x)	((x)->flags & CONF_FLAGS_EXEMPTDNSBL)
#define IsConfExemptProxy(x)	((x)->flags & CONF_FLAGS_EXEMPTPROXY)
#define IsConfExtendChans(x)	((x)->flags & CONF_FLAGS_EXTEND_CHANS)
#define IsConfSSLNeeded(x)	((x)->flags & CONF_FLAGS_NEED_SSL)

/* flag definitions for opers now in client.h */

struct config_file_entry
{
	const char *dpath;	/* DPATH if set from command line */
	const char *configfile;

	char *default_operstring;
	char *default_adminstring;
	char *servicestring;
	char *kline_reason;

	char *identifyservice;
	char *identifycommand;

	char *sasl_service;

	unsigned char compression_level;
	int disable_fake_channels;
	int dots_in_ident;
	int failed_oper_notice;
	int anti_nick_flood;
	int anti_spam_exit_message_time;
	int max_accept;
	int max_monitor;
	int max_nick_time;
	int max_nick_changes;
	int ts_max_delta;
	int ts_warn_delta;
	int dline_with_reason;
	int kline_with_reason;
	int kline_delay;
	int warn_no_nline;
	int nick_delay;
	int non_redundant_klines;
	int stats_e_disabled;
	int stats_c_oper_only;
	int stats_y_oper_only;
	int stats_h_oper_only;
	int stats_o_oper_only;
	int stats_k_oper_only;
	int stats_i_oper_only;
	int stats_P_oper_only;
	int map_oper_only;
	int operspy_admin_only;
	int pace_wait;
	int pace_wait_simple;
	int short_motd;
	int no_oper_flood;
	int hide_server;
	int hide_spoof_ips;
	int hide_error_messages;
	int client_exit;
	int oper_only_umodes;
	int oper_umodes;
	int oper_snomask;
	int max_targets;
	int caller_id_wait;
	int min_nonwildcard;
	int min_nonwildcard_simple;
	int default_floodcount;
	int default_ident_timeout;
	int ping_cookie;
	int tkline_expire_notices;
	int use_whois_actually;
	int disable_auth;
	int connect_timeout;
	int burst_away;
	int reject_ban_time;
	int reject_after_count;
	int reject_duration;
	int throttle_count;
	int throttle_duration;
	int target_change;
	int collision_fnc;
	int resv_fnc;
	int default_umodes;
	int global_snotices;
	int operspy_dont_care_user_info;
	int use_propagated_bans;
	int max_ratelimit_tokens;
	int away_interval;

	int client_flood_max_lines;
	int client_flood_burst_rate;
	int client_flood_burst_max;
	int client_flood_message_time;
	int client_flood_message_num;

	unsigned int nicklen;
	int certfp_method;

	int hide_opers_in_whois;
};

struct config_channel_entry
{
	int use_except;
	int use_invex;
	int use_forward;
	int use_knock;
	int knock_delay;
	int knock_delay_channel;
	int max_bans;
	int max_bans_large;
	int max_chans_per_user;
	int max_chans_per_user_large;
	int no_create_on_split;
	int no_join_on_split;
	int default_split_server_count;
	int default_split_user_count;
	int burst_topicwho;
	int kick_on_split_riding;
	int only_ascii_channels;
	int resv_forcepart;
	int channel_target_change;
	int disable_local_channels;
	unsigned int autochanmodes;
	int displayed_usercount;
	int strip_topic_colors;
};

struct config_server_hide
{
	int flatten_links;
	int links_delay;
	int hidden;
	int disable_hidden;
};

struct admin_info
{
	char *name;
	char *description;
	char *email;
};

struct alias_entry
{
	std::string name;
	std::string target;
	int flags;			/* reserved for later use */
};

/* All variables are GLOBAL */
extern struct config_file_entry ConfigFileEntry;	/* defined in ircd.c */
extern struct config_channel_entry ConfigChannel;	/* defined in channel.c */
extern struct config_server_hide ConfigServerHide;	/* defined in s_conf.c */
extern struct server_info ServerInfo;	/* defined in ircd.c */
extern struct admin_info AdminInfo;	/* defined in ircd.c */
/* End GLOBAL section */

extern rb_dlink_list service_list;

extern rb_dlink_list prop_bans;

typedef enum temp_list
{
	TEMP_MIN,
	TEMP_HOUR,
	TEMP_DAY,
	TEMP_WEEK,
	LAST_TEMP_TYPE
} temp_list;

extern rb_dlink_list temp_klines[LAST_TEMP_TYPE];
extern rb_dlink_list temp_dlines[LAST_TEMP_TYPE];
extern unsigned long cidr_to_bitmask[];

void init_s_conf(void);

struct ConfItem *make_conf(void);
void free_conf(struct ConfItem *);

rb_dlink_node *find_prop_ban(unsigned int status, const char *user, const char *host);
void deactivate_conf(struct ConfItem *, rb_dlink_node *, time_t);
void replace_old_ban(struct ConfItem *);

void read_conf_files(bool cold);

int attach_conf(client::client *, struct ConfItem *);
int check_client(client::client *client_p, client::client *source_p, const char *);

int detach_conf(client::client *);

struct ConfItem *find_tkline(const char *, const char *, struct sockaddr *);
char *show_iline_prefix(client::client *, struct ConfItem *, char *);
void get_printable_conf(struct ConfItem *,
			       char **, char **, const char **, char **, int *, char **);
char *get_user_ban_reason(struct ConfItem *aconf);
void get_printable_kline(client::client *, struct ConfItem *,
				char **, char **, char **, char **);

int conf_yy_fatal_error(const char *);
int conf_fgets(char *, int, FILE *);

int valid_wild_card(const char *, const char *);
void add_temp_kline(struct ConfItem *);
void add_temp_dline(struct ConfItem *);
void report_temp_klines(client::client *);
void show_temp_klines(client::client *, rb_dlink_list *);

bool rehash(bool);
void rehash_bans(void);

int conf_add_server(struct ConfItem *, int);
void conf_add_class_to_conf(struct ConfItem *);
void conf_add_me(struct ConfItem *);
void conf_add_class(struct ConfItem *, int);
void conf_add_d_conf(struct ConfItem *);
void flush_expired_ips(void *);

char *get_oper_name(client::client *client_p);

}      // namespace conf
}      // namespace ircd
#endif // __cplusplus
