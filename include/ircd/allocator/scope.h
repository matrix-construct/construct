// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_ALLOCATOR_SCOPE_H

namespace ircd::allocator
{
	struct scope;
}

struct ircd::allocator::scope
{
	using alloc_closure = std::function<void *(const size_t &)>;
	using realloc_closure = std::function<void *(void *const &ptr, const size_t &)>;
	using free_closure = std::function<void (void *const &ptr)>;

	static void hook_init() noexcept;
	static void hook_fini() noexcept;

	static scope *current;
	scope *theirs;
	alloc_closure user_alloc;
	realloc_closure user_realloc;
	free_closure user_free;

  public:
	scope(alloc_closure = {}, realloc_closure = {}, free_closure = {});
	scope(const scope &) = delete;
	scope(scope &&) = delete;
	~scope() noexcept;
};
