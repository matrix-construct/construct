// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::burst
{
	static void cache_warming(const node &, const opts &);
	static void gossip(const node &, const opts &);

	extern conf::item<seconds> cache_warmup_time;
}

decltype(ircd::m::burst::log)
ircd::m::burst::log
{
	"m.burst"
};

decltype(ircd::m::burst::cache_warmup_time)
ircd::m::burst::cache_warmup_time
{
	{ "name",     "ircd.m.cache_warmup_time" },
	{ "default",  3600L                      },
};

ircd::m::burst::burst::burst(const node &node,
                             const opts &opts)
try
{
	log::debug
	{
		log, "Bursting to node %s",
		string_view{node.node_id},
	};

	fed::clear_error(node.node_id);

	if(opts.cache_warming)
		if(ircd::uptime() < seconds(cache_warmup_time))
			cache_warming(node, opts);

	if(opts.gossip)
		gossip(node, opts);
}
catch(const ctx::interrupted &e)
{
	throw;
}
catch(const std::exception &e)
{
	log::derror
	{
		log, "Burst to '%s' :%s",
		string_view{node.node_id},
		e.what()
	};
}

void
ircd::m::burst::gossip(const node &node,
                       const opts &opts)
try
{


}
catch(const ctx::interrupted &e)
{
	throw;
}
catch(const std::exception &e)
{
	log::derror
	{
		log, "Gossip to '%s' :%s",
		string_view{node.node_id},
		e.what()
	};
}

/// We can smoothly warmup some memory caches after daemon startup as the
/// requests trickle in from remote servers. This function is invoked after
/// a remote contacts and reveals its identity with the X-Matrix verification.
void
ircd::m::burst::cache_warming(const node &node,
                              const opts &opts)
try
{
	// Make a query through SRV and A records.
	//net::dns::resolve(origin, net::dns::prefetch_ipport);
}
catch(const ctx::interrupted &e)
{
	throw;
}
catch(const std::exception &e)
{
	log::derror
	{
		log, "Cache warming for '%s' :%s",
		string_view{node.node_id},
		e.what()
	};
}
