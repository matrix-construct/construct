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
:std::tuple<it, it>
{
	using iterator = it;
	using value_type = typename std::remove_pointer<iterator>::type;

	operator string_view() const;
	explicit operator std::string_view() const;
	explicit operator std::string() const;

	auto &begin() const                { return std::get<0>(*this);            }
	auto &begin()                      { return std::get<0>(*this);            }
	auto &end() const                  { return std::get<1>(*this);            }
	auto &end()                        { return std::get<1>(*this);            }

	auto &operator[](const size_t &i) const;
	auto &operator[](const size_t &i);

	// For boost::spirit conceptual compliance.
	auto empty() const                 { return ircd::buffer::empty(*this);    }

	buffer(const it &start, const it &stop);
	buffer(const it &start, const size_t &size);
	buffer();
};

template<class it>
ircd::buffer::buffer<it>::buffer()
:buffer{nullptr, nullptr}
{}

template<class it>
ircd::buffer::buffer<it>::buffer(const it &start,
                                 const size_t &size)
:buffer{start, start + size}
{}

template<class it>
ircd::buffer::buffer<it>::buffer(const it &start,
                                 const it &stop)
:std::tuple<it, it>{start, stop}
{}

template<class it>
auto &
ircd::buffer::buffer<it>::operator[](const size_t &i)
const
{
	return *(begin() + i);
}

template<class it>
auto &
ircd::buffer::buffer<it>::operator[](const size_t &i)
{
	return *(begin() + i);
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
ircd::buffer::buffer<it>::operator string_view()
const
{
	return { reinterpret_cast<const char *>(data(*this)), size(*this) };
}
