/*
 *  charybdis: A slightly useful ircd.
 *  supported.c: isupport (005) numeric
 *
 * Copyright (C) 2006 Jilles Tjoelker
 *  Copyright (C) 2016 Charybdis Development Team
 *  Copyright (C) 2016 Jason Volk <jason@zemos.net>
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

using namespace ircd;

std::map<std::string, supported::value> supported::map;

void
supported::init()
{
	// These should all eventually get filed away into their own
	// subsystem's namespace

	add("CHANTYPES", [](ostream &s)
	{
		s << ConfigChannel.disable_local_channels? "#" : "&#";
	});

	add("EXCEPTS", []
	{
		return ConfigChannel.use_except;
	});

	add("INVEX", []
	{
		return ConfigChannel.use_invex;
	});

	add("CHANMODES", [](ostream &s)
	{
		using namespace chan::mode;

		s << categories[uint(category::A)] << ',';
		s << categories[uint(category::B)] << ',';
		s << categories[uint(category::C)] << ',';
		s << categories[uint(category::D)];
	});

	add("CHANLIMIT", [](ostream &s)
	{
		char result[30];
		snprintf(result, sizeof(result), "%s:%i",
		         ConfigChannel.disable_local_channels? "#" : "&#",
		         ConfigChannel.max_chans_per_user);

		s << result;
	});

	add("PREFIX", "(ov)@+");

	add("MAXLIST", [](ostream &s)
	{
		char result[30];
		snprintf(result, sizeof result, "bq%s%s:%i",
		         ConfigChannel.use_except ? "e" : "",
		         ConfigChannel.use_invex ? "I" : "",
		         ConfigChannel.max_bans);

		s << result;
	});

	add("MODES", chan::mode::MAXPARAMS);

	add("NETWORK", [](ostream &s)
	{
		s << ServerInfo.network_name;
	});

	add("STATUSMSG", "@+");
	add("CALLERID", [](ostream &s)
	{
		if(ConfigFileEntry.oper_only_umodes & umode::table['g'])
			return;

		s << 'g';
	});

	add("CASEMAPPING", "rfc1459");
	add("NICKLEN", [](ostream &s)
	{
		char result[200];
		snprintf(result, sizeof(result), "%u", ConfigFileEntry.nicklen - 1);
		s << result;
	});

	add("MAXNICKLEN", NICKLEN - 1);

	add("CHANNELLEN", LOC_CHANNELLEN);

	add("TOPICLEN", TOPICLEN);

	add("DEAF", [](ostream &s)
	{
		if(ConfigFileEntry.oper_only_umodes & umode::table['D'])
			return;

		s << 'D';
	});

	add("TARGMAX", [](ostream &s)
	{
		char result[200];
		snprintf(result, sizeof result, "NAMES:1,LIST:1,KICK:1,WHOIS:1,PRIVMSG:%d,NOTICE:%d,ACCEPT:,MONITOR:",
		         ConfigFileEntry.max_targets,
		         ConfigFileEntry.max_targets);

		s << result;
	});

	add("EXTBAN", [](ostream &s)
	{
		const char *const p(chan::get_extban_string());
		if(!EmptyString(p))
			s << "$," << p;
	});

	add("CLIENTVER", "3.0");
}

bool
supported::del(const std::string &key)
{
	return map.erase(key);
}

void
supported::show(client &client)
{
	uint leading(strlen(client.name));

	// UID
	if(!my(client) && leading < 9)
		leading = 9;

	/* :<me.name> 005 <nick> <params> :are supported by this server */
	/* form_str(RPL_ISUPPORT) is %s :are supported by this server */
	leading += strlen(me.name) + 1 + strlen(form_str(RPL_ISUPPORT));

	//TODO: XXX: It's almost time for the ircstream
	uint nparams(3);
	std::ostringstream buf;
	for(const auto &pit : map)
	{
		const auto &key(pit.first);
		const auto &val(pit.second);
		const auto len(size(buf));
		val(key, buf);
		buf << ' ';
		++nparams;

		if(size(buf) + leading >= 510 || nparams > 14)
		{
			buf.seekp(len, std::ios::beg);
			sendto_one_numeric(&client, RPL_ISUPPORT, form_str(RPL_ISUPPORT), buf.str().c_str());
			buf.str(std::string{});
			nparams = 3;
		}
		else continue;

		val(key, buf);
		buf << ' ';
		++nparams;
	}

	if(nparams > 3)
		sendto_one_numeric(&client, RPL_ISUPPORT, form_str(RPL_ISUPPORT), buf.str().c_str());
}

ostream &
supported::value::operator()(const std::string &key, ostream &s)
const
{
	using type = supported::type;
	switch(this->type)
	{
		case type::FUNC_STREAM:
			s << key << '=';
			func_stream(s);
			break;

		case type::FUNC_BOOLEAN:
			if(func_boolean())
				s << key;
			break;

		case type::STRING:
			s << key << '=' << string;
			break;

		case type::INTEGER:
			s << key << '=' << integer;
			break;

		case type::BOOLEAN:
			s << key;
			break;
	};

	return s;
}

supported::value::value(const std::function<void (ostream &)> &val)
:type{supported::type::FUNC_STREAM}
,func_stream{val}
{
}

supported::value::value(const std::function<bool ()> &val)
:type{supported::type::FUNC_BOOLEAN}
,func_boolean{val}
{
}

supported::value::value(const std::string &val)
:type{supported::type::STRING}
,string{val}
{
}

supported::value::value(const int64_t &val)
:type{supported::type::INTEGER}
,integer{val}
{
}

supported::value::value()
:type{supported::type::BOOLEAN}
{
}

supported::value::~value()
noexcept
{
}
