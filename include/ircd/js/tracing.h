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
#define HAVE_IRCD_JS_TRACING_H

namespace ircd {
namespace js   {

struct tracing
{
	struct thing
	{
		void *ptr;
		jstype type;

		template<class T> operator const JS::Heap<T> &() const;
		template<class T> operator JS::Heap<T> &();
	};

	using list = std::list<thing>;

	list heap;

	void operator()(JSTracer *const &);

	tracing();
	~tracing() noexcept;
};

template<class T>
tracing::thing::operator JS::Heap<T> &()
{
	return *reinterpret_cast<JS::Heap<T> *>(ptr);
}

template<class T>
tracing::thing::operator const JS::Heap<T> &()
const
{
	return *reinterpret_cast<const JS::Heap<T> *>(ptr);
}

} // namespace js
} // namespace ircd
