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
#define HAVE_IRCD_JS_DEL_H

namespace ircd {
namespace js   {

// Delete the private data (see: priv.h).
void del(JSObject *const &obj, priv_t);

void del(const object::handle &obj, const id::handle &id);
void del(const object::handle &obj, const id &id);
void del(const object::handle &obj, const uint32_t &idx);
void del(const object::handle &src, const char *const path);

} // namespace js
} // namespace ircd
