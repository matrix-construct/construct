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
#define HAVE_IRCD_JS_FUNCTION_H

namespace ircd {
namespace js   {

string decompile(const JS::Handle<JSFunction *> &, const bool &pretty = false);
string display_name(const JSFunction &);
string name(const JSFunction &);
uint16_t arity(const JSFunction &f);

struct function
:JS::Rooted<JSFunction *>
{
	using handle = JS::HandleFunction;
	using handle_mutable = JS::MutableHandleFunction;

	operator JSObject *() const;
	explicit operator script() const;
	explicit operator string() const;

	value operator()(const object &, const JS::HandleValueArray &args) const;
	value operator()(const object &) const;

	// new function
	function(JS::AutoObjectVector &stack,
	         const JS::CompileOptions &opts,
	         const char *const &name,
	         const std::vector<std::string> &args,
	         const std::string &src);

	explicit function(const value &);
	function(JSFunction *const &);
	function(JSFunction &);
	function();
	function(function &&) noexcept;
	function(const function &) = delete;
	function &operator=(function &&) noexcept;
};

inline
function::function()
:JS::Rooted<JSFunction *>{*cx}
{
}

inline
function::function(function &&other)
noexcept
:JS::Rooted<JSFunction *>{*cx, other}
{
	other.set(nullptr);
}

inline function &
function::operator=(function &&other)
noexcept
{
	set(other.get());
	other.set(nullptr);
	return *this;
}

inline
function::function(JSFunction &func)
:JS::Rooted<JSFunction *>{*cx, &func}
{
}

inline
function::function(JSFunction *const &func)
:JS::Rooted<JSFunction *>{*cx, func}
{
	if(unlikely(!get()))
		throw internal_error("NULL function");
}

inline
function::function(const value &val)
:JS::Rooted<JSFunction *>
{
	*cx,
	JS_ValueToFunction(*cx, val)
}
{
	if(!get())
		throw type_error("value is not a function");
}

inline
function::operator string()
const
{
	return decompile(*this, true);
}

inline
function::operator script()
const
{
	return JS_GetFunctionScript(*cx, *this);
}

inline
function::operator JSObject *()
const
{
	const auto ret(JS_GetFunctionObject(get()));
	if(unlikely(!ret))
		throw type_error("function cannot cast to Object");

	return ret;
}

inline uint16_t
arity(const JSFunction &f)
{
	return JS_GetFunctionArity(const_cast<JSFunction *>(&f));
}

inline string
name(const JSFunction &f)
{
	const auto ret(JS_GetFunctionId(const_cast<JSFunction *>(&f)));
	return ret? string(ret) : string("<unnamed>");
}

inline string
display_name(const JSFunction &f)
{
	const auto ret(JS_GetFunctionDisplayId(const_cast<JSFunction *>(&f)));
	return ret? string(ret) : string("<anonymous>");
}

inline string
decompile(const JS::Handle<JSFunction *> &f,
          const bool &pretty)
{
	uint flags(0);
	flags |= pretty? 0 : JS_DONT_PRETTY_PRINT;
	return JS_DecompileFunction(*cx, f, flags);
}

} // namespace js
} // namespace ircd
