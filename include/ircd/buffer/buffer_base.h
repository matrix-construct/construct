// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_BUFFER_BASE_H

/// Base for all buffer types
///
template<class it>
struct ircd::buffer::buffer
:std::pair<it, it>
{
	using iterator = it;
	using value_type = typename std::remove_pointer<iterator>::type;

	auto &begin() const                { return std::get<0>(*this);            }
	auto &begin()                      { return std::get<0>(*this);            }
	auto &end() const                  { return std::get<1>(*this);            }
	auto &end()                        { return std::get<1>(*this);            }

	bool null() const;
	bool empty() const;                // For boost::spirit conceptual compliance.

	auto &operator[](const size_t &i) const;
	auto &operator[](const size_t &i);

	explicit operator const it &() const;
	explicit operator std::string_view() const;
	explicit operator std::string() const;
	operator string_view() const;

	buffer(const it &start, const it &stop);
	buffer(const it &start, const size_t &size);
	buffer();
};

template<class it>
inline __attribute__((always_inline))
ircd::buffer::buffer<it>::buffer()
:buffer{nullptr, nullptr}
{}

template<class it>
inline __attribute__((always_inline))
ircd::buffer::buffer<it>::buffer(const it &start,
                                 const size_t &size)
:buffer{start, start + size}
{}

template<class it>
inline __attribute__((always_inline, flatten))
ircd::buffer::buffer<it>::buffer(const it &start,
                                 const it &stop)
:std::pair<it, it>{start, stop}
{}

template<class it>
inline __attribute__((always_inline, flatten))
ircd::buffer::buffer<it>::operator string_view()
const
{
	return { reinterpret_cast<const char *>(data(*this)), size(*this) };
}

template<class it>
ircd::buffer::buffer<it>::operator std::string()
const
{
	return { reinterpret_cast<const char *>(data(*this)), size(*this) };
}

template<class it>
ircd::buffer::buffer<it>::operator std::string_view()
const
{
	return { reinterpret_cast<const char *>(data(*this)), size(*this) };
}

template<class it>
auto &
ircd::buffer::buffer<it>::operator[](const size_t &i)
{
	assert(begin() + i < end());
	return *(begin() + i);
}

template<class it>
auto &
ircd::buffer::buffer<it>::operator[](const size_t &i)
const
{
	assert(begin() + i < end());
	return *(begin() + i);
}

template<class it>
bool
ircd::buffer::buffer<it>::empty()
const
{
	return null() || std::distance(begin(), end()) == 0;
}

template<class it>
bool
ircd::buffer::buffer<it>::null()
const
{
	return !begin();
}

template<class it>
ircd::buffer::buffer<it>::operator
const it &()
const
{
	return begin();
}
