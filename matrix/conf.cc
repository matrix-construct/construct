// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	struct conf_room;
}

using namespace ircd;

extern "C" void default_conf(const string_view &prefix);
extern "C" void rehash_conf(const string_view &prefix, const bool &existing);
extern "C" void reload_conf();
extern "C" void refresh_conf();
static void init_conf_item(conf::item<> &);

struct ircd::m::conf_room
{
	m::room::id::buf room_id
	{
		"conf", my_host()
	};

	m::room room
	{
	    room_id
	};

    operator const m::room &() const
    {
        return room;
    }
};

// This module registers with conf::on_init to be called back
// when a conf item is initialized; when this module is unloaded
// we have to unregister that listener using this state.
decltype(conf::on_init)::const_iterator
conf_on_init_iter
{
	end(conf::on_init)
};

void conf_on_init()
{
	conf_on_init_iter = conf::on_init.emplace(end(conf::on_init), init_conf_item);
	reload_conf();
}

void conf_on_fini()
{
	assert(conf_on_init_iter != end(conf::on_init));
	conf::on_init.erase(conf_on_init_iter);
}

/// Set to false to quiet errors from a conf item failing to set
bool
item_error_log
{
	true
};

extern "C" m::event::id::buf
set_conf_item(const m::user::id &sender,
              const string_view &key,
              const string_view &val)
{
	if(conf::exists(key) && !conf::persists(key))
	{
		conf::set(key, val);
		return {};
	}

	const m::conf_room conf_room;
	return send(conf_room, sender, "ircd.conf.item", key,
	{
		{ "value", val }
	});
}

extern "C" void
get_conf_item(const string_view &key,
              const std::function<void (const string_view &)> &closure)
{
	static const m::event::fetch::opts fopts
	{
		m::event::keys::include { "content" }
	};

	const m::conf_room conf_room;
	const m::room::state state
	{
		conf_room, &fopts
	};

	state.get("ircd.conf.item", key, [&closure]
	(const m::event &event)
	{
		const auto &value
		{
			unquote(at<"content"_>(event).at("value"))
		};

		closure(value);
	});
}

static void
conf_updated(const m::event &event)
try
{
	const auto &content
	{
		at<"content"_>(event)
	};

	const auto &key
	{
		at<"state_key"_>(event)
	};

	const string_view &value
	{
		unquote(content.at("value"))
	};

	if(run::level == run::level::START && !conf::exists(key))
		return;

	// Conf items marked with a persist=false property are not read from
	// the conf room into the item, even if the value exists in the room.
	if(conf::exists(key) && !conf::persists(key))
		return;

	log::debug
	{
		"Updating conf [%s] => %s", key, value
	};

	ircd::conf::set(key, value);
}
catch(const std::exception &e)
{
	if(item_error_log)
		log::error
		{
			"Failed to set conf item '%s' :%s",
			json::get<"state_key"_>(event),
			e.what()
		};
}

static void
conf_updated(const m::event::idx &event_idx)
try
{
	static const m::event::fetch::opts fopts
	{
		m::event::keys::include { "content", "state_key" }
	};

	const m::event::fetch event
	{
		event_idx, fopts
	};

	conf_updated(event);
}
catch(const std::exception &e)
{
	if(item_error_log)
		log::error
		{
			"Failed to set conf item by event_idx:%lu :%s",
			event_idx,
			e.what()
		};
}

static void
handle_conf_updated(const m::event &event,
                    m::vm::eval &)
{
	conf_updated(event);
}

m::hookfn<m::vm::eval &>
conf_updated_hook
{
	handle_conf_updated,
	{
		{ "_site",       "vm.effect"       },
		{ "room_id",     "!conf"           },
		{ "type",        "ircd.conf.item"  },
	}
};

static void
init_conf_items()
{
	static const m::event::fetch::opts fopts
	{
		m::event::keys::include { "content", "state_key" }
	};

	const m::conf_room conf_room;
	const m::room::state state
	{
		conf_room, &fopts
	};

	state.prefetch("ircd.conf.item");
	state.for_each("ircd.conf.item", []
	(const auto &, const auto &state_key, const auto &event_idx)
	{
		if(!conf::exists(state_key))
			return true;

		conf_updated(event_idx);
		return true;
	});
}

static void
init_conf_item(conf::item<> &item)
{
	const m::conf_room conf_room;
	const m::room::state state
	{
		conf_room
	};

	const auto &event_idx
	{
		state.get(std::nothrow, "ircd.conf.item", item.name)
	};

	if(!event_idx)
		return;

	conf_updated(event_idx);
}

static m::event::id::buf
create_conf_item(const string_view &key,
                 const conf::item<> &item)
try
{
	thread_local char vbuf[4_KiB];
	const string_view &val
	{
		item.get(vbuf)
	};

	return set_conf_item(m::me.user_id, key, val);
}
catch(const std::exception &e)
{
	if(item_error_log) log::error
	{
		"Failed to create conf item '%s' :%s",
		key,
		e.what()
	};

	return {};
}

static void
create_conf_room(const m::event &,
                 m::vm::eval &)
{
	const m::conf_room conf_room;
	m::create(conf_room.room_id, m::me.user_id);
	//rehash_conf({}, true);
}

m::hookfn<m::vm::eval &>
create_conf_room_hook
{
	create_conf_room,
	{
		{ "_site",       "vm.effect"      },
		{ "room_id",     "!ircd"          },
		{ "type",        "m.room.create"  },
	}
};

void
rehash_conf(const string_view &prefix,
            const bool &existing)
{
	const m::conf_room conf_room;
	const m::room::state state
	{
		conf_room
	};

	for(const auto &p : conf::items)
	{
		const auto &key{p.first};
		if(prefix && !startswith(key, prefix))
			continue;

		const auto &item{p.second}; assert(item);

		// Conf items marked with a persist=false property are not written
		// to the conf room.
		if(!item->feature.get("persist", true))
			continue;

		// Use the `existing` argument to toggle a force-overwrite
		if(!existing)
			if(state.has("ircd.conf.item", key))
				continue;

		create_conf_item(key, *item);
	}
}

void
default_conf(const string_view &prefix)
{
	for(const auto &p : conf::items)
	{
		const auto &key{p.first};
		if(prefix && !startswith(key, prefix))
			continue;

		const auto &item{p.second}; assert(item);
		const auto value
		{
			unquote(item->feature["default"])
		};

		conf::set(key, value);
	}
}

void
reload_conf()
{
	init_conf_items();
}

void
refresh_conf()
{
	ircd::conf::reset();
}
