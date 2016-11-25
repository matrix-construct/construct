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
#define HAVE_IRCD_JS_FOR_EACH_H

namespace ircd {
namespace js   {

enum class iter
{
	none          = 0,
	enumerate     = JSITER_ENUMERATE,
	for_each      = JSITER_FOREACH,
	key_val       = JSITER_KEYVALUE,
	own_only      = JSITER_OWNONLY,
	hidden        = JSITER_HIDDEN,
	symbols       = JSITER_SYMBOLS,
	symbols_only  = JSITER_SYMBOLSONLY,
};

// Key iteration (as id type)
using each_id = std::function<void (const id &)>;
void for_each(object::handle, const iter &flags, const each_id &);
void for_each(object::handle, const each_id &);

// Key iteration (as value)
using each_key = std::function<void (const value &)>;
void for_each(object::handle, const each_key &);
void for_each(object::handle, const iter &flags, const each_key &);

// Key/Value iteration (as value => value)
using each_key_val = std::function<void (const value &, const value &)>;
void for_each(object::handle, const each_key_val &);
void for_each(object::handle, const iter &flags, const each_key_val &);

} // namespace js
} // namespace ircd
