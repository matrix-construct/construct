/*
 *  ircd-ratbox: A slightly useful ircd.
 *  channel.h: The ircd channel header.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *  Copyright (C) 2004-2016 Charybdis Development Team
 *  Copyright (C) 2016 Jason Volk <jason@zemos.net>
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

IRCD_EXCEPTION(ircd::error,       error)
IRCD_EXCEPTION(error,             not_found)
IRCD_EXCEPTION(error,             invalid_argument)
IRCD_EXCEPTION(invalid_argument,  invalid_list)

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

	ban(const std::string &banstr,
	    const std::string &who      = {},
	    const std::string &forward  = {},
	    const time_t &when          = 0);
};

bool operator<(const ban &a, const ban &b);
using list = std::set<ban, std::less<ban>>;

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

uint operator&(const modes &, const mode::type &);
uint operator|(const modes &, const mode::type &);
uint operator~(const modes &);

struct membership
{
	using global = std::map<client::client *, membership>;
	using local = std::list<membership *>;

	uint flags;
	time_t bants;
	global::iterator git;
	local::iterator lit;

	membership(const uint &flags = 0);
	~membership() noexcept;
};

bool is_chanop(const membership &);
bool is_chanop(const membership *const &);
bool is_voiced(const membership &);
bool is_voiced(const membership *const &);
bool can_send_banned(const membership &);
bool can_send_banned(const membership *const &);
const char *find_status(const membership *const &msptr, const int &combine);
const client::client &get_client(const membership &);
client::client &get_client(membership &);

struct members
{
	membership::global global;
	membership::local local;

	members();
	~members() noexcept;
};

bool empty(const members &);
size_t size(const members &);
bool local_empty(const members &);
size_t local_size(const members &);
bool exists(const members &, const client::client &);
const membership *get(const members &, const client::client &, std::nothrow_t);
const membership &get(const members &, const client::client &);
membership *get(members &, const client::client &, std::nothrow_t);
membership &get(members &, const client::client &);

bool has_prefix(const char *const &name);
bool has_prefix(const std::string &name);
bool valid_name(const char *const &name);
bool valid_name(const std::string &name);

struct chan
{
	std::string name;
	struct modes mode;
	std::string mode_lock;
	struct topic topic;
	struct members members;
	std::set<client::client *> invites;
	list bans;
	list excepts;
	list invexs;
	list quiets;

	uint join_count;                     // joins within delta
	uint join_delta;                     // last ts of join
	uint flood_noticed;
	int received_number_of_privmsgs;
	time_t first_received_message_time;  // channel flood control
	time_t last_knock;                   // don't allow knock to flood
	time_t bants;
	time_t channelts;
	time_t last_checked_ts;
	const client::client *last_checked_client;
	mode::type last_checked_type;
	bool last_checked_result;

	chan(const std::string &name);
	~chan() noexcept;
};

const auto &name(const chan &);
bool is_secret(const chan &);
bool is_secret(const chan *const &);
bool is_hidden(const chan &);
bool is_hidden(const chan *const &);
bool is_public(const chan &);
bool is_public(const chan *const &);
bool is_member(const chan &c, const client::client &);
bool is_member(const chan *const &, const client::client *const &);
bool can_show(const chan &, const client::client &);
bool can_show(const chan *const &, const client::client *const &);

enum : int
{
	CAN_SEND_NO     = 0,
	CAN_SEND_NONOP  = 1,
	CAN_SEND_OPV    = 2,
};
int can_send(chan *, client::client *, membership *);
int can_join(client::client *source, chan *, const char *key, const char **forward);

bool cache_check(const chan &, const mode::type &, const client::client &, bool &result);
void cache_result(chan &, const mode::type &, const client::client &, const bool &result, membership *const &msptr = nullptr);
void cache_invalidate(chan &, const mode::type &, time_t time = 0);

const list &get(const chan &, const mode::type &);
list &get(chan &, const mode::type &);
size_t size(const chan &, const mode::type &);
bool empty(const chan &, const mode::type &);
size_t lists_size(const chan &);

struct check_data
{
	membership *msptr;
	const char *host;
	const char *iphost;
	const char **forward;
};
bool check(chan &, const mode::type &, const client::client &, check_data *const &data = nullptr);
const ban &get(const chan &, const mode::type &, const std::string &mask);
bool add(chan &, const mode::type &, const std::string &mask, client::client &source, const std::string &forward = {});
bool del(chan &, const mode::type &, const std::string &mask);

void del_invite(chan &, client::client &);
void clear_invites(chan &);

bool flood_attack_channel(int p_or_n, client::client *source, chan *);
void invalidate_bancache_user(client::client *);
void channel_member_names(chan *, client::client *, int show_eon);
const char *channel_modes(chan *, client::client *who);
chan *find_bannickchange_channel(client::client *);
void check_spambot_warning(client::client *source, const char *name);
void check_splitmode(void *);
void set_channel_topic(chan *, const char *topic, const char *topic_info, time_t topicts);
void init_chcap_usage_counts(void);
void set_chcap_usage_counts(client::client *serv_p);
void unset_chcap_usage_counts(client::client *serv_p);
void send_cap_mode_changes(client::client *, client::client *source, chan *, mode::change foo[], int);
void resv_chan_forcepart(const char *name, const char *reason, int temp_time);
void set_channel_mode(client::client *, client::client *source, chan *, membership *, int parc, const char *parv[]);
void set_channel_mlock(client::client *, client::client *source, chan *, const char *newmlock, bool propagate);
int match_extban(const char *banstr, client::client *, chan *, long mode_type);
int valid_extban(const char *banstr, client::client *, chan *, long mode_type);
const char * get_extban_string(void);
int get_channel_access(client::client *source, chan *, membership *, int dir, const char *modestr);
void send_join(chan &, client::client &);

auto empty(const chan &);
auto local_size(const chan &);
auto size(const chan &);
void add(chan &, client::client &, const int &flags = PEON);
void del(chan &, client::client &);
void del(client::client &);             // remove from all channels

// Channels
extern std::map<const std::string *, std::unique_ptr<chan>, rfc1459::less> chans;

bool exists(const std::string &name);
chan *get(const std::string &name, std::nothrow_t);
chan &get(const std::string &name);
chan &add(const std::string &name, client::client &);         // get or add (but does not join the client)
bool del(const std::string &name);
bool del(const chan &);

// Initialize subsystem
void init();


inline auto
size(const chan &chan)
{
	return size(chan.members);
}

inline auto
local_size(const chan &chan)
{
	return local_size(chan.members);
}

inline auto
empty(const chan &chan)
{
	return empty(chan.members);
}

inline bool
is_secret(const chan &c)
{
	return c.mode.mode & mode::SECRET;
}

inline bool
is_secret(const chan *const &c)
{
	return c && is_secret(*c);
}

inline bool
is_hidden(const chan &c)
{
	return c.mode.mode & mode::PRIVATE;
}

inline bool
is_hidden(const chan *const &c)
{
	return c && is_hidden(*c);
}

inline bool
is_public(const chan &c)
{
	return ~c.mode.mode & (mode::PRIVATE | mode::SECRET);
}

inline bool
is_public(const chan *const &c)
{
	return !c || is_public(*c);
}

inline bool
is_member(const chan &c, const client::client &client)
{
	return exists(c.members, client);
}

inline bool
is_member(const chan *const &c, const client::client *const &client)
{
	return c && client && is_member(*c, *client);
}

inline bool
can_show(const chan &c, const client::client &client)
{
	return is_public(c) || is_member(c, client);
}

inline bool
can_show(const chan *const &c, const client::client *const &client)
{
	return is_public(c) || is_member(c, client);
}

inline bool
has_prefix(const char *const &name)
{
	return name && rfc1459::is_chan_prefix(name[0]);
}

inline bool
has_prefix(const std::string &name)
{
	return !name.empty() && rfc1459::is_chan_prefix(name[0]);
}

inline bool
valid_name(const char *const &name)
{
	return name && name[0] && std::all_of(name, name + strlen(name), rfc1459::is_chan);
}

inline bool
valid_name(const std::string &name)
{
	return !name.empty() && std::all_of(begin(name), end(name), rfc1459::is_chan);
}

inline const auto &
name(const chan &chan)
{
	return chan.name;
}

inline size_t
local_size(const members &m)
{
	return std::distance(begin(m.local), end(m.local));
}

inline bool
local_empty(const members &m)
{
	return m.local.empty();
}

inline size_t
size(const members &m)
{
	return m.global.size();
}

inline bool
empty(const members &m)
{
	return m.global.empty();
}

inline bool
exists(const members &m, const client::client &c)
{
	return m.global.count(const_cast<client::client *>(&c));
}

inline client::client &
get_client(membership &m)
{
	return *m.git->first;
}

inline const client::client &
get_client(const membership &m)
{
	return *m.git->first;
}

inline bool
can_send_banned(const membership *const &m)
{
	return m && can_send_banned(*m);
}

inline bool
can_send_banned(const membership &m)
{
	return m.flags & (BANNED | QUIETED);
}

inline bool
is_voiced(const membership *const &m)
{
	return m && is_voiced(*m);
}

inline bool
is_voiced(const membership &m)
{
	return m.flags & VOICE;
}

inline bool
is_chanop(const membership *const &m)
{
	return m && is_chanop(*m);
}

inline bool
is_chanop(const membership &m)
{
	return m.flags & CHANOP;
}

inline uint
operator~(const modes &modes)
{
	return ~modes.mode;
}

inline uint
operator&(const modes &modes, const mode::type &value)
{
	return modes.mode & value;
}

inline uint
operator|(const modes &modes, const mode::type &value)
{
	return modes.mode | value;
}

inline bool
operator<(const ban &a, const ban &b)
{
	return irccmp(a.banstr, b.banstr);
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
