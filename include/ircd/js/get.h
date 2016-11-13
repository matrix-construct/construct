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
