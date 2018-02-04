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
#define HAVE_IRCD_JS_HAS_H

namespace ircd {
namespace js   {

// Test the reserve slots of the object
bool has(const JSObject *const &obj, const reserved &id);

// Test the private slot of an object
bool has(const JSObject *const &obj, priv_t);

bool has(const object::handle &obj, const id::handle &id);
bool has(const object::handle &obj, const id &id);
bool has(const object::handle &obj, const uint32_t &idx);
bool has(const object::handle &src, const char *const path);

} // namespace js
} // namespace ircd
