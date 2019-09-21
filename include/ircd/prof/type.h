// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_PROF_TYPE_H

namespace ircd::prof
{
	struct type;
	enum dpl :uint8_t;
}

/// Type descriptor for prof events. This structure is used to aggregate
/// information that describes a profiling event type, including whether
/// the kernel or the user is being profiled (dpl), the principal counter
/// type being profiled (counter) and any other contextual attributes.
struct ircd::prof::type
{
	enum dpl dpl {0};
	uint8_t type_id {0};
	uint8_t counter {0};
	uint8_t cacheop {0};
	uint8_t cacheres {0};

	type(const event &);
	type(const enum dpl & = (enum dpl)0,
	     const uint8_t &attr_type = 0,
	     const uint8_t &counter = 0,
	     const uint8_t &cacheop = 0,
	     const uint8_t &cacheres = 0);
};

/// Selector for whether the type applies for profiling the user or kernel.
enum ircd::prof::dpl
:std::underlying_type<ircd::prof::dpl>::type
{
	KERNEL  = 0,
	USER    = 1,
};
