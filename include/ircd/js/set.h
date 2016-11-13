/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#define HAVE_IRCD_JS_SET_H

namespace ircd {
namespace js   {

// Set the reserved data slots
void set(JSObject *const &obj, const reserved &slot, const JS::Value &);

// Set the private data (see: priv.h) Proper delete of any existing on overwrite.
void set(JSObject *const &obj, priv_data &);
void set(JSObject *const &obj, const std::shared_ptr<priv_data> &);

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
