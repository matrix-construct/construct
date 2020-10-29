// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_IFADDRS_H

#ifdef HAVE_IFADDRS_H
bool
ircd::net::addrs::has_usable_ipv6_interface()
try
{
	return !for_each([](const addr &a)
	{
		if(a.family != AF_INET6)
			return true;

		if(a.scope_id != 0) // global scope
			return true;

		if(~a.flags & IFF_UP) // not up
			return true;

		if(a.flags & IFF_LOOPBACK) // not usable
			return true;

		// return false to break
		return false;
	});
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Failed to check for usable IPv6 interfaces :%s",
		e.what()
	};

	return false;
}
#else
bool
ircd::net::addrs::has_usable_ipv6_interface()
{
	return false;
}
#endif

#ifdef HAVE_IFADDRS_H
[[GCC::optimize(0), clang::optnone]] //XXX: trouble
bool
ircd::net::addrs::for_each(const closure &closure)
{
	return for_each(raw_closure{[&closure]
	(const struct ::ifaddrs *const &ifa)
	{
		addr a;
		a.name = ifa->ifa_name;
		a.flags = ifa->ifa_flags;
		if(ifa->ifa_addr) switch(ifa->ifa_addr->sa_family)
		{
			case AF_INET6:
			{
				const auto sin(reinterpret_cast<const struct ::sockaddr_in6 *>(ifa->ifa_addr));
				const auto ip(reinterpret_cast<const uint128_t *>(sin->sin6_addr.s6_addr));
				a.family = sin->sin6_family;
				a.scope_id = sin->sin6_scope_id;
				a.flowinfo = sin->sin6_flowinfo;
				a.address =
				{
					ntoh(*ip), sin->sin6_port
				};
				break;
			}

			case AF_INET:
			{
				const auto &sin(reinterpret_cast<const struct ::sockaddr_in *>(ifa->ifa_addr));
				a.family = sin->sin_family;
				a.address =
				{
					ntoh(sin->sin_addr.s_addr), sin->sin_port
				};
				break;
			}

			default:
				return true;
		}

		return closure(a);
	}});
}
#else
bool
ircd::net::addrs::for_each(const closure &closure)
{
	return true;
}
#endif

#ifdef HAVE_IFADDRS_H
bool
ircd::net::addrs::for_each(const raw_closure &closure)
{
	struct ::ifaddrs *ifap_;
	syscall(::getifaddrs, &ifap_);
	const custom_ptr<struct ::ifaddrs> ifap
	{
		ifap_, ::freeifaddrs
	};

	for(auto ifa(ifap.get()); ifa; ifa = ifa->ifa_next)
		if(!closure(ifa))
			return false;

	return true;
}
#else
bool
ircd::net::addrs::for_each(const raw_closure &closure)
{
	return true;
}
#endif
