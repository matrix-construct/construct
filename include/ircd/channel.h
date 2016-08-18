/*
 *  ircd-ratbox: A slightly useful ircd.
 *  channel.h: The ircd channel header.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
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
#define HAVE_IRCD_CHANNEL_H

#ifdef __cplusplus
namespace ircd {
namespace chan {

enum status
{
	PEON                = 0x0000,          // normal member of channel
	VOICE               = 0x0001,	       // the power to speak
	CHANOP              = 0x0002,          // Channel operator
	BANNED              = 0x0008,          // cached as banned
	QUIETED             = 0x0010,          // cached as being +q victim
	ONLY_OPERS          = 0x0020,
	ONLY_SERVERS        = 0x0040,
	ONLY_CHANOPS        = CHANOP,
	ONLY_CHANOPSVOICED  = CHANOP | VOICE,
	ALL_MEMBERS         = PEON,
};

struct topic
{
	std::string text;
	std::string info;
	time_t time = 0;

	operator bool() const;
};

struct ban
{
	static constexpr auto LEN = 195;

	std::string banstr;
	std::string who;
	std::string forward;
	time_t when;
	rb_dlink_node node = {0};

	ban(const std::string &banstr, const std::string &who, const std::string &forward);
};

struct modes
{
	static constexpr size_t KEYLEN = 24; // 23+1 for \0

	uint mode;
	int limit;
	char key[KEYLEN];
	uint join_num;
	uint join_time;
	char forward[LOC_CHANNELLEN + 1];

	modes();
};

struct membership
{
	rb_dlink_node channode;
	rb_dlink_node locchannode;
	rb_dlink_node usernode;

	struct chan *chan;
	Client *client;
	unsigned int flags;

	time_t bants;

	membership();
	~membership() noexcept;
};

bool is_chanop(const membership &);
bool is_chanop(const membership *const &);
bool is_voiced(const membership &);
bool is_voiced(const membership *const &);
bool is_chanop_voiced(const membership &);
bool is_chanop_voiced(const membership *const &);
bool can_send_banned(const membership &);
bool can_send_banned(const membership *const &);
const char *find_status(const membership *const &msptr, const int &combine);

struct chan
{
	std::string name;
	struct modes mode;
	std::string mode_lock;
	struct topic topic;

	rb_dlink_list members;
	rb_dlink_list locmembers;

	rb_dlink_list invites;
	rb_dlink_list banlist;
	rb_dlink_list exceptlist;
	rb_dlink_list invexlist;
	rb_dlink_list quietlist;

	uint join_count;                     // joins within delta
	uint join_delta;                     // last ts of join
	uint flood_noticed;
	int received_number_of_privmsgs;
	time_t first_received_message_time;  // channel flood control
	time_t last_knock;                   // don't allow knock to flood
	time_t bants;
	time_t channelts;
	time_t last_checked_ts;
	client *last_checked_client;
	uint last_checked_type;
	int last_checked_result;

	rb_dlink_node node = {0};

	chan(const std::string &name);
	~chan() noexcept;
};

bool secret(const chan &);
bool secret(const chan *const &);
bool hidden(const chan &);
bool hidden(const chan *const &);
bool pub(const chan &);
bool pub(const chan *const &);
bool can_show(const chan &, const client &);
bool can_show(const chan *const &, const client *const &);
bool is_member(const chan &c, const client &);
bool is_member(const chan *const &, const client *const &);
bool is_name(const char *const &name);

enum : int
{
	CAN_SEND_NO     = 0,
	CAN_SEND_NONOP  = 1,
	CAN_SEND_OPV    = 2,
};
int can_send(chan *, client *, membership *);

bool flood_attack_channel(int p_or_n, client *source, chan *);
int is_banned(chan *, client *who, membership *, const char *, const char *, const char **);
int is_quieted(chan *, client *who, membership *, const char *, const char *);
int can_join(client *source, chan *, const char *key, const char **forward);
void add_user_to_channel(chan *, client *, int flags);
void remove_user_from_channel(membership *);
void remove_user_from_channels(client *);
void invalidate_bancache_user(client *);
void free_channel_list(rb_dlink_list *);
bool check_channel_name(const char *name);
void channel_member_names(chan *, client *, int show_eon);
void del_invite(chan *, client *who);
const char *channel_modes(chan *, client *who);
chan *find_bannickchange_channel(client *);
void check_spambot_warning(client *source, const char *name);
void check_splitmode(void *);
void set_channel_topic(chan *, const char *topic, const char *topic_info, time_t topicts);
void init_chcap_usage_counts(void);
void set_chcap_usage_counts(client *serv_p);
void unset_chcap_usage_counts(client *serv_p);
void send_cap_mode_changes(client *, client *source, chan *, mode::change foo[], int);
void resv_chan_forcepart(const char *name, const char *reason, int temp_time);
void set_channel_mode(client *, client *source, chan *, membership *, int parc, const char *parv[]);
void set_channel_mlock(client *, client *source, chan *, const char *newmlock, bool propagate);
bool add_id(client *source, chan *, const char *banid, const char *forward, rb_dlink_list * list, long mode_type);
ban *del_id(chan *, const char *banid, rb_dlink_list * list, long mode_type);
int match_extban(const char *banstr, client *, chan *, long mode_type);
int valid_extban(const char *banstr, client *, chan *, long mode_type);
const char * get_extban_string(void);
int get_channel_access(client *source, chan *, membership *, int dir, const char *modestr);
membership *find_channel_membership(chan *chptr, client *client_p);
void send_join(chan &, client &);

//extern std::map<std::string, std::unique_ptr<chan>> chans;
extern rb_dlink_list global_channel_list;


void init();


inline bool
secret(const chan &c)
{
	return c.mode.mode & mode::SECRET;
}

inline bool
secret(const chan *const &c)
{
	return c && secret(*c);
}

inline bool
hidden(const chan &c)
{
	return c.mode.mode & mode::PRIVATE;
}

inline bool
hidden(const chan *const &c)
{
	return c && hidden(*c);
}

inline bool
pub(const chan &c)
{
	return ~c.mode.mode & (mode::PRIVATE | mode::SECRET);
}

inline bool
pub(const chan *const &c)
{
	return !c || pub(*c);
}

inline bool
is_member(const chan &c, const client &client)
{
	//return client.user && get(c, client, std::nothrow) != nullptr;
	return find_channel_membership(const_cast<struct chan *>(&c), const_cast<Client *>(&client)) != nullptr;
}

inline bool
is_member(const chan *const &c, const client *const &client)
{
	return c && client && is_member(*c, *client);
}

inline bool
can_show(const chan &c, const client &client)
{
	return pub(c) || is_member(c, client);
}

inline bool
can_show(const chan *const &c, const client *const &client)
{
	return pub(c) || is_member(c, client);
}

inline bool
is_name(const char *const &name)
{
	return name && (*name == '#' || *name == '&');
}

inline bool
is_chanop(const membership &m)
{
	return m.flags & CHANOP;
}

inline bool
is_chanop(const membership *const &m)
{
	return m && is_chanop(*m);
}

inline bool
is_voiced(const membership &m)
{
	return m.flags & VOICE;
}

inline bool
is_voiced(const membership *const &m)
{
	return m && is_voiced(*m);
}

inline bool
is_chanop_voiced(const membership &m)
{
	return m.flags & (VOICE | CHANOP);
}

inline bool
is_chanop_voiced(const membership *const &m)
{
	return m && is_chanop_voiced(*m);
}

inline bool
can_send_banned(const membership &m)
{
	return m.flags & (BANNED | QUIETED);
}

inline bool
can_send_banned(const membership *const &m)
{
	return m && can_send_banned(*m);
}

inline bool
operator!(const topic &topic)
{
	return !bool(topic);
}

inline
topic::operator bool()
const
{
	return !text.empty();
}

}      // namespace chan
}      // namespace ircd
#endif // __cplusplus
