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
#define HAVE_IRCD_JS_GET_H

namespace ircd {
namespace js   {

// Get reserved data slots
JS::Value get(JSObject *const &obj, const reserved &id);

// Get private data slot
template<class T> T &get(JSObject *const &, priv_t);

value get(const object::handle &obj, const id::handle &id);
value get(const object::handle &obj, const id &id);
value get(const object::handle &obj, const uint32_t &idx);
value get(const object::handle &src, const char *const path);

} // namespace js
} // namespace ircd

template<class T>
T &
ircd::js::get(JSObject *const &obj,
              priv_t)
{
	if(unlikely(~flags(obj) & JSCLASS_HAS_PRIVATE))
        throw error("get(priv): Object has no private slot");

	auto *const vp(JS_GetPrivate(obj));
	auto *const sp(reinterpret_cast<priv_ptr *>(vp));
	if(unlikely(!sp || !*sp))
		throw error("get(priv): Object has no private data set");

	return *reinterpret_cast<T *>(sp->get());
}
