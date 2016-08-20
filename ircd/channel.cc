/*
 *  ircd-ratbox: A slightly useful ircd.
 *  channel.c: Controls channels.
 *
 * Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 * Copyright (C) 1996-2002 Hybrid Development Team
 * Copyright (C) 2002-2005 ircd-ratbox development team
 * Copyright (C) 2005-2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
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

using namespace ircd;

int h_can_join;
int h_can_send;
int h_get_channel_access;
struct config_channel_entry ircd::ConfigChannel;

std::map<const std::string *, std::unique_ptr<chan::chan>, rfc1459::less> chan::chans;

void
chan::init()
{
	h_can_join = register_hook("can_join");
	h_can_send = register_hook("can_send");
	h_get_channel_access = register_hook("get_channel_access");
}

bool
chan::del(const chan &chan)
{
	return del(name(chan));
}

bool
chan::del(const std::string &name)
{
	return chans.erase(&name);
}

chan::chan &
chan::add(const std::string &name,
          client &client)
{
	if (name.empty())
		throw invalid_argument();

	if (name.size() > CHANNELLEN && IsServer(&client))
		sendto_realops_snomask(SNO_DEBUG, L_ALL,
		                       "*** Long channel name from %s (%lu > %d): %s",
		                       client.name,
		                       name.size(),
		                       CHANNELLEN,
		                       name.c_str());

	if (name.size() > CHANNELLEN)
		throw invalid_argument();

	const auto it(chans.lower_bound(&name));
	if (it != end(chans))
	{
		auto &chan(*it->second);
		if (irccmp(chan.name, name) == 0)
			return chan;
	}

	auto chan(std::make_unique<chan>(name));
	const auto name_p(&chan->name);
	const auto iit(chans.emplace_hint(it, name_p, std::move(chan)));
	return *iit->second;
}

chan::chan &
chan::get(const std::string &name)
try
{
	return *chans.at(&name);
}
catch(const std::out_of_range &e)
{
	throw not_found();
}

chan::chan *
chan::get(const std::string &name,
          std::nothrow_t)
{
	const auto it(chans.find(&name));
	if (it == end(chans))
		return nullptr;

	const auto &ptr(it->second);
	return ptr.get();
}

bool
chan::exists(const std::string &name)
{
	return chans.count(&name);
}

chan::chan::chan(const std::string &name)
:name{name}
,mode{}
,mode_lock{}
,topic{}
,members{}
,join_count{0}
,join_delta{0}
,flood_noticed{0}
,received_number_of_privmsgs{0}
,first_received_message_time{0}
,last_knock{0}
,bants{0}
,channelts{rb_current_time()}
,last_checked_ts{0}
,last_checked_client{nullptr}
,last_checked_type{mode::type(0)}
,last_checked_result{0}
{
}

chan::chan::~chan()
noexcept
{
	clear_invites(*this);
}

/* remove_user_from_channels()
 *
 * input        - user to remove from all channels
 * output       -
 * side effects - user is removed from all channels
 */
void
chan::del(client &client)
{
	auto &client_chans(client.user->channel);
	for(const auto &pit : client_chans)
	{
		auto &chan(*pit.first);
		auto &member(*pit.second);

		if (my(client))
			chan.members.local.erase(member.lit);

		chan.members.global.erase(member.git);
		chan.invites.erase(&client);

		if (empty(chan) && ~chan.mode & mode::PERMANENT)
			del(chan);
	}

	client_chans.clear();
	client.user->invited.clear();
}

void
chan::del(chan &chan,
          client &client)
{
	// These are the relevant containers.
	auto &global(chan.members.global);
	auto &local(chan.members.local);
	auto &client_chans(client.user->channel);

	const auto it(client_chans.find(&chan));
	if (it == end(client_chans))
		return;

	auto &member(*it->second);

	if (my(client))
		chan.members.local.erase(member.lit);

	chan.members.global.erase(member.git);      // The member is destroyed at this point.
	client_chans.erase(it);

	if (empty(chan) && ~chan.mode & mode::PERMANENT)
		del(chan);
}

void
chan::add(chan &chan,
          client &client,
          const int &flags)
{
	if (!client.user)
	{
		s_assert(0);
		return;
	}

	// These are the relevant containers.
	auto &global(chan.members.global);
	auto &local(chan.members.local);
	auto &client_chans(client.user->channel);

	// Add client to channel's global map
	const auto iit(global.emplace(&client, flags));
	if (!iit.second)
		return;

	// The global map hosts the membership structure.
	auto &member(iit.first->second);

	// Add iterator (pointer to the element) for constant time lookup/removal
	member.git = iit.first;

	// Add membership to a local list which can be iterated for local IO;
	if (my(client))
		member.lit = local.emplace(end(local), &member);

	// Add channel to client's channel map, pointing to the membership;
	client_chans.emplace(&chan, &member);
}


chan::members::members()
{
}

chan::members::~members()
noexcept
{
}

chan::membership::membership(const uint &flags)
:flags{flags}
,bants{0}
{
}

chan::membership::~membership()
noexcept
{
}

chan::modes::modes()
:mode{0}
,limit{0}
,key{0}
,join_num{0}
,join_time{0}
,forward{0}
{
}

chan::ban::ban(const std::string &banstr,
               const std::string &who,
               const std::string &forward,
               const time_t &when)
:banstr{banstr}
,who{who}
,forward{forward}
,when{when}
{
}

void
chan::send_join(chan &chan,
                Client &client)
{
	if (!IsClient(&client))
		return;

	sendto_channel_local_with_capability(ALL_MEMBERS, NOCAPS, CLICAP_EXTENDED_JOIN, &chan,
	                                     ":%s!%s@%s JOIN %s",
	                                     client.name,
	                                     client.username,
	                                     client.host,
	                                     chan.name.c_str());

	sendto_channel_local_with_capability(ALL_MEMBERS, CLICAP_EXTENDED_JOIN, NOCAPS, &chan,
	                                     ":%s!%s@%s JOIN %s %s :%s",
	                                     client.name,
	                                     client.username,
	                                     client.host,
	                                     chan.name.c_str(),
	                                     EmptyString(client.user->suser)? "*" : client.user->suser,
	                                     client.info);

	// Send away message to away-notify enabled clients.
	if (client.user->away)
		sendto_channel_local_with_capability_butone(&client, ALL_MEMBERS, CLICAP_AWAY_NOTIFY, NOCAPS, &chan,
		                                            ":%s!%s@%s AWAY :%s",
		                                            client.name,
		                                            client.username,
		                                            client.host,
		                                            client.user->away);
}

chan::membership &
chan::get(members &members,
          const client &client)
{
	if (!is_client(client))
		throw invalid_argument();

	const auto key(const_cast<ircd::chan::client *>(&client)); //TODO: temp elaborated
	const auto it(members.global.find(key));
	if (it == end(members.global))
		throw not_found();

	return it->second;
}

const chan::membership &
chan::get(const members &members,
          const client &client)
{
	if (!is_client(client))
		throw invalid_argument();

	const auto key(const_cast<ircd::chan::client *>(&client)); //TODO: temp elaborated
	const auto it(members.global.find(key));
	if (it == end(members.global))
		throw not_found();

	return it->second;
}

chan::membership *
chan::get(members &members,
          const client &client,
          std::nothrow_t)
{
	if (!is_client(client))
		return nullptr;

	const auto key(const_cast<ircd::chan::client *>(&client)); //TODO: temp elaborated
	const auto it(members.global.find(key));
	if (it == end(members.global))
		return nullptr;

	return &it->second;
}

const chan::membership *
chan::get(const members &members,
          const client &client,
          std::nothrow_t)
{
	if (!is_client(client))
		return nullptr;

	const auto key(const_cast<ircd::chan::client *>(&client)); //TODO: temp elaborated
	const auto it(members.global.find(key));
	if (it == end(members.global))
		return nullptr;

	return &it->second;
}

/* find_channel_status()
 *
 * input	- membership to get status for, whether we can combine flags
 * output	- flags of user on channel
 * side effects -
 */
const char *
chan::find_status(const membership *const &msptr,
                  const int &combine)
{
	static char buffer[3];
	char *p;

	p = buffer;

	if(is_chanop(msptr))
	{
		if(!combine)
			return "@";
		*p++ = '@';
	}

	if(is_voiced(msptr))
		*p++ = '+';

	*p = '\0';
	return buffer;
}

/* invalidate_bancache_user()
 *
 * input	- user to invalidate ban cache for
 * output	-
 * side effects - ban cache is invalidated for all memberships of that user
 *                to be used after a nick change
 */
void
chan::invalidate_bancache_user(client *client)
{
	if(!client)
		return;

	for(const auto &pair : client->user->channel)
	{
		auto &chan(*pair.first);
		auto &member(*pair.second);
		member.bants = 0;
		member.flags &= ~BANNED;
	}
}

/* channel_pub_or_secret()
 *
 * input	- channel
 * output	- "=" if public, "@" if secret, else "*"
 * side effects	-
 */
static const char *
channel_pub_or_secret(chan::chan *const &chptr)
{
	return chan::is_public(chptr)?  "=":
	       chan::is_secret(chptr)?  "@":
	                                "*";
}

/* channel_member_names()
 *
 * input	- channel to list, client to list to, show endofnames
 * output	-
 * side effects - client is given list of users on channel
 */
void
chan::channel_member_names(chan *chptr, client *client_p, int show_eon)
{
	client *target_p;
	rb_dlink_node *ptr;
	char lbuf[BUFSIZE];
	char *t;
	int mlen;
	int tlen;
	int cur_len;
	int stack = IsCapable(client_p, CLICAP_MULTI_PREFIX);

	if(can_show(chptr, client_p))
	{
		cur_len = mlen = sprintf(lbuf, form_str(RPL_NAMREPLY),
					    me.name, client_p->name,
					    channel_pub_or_secret(chptr), chptr->name.c_str());

		t = lbuf + cur_len;

		for(const auto &pair : chptr->members.global)
		{
			auto *const target_p(pair.first);
			const auto &member(pair.second);

			if(IsInvisible(target_p) && !is_member(chptr, client_p))
				continue;

			if (IsCapable(client_p, CLICAP_USERHOST_IN_NAMES))
			{
				/* space, possible "@+" prefix */
				if (cur_len + strlen(target_p->name) + strlen(target_p->username) + strlen(target_p->host) + 5 >= BUFSIZE - 5)
				{
					*(t - 1) = '\0';
					sendto_one(client_p, "%s", lbuf);
					cur_len = mlen;
					t = lbuf + mlen;
				}

				tlen = sprintf(t, "%s%s!%s@%s ", find_status(&member, stack),
						  target_p->name, target_p->username, target_p->host);
			}
			else
			{
				/* space, possible "@+" prefix */
				if(cur_len + strlen(target_p->name) + 3 >= BUFSIZE - 3)
				{
					*(t - 1) = '\0';
					sendto_one(client_p, "%s", lbuf);
					cur_len = mlen;
					t = lbuf + mlen;
				}

				tlen = sprintf(t, "%s%s ", find_status(&member, stack),
						  target_p->name);
			}

			cur_len += tlen;
			t += tlen;
		}

		/* The old behaviour here was to always output our buffer,
		 * even if there are no clients we can show.  This happens
		 * when a client does "NAMES" with no parameters, and all
		 * the clients on a -sp channel are +i.  I dont see a good
		 * reason for keeping that behaviour, as it just wastes
		 * bandwidth.  --anfl
		 */
		if(cur_len != mlen)
		{
			*(t - 1) = '\0';
			sendto_one(client_p, "%s", lbuf);
		}
	}

	if(show_eon)
		sendto_one(client_p, form_str(RPL_ENDOFNAMES),
			   me.name, client_p->name, chptr->name.c_str());
}

void
chan::clear_invites(chan &chan)
{
	for (auto &client : chan.invites)
		client->user->invited.erase(&chan);
}

void
chan::del_invite(chan &chan, client &client)
{
	chan.invites.erase(&client);
	client.user->invited.erase(&chan);
}

bool
chan::cache_check(const chan &chan,
                  const mode::type &type,
                  const client &who,
                  bool &result)
{
	using namespace mode;

	if (chan.last_checked_client != &who)
		return false;

	if (chan.last_checked_ts <= chan.bants)
		return false;

	switch (type)
	{
		case BAN:
		case QUIET:
			result = chan.last_checked_result;
			return true;

		default:
			return false;
	}
}

void
chan::cache_result(chan &chan,
                   const mode::type &type,
                   const client &client,
                   const bool &result,
                   membership *const &membership)
{
	using namespace mode;

	switch (type)
	{
		case BAN:
		case QUIET:
			break;

		default:
			return;
	}

	chan.last_checked_client = &client;
	chan.last_checked_type = type;
	chan.last_checked_result = result;
	chan.last_checked_ts = rb_current_time();

	if (membership)
	{
		membership->bants = chan.bants;

		if (result)
			membership->flags |= BANNED;
		else
			membership->flags &= ~BANNED;
	}
}

bool
chan::check(chan &chan,
            const mode::type &type,
            const client &client,
            check_data *const &data)
{
	bool result;
	if (cache_check(chan, type, client, result))
		return result;

	if (!my(client))
		return false;

	enum srcs { HOST, IPHOST, ALTHOST, IP4HOST, _NUM_ };
	char src[num_of<srcs>()][NICKLEN + USERLEN + HOSTLEN + 6];
	const char *s[num_of<srcs>()]
	{
		data? data->host : nullptr,
		data? data->iphost : nullptr
	};

	// if the buffers havent been built, do it here
	if (!s[HOST])
	{
		sprintf(src[HOST], "%s!%s@%s", client.name, client.username, client.host);
		s[HOST] = src[HOST];
	}

	if (!s[IPHOST])
	{
		sprintf(src[IPHOST], "%s!%s@%s", client.name, client.username, client.sockhost);
		s[IPHOST] = src[IPHOST];
	}

	if (client.localClient->mangledhost)
	{
		// if host mangling mode enabled, also check their real host
		if (strcmp(client.host, client.localClient->mangledhost) == 0)
		{
			sprintf(src[ALTHOST], "%s!%s@%s", client.name, client.username, client.orighost);
			s[ALTHOST] = src[ALTHOST];
		}
		// if host mangling mode not enabled and no other spoof, check the mangled form of their host
		else if (!IsDynSpoof(&client))
		{
			sprintf(src[ALTHOST], "%s!%s@%s", client.name, client.username, client.localClient->mangledhost);
			s[ALTHOST] = src[ALTHOST];
		}
	}

#ifdef RB_IPV6
	struct sockaddr_in ip4;
	if (GET_SS_FAMILY(&client.localClient->ip) == AF_INET6 &&
			rb_ipv4_from_ipv6((const struct sockaddr_in6 *)&client.localClient->ip, &ip4))
	{
		sprintf(src[IP4HOST], "%s!%s@", client.name, client.username);
		char *const pos(src[IP4HOST] + strlen(src[IP4HOST]));
		rb_inet_ntop_sock((struct sockaddr *)&ip4, pos, src[IP4HOST] + sizeof(src[IP4HOST]) - pos);
		s[IP4HOST] = src[IP4HOST];
	}
#endif

	const auto matched([&s, &chan, &client]
	(const ban &ban)
	{
		const auto &mask(ban.banstr);
		if (match(mask, s[HOST]))
			return true;

		if (match(mask, s[IPHOST]))
			return true;

		if (match_cidr(mask, s[IPHOST]))
			return true;

		if (s[ALTHOST] && match(mask, s[ALTHOST]))
			return true;

		if (match_extban(mask.c_str(), const_cast<Client *>(&client), &chan, mode::BAN))
			return true;

		#ifdef RB_IPV6
		if (s[IP4HOST] && match(mask, s[IP4HOST]))
			return true;

		if (s[IP4HOST] && match_cidr(mask, s[IP4HOST]))
			return true;
		#endif

		return false;
	});

	const auto &list(get(chan, type));
	const auto it(std::find_if(begin(list), end(list), matched));
	const auto msptr(data? data->msptr : nullptr);
	result = it != end(list);

	if (result && ircd::ConfigChannel.use_except)
	{
		const auto &list(get(chan, mode::EXCEPTION));
		if (std::any_of(begin(list), end(list), matched))
		{
			cache_result(chan, type, client, false, msptr);
			return false;
		}
	}

	if (result && data && data->forward)
	{
		const auto &ban(*it);
		if (!ban.forward.empty())
			*data->forward = ban.forward.c_str();   //TODO: XXX: ???
	}

	cache_result(chan, type, client, result, msptr);
	return result;
}

/* can_join()
 *
 * input	- client to check, channel to check for, key
 * output	- reason for not being able to join, else 0, channel name to forward to
 * side effects -
 * caveats      - this function should only be called on a local user.
 */
int
chan::can_join(client *source_p, chan *chptr, const char *key, const char **forward)
{
	rb_dlink_node *ptr;
	bool invited(false);
	ban *invex = NULL;
	char src_host[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_iphost[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_althost[NICKLEN + USERLEN + HOSTLEN + 6];
	int use_althost = 0;
	int i = 0;
	hook_data_channel moduledata;

	s_assert(source_p->localClient != NULL);

	moduledata.client = source_p;
	moduledata.chptr = chptr;
	moduledata.approved = 0;

	sprintf(src_host, "%s!%s@%s", source_p->name, source_p->username, source_p->host);
	sprintf(src_iphost, "%s!%s@%s", source_p->name, source_p->username, source_p->sockhost);
	if(source_p->localClient->mangledhost != NULL)
	{
		/* if host mangling mode enabled, also check their real host */
		if(!strcmp(source_p->host, source_p->localClient->mangledhost))
		{
			sprintf(src_althost, "%s!%s@%s", source_p->name, source_p->username, source_p->orighost);
			use_althost = 1;
		}
		/* if host mangling mode not enabled and no other spoof,
		 * also check the mangled form of their host */
		else if (!IsDynSpoof(source_p))
		{
			sprintf(src_althost, "%s!%s@%s", source_p->name, source_p->username, source_p->localClient->mangledhost);
			use_althost = 1;
		}
	}

	check_data data
	{
		nullptr,
		src_host,
		src_iphost,
		forward
	};

	if (check(*chptr, mode::BAN, *source_p, &data))
	{
		moduledata.approved = ERR_BANNEDFROMCHAN;
		goto finish_join_check;
	}

	if (*chptr->mode.key && (EmptyString(key) || irccmp(chptr->mode.key, key)))
	{
		moduledata.approved = ERR_BADCHANNELKEY;
		goto finish_join_check;
	}

	/* All checks from this point on will forward... */
	if (forward)
		*forward = chptr->mode.forward;

	if (chptr->mode.mode & mode::INVITEONLY)
	{
		if (!(invited = chptr->invites.count(source_p)))
		{
			if (!ConfigChannel.use_invex)
				moduledata.approved = ERR_INVITEONLYCHAN;

			const auto &list(get(*chptr, mode::INVEX));
			const bool matched(std::any_of(begin(list), end(list), [&]
			(const auto &ban)
			{
				const auto &mask(ban.banstr);
				if (match(mask, src_host))
					return true;

				if (match(mask, src_iphost))
					return true;

				if (match_cidr(mask, src_iphost))
					return true;

				if (match_extban(mask.c_str(), source_p, chptr, mode::INVEX))
					return true;

				if (use_althost && match(mask, src_althost))
					return true;

				return false;
			}));

			if (!matched)
				moduledata.approved = ERR_INVITEONLYCHAN;
		}
	}

	if(chptr->mode.limit && size(chptr->members) >= ulong(chptr->mode.limit))
		i = ERR_CHANNELISFULL;
	if(chptr->mode.mode & mode::REGONLY && EmptyString(source_p->user->suser))
		i = ERR_NEEDREGGEDNICK;
	/* join throttling stuff --nenolod */
	else if(chptr->mode.join_num > 0 && chptr->mode.join_time > 0)
	{
		if ((rb_current_time() - chptr->join_delta <=
			chptr->mode.join_time) && (chptr->join_count >=
			chptr->mode.join_num))
			i = ERR_THROTTLE;
	}

	/* allow /invite to override +l/+r/+j also -- jilles */
	if (i != 0 && !invited)
	{
		if (!(invited = chptr->invites.count(source_p)))
			moduledata.approved = i;
	}

finish_join_check:
	call_hook(h_can_join, &moduledata);

	return moduledata.approved;
}

/* can_send()
 *
 * input	- user to check in channel, membership pointer
 * output	- whether can explicitly send or not, else CAN_SEND_NONOP
 * side effects -
 */
int
chan::can_send(chan *chptr, client *source_p, membership *msptr)
{
	hook_data_channel_approval moduledata;

	moduledata.approved = CAN_SEND_NONOP;
	moduledata.dir = MODE_QUERY;

	if(IsServer(source_p) || IsService(source_p))
		return CAN_SEND_OPV;

	if(MyClient(source_p) && hash_find_resv(chptr->name.c_str()) &&
	   !IsOper(source_p) && !IsExemptResv(source_p))
		moduledata.approved = CAN_SEND_NO;

	if(msptr == NULL)
	{
		msptr = get(chptr->members, *source_p, std::nothrow);

		if(msptr == NULL)
		{
			/* if its +m or +n and theyre not in the channel,
			 * they cant send.  we dont check bans here because
			 * theres no possibility of caching them --fl
			 */
			if(chptr->mode.mode & mode::NOPRIVMSGS || chptr->mode.mode & mode::MODERATED)
				moduledata.approved = CAN_SEND_NO;
			else
				moduledata.approved = CAN_SEND_NONOP;

			return moduledata.approved;
		}
	}

	if(chptr->mode.mode & mode::MODERATED)
		moduledata.approved = CAN_SEND_NO;

	if(MyClient(source_p))
	{
		/* cached can_send */
		if(msptr->bants == chptr->bants)
		{
			if(can_send_banned(msptr))
				moduledata.approved = CAN_SEND_NO;
		} else {
			check_data data
			{
				msptr
			};

			if (check(*chptr, mode::BAN, *source_p, &data))
				moduledata.approved = CAN_SEND_NO;

			if (check(*chptr, mode::QUIET, *source_p, &data))
				moduledata.approved = CAN_SEND_NO;
		}
	}

	if (is_chanop(msptr) || is_voiced(msptr))
		moduledata.approved = CAN_SEND_OPV;

	moduledata.client = source_p;
	moduledata.chptr = chptr;
	moduledata.msptr = msptr;
	moduledata.target = NULL;
	moduledata.dir = moduledata.approved == CAN_SEND_NO? MODE_ADD : MODE_QUERY;

	call_hook(h_can_send, &moduledata);

	return moduledata.approved;
}

/*
 * flood_attack_channel
 * inputs       - flag 0 if PRIVMSG 1 if NOTICE. RFC
 *                says NOTICE must not auto reply
 *              - pointer to source Client
 *              - pointer to target channel
 * output       - true if target is under flood attack
 * side effects	- check for flood attack on target chptr
 */
bool
chan::flood_attack_channel(int p_or_n, client *source_p, chan *chptr)
{
	int delta;

	if(GlobalSetOptions.floodcount && MyClient(source_p))
	{
		if((chptr->first_received_message_time + 1) < rb_current_time())
		{
			delta = rb_current_time() - chptr->first_received_message_time;
			chptr->received_number_of_privmsgs -= delta;
			chptr->first_received_message_time = rb_current_time();
			if(chptr->received_number_of_privmsgs <= 0)
			{
				chptr->received_number_of_privmsgs = 0;
				chptr->flood_noticed = 0;
			}
		}

		if((chptr->received_number_of_privmsgs >= GlobalSetOptions.floodcount)
		   || chptr->flood_noticed)
		{
			if(chptr->flood_noticed == 0)
			{
				sendto_realops_snomask(SNO_BOTS, chptr->name[0] == '&' ? L_ALL : L_NETWIDE,
						     "Possible Flooder %s[%s@%s] on %s target: %s",
						     source_p->name, source_p->username,
						     source_p->orighost,
						     source_p->servptr->name, chptr->name.c_str());
				chptr->flood_noticed = 1;

				/* Add a bit of penalty */
				chptr->received_number_of_privmsgs += 2;
			}
			if(MyClient(source_p) && (p_or_n != 1))
				sendto_one(source_p,
					   ":%s NOTICE %s :*** Message to %s throttled due to flooding",
					   me.name, source_p->name, chptr->name.c_str());
			return true;
		}
		else
			chptr->received_number_of_privmsgs++;
	}

	return false;
}

/* find_bannickchange_channel()
 * Input: client to check
 * Output: channel preventing nick change
 */
chan::chan *
chan::find_bannickchange_channel(client *client_p)
{
	chan *chptr;
	membership *msptr;
	rb_dlink_node *ptr;
	char src_host[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_iphost[NICKLEN + USERLEN + HOSTLEN + 6];

	if (!MyClient(client_p))
		return NULL;

	sprintf(src_host, "%s!%s@%s", client_p->name, client_p->username, client_p->host);
	sprintf(src_iphost, "%s!%s@%s", client_p->name, client_p->username, client_p->sockhost);

	for(const auto &pit : client_p->user->channel)
	{
		auto &chptr(pit.first);
		auto &msptr(pit.second);

		if (is_chanop(msptr) || is_voiced(msptr))
			continue;
		/* cached can_send */
		if (msptr->bants == chptr->bants)
		{
			if (can_send_banned(msptr))
				return chptr;
		} else {
			check_data data
			{
				msptr,
				src_host,
				src_iphost,
			};

			if (check(*chptr, mode::BAN, *client_p, &data))
				return chptr;

			if (check(*chptr, mode::QUIET, *client_p, &data))
				return chptr;
		}
	}

	return NULL;
}

/* void check_spambot_warning(client *source_p)
 * Input: Client to check, channel name or NULL if this is a part.
 * Output: none
 * Side-effects: Updates the client's oper_warn_count_down, warns the
 *    IRC operators if necessary, and updates join_leave_countdown as
 *    needed.
 */
void
chan::check_spambot_warning(client *source_p, const char *name)
{
	int t_delta;
	int decrement_count;
	if((GlobalSetOptions.spam_num &&
	    (source_p->localClient->join_leave_count >= GlobalSetOptions.spam_num)))
	{
		if(source_p->localClient->oper_warn_count_down > 0)
			source_p->localClient->oper_warn_count_down--;
		else
			source_p->localClient->oper_warn_count_down = 0;
		if(source_p->localClient->oper_warn_count_down == 0 &&
				name != NULL)
		{
			/* Its already known as a possible spambot */
			sendto_realops_snomask(SNO_BOTS, L_NETWIDE,
					     "User %s (%s@%s) trying to join %s is a possible spambot",
					     source_p->name,
					     source_p->username, source_p->orighost, name);
			source_p->localClient->oper_warn_count_down = OPER_SPAM_COUNTDOWN;
		}
	}
	else
	{
		if((t_delta =
		    (rb_current_time() - source_p->localClient->last_leave_time)) >
		   JOIN_LEAVE_COUNT_EXPIRE_TIME)
		{
			decrement_count = (t_delta / JOIN_LEAVE_COUNT_EXPIRE_TIME);
			if(name != NULL)
				;
			else if(decrement_count > source_p->localClient->join_leave_count)
				source_p->localClient->join_leave_count = 0;
			else
				source_p->localClient->join_leave_count -= decrement_count;
		}
		else
		{
			if((rb_current_time() -
			    (source_p->localClient->last_join_time)) < GlobalSetOptions.spam_time)
			{
				/* oh, its a possible spambot */
				source_p->localClient->join_leave_count++;
			}
		}
		if(name != NULL)
			source_p->localClient->last_join_time = rb_current_time();
		else
			source_p->localClient->last_leave_time = rb_current_time();
	}
}

/* check_splitmode()
 *
 * input	-
 * output	-
 * side effects - compares usercount and servercount against their split
 *                values and adjusts splitmode accordingly
 */
void
chan::check_splitmode(void *unused)
{
	if(splitchecking && (ConfigChannel.no_join_on_split || ConfigChannel.no_create_on_split))
	{
		/* not split, we're being asked to check now because someone
		 * has left
		 */
		if(!splitmode)
		{
			if(eob_count < split_servers || Count.total < split_users)
			{
				splitmode = 1;
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						     "Network split, activating splitmode");
				check_splitmode_ev = rb_event_addish("check_splitmode", check_splitmode, NULL, 2);
			}
		}
		/* in splitmode, check whether its finished */
		else if(eob_count >= split_servers && Count.total >= split_users)
		{
			splitmode = 0;

			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					     "Network rejoined, deactivating splitmode");

			rb_event_delete(check_splitmode_ev);
			check_splitmode_ev = NULL;
		}
	}
}


/* set_channel_topic()
 *
 * input	- channel, topic to set, topic info and topic ts
 * output	-
 * side effects - channels topic, topic info and TS are set.
 */
void
chan::set_channel_topic(chan *chptr, const char *topic, const char *topic_info, time_t topicts)
{
	//TODO: XXX: is the topic* len previously truncated at TOPICLEN
	//TODO: XXX: is the topc_info* previously truncated at USERHOST_REPLYLEN

	if(strlen(topic) > 0)
	{
		chptr->topic.text = topic;
		chptr->topic.info = topic_info;
		chptr->topic.time = topicts;
	}
	else
	{
		chptr->topic.text.clear();
		chptr->topic.info.clear();
		chptr->topic.time = 0;
	}
}

/* channel_modes()
 *
 * inputs       - pointer to channel
 *              - pointer to client
 * output       - string with simple modes
 * side effects - result from previous calls overwritten
 *
 * Stolen from ShadowIRCd 4 --nenolod
 */
const char *
chan::channel_modes(chan *chptr, client *client_p)
{
	int i;
	char buf1[BUFSIZE];
	char buf2[BUFSIZE];
	static char final[BUFSIZE];
	char *mbuf = buf1;
	char *pbuf = buf2;

	*mbuf++ = '+';
	*pbuf = '\0';

	for (i = 0; i < 256; i++)
	{
		if(mode::table[i].set_func == mode::functor::hidden && (!IsOper(client_p) || !IsClient(client_p)))
			continue;
		if(chptr->mode.mode & mode::table[i].type)
			*mbuf++ = i;
	}

	if(chptr->mode.limit)
	{
		*mbuf++ = 'l';

		if(!IsClient(client_p) || is_member(chptr, client_p))
			pbuf += sprintf(pbuf, " %d", chptr->mode.limit);
	}

	if(*chptr->mode.key)
	{
		*mbuf++ = 'k';

		if(pbuf > buf2 || !IsClient(client_p) || is_member(chptr, client_p))
			pbuf += sprintf(pbuf, " %s", chptr->mode.key);
	}

	if(chptr->mode.join_num)
	{
		*mbuf++ = 'j';

		if(pbuf > buf2 || !IsClient(client_p) || is_member(chptr, client_p))
			pbuf += sprintf(pbuf, " %d:%d", chptr->mode.join_num,
					   chptr->mode.join_time);
	}

	if(*chptr->mode.forward &&
			(ConfigChannel.use_forward || !IsClient(client_p)))
	{
		*mbuf++ = 'f';

		if(pbuf > buf2 || !IsClient(client_p) || is_member(chptr, client_p))
			pbuf += sprintf(pbuf, " %s", chptr->mode.forward);
	}

	*mbuf = '\0';

	rb_strlcpy(final, buf1, sizeof final);
	rb_strlcat(final, buf2, sizeof final);
	return final;
}

/* void send_cap_mode_changes(client *client_p,
 *                        client *source_p,
 *                        chan *chptr, int cap, int nocap)
 * Input: The client sending(client_p), the source client(source_p),
 *        the channel to send mode changes for(chptr)
 * Output: None.
 * Side-effects: Sends the appropriate mode changes to capable servers.
 *
 * Reverted back to my original design, except that we now keep a count
 * of the number of servers which each combination as an optimisation, so
 * the capabs combinations which are not needed are not worked out. -A1kmm
 *
 * Removed most code here because we don't need to be compatible with ircd
 * 2.8.21+CSr and stuff.  --nenolod
 */
void
chan::send_cap_mode_changes(client *client_p, client *source_p,
		      chan *chptr, mode::change mode_changes[], int mode_count)
{
	static char modebuf[BUFSIZE];
	static char parabuf[BUFSIZE];
	int i, mbl, pbl, nc, mc, preflen, len;
	char *pbuf;
	const char *arg;
	int dir;
	int arglen = 0;

	/* Now send to servers... */
	mc = 0;
	nc = 0;
	pbl = 0;
	parabuf[0] = 0;
	pbuf = parabuf;
	dir = MODE_QUERY;

	mbl = preflen = sprintf(modebuf, ":%s TMODE %ld %s ",
				   use_id(source_p), (long) chptr->channelts,
				   chptr->name.c_str());

	/* loop the list of - modes we have */
	for (i = 0; i < mode_count; i++)
	{
		/* if they dont support the cap we need, or they do support a cap they
		 * cant have, then dont add it to the modebuf.. that way they wont see
		 * the mode
		 */
		if (mode_changes[i].letter == 0)
			continue;

		if (!EmptyString(mode_changes[i].id))
			arg = mode_changes[i].id;
		else
			arg = mode_changes[i].arg;

		if(arg)
		{
			arglen = strlen(arg);

			/* dont even think about it! --fl */
			if(arglen > mode::BUFLEN - 5)
				continue;
		}

		/* if we're creeping past the buf size, we need to send it and make
		 * another line for the other modes
		 * XXX - this could give away server topology with uids being
		 * different lengths, but not much we can do, except possibly break
		 * them as if they were the longest of the nick or uid at all times,
		 * which even then won't work as we don't always know the uid -A1kmm.
		 */
		if(arg && ((mc == mode::MAXPARAMSSERV) ||
			   ((mbl + pbl + arglen + 4) > (BUFSIZE - 3))))
		{
			if(nc != 0)
				sendto_server(client_p, chptr, NOCAPS, NOCAPS,
					      "%s %s", modebuf, parabuf);
			nc = 0;
			mc = 0;

			mbl = preflen;
			pbl = 0;
			pbuf = parabuf;
			parabuf[0] = 0;
			dir = MODE_QUERY;
		}

		if(dir != mode_changes[i].dir)
		{
			modebuf[mbl++] = (mode_changes[i].dir == MODE_ADD) ? '+' : '-';
			dir = mode_changes[i].dir;
		}

		modebuf[mbl++] = mode_changes[i].letter;
		modebuf[mbl] = 0;
		nc++;

		if(arg != NULL)
		{
			len = sprintf(pbuf, "%s ", arg);
			pbuf += len;
			pbl += len;
			mc++;
		}
	}

	if(pbl && parabuf[pbl - 1] == ' ')
		parabuf[pbl - 1] = 0;

	if(nc != 0)
		sendto_server(client_p, chptr, NOCAPS, NOCAPS, "%s %s", modebuf, parabuf);
}

void
chan::resv_chan_forcepart(const char *name, const char *reason, int temp_time)
{
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	chan *chptr;
	membership *msptr;
	client *target_p;

	if(!ConfigChannel.resv_forcepart)
		return;

	/* for each user on our server in the channel list
	 * send them a PART, and notify opers.
	 */
	chptr = get(name, std::nothrow);
	if(chptr != NULL)
	{
		// Iterate a copy of the local list while all of this is going on
		auto local(chptr->members.local);
		for(auto &msptr : local)
		{
			const auto target_p(msptr->git->first);

			if(IsExemptResv(target_p))
				continue;

			sendto_server(target_p, chptr, CAP_TS6, NOCAPS,
			              ":%s PART %s", target_p->id, chptr->name.c_str());

			sendto_channel_local(ALL_MEMBERS, chptr, ":%s!%s@%s PART %s :%s",
			                     target_p->name, target_p->username,
			                     target_p->host, chptr->name.c_str(), target_p->name);

			del(*chptr, *target_p);

			/* notify opers & user they were removed from the channel */
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
			                     "Forced PART for %s!%s@%s from %s (%s)",
			                     target_p->name, target_p->username,
			                     target_p->host, name, reason);

			if(temp_time > 0)
				sendto_one_notice(target_p, ":*** Channel %s is temporarily unavailable on this server.",
				           name);
			else
				sendto_one_notice(target_p, ":*** Channel %s is no longer available on this server.",
				           name);
		}
	}
}

bool
chan::add(chan &chan,
          const mode::type &type,
          const std::string &mask,
          client &source,
          const std::string &forward)

{
	// dont let local clients overflow the banlist, or set redundant bans
	if (my(source))
	{
		const bool large(chan.mode & mode::EXLIMIT);
		const size_t &max(large? ConfigChannel.max_bans_large : ConfigChannel.max_bans);
		if (lists_size(chan) > max)
		{
			sendto_one(&source, form_str(ERR_BANLISTFULL),
			           me.name,
			           source.name,
			           chan.name.c_str(),
			           mask.c_str());

			return false;
		}

		const auto matches([&mask](const ban &ban)
		{
			return match_mask(ban.banstr, mask);
		});

		const auto &list(get(chan, type));
		if (std::any_of(begin(list), end(list), matches))
			return false;
    }

	auto &list(get(chan, type));
	auto insertion(list.emplace(mask, std::string{}, forward, rb_current_time()));
	if (!insertion.second)
		return false;

	auto &ban(const_cast<struct ban &>(*insertion.first));
	if (is_person(source))
	{
		static char who[USERHOST_REPLYLEN];
		snprintf(who, sizeof(who), "%s!%s@%s", source.name, source.username, source.host);
		ban.who = who;
	}
	else ban.who = source.name;

	cache_invalidate(chan, type, ban.when);
	return true;
}

bool
chan::del(chan &chan,
          const mode::type &type,
          const std::string &mask)

{
	if (mask.empty())
		return false;

	auto &list(get(chan, type));
	if (!list.erase(mask))
		return false;

	cache_invalidate(chan, type);
	return true;
}

const chan::ban &
chan::get(const chan &chan,
          const mode::type &type,
          const std::string &mask)
{
	const auto &list(get(chan, type));
	const auto &it(list.find(mask));
	if (it == end(list))
		throw not_found();

	return *it;
}

void
chan::cache_invalidate(chan &chan,
                       const mode::type &type,
                       time_t time)
{
	using namespace mode;

	if (!time)
		time = rb_current_time();

	switch (type)
	{
		case BAN:
		case QUIET:
		case EXCEPTION:
			chan.bants = time;
			break;

		default:
			break;
	}
}

size_t
chan::lists_size(const chan &chan)
{
	using namespace mode;

	return size(chan, BAN) +
	       size(chan, EXCEPTION) +
	       size(chan, INVEX) +
	       size(chan, QUIET);
}

size_t
chan::size(const chan &chan,
           const mode::type &type)
{
	return get(chan, type).size();
}

bool
chan::empty(const chan &chan,
            const mode::type &type)
{
	return get(chan, type).empty();
}

chan::list &
chan::get(chan &chan,
          const mode::type &type)
{
	using namespace mode;

	switch (type)
	{
		case BAN:           return chan.bans;
		case EXCEPTION:     return chan.excepts;
		case INVEX:         return chan.invexs;
		case QUIET:         return chan.quiets;
		default:
			throw invalid_list();
	}
}

const chan::list &
chan::get(const chan &chan,
          const mode::type &type)
{
	using namespace mode;

	switch (type)
	{
		case BAN:           return chan.bans;
		case EXCEPTION:     return chan.excepts;
		case INVEX:         return chan.invexs;
		case QUIET:         return chan.quiets;
		default:
			throw invalid_list();
	}
}
