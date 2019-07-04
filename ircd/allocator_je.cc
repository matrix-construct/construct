// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_JEMALLOC_H

namespace ircd::allocator
{
	static std::function<void (std::ostream &, const string_view &)> je_stats_callback;
	static void je_stats_handler(void *, const char *);

	extern info::versions je_malloc_version_api;
	extern info::versions je_malloc_version_abi;
}

decltype(ircd::allocator::je_malloc_version_api)
ircd::allocator::je_malloc_version_api
{
	"jemalloc", info::versions::API, 0,
	#ifdef HAVE_JEMALLOC_H
	{
		JEMALLOC_VERSION_MAJOR,
		JEMALLOC_VERSION_MINOR,
		JEMALLOC_VERSION_BUGFIX
	},
	JEMALLOC_VERSION
	#endif
};

decltype(ircd::allocator::je_malloc_version_abi)
ircd::allocator::je_malloc_version_abi
{
	"jemalloc", info::versions::ABI, //TODO: get this
};

#if defined(IRCD_ALLOCATOR_USE_JEMALLOC) && defined(HAVE_JEMALLOC_H)
bool
ircd::allocator::trim(const size_t &pad)
noexcept
{
	return false;
}
#endif

#if defined(IRCD_ALLOCATOR_USE_JEMALLOC) && defined(HAVE_JEMALLOC_H)
ircd::string_view
ircd::allocator::get(const string_view &key_,
                     const mutable_buffer &buf)
{
	thread_local char key[128];
	strlcpy(key, key_);

	size_t len(size(buf));
	syscall(::mallctl, key, data(buf), &len, nullptr, 0UL);
	return string_view
	{
		data(buf), len
	};
}
#endif

#if defined(IRCD_ALLOCATOR_USE_JEMALLOC) && defined(HAVE_JEMALLOC_H)
ircd::string_view
ircd::allocator::set(const string_view &key_,
                     const string_view &val,
                     const mutable_buffer &cur)
{
	thread_local char key[128];
	strlcpy(key, key_);

	size_t curlen(size(cur));
	syscall(::mallctl, key, data(cur), &curlen, const_cast<char *>(data(val)), size(val));
	return string_view
	{
		data(cur), curlen
	};
}
#endif

void
ircd::allocator::je_stats_handler(void *const ptr,
                                  const char *const msg)
try
{
	auto &out
	{
		*reinterpret_cast<std::stringstream *>(ptr)
	};

	je_stats_callback(out, msg);
}
catch(const std::bad_function_call &)
{
	assert(0);
	return;
}

#if defined(IRCD_ALLOCATOR_USE_JEMALLOC) && defined(HAVE_JEMALLOC_H)
ircd::string_view
ircd::allocator::info(const mutable_buffer &buf)
{
	std::stringstream out;
	pubsetbuf(out, buf);

	je_stats_callback = []
	(auto &out, const string_view &msg)
	{
		out << msg;
	};

	static const char *const &opts
	{
		""
	};

	malloc_stats_print(je_stats_handler, &out, opts);
	out << std::endl;
	return view(out, buf);
}
#endif

#if defined(IRCD_ALLOCATOR_USE_JEMALLOC) && defined(HAVE_JEMALLOC_H)
void
ircd::allocator::scope::hook_init()
noexcept
{
}
#endif

#if defined(IRCD_ALLOCATOR_USE_JEMALLOC) && defined(HAVE_JEMALLOC_H)
void
ircd::allocator::scope::hook_fini()
noexcept
{
}
#endif
