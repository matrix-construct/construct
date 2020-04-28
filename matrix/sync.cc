// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::sync
{
	extern const ctx::pool::opts pool_opts;
}

decltype(ircd::m::sync::stats_info)
ircd::m::sync::stats_info
{
	{ "name",     "ircd.m.sync.stats.info" },
	{ "default",  false                    },
};

decltype(ircd::m::sync::log)
ircd::m::sync::log
{
	"m.sync", 's'
};

decltype(ircd::m::sync::pool_opts)
ircd::m::sync::pool_opts
{
	ctx::DEFAULT_STACK_SIZE, 0, -1, 0
};

decltype(ircd::m::sync::pool)
ircd::m::sync::pool
{
	"m.sync", pool_opts
};

template<>
decltype(ircd::util::instance_multimap<std::string, ircd::m::sync::item, std::less<>>::map)
ircd::util::instance_multimap<std::string, ircd::m::sync::item, std::less<>>::map
{};

template<>
decltype(ircd::util::instance_list<ircd::m::sync::data>::allocator)
ircd::util::instance_list<ircd::m::sync::data>::allocator
{};

template<>
decltype(ircd::util::instance_list<ircd::m::sync::data>::list)
ircd::util::instance_list<ircd::m::sync::data>::list
{
	allocator
};

bool
ircd::m::sync::for_each(const item_closure_bool &closure)
{
	auto it(begin(item::map));
	for(; it != end(item::map); ++it)
		if(!closure(*it->second))
			return false;

	return true;
}

bool
ircd::m::sync::for_each(const string_view &prefix,
                        const item_closure_bool &closure)
{
	const auto depth
	{
		token_count(prefix, '.')
	};

	auto it
	{
		item::map.lower_bound(prefix)
	};

	for(; it != end(item::map); ++it)
	{
		const auto item_depth
		{
			token_count(it->first, '.')
		};

		if(item_depth > depth + 1)
			continue;

		if(it->first == prefix)
			continue;

		if(item_depth < depth + 1)
			break;

		if(!closure(*it->second))
			return false;
	}

	return true;
}

ircd::string_view
ircd::m::sync::make_since(const mutable_buffer &buf,
                          const int64_t &val)
{
	return fmt::sprintf
	{
		buf, "ctor_%lu",
		val
	};
}

ircd::string_view
ircd::m::sync::make_since(const mutable_buffer &buf,
                          const m::events::range &val)
{
	return fmt::sprintf
	{
		buf, "ctor_%lu_%lu",
		val.first,
		val.second,
	};
}

ircd::string_view
ircd::m::sync::loghead(const data &data)
{
	thread_local char headbuf[256], rembuf[128], iecbuf[2][64], tmbuf[32];

	const auto remstr
	{
		data.client?
			string(rembuf, ircd::remote(*data.client)):
			string_view{}
	};

	const auto flush_bytes
	{
		data.stats?
			data.stats->flush_bytes:
			0U
	};

	const auto flush_count
	{
		data.stats?
			data.stats->flush_count:
			0U
	};

	const auto tmstr
	{
		data.stats?
			ircd::pretty(tmbuf, data.stats->timer.at<milliseconds>(), true):
			string_view{}
	};

	return fmt::sprintf
	{
		headbuf, "%s %s %ld:%lu|%lu%s chunk:%zu sent:%s of %s in %s",
		remstr,
		string_view{data.user.user_id},
		data.range.first,
		data.range.second,
		vm::sequence::retired,
		data.phased?
			"|P"_sv : ""_sv,
		flush_count,
		ircd::pretty(iecbuf[1], iec(flush_bytes)),
		data.out?
			ircd::pretty(iecbuf[0], iec(flush_bytes + size(data.out->completed()))):
			string_view{},
		tmstr
	};
}

//
// item
//

//
// item::item
//

ircd::m::sync::item::item(std::string name,
                          handle polylog,
                          handle linear,
                          const json::members &feature)
:instance_multimap
{
	std::move(name)
}
,conf_name
{
	fmt::snstringf{128, "ircd.m.sync.%s.enable", this->name()},
	fmt::snstringf{128, "ircd.m.sync.%s.stats.debug", this->name()},
}
,enable
{
	{ "name",     conf_name[0] },
	{ "default",  true         },
}
,stats_debug
{
	{ "name",     conf_name[1] },
	{ "default",  false        },
}
,_polylog
{
	std::move(polylog)
}
,_linear
{
	std::move(linear)
}
,feature
{
	feature
}
,opts
{
	this->feature
}
,phased
{
	opts.get<bool>("phased", false)
}
{
	log::debug
	{
		log, "Registered sync item(%p) '%s' (%zu features)",
		this,
		this->name(),
		opts.size(),
	};
}

ircd::m::sync::item::~item()
noexcept
{
	log::debug
	{
		log, "Unregistered sync item(%p) '%s'",
		this,
		this->name()
	};
}

bool
ircd::m::sync::item::polylog(data &data)
try
{
	// Skip the item if disabled by configuration
	if(!enable)
		return false;

	if(data.phased && !phased && int64_t(data.range.first) < 0)
		return false;

	#ifdef RB_DEBUG
	sync::stats stats
	{
		data.stats && (stats_info || stats_debug)?
			*data.stats:
			sync::stats{}
	};

	if(data.stats && (stats_info || stats_debug))
		stats.timer = {};
	#endif

	const bool ret
	{
		_polylog(data)
	};

	#ifdef RB_DEBUG
	if(data.stats && (stats_info || stats_debug))
	{
		//data.out.flush();
		thread_local char tmbuf[32];
		log::debug
		{
			log, "polylog %s commit:%b '%s' %s",
			loghead(data),
			ret,
			name(),
			ircd::pretty(tmbuf, stats.timer.at<microseconds>(), true)
		};
	}
	#endif

	this_ctx::interruption_point();
	return ret;
}
catch(const std::bad_function_call &e)
{
	log::dwarning
	{
		log, "polylog %s '%s' missing handler :%s",
		loghead(data),
		name(),
		e.what()
	};

	return false;
}
catch(const m::error &e)
{
	log::derror
	{
		log, "polylog %s '%s' :%s %s",
		loghead(data),
		name(),
		e.what(),
		e.content
	};

	return false;
}
catch(const std::exception &e)
{
	log::derror
	{
		log, "polylog %s '%s' :%s",
		loghead(data),
		name(),
		e.what()
	};

	throw;
}

bool
ircd::m::sync::item::linear(data &data)
try
{
	if(!enable)
		return false;

	return _linear(data);
}
catch(const std::bad_function_call &e)
{
	thread_local char rembuf[128];
	log::dwarning
	{
		log, "linear %s '%s' missing handler :%s",
		loghead(data),
		name(),
		e.what()
	};

	return false;
}
catch(const m::error &e)
{
	log::derror
	{
		log, "linear %s '%s' :%s %s",
		loghead(data),
		name(),
		e.what(),
		e.content
	};

	return false;
}
catch(const std::exception &e)
{
	log::derror
	{
		log, "linear %s '%s' :%s",
		loghead(data),
		name(),
		e.what()
	};

	throw;
}

size_t
ircd::m::sync::item::children()
const
{
	size_t ret(0);
	sync::for_each(this->name(), [&ret]
	(auto &item)
	{
		++ret;
		return true;
	});

	return ret;
}

ircd::string_view
ircd::m::sync::item::member_name()
const
{
	return token_last(name(), '.');
}

ircd::string_view
ircd::m::sync::item::name()
const
{
	return this->instance_multimap::it->first;
}

//
// sync/args.h
//

ircd::conf::item<ircd::milliseconds>
ircd::m::sync::args::timeout_max
{
	{ "name",     "ircd.client.sync.timeout.max"  },
	{ "default",  180 * 1000L                     },
};

ircd::conf::item<ircd::milliseconds>
ircd::m::sync::args::timeout_min
{
	{ "name",     "ircd.client.sync.timeout.min"  },
	{ "default",  15 * 1000L                      },
};

ircd::conf::item<ircd::milliseconds>
ircd::m::sync::args::timeout_default
{
	{ "name",     "ircd.client.sync.timeout.default"  },
	{ "default",  90 * 1000L                          },
};

//
// args::args
//

ircd::m::sync::args::args(const ircd::resource::request &request)
try
:filter_id
{
	request.query["filter"]
}
,since_token
{
	split(lstrip(request.query.get("since", "0"_sv), "ctor_"), '_')
}
,since
{
	lex_cast<uint64_t>(since_token.first)
}
,next_batch_token
{
	request.query.get("next_batch", since_token.second)
}
,next_batch
{
	uint64_t(lex_cast<uint64_t>(next_batch_token?: "-1"_sv))
}
,timesout
{
	ircd::now<system_point>() + std::clamp
	(
		request.query.get("timeout", milliseconds(timeout_default)),
		milliseconds(timeout_min),
		milliseconds(timeout_max)
	)
}
,full_state
{
	request.query.get("full_state", false)
}
,set_presence
{
	request.query.get("set_presence", true)
}
,phased
{
	request.query.get("phased", true)
}
,semaphore
{
	request.query.get("semaphore", false)
}
{
}
catch(const bad_lex_cast &e)
{
	throw m::BAD_REQUEST
	{
		"Since parameter invalid :%s", e.what()
	};
}
