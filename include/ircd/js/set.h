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
#define HAVE_IRCD_JS_SET_H

namespace ircd {
namespace js   {

// Set the reserved data slots
void set(JSObject *const &obj, const reserved &slot, const JS::Value &);

// Set the private data (see: priv.h) Proper delete of any existing on overwrite.
void set(JSObject *const &obj, priv_data &);
void set(JSObject *const &obj, const std::shared_ptr<priv_data> &);

void set(const object::handle &obj, const id::handle &id, const value::handle &val);
void set(const object::handle &obj, const id::handle &id, const value &val);
void set(const object::handle &obj, const id &id, const value &val);
template<class T> void set(const object::handle &obj, const uint32_t &idx, T&& t);
void set(const object::handle &src, const char *const path, const value &val);

template<class T>
void
set(const object::handle &obj,
    const uint32_t &idx,
    T&& t)
{
	if(!JS_SetElement(*cx, obj, idx, std::forward<T>(t)))
		throw jserror(jserror::pending);
}

} // namespace js
} // namespace ircd
