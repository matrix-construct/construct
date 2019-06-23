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
#define HAVE_IRCD_UTIL_CUSTOM_PTR_H

namespace ircd {
inline namespace util
{
	template<class T> struct custom_ptr;
}}

template<class T>
struct ircd::util::custom_ptr
:std::unique_ptr<T, std::function<void (T *)>>
{
	operator T *() const;

	using std::unique_ptr<T, std::function<void (T *)>>::unique_ptr;
};

template<class T>
ircd::util::custom_ptr<T>::operator
T *()
const
{
	return this->get();
}
