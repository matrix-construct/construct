// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

// Internal utils
namespace ircd::m
{
	static bool _hook_match(const m::event &matching, const m::event &);
	static void _hook_fix_state_key(const json::members &, json::member &);
	static void _hook_fix_room_id(const json::members &, json::member &);
	static void _hook_fix_sender(const json::members &, json::member &);
	static json::strung _hook_make_feature(const json::members &);
}

/// Instance list linkage for all hook sites
template<>
decltype(ircd::util::instance_list<ircd::m::hook::base::site>::allocator)
ircd::util::instance_list<ircd::m::hook::base::site>::allocator
{};

template<>
decltype(ircd::util::instance_list<ircd::m::hook::base::site>::list)
ircd::util::instance_list<ircd::m::hook::base::site>::list
{
	allocator
};

/// Instance list linkage for all hooks
template<>
decltype(ircd::util::instance_list<ircd::m::hook::base>::allocator)
ircd::util::instance_list<ircd::m::hook::base>::allocator
{};

template<>
decltype(ircd::util::instance_list<ircd::m::hook::base>::list)
ircd::util::instance_list<ircd::m::hook::base>::list
{
	allocator
};

//
// hook::maps
//

struct ircd::m::hook::maps
{
	std::multimap<string_view, base *> origin;
	std::multimap<string_view, base *> room_id;
	std::multimap<string_view, base *> sender;
	std::multimap<string_view, base *> state_key;
	std::multimap<string_view, base *> type;
	std::vector<base *> always;

	size_t match(const event &match, const std::function<bool (base &)> &) const;
	size_t add(base &hook, const event &matching);
	size_t del(base &hook, const event &matching);

	maps();
	~maps() noexcept;
};

ircd::m::hook::maps::maps()
{
}

ircd::m::hook::maps::~maps()
noexcept
{
}

size_t
ircd::m::hook::maps::add(base &hook,
                         const event &matching)
{
	size_t ret{0};
	const auto map{[&hook, &ret]
	(auto &map, const string_view &value)
	{
		map.emplace(value, &hook);
		++ret;
	}};

	if(json::get<"origin"_>(matching))
		map(origin, at<"origin"_>(matching));

	if(json::get<"room_id"_>(matching))
		map(room_id, at<"room_id"_>(matching));

	if(json::get<"sender"_>(matching))
		map(sender, at<"sender"_>(matching));

	if(json::get<"state_key"_>(matching))
		map(state_key, at<"state_key"_>(matching));

	if(json::get<"type"_>(matching))
		map(type, at<"type"_>(matching));

	// Hook had no mappings which means it will match everything.
	// We don't increment the matcher count for this case.
	if(!ret)
		always.emplace_back(&hook);

	return ret;
}

size_t
ircd::m::hook::maps::del(base &hook,
                         const event &matching)
{
	size_t ret{0};
	const auto unmap{[&hook, &ret]
	(auto &map, const string_view &value)
	{
		auto pit{map.equal_range(value)};
		while(pit.first != pit.second)
			if(pit.first->second == &hook)
			{
				pit.first = map.erase(pit.first);
				++ret;
			}
			else ++pit.first;
	}};

	// Unconditional attempt to remove from always.
	std::remove(begin(always), end(always), &hook);

	if(json::get<"origin"_>(matching))
		unmap(origin, at<"origin"_>(matching));

	if(json::get<"room_id"_>(matching))
		unmap(room_id, at<"room_id"_>(matching));

	if(json::get<"sender"_>(matching))
		unmap(sender, at<"sender"_>(matching));

	if(json::get<"state_key"_>(matching))
		unmap(state_key, at<"state_key"_>(matching));

	if(json::get<"type"_>(matching))
		unmap(type, at<"type"_>(matching));

	return ret;
}

size_t
ircd::m::hook::maps::match(const event &event,
                           const std::function<bool (base &)> &callback)
const
{
	std::set<base *> matching
	{
		begin(always), end(always)
	};

	const auto site_match{[&matching]
	(auto &map, const string_view &key)
	{
		auto pit{map.equal_range(key)};
		for(; pit.first != pit.second; ++pit.first)
			matching.emplace(pit.first->second);
	}};

	if(json::get<"origin"_>(event))
		site_match(origin, at<"origin"_>(event));

	if(json::get<"room_id"_>(event))
		site_match(room_id, at<"room_id"_>(event));

	if(json::get<"sender"_>(event))
		site_match(sender, at<"sender"_>(event));

	if(json::get<"type"_>(event))
		site_match(type, at<"type"_>(event));

	if(json::get<"state_key"_>(event))
		site_match(state_key, at<"state_key"_>(event));

	auto it(begin(matching));
	while(it != end(matching))
	{
		const base &hook(**it);
		if(!_hook_match(hook.matching, event))
			it = matching.erase(it);
		else
			++it;
	}

	size_t ret{0};
	for(auto it(begin(matching)); it != end(matching); ++it, ++ret)
		if(!callback(**it))
			return ret;

	return ret;
}

//
// hook::base
//

/// Primary hook ctor
ircd::m::hook::base::base(const json::members &members)
:_feature
{
	members // _hook_make_feature(members)
}
,feature
{
	_feature
}
,matching
{
	feature
}
{
	site *site; try
	{
		if((site = find_site()))
			site->add(*this);
	}
	catch(...)
	{
		if(registered)
		{
			auto *const site(find_site());
			assert(site != nullptr);
			site->del(*this);
		}
	}
}

ircd::m::hook::base::~base()
noexcept
{
	if(!registered)
		return;

	auto *const site
	{
		find_site()
	};

	// should be non-null  if !registered
	assert(site != nullptr);

	// if someone is calling and inside this hook we shouldn't be destructing
	assert(calling == 0);

	// if someone is calling the hook::site but inside some other hook, we can
	// still remove this hook from the site.
	//assert(site->calling == 0);

	site->del(*this);
}

ircd::string_view
ircd::m::hook::base::site_name()
const try
{
	return unquote(feature.at("_site"));
}
catch(const std::out_of_range &e)
{
	throw panic
	{
		"Hook %p must name a '_site' to register with.", this
	};
}

ircd::m::hook::base::site *
ircd::m::hook::base::find_site()
const
{
	const auto &site_name
	{
		this->site_name()
	};

	if(!site_name)
		return nullptr;

	for(auto *const &site : m::hook::base::site::list)
		if(site->name() == site_name)
			return site;

	return nullptr;
}

uint
ircd::m::hook::base::id()
const
{
	uint ret(0);
	for(auto *const &hook : m::hook::base::list)
		if(hook != this)
			++ret;
		else
			return ret;

	throw std::out_of_range
	{
		"Hook not found in instance list."
	};
}

//
// hook::site
//

//
// hook::site::site
//

ircd::m::hook::base::site::site(const json::members &members)
:_feature
{
	members
}
,feature
{
	_feature
}
,maps
{
	std::make_unique<struct maps>()
}
,exceptions
{
	feature.get<bool>("exceptions", true)
}
,interrupts
{
	feature.get<bool>("interrupts", true)
}
{
	for(const auto &site : list)
		if(site->name() == name() && site != this)
			throw error
			{
				"Hook site '%s' already registered at site:%u",
				name(),
				site->id(),
			};

	// Find and register all of the orphan hooks which were constructed before
	// this site was constructed.
	for(auto *const &hook : m::hook::base::list)
		if(hook->site_name() == name())
			add(*hook);
}

ircd::m::hook::base::site::~site()
noexcept
{
	assert(!calling);
	const std::vector<base *> hooks
	{
		begin(this->hooks), end(this->hooks)
	};

	for(auto *const hook : hooks)
		del(*hook);
}

void
ircd::m::hook::base::site::match(const event &event,
                                 const std::function<bool (base &)> &callback)
{
	maps->match(event, callback);
}

bool
ircd::m::hook::base::site::add(base &hook)
{
	assert(!hook.registered);
	assert(hook.site_name() == name());
	assert(hook.matchers == 0);

	if(!hooks.emplace(&hook).second)
	{
		log::warning
		{
			log, "Hook:%u already registered to site:%u :%s",
			hook.id(),
			id(),
			name(),
		};

		return false;
	}

	assert(maps);
	const size_t matched
	{
		maps->add(hook, hook.matching)
	};

	hook.matchers = matched;
	hook.registered = true;
	matchers += matched;
	++count;

	log::debug
	{
		log, "Registered hook:%u to site:%u :%s",
		hook.id(),
		id(),
		name(),
	};

	return true;
}

bool
ircd::m::hook::base::site::del(base &hook)
{
	log::debug
	{
		log, "Removing hook:%u from site:%u :%s",
		hook.id(),
		id(),
		name(),
	};

	assert(hook.registered);
	assert(hook.site_name() == name());

	const size_t matched
	{
		maps->del(hook, hook.matching)
	};

	const auto erased
	{
		hooks.erase(&hook)
	};

	hook.matchers -= matched;
	hook.registered = false;
	matchers -= matched;
	--count;
	assert(hook.matchers == 0);
	assert(erased);
	return true;
}

ircd::string_view
ircd::m::hook::base::site::name()
const try
{
	return unquote(feature.at("name"));
}
catch(const std::out_of_range &e)
{
	throw panic
	{
		"Hook site %p requires a name", this
	};
}

uint
ircd::m::hook::base::site::id()
const
{
	uint ret(0);
	for(auto *const &site : m::hook::base::site::list)
		if(site != this)
			++ret;
		else
			return ret;

	throw std::out_of_range
	{
		"Hook site not found in instance list."
	};
}
//
// hook<void>
//

ircd::m::hook::hook<void>::hook(const json::members &feature,
                                decltype(function) function)
:base{feature}
,function{std::move(function)}
{
}

ircd::m::hook::hook<void>::hook(decltype(function) function,
                                const json::members &feature)
:base{feature}
,function{std::move(function)}
{
}

ircd::m::hook::site<void>::site(const json::members &feature)
:base::site{feature}
{
}

void
ircd::m::hook::site<void>::operator()(const event &event)
{
	base *cur {nullptr};
	operator()(&cur, event);
}

void
ircd::m::hook::site<void>::operator()(base **const &cur,
                                      const event &event)
{
	const ctx::uninterruptible::nothrow ui
	{
		!interrupts
	};

	// Iterate all matching hooks
	match(event, [this, &cur, &event]
	(base &base)
	{
		// Indicate which hook we're entering
		const scope_restore entered
		{
			*cur, std::addressof(base)
		};

		auto &hfn
		{
			dynamic_cast<hook<void> &>(base)
		};

		call(hfn, event);
		return true;
	});
}

void
ircd::m::hook::site<void>::call(hook<void> &hfn,
                                const event &event)
try
{
	// stats for site
	++calls;
	const scope_count site_calling
	{
		calling
	};

	// stats for hook
	++hfn.calls;
	const scope_count hook_calling
	{
		hfn.calling
	};

	// call hook
	hfn.function(event);
}
catch(const ctx::interrupted &e)
{
	if(exceptions && interrupts)
		throw;

	log::logf
	{
		log, interrupts? log::DERROR: log::ERROR,
		"site:%u hook:%u %s error :%s",
		id(),
		hfn.id(),
		string_view{hfn.feature},
		e.what(),
	};
}
catch(const std::exception &e)
{
	if(exceptions)
		throw;

	log::critical
	{
		log, "Unhandled site:%u hook:%u %s error :%s",
		this->id(),
		hfn.id(),
		string_view{hfn.feature},
		e.what(),
	};
}

//
// hook internal
//

/// Internal interface which manipulates the initializer supplied by the
/// developer to the hook to create the proper JSON output. i.e They supply
/// a "room_id" of "!config" which has no hostname, that is added here
/// depending on my_host() in the deployment runtime...
///
ircd::json::strung
ircd::m::_hook_make_feature(const json::members &members)
{
	const ctx::critical_assertion ca;
	std::vector<json::member> copy
	{
		begin(members), end(members)
	};

	for(auto &member : copy) switch(hash(member.first))
	{
		case hash("room_id"):
			_hook_fix_room_id(members, member);
			continue;

		case hash("sender"):
			_hook_fix_sender(members, member);
			continue;

		case hash("state_key"):
			_hook_fix_state_key(members, member);
			continue;
	}

	return { copy.data(), copy.data() + copy.size() };
}

void
ircd::m::_hook_fix_sender(const json::members &members,
                          json::member &member)
{
	// Rewrite the sender if the supplied input has no hostname
	if(valid_local_only(id::USER, member.second))
	{
		assert(my_host());
		thread_local char buf[256];
		member.second = id::user { buf, member.second, my_host() };
	}

	validate(id::USER, member.second);
}

void
ircd::m::_hook_fix_room_id(const json::members &members,
                           json::member &member)
{
	// Rewrite the room_id if the supplied input has no hostname
	if(valid_local_only(id::ROOM, member.second))
	{
		assert(my_host());
		thread_local char buf[256];
		member.second = id::room { buf, member.second, my_host() };
	}

	validate(id::ROOM, member.second);
}

void
ircd::m::_hook_fix_state_key(const json::members &members,
                             json::member &member)
{
	const bool is_member_event
	{
		end(members) != std::find_if(begin(members), end(members), []
		(const auto &member)
		{
			return member.first == "type" && member.second == "m.room.member";
		})
	};

	if(!is_member_event)
		return;

	// Rewrite the sender if the supplied input has no hostname
	if(valid_local_only(id::USER, member.second))
	{
		assert(my_host());
		thread_local char buf[256];
		member.second = id::user { buf, member.second, my_host() };
	}

	validate(id::USER, member.second);
}

bool
ircd::m::_hook_match(const m::event &matching,
                     const m::event &event)
{
	if(json::get<"origin"_>(matching))
		if(at<"origin"_>(matching) != json::get<"origin"_>(event))
			return false;

	if(json::get<"room_id"_>(matching))
		if(at<"room_id"_>(matching) != json::get<"room_id"_>(event))
			return false;

	if(json::get<"sender"_>(matching))
		if(at<"sender"_>(matching) != json::get<"sender"_>(event))
			return false;

	if(json::get<"type"_>(matching))
		if(at<"type"_>(matching) != json::get<"type"_>(event))
			return false;

	if(json::get<"state_key"_>(matching))
		if(at<"state_key"_>(matching) != json::get<"state_key"_>(event))
			return false;

	if(membership(matching))
		if(membership(matching) != membership(event))
			return false;

	if(json::get<"content"_>(matching))
		if(json::get<"type"_>(event) == "m.room.message")
			if(at<"content"_>(matching).has("msgtype"))
				if(at<"content"_>(matching).get("msgtype") != json::get<"content"_>(event).get("msgtype"))
					return false;

	return true;
}
