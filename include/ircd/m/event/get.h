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
#define HAVE_IRCD_M_EVENT_GET_H

namespace ircd::m
{
	// Fetch the value for a single property of an event into the closure.
	bool get(std::nothrow_t, const event::idx &, const string_view &key, const event::fetch::view_closure &);
	bool get(std::nothrow_t, const event::id &, const string_view &key, const event::fetch::view_closure &);
	void get(const event::idx &, const string_view &key, const event::fetch::view_closure &);
	void get(const event::id &, const string_view &key, const event::fetch::view_closure &);

	// Copies value into buffer returning view.
	const_buffer get(std::nothrow_t, const event::idx &, const string_view &key, const mutable_buffer &out);
	const_buffer get(std::nothrow_t, const event::id &, const string_view &key, const mutable_buffer &out);
	const_buffer get(const event::idx &, const string_view &key, const mutable_buffer &out);
	const_buffer get(const event::id &, const string_view &key, const mutable_buffer &out);

	// Allocates and copies into string.
	std::string get(std::nothrow_t, const event::idx &, const string_view &key);
	std::string get(std::nothrow_t, const event::id &, const string_view &key);
	std::string get(const event::idx &, const string_view &key);
	std::string get(const event::id &, const string_view &key);

	template<class T>
	typename std::enable_if<std::is_integral<T>::value, bool>::type
	get(const event::idx &, const string_view &key, T &ret);

	template<class T>
	typename std::enable_if<std::is_integral<T>::value, T>::type
	get(const event::idx &, const string_view &key);
}

template<class T>
typename std::enable_if<std::is_integral<T>::value, T>::type
ircd::m::get(const event::idx &event_idx,
             const string_view &key)
{
	T ret;
	const mutable_buffer buf
	{
		reinterpret_cast<char *>(&ret), sizeof(ret)
	};

	get(event_idx, key, buf);
	return ret;
}

template<class T>
typename std::enable_if<std::is_integral<T>::value, bool>::type
ircd::m::get(const event::idx &event_idx,
             const string_view &key,
             T &ret)
{
	const mutable_buffer buf
	{
		reinterpret_cast<char *>(&ret), sizeof(ret)
	};

	const auto rbuf
	{
		get(std::nothrow, event_idx, key, buf)
	};

	assert(size(rbuf) >= sizeof(T));
	return !empty(rbuf);
}
