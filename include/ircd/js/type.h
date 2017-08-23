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
#define HAVE_IRCD_JS_TYPE_H

namespace ircd {
namespace js   {

enum class jstype
{
	VALUE,
	OBJECT,
	FUNCTION,
	SCRIPT,
	STRING,
	SYMBOL,
	ID,
};

template<class T> constexpr jstype type()             { assert(0); return jstype::VALUE;           }
template<> constexpr jstype type<JS::Value>()         { return jstype::VALUE;                      }
template<> constexpr jstype type<JSObject *>()        { return jstype::OBJECT;                     }
template<> constexpr jstype type<JSFunction *>()      { return jstype::FUNCTION;                   }
template<> constexpr jstype type<JSScript *>()        { return jstype::SCRIPT;                     }
template<> constexpr jstype type<JSString *>()        { return jstype::STRING;                     }
template<> constexpr jstype type<JS::Symbol *>()      { return jstype::SYMBOL;                     }
template<> constexpr jstype type<jsid>()              { return jstype::ID;                         }

constexpr jstype
type(const JSType &t)
{
	switch(t)
	{
		case JSTYPE_VOID:         return jstype::VALUE;
		case JSTYPE_OBJECT:       return jstype::OBJECT;
		case JSTYPE_FUNCTION:     return jstype::FUNCTION;
		case JSTYPE_STRING:       return jstype::STRING;
		case JSTYPE_SYMBOL:       return jstype::SYMBOL;
		case JSTYPE_NUMBER:       return jstype::VALUE;
		case JSTYPE_BOOLEAN:      return jstype::VALUE;
		case JSTYPE_NULL:         return jstype::VALUE;
		case JSTYPE_LIMIT:        return jstype::VALUE;
	}

	return jstype::VALUE;
}

} // namespace js
} // namespace ircd
