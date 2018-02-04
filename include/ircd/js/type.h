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
