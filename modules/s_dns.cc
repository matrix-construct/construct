// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd::net;
using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Server Domain Names Cache & Modular Components"
};

std::multimap<std::string, rfc1035::record::A, std::less<>>
cache_A;

std::multimap<std::string, rfc1035::record::SRV, std::less<>>
cache_SRV;

extern "C" ircd::rfc1035::record *
cache__put_error(const rfc1035::question &question,
                 const uint &code)
{
	const auto &host
	{
		rstrip(question.name, '.')
	};

	assert(!empty(host));
	switch(question.qtype)
	{
		case 1: // A
		{
			auto &map{cache_A};
			auto pit
			{
				map.equal_range(host)
			};

			auto it
			{
				pit.first != pit.second?
					map.erase(pit.first, pit.second):
					pit.first
			};

			rfc1035::record::A record;
			record.ttl = ircd::time() + seconds(dns::cache::clear_nxdomain).count(); //TODO: code
			it = map.emplace_hint(it, host, record);
			return &it->second;
		}

		case 33: // SRV
		{
			auto &map{cache_SRV};
			auto pit
			{
				map.equal_range(host)
			};

			auto it
			{
				pit.first != pit.second?
					map.erase(pit.first, pit.second):
					pit.first
			};

			rfc1035::record::SRV record;
			record.ttl = ircd::time() + seconds(dns::cache::clear_nxdomain).count(); //TODO: code
			it = map.emplace_hint(it, host, record);
			return &it->second;
		}
	}

	return nullptr;
}

extern "C" ircd::rfc1035::record *
cache__put(const rfc1035::question &question,
           const rfc1035::answer &answer)
{
	const auto &host
	{
		rstrip(question.name, '.')
	};

	assert(!empty(host));
	switch(answer.qtype)
	{
		case 1: // A
		{
			auto &map{cache_A};
			auto pit
			{
				map.equal_range(host)
			};

			auto it(pit.first);
			while(it != pit.second)
			{
				const auto &rr{it->second};
				if(rr == answer)
					it = map.erase(it);
				else
					++it;
			}

			const auto &iit
			{
				map.emplace_hint(it, host, answer)
			};

			return &iit->second;
		}

		case 33: // SRV
		{
			auto &map{cache_SRV};
			auto pit
			{
				map.equal_range(host)
			};

			auto it(pit.first);
			while(it != pit.second)
			{
				const auto &rr{it->second};
				if(rr == answer)
					it = map.erase(it);
				else
					++it;
			}

			const auto &iit
			{
				map.emplace_hint(it, host, answer)
			};

			return &iit->second;
		}

		default:
			return nullptr;
	}
}

/// This function has an opportunity to respond from the DNS cache. If it
/// returns true, that indicates it responded by calling back the user and
/// nothing further should be done for them. If it returns false, that
/// indicates it did not respond and to proceed normally. The response can
/// be of a cached successful result, or a cached error. Both will return
/// true.
extern "C" bool
cache__get(const hostport &hp,
           const dns::opts &opts,
           const dns::callback &cb)
{
	// It's no use putting the result record array on the stack in case this
	// function is either called from an ircd::ctx or calls back an ircd::ctx.
	// If the ctx yields the records can still be evicted from the cache.
	// It's better to just force the user to conform here rather than adding
	// ref counting and other pornographic complications to this cache.
	const ctx::critical_assertion ca;
	thread_local std::array<const rfc1035::record *, dns::MAX_COUNT> record;
	std::exception_ptr eptr;
	size_t count{0};

	//TODO: Better deduction
	if(hp.service || opts.srv) // deduced SRV query
	{
		assert(!empty(host(hp)));
		thread_local char srvbuf[512];
		const string_view srvhost
		{
			dns::make_SRV_key(srvbuf, hp, opts)
		};

		auto &map{cache_SRV};
		const auto pit{map.equal_range(srvhost)};
		if(pit.first == pit.second)
			return false;

		const auto &now{ircd::time()};
		for(auto it(pit.first); it != pit.second; )
		{
			const auto &rr{it->second};

			// Cached entry is too old, ignore and erase
			if(rr.ttl < now)
			{
				it = map.erase(it);
				continue;
			}

			// Cached entry is a cached error, we set the eptr, but also
			// include the record and increment the count like normal.
			if((!rr.tgt || !rr.port) && opts.nxdomain_exceptions && !eptr)
			{
				//TODO: we don't cache what the error was, assuming it's
				//TODO: NXDomain can be incorrect and in bad ways downstream...
				static const auto rcode{3}; //NXDomain
				eptr = std::make_exception_ptr(rfc1035::error
				{
					"protocol error #%u (cached) :%s", rcode, rfc1035::rcode.at(rcode)
				});
			}

			if(count < record.size())
				record.at(count++) = &rr;

			++it;
		}
	}
	else // Deduced A query (for now)
	{
		auto &map{cache_A};
		const auto &key{rstrip(host(hp), '.')};
		if(unlikely(empty(key)))
			return false;

		const auto pit{map.equal_range(key)};
		if(pit.first == pit.second)
			return false;

		const auto &now{ircd::time()};
		for(auto it(pit.first); it != pit.second; )
		{
			const auto &rr{it->second};

			// Cached entry is too old, ignore and erase
			if(rr.ttl < now)
			{
				it = map.erase(it);
				continue;
			}

			// Cached entry is a cached error, we set the eptr, but also
			// include the record and increment the count like normal.
			if(!rr.ip4 && !eptr)
			{
				//TODO: we don't cache what the error was, assuming it's
				//TODO: NXDomain can be incorrect and in bad ways downstream...
				static const auto rcode{3}; //NXDomain
				eptr = std::make_exception_ptr(rfc1035::error
				{
					"protocol error #%u (cached) :%s", rcode, rfc1035::rcode.at(rcode)
				});
			}

			if(count < record.size())
				record.at(count++) = &rr;

			++it;
		}
	}

	assert(count || !eptr);        // no error if no cache response
	assert(!eptr || count == 1);   // if error, should only be one entry.

	if(count)
		cb(std::move(eptr), hp, vector_view<const rfc1035::record *>(record.data(), count));

	return count;
}

extern "C" bool
cache__for_each(const uint16_t &type,
                const dns::cache::closure &closure)
{
	switch(type)
	{
		case 1: // A
		{
			for(const auto &pair : cache_A)
			{
				const auto &host(pair.first);
				const auto &record(pair.second);
				if(!closure(host, record))
					return false;
			}

			return true;
		}

		case 33: // SRV
		{
			for(const auto &pair : cache_SRV)
			{
				const auto &host(pair.first);
				const auto &record(pair.second);
				if(!closure(host, record))
					return false;
			}

			return true;
		}

		default:
			return true;
	}
}
