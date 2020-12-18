// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_IOS_DESCRIPTOR_H

namespace ircd::ios
{
	struct handler;
	struct descriptor;

	const string_view &name(const descriptor &);
}

struct ircd::ios::descriptor
:instance_list<descriptor>
{
	struct stats;

	static uint64_t ids;

	static void *default_allocator(handler &, const size_t &);
	static void default_deallocator(handler &, void *const &, const size_t &) noexcept;

	string_view name;
	uint64_t id {++ids};
	std::unique_ptr<struct stats> stats;
	void *(*allocator)(handler &, const size_t &);
	void (*deallocator)(handler &, void *const &, const size_t &);
	std::vector<std::array<uint64_t, 2>> history; // epoch, cycles
	uint8_t history_pos {0};
	bool continuation {false};

	descriptor(const string_view &name,
	           const decltype(allocator) & = default_allocator,
	           const decltype(deallocator) & = default_deallocator,
	           const bool &continuation = false) noexcept;

	descriptor(descriptor &&) = delete;
	descriptor(const descriptor &) = delete;
	~descriptor() noexcept;
};

struct ircd::ios::descriptor::stats
{
	using value_type = uint64_t;
	using item = ircd::stats::item<value_type *>;

	value_type value[11];
	size_t items;

  public:
	item queued;
	item calls;
	item faults;
	item allocs;
	item alloc_bytes;
	item frees;
	item free_bytes;
	item slice_total;
	item slice_last;
	item latency_total;
	item latency_last;

	stats(descriptor &);
	stats() = delete;
	stats(const stats &) = delete;
	stats &operator=(const stats &) = delete;
	~stats() noexcept;
};

[[gnu::hot]]
inline void
ircd::ios::descriptor::default_deallocator(handler &handler,
                                           void *const &ptr,
                                           const size_t &size)
noexcept
{
	#ifdef __clang__
		::operator delete(ptr);
	#else
		::operator delete(ptr, size);
	#endif
}

[[gnu::hot]]
inline void *
ircd::ios::descriptor::default_allocator(handler &handler,
                                         const size_t &size)
{
	return ::operator new(size);
}

inline const ircd::string_view &
ircd::ios::name(const descriptor &descriptor)
{
	return descriptor.name;
}
