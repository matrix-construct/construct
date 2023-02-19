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
#define HAVE_IRCD_JSON_STACK_CHASE_H

/// This device chases the current active path by updating its member pointers.
struct ircd::json::stack::chase
{
	array *a {nullptr};
	object *o {nullptr};
	member *m {nullptr};

	bool next() noexcept;
	bool prev() noexcept;

	chase(stack &s, const bool prechase = false) noexcept;
	chase() = default;
};

/// This device chases the current active path by updating its member pointers.
struct ircd::json::stack::const_chase
{
	const array *a {nullptr};
	const object *o {nullptr};
	const member *m {nullptr};

	bool next() noexcept;
	bool prev() noexcept;

	const_chase(const stack &s, const bool prechase = false) noexcept;
	const_chase() = default;
};
