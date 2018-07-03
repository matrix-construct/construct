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
#define HAVE_IRCD_JS_ERROR_H

namespace ircd {
namespace js   {

void replace_message(JSErrorReport &report, const char *fmt, ...) AFP(2, 3);

struct jserror
:js::error
{
	IRCD_OVERLOAD(pending)

	root<JS::Value> val;

	void generate_what_js(const JSErrorReport &report);
	void generate_what_our(const JSErrorReport &report);

	void create(JSErrorReport &);
	void create(const JSErrorReport &);
	void generate(const JSExnType &type, const char *const &fmt, const va_rtti &ap);
	void set_pending() const;
	void set_uncatchable() const;

	jserror(pending_t);
	jserror(generate_skip_t);
	jserror(const JSErrorReport &);
	template<class... args> jserror(const char *const &fmt = " ", args&&...);
	jserror(JSObject &);
	jserror(JSObject *const &);
	jserror(const JS::Value &);
};

#define IRCD_JS_ERROR_DEF(name, type)                 \
struct name                                           \
:jserror                                              \
{                                                     \
    name(const char *const fmt = " ", ...) AFP(2, 3)  \
    :jserror(generate_skip)                           \
    {                                                 \
        va_list ap;                                   \
        va_start(ap, fmt);                            \
        generate(type, fmt, ap);                      \
        va_end(ap);                                   \
    }                                                 \
};

IRCD_JS_ERROR_DEF( internal_error,   JSEXN_INTERNALERR )
IRCD_JS_ERROR_DEF( eval_error,       JSEXN_EVALERR )
IRCD_JS_ERROR_DEF( range_error,      JSEXN_RANGEERR )
IRCD_JS_ERROR_DEF( reference_error,  JSEXN_REFERENCEERR )
IRCD_JS_ERROR_DEF( syntax_error,     JSEXN_SYNTAXERR )
IRCD_JS_ERROR_DEF( type_error,       JSEXN_TYPEERR )
IRCD_JS_ERROR_DEF( uri_error,        JSEXN_URIERR )

} // namespace js
} // namespace ircd

template<class... args>
ircd::js::jserror::jserror(const char *const &fmt,
                           args&&... a)
:ircd::js::error{generate_skip}
,val{}
{
    generate(JSEXN_ERR, fmt, va_rtti{std::forward<args>(a)...});
}
