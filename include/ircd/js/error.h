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
#define HAVE_IRCD_JS_ERROR_H

namespace ircd {
namespace js   {

class error_handler
{
	friend class runtime;
	using closure = std::function<void (const char *const &msg, JSErrorReport &)>;

	error_handler *theirs;
	closure handler;

  public:
	error_handler(const closure &handler);
	error_handler(closure &&handler);
	~error_handler() noexcept;
};

struct jserror
:js::error
{
  protected:
	std::u16string msg;
	JSErrorReport report;

	void generate(const JSExnType &type, const char *const &fmt, va_list ap);

  public:
	JS::Value create_error(const JS::HandleObject &stack, const JS::HandleString &file, const std::pair<uint, uint> &linecol) const;
	JS::Value create_error() const;

	void set_pending() const;

	jserror(generate_skip_t);
	jserror(const char *fmt = " ", ...) AFP(2, 3);
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
