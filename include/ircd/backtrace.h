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
#define HAVE_IRCD_BACKTRACE_H

namespace ircd
{
	struct backtrace;
}

struct ircd::backtrace
{
	void **array;
	size_t count;

  public:
	void *operator[](const size_t &) const;
	const size_t &size() const;

	backtrace(void **const &, const size_t &);
	backtrace(const mutable_buffer &);
	backtrace();
};

inline void *
ircd::backtrace::operator[](const size_t &i)
const
{
	return i < count? array[i] : nullptr;
}

inline const size_t &
ircd::backtrace::size()
const
{
	return count;
}
