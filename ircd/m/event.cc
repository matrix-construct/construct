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
 */

#include <ircd/m/m.h>

ircd::database *
ircd::m::event::events
{};

ircd::m::event::event(const id &id,
                      const mutable_buffer &buf)
{
	fetch tab
	{
		id, buf
	};

	new (this) event{tab};
}

ircd::m::event::event(fetch &tab)
{
	io::acquire(tab);

	if(bool(tab.error))
		std::rethrow_exception(tab.error);

	new (this) super_type{tab.pdu};
}

ircd::m::event::temporality
ircd::m::temporality(const event &event,
                     const int64_t &rel)
{
	const auto &depth
	{
		json::get<"depth"_>(event)
	};

	return depth > rel?   event::temporality::FUTURE:
	       depth == rel?  event::temporality::PRESENT:
	                      event::temporality::PAST;
}

ircd::m::event::lineage
ircd::m::lineage(const event &event)
{
	const json::array prev[]
	{
		json::get<"prev_events"_>(event),
		json::get<"auth_events"_>(event),
		json::get<"prev_state"_>(event),
	};

	const auto count{std::accumulate(begin(prev), end(prev), size_t(0), []
	(auto ret, const auto &array)
	{
		return ret += array.count();
	})};

	return count > 1?   event::lineage::MERGE:
	       count == 1?  event::lineage::FORWARD:
	                    event::lineage::ROOT;
}

ircd::string_view
ircd::m::reflect(const event::lineage &lineage)
{
	switch(lineage)
	{
		case event::lineage::MERGE:    return "MERGE";
		case event::lineage::FORWARD:  return "FORWARD";
		case event::lineage::ROOT:     return "ROOT";
	}

	return "?????";
}

ircd::string_view
ircd::m::reflect(const event::temporality &temporality)
{
	switch(temporality)
	{
		case event::temporality::FUTURE:   return "FUTURE";
		case event::temporality::PRESENT:  return "PRESENT";
		case event::temporality::PAST:     return "PAST";
	}

	return "?????";
}

size_t
ircd::m::degree(const event &event)
{
	return degree(event::prev{event});
}

size_t
ircd::m::degree(const event::prev &prev)
{
	size_t ret{0};
	json::for_each(prev, [&ret]
	(const auto &, const json::array &prevs)
	{
		ret += prevs.count();
	});

	return ret;
}

size_t
ircd::m::count(const event::prev &prev)
{
	size_t ret{0};
	m::for_each(prev, [&ret](const event::id &event_id)
	{
		++ret;
	});

	return ret;
}

void
ircd::m::for_each(const event::prev &prev,
                  const std::function<void (const event::id &)> &closure)
{
	json::for_each(prev, [&closure]
	(const auto &key, const json::array &prevs)
	{
		for(const json::array &prev : prevs)
		{
			const event::id &id{unquote(prev[0])};
			closure(id);
		}
	});
}

std::string
ircd::m::pretty(const event::prev &prev)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 2048);

	const auto out{[&s]
	(const string_view &key, auto&& val)
	{
		if(json::defined(val))
			s << key << ": " << val << std::endl;
	}};

	const auto &auth_events{json::get<"auth_events"_>(prev)};
	for(const json::array auth_event : auth_events)
		out("auth_event", unquote(auth_event[0]));

	const auto &prev_states{json::get<"prev_state"_>(prev)};
	for(const json::array prev_state : prev_states)
		out("prev_state", unquote(prev_state[0]));

	const auto &prev_events{json::get<"prev_events"_>(prev)};
	for(const json::array prev_event : prev_events)
		out("prev_event", unquote(prev_event[0]));

	resizebuf(s, ret);
	return ret;
}

std::string
ircd::m::pretty_oneline(const event::prev &prev)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 1024);

	const auto &auth_events{json::get<"auth_events"_>(prev)};
	s << "A[ ";
	for(const json::array auth_event : auth_events)
		s << unquote(auth_event[0]) << " ";
	s << "] ";

	const auto &prev_states{json::get<"prev_state"_>(prev)};
	s << "S[ ";
	for(const json::array prev_state : prev_states)
		s << unquote(prev_state[0]) << " ";
	s << "] ";

	const auto &prev_events{json::get<"prev_events"_>(prev)};
	s << "E[ ";
	for(const json::array prev_event : prev_events)
		s << unquote(prev_event[0]) << " ";
	s << "] ";

	resizebuf(s, ret);
	return ret;
}

std::string
ircd::m::pretty(const event &event)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 2048);

	const auto out{[&s]
	(const string_view &key, auto&& val)
	{
		if(json::defined(val))
			s << std::setw(16) << std::right << key << ": " << val << std::endl;
	}};

	const string_view top_keys[]
	{
		"origin",
		"event_id",
		"room_id",
		"sender",
		"type",
		"depth",
		"state_key",
		"membership",
	};

	json::for_each(event, top_keys, out);

	const auto &hashes{json::get<"hashes"_>(event)};
	for(const auto &hash : hashes)
	{
		s << std::setw(16) << std::right << "[hash]" << ": "
		  << hash.first
		  //<< " "
		  //<< hash.second
		  << std::endl;
	}

	const auto &signatures{json::get<"signatures"_>(event)};
	for(const auto &signature : signatures)
	{
		s << std::setw(16) << std::right << "[signature]" << ": "
		  << signature.first << " ";

		for(const auto &key : json::object{signature.second})
			s << key.first << " ";

		s << std::endl;
	}

	const json::object &contents{json::get<"content"_>(event)};
	if(!contents.empty())
	{
		s << std::setw(16) << std::right << "[content]" << ": ";
		for(const auto &content : contents)
			s << content.first << ", ";
		s << std::endl;
	}

	const auto &auth_events{json::get<"auth_events"_>(event)};
	for(const json::array auth_event : auth_events)
		out("[auth_event]", unquote(auth_event[0]));

	const auto &prev_states{json::get<"prev_state"_>(event)};
	for(const json::array prev_state : prev_states)
		out("[prev_state]", unquote(prev_state[0]));

	const auto &prev_events{json::get<"prev_events"_>(event)};
	for(const json::array prev_event : prev_events)
		out("[prev_event]", unquote(prev_event[0]));

	resizebuf(s, ret);
	return ret;
}

std::string
ircd::m::pretty_oneline(const event &event)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 1024);

	const auto out{[&s]
	(const string_view &key, auto&& val)
	{
		if(defined(val))
			s << val << " ";
		else
			s << "* ";
	}};

	const string_view top_keys[]
	{
		"origin",
		"event_id",
		"room_id",
		"sender",
		"depth",
	};

	s << ':';
	json::for_each(event, top_keys, out);

	const auto &auth_events{json::get<"auth_events"_>(event)};
	s << "pa:" << auth_events.count() << " ";

	const auto &prev_states{json::get<"prev_state"_>(event)};
	s << "ps:" << prev_states.count() << " ";

	const auto &prev_events{json::get<"prev_events"_>(event)};
	s << "pe:" << prev_events.count() << " ";

	const auto &hashes{json::get<"hashes"_>(event)};
	s << "[ ";
	for(const auto &hash : hashes)
		s << hash.first << " ";
	s << "] ";

	const auto &signatures{json::get<"signatures"_>(event)};
	s << "[ ";
	for(const auto &signature : signatures)
	{
		s << signature.first << "[ ";
		for(const auto &key : json::object{signature.second})
			s << key.first << " ";

		s << "] ";
	}
	s << "] ";

	out("type", json::get<"type"_>(event));

	const auto &state_key
	{
		json::get<"state_key"_>(event)
	};

	if(defined(state_key) && empty(state_key))
		s << "\"\"" << " ";
	else if(defined(state_key))
		s << state_key << " ";
	else
		s << "*" << " ";

	out("membership", json::get<"membership"_>(event));

	const json::object &contents{json::get<"content"_>(event)};
	if(!contents.empty())
	{
		s << "+" << string_view{contents}.size() << " bytes :";
		for(const auto &content : contents)
			s << content.first << " ";
	}

	resizebuf(s, ret);
	return ret;
}
