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
#define HAVE_IRCD_BUFFER_PARSE_BUFFER_H

/// The parse_buffer is the const version of a window_buffer. It is basically
/// the same idea with the underlying data is being read rather than written.
///
struct ircd::buffer::parse_buffer
:const_buffer
{
	using closure = std::function<size_t (const const_buffer &)>;
	using closure_cbuf = std::function<const_buffer (const const_buffer &)>;
	using closure_spirit = std::function<bool (const char *&, const char *const &)>;

	const_buffer base;

	size_t remaining() const;
	size_t consumed() const;

	const_buffer completed() const;
	explicit operator const_buffer() const;

	const_buffer operator()(const closure &);
	const_buffer operator()(const closure_cbuf &);
	const_buffer operator()(const closure_spirit &);
	const_buffer rewind(const size_t &n = 1);

	parse_buffer(const const_buffer &base);
	parse_buffer(const window_buffer &base);
	parse_buffer() = default;
};

inline
ircd::buffer::parse_buffer::parse_buffer(const const_buffer &buf)
:const_buffer{buf}
,base{buf}
{}

inline ircd::buffer::const_buffer
ircd::buffer::parse_buffer::rewind(const size_t &n)
{
	const size_t nmax
	{
		std::min(n, consumed())
	};

	static_cast<const_buffer &>(*this).begin() -= nmax;
	assert(base.begin() <= begin());
	assert(begin() <= base.end());
	return completed();
}

inline ircd::buffer::const_buffer
ircd::buffer::parse_buffer::operator()(const closure_spirit &closure)
{
	return operator()([&closure]
	(const const_buffer &buf) -> size_t
	{
		const char *start(data(buf));
		const char *const stop(start + size(buf));
		if(!closure(start, stop))
			return 0;

		assert(start <= stop);
		assert(start >= data(buf));
		return start - data(buf);
	});
}

inline ircd::buffer::const_buffer
ircd::buffer::parse_buffer::operator()(const closure_cbuf &closure)
{
	return operator()([&closure]
	(const const_buffer &buf)
	{
		return size(closure(buf));
	});
}

inline ircd::buffer::const_buffer
ircd::buffer::parse_buffer::operator()(const closure &closure)
{
	consume(*this, closure(*this));
	return completed();
}

inline ircd::buffer::parse_buffer::operator
const_buffer()
const
{
	return completed();
}

inline ircd::buffer::const_buffer
ircd::buffer::parse_buffer::completed()
const
{
	assert(base.begin() <= begin());
	assert(base.begin() + consumed() <= base.end());
	return
	{
		base.begin(), base.begin() + consumed()
	};
}

inline size_t
ircd::buffer::parse_buffer::consumed()
const
{
	assert(begin() >= base.begin());
	assert(begin() <= base.end());
	return std::distance(base.begin(), begin());
}

inline size_t
ircd::buffer::parse_buffer::remaining()
const
{
	assert(begin() <= base.end());
	const size_t ret(std::distance(begin(), base.end()));
	assert(ret == size(*this));
	return ret;
}
