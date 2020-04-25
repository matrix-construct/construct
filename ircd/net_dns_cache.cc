// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::net::dns::cache::min_ttl)
ircd::net::dns::cache::min_ttl
{
	{ "name",     "ircd.net.dns.cache.min_ttl" },
	{ "default",  28800L                       },
};

decltype(ircd::net::dns::cache::error_ttl)
ircd::net::dns::cache::error_ttl
{
	{ "name",     "ircd.net.dns.cache.error_ttl" },
	{ "default",  1200L                          },
};

decltype(ircd::net::dns::cache::nxdomain_ttl)
ircd::net::dns::cache::nxdomain_ttl
{
	{ "name",     "ircd.net.dns.cache.nxdomain_ttl" },
	{ "default",  86400L                            },
};

decltype(ircd::net::dns::cache::waiting)
ircd::net::dns::cache::waiting;

decltype(ircd::net::dns::cache::mutex)
ircd::net::dns::cache::mutex;

decltype(ircd::net::dns::cache::dock)
ircd::net::dns::cache::dock;

bool
ircd::net::dns::cache::put(const hostport &h,
                           const opts &o,
                           const uint &r,
                           const string_view &m)
try
{
	using prototype = bool (const hostport &, const opts &, const uint &, const string_view &);

	static mods::import<prototype> call
	{
		"net_dns_cache", "ircd::net::dns::cache::put"
	};

	return call(h, o, r, m);
}
catch(const mods::unavailable &e)
{
	thread_local char buf[rfc1035::NAME_BUFSIZE];
	log::dwarning
	{
		log, "Failed to put error for '%s' in DNS cache :%s",
		string(buf, h),
		e.what()
	};

	return false;
}

bool
ircd::net::dns::cache::put(const hostport &h,
                           const opts &o,
                           const records &r)
try
{
	using prototype = bool (const hostport &, const opts &, const records &);

	static mods::import<prototype> call
	{
		"net_dns_cache", "ircd::net::dns::cache::put"
	};

	return call(h, o, r);
}
catch(const mods::unavailable &e)
{
	thread_local char buf[rfc1035::NAME_BUFSIZE];
	log::dwarning
	{
		log, "Failed to put '%s' in DNS cache :%s",
		string(buf, h),
		e.what()
	};

	return false;
}

/// This function has an opportunity to respond from the DNS cache. If it
/// returns true, that indicates it responded by calling back the user and
/// nothing further should be done for them. If it returns false, that
/// indicates it did not respond and to proceed normally. The response can
/// be of a cached successful result, or a cached error. Both will return
/// true.
bool
ircd::net::dns::cache::get(const hostport &h,
                           const opts &o,
                           const callback &c)
try
{
	using prototype = bool (const hostport &, const opts &, const callback &);

	static mods::import<prototype> call
	{
		"net_dns_cache", "ircd::net::dns::cache::get"
	};

	return call(h, o, c);
}
catch(const mods::unavailable &e)
{
	thread_local char buf[rfc1035::NAME_BUFSIZE];
	log::dwarning
	{
		log, "Failed to get '%s' from DNS cache :%s",
		string(buf, h),
		e.what()
	};

	return false;
}

bool
ircd::net::dns::cache::for_each(const hostport &h,
                                const opts &o,
                                const closure &c)
{
	using prototype = bool (const hostport &, const opts &, const closure &);

	static mods::import<prototype> call
	{
		"net_dns_cache", "ircd::net::dns::cache::for_each"
	};

	return call(h, o, c);
}

bool
ircd::net::dns::cache::for_each(const string_view &type,
                                const closure &c)
{
	using prototype = bool (const string_view &, const closure &);

	static mods::import<prototype> call
	{
		"net_dns_cache", "ircd::net::dns::cache::for_each"
	};

	return call(type, c);
}

ircd::string_view
ircd::net::dns::cache::make_type(const mutable_buffer &out,
                                 const uint16_t &type)
try
{
	return make_type(out, rfc1035::rqtype.at(type));
}
catch(const std::out_of_range &)
{
	throw error
	{
		"Record type[%u] is not recognized", type
	};
}

ircd::string_view
ircd::net::dns::cache::make_type(const mutable_buffer &out,
                                 const string_view &type)
{
	return fmt::sprintf
	{
		out, "ircd.dns.rrs.%s", type
	};
}

//
// cache::waiter
//

bool
ircd::net::dns::cache::operator==(const waiter &a, const waiter &b)
noexcept
{
	return
	a.opts.qtype == b.opts.qtype &&
	a.key && b.key &&
	a.key == b.key;
}

bool
ircd::net::dns::cache::operator!=(const waiter &a, const waiter &b)
noexcept
{
	return !operator==(a, b);
}

//
// cache::waiter::waiter
//

ircd::net::dns::cache::waiter::waiter(const hostport &hp,
                                      const dns::opts &opts,
                                      dns::callback &&callback)
:callback
{
	std::move(callback)
}
,opts
{
	opts
}
,port
{
	net::port(hp)
}
,key
{
	opts.qtype == 33?
		make_SRV_key(keybuf, hp, opts):
		strlcpy(keybuf, host(hp))
}
{
	this->opts.srv = {};
	this->opts.proto = {};
	assert(this->opts.qtype);
}

/// Note complications due to reentrance and other factors:
/// - This function is invoked from several different places on both the
/// timeout and receive contexts, in addition to any evaluator context.
/// - This function calls back to users making DNS queries, and they may
/// conduct another query in their callback frame -- mid-loop in this
/// function.
size_t
ircd::net::dns::cache::waiter::call(const uint16_t &type,
                                    const string_view &tgt,
                                    const json::array &rrs)
{
	const ctx::uninterruptible::nothrow ui;
	size_t ret(0), last; do
	{
		const std::lock_guard lock
		{
			mutex
		};

		auto it(begin(waiting));
		for(last = ret; it != end(waiting);)
			if(call(*it, type, tgt, rrs))
			{
				it = waiting.erase(it);
				++ret;
				continue;
			}
			else ++it;
	}
	while(last < ret);

	if(ret)
		dock.notify_all();

	return ret;
}

bool
ircd::net::dns::cache::waiter::call(waiter &waiter,
                                    const uint16_t &type,
                                    const string_view &tgt,
                                    const json::array &rrs)
try
{
	if(tgt != waiter.key)
		return false;

	if(type != waiter.opts.qtype)
		return false;

	const hostport &target
	{
		waiter.opts.qtype == 33?
			unmake_SRV_key(waiter.key):
			waiter.key,

		waiter.port
	};

	assert(waiter.callback);
	waiter.callback(target, rrs);
	return true;
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "callback:%p %s,%s :%s",
		(const void *)&waiter,
		type,
		tgt,
		e.what(),
	};

	return true;
}
