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
#define HAVE_IRCD_UTIL_ITERATOR_H

namespace ircd {
inline namespace util {

//
// To collapse pairs of iterators down to a single type
//

template<class T>
struct iterpair
:std::pair<T, T>
{
	using std::pair<T, T>::pair;
};

template<class T>
T &
begin(iterpair<T> &i)
{
	return std::get<0>(i);
}

template<class T>
T &
end(iterpair<T> &i)
{
	return std::get<1>(i);
}

template<class T>
const T &
begin(const iterpair<T> &i)
{
	return std::get<0>(i);
}

template<class T>
const T &
end(const iterpair<T> &i)
{
	return std::get<1>(i);
}

//
// To collapse pairs of iterators down to a single type
// typed by an object with iterator typedefs.
//

template<class T>
using iterators = std::pair<typename T::iterator, typename T::iterator>;

template<class T>
using const_iterators = std::pair<typename T::const_iterator, typename T::const_iterator>;

template<class T>
typename T::iterator
begin(const iterators<T> &i)
{
	return i.first;
}

template<class T>
typename T::iterator
end(const iterators<T> &i)
{
	return i.second;
}

template<class T>
typename T::const_iterator
begin(const const_iterators<T> &ci)
{
	return ci.first;
}

template<class T>
typename T::const_iterator
end(const const_iterators<T> &ci)
{
	return ci.second;
}

} // namespace util
} // namespace ircd
