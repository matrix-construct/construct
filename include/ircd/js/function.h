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
bool is_ctor(const JSFunction &f);

namespace basic {

template<lifetime L>
struct function
:root<JSFunction *, L>
{
	operator JSObject *() const;
	explicit operator script<L>() const;
	explicit operator string<L>() const;

	// js::value/js::object == lifetime::stack
	js::value operator()(const js::object &, const vector<js::value>::handle &args) const;
	template<class... args> js::value operator()(const js::object &, args&&...) const;

	// new function
	template<class string_t>
	function(JS::AutoObjectVector &stack,
	         const JS::CompileOptions &opts,
	         const char *const &name,
	         const std::vector<string_t> &args,
	         const string_t &src);

	using root<JSFunction *, L>::root;
	explicit function(const value<L> &);
	function(JSFunction *const &);
	function(JSFunction &);
};

} // namespace basic

using function = basic::function<lifetime::stack>;
using heap_function = basic::function<lifetime::heap>;

//
// Implementation
//
namespace basic {

template<lifetime L>
function<L>::function(JSFunction &func)
:function<L>::root::type{&func}
{
}

template<lifetime L>
function<L>::function(JSFunction *const &func)
:function<L>::root::type{func}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL function");
}

template<lifetime L>
function<L>::function(const value<L> &val)
:function<L>::root::type
{
	JS_ValueToFunction(*cx, val)
}
{
	if(!this->get())
		throw type_error("value is not a function");
}

template<lifetime L>
template<class string_t>
function<L>::function(JS::AutoObjectVector &stack,
                      const JS::CompileOptions &opts,
                      const char *const &name,
                      const std::vector<string_t> &args,
                      const string_t &src)
:function<L>::root::type{}
{
	std::vector<const typename string_t::value_type *> argp(args.size());
	std::transform(begin(args), end(args), begin(argp), []
	(const string_t &arg)
	{
		return arg.data();
	});

	if(!JS::CompileFunction(*cx,
	                        stack,
	                        opts,
	                        name,
	                        argp.size(),
	                        &argp.front(),
	                        src.data(),
	                        src.size(),
	                        &(*this)))
	{
		throw syntax_error("Failed to compile function");
	}
}

template<lifetime L>
template<class... args>
js::value
function<L>::operator()(const js::object &that,
                        args&&... a)
const
{
	vector<basic::value<lifetime::stack>> argv
	{{
		std::forward<args>(a)...
	}};

	return call(*this, that, decltype(argv)::handle());
}

template<lifetime L>
js::value
function<L>::operator()(const js::object &that,
                        const vector<js::value>::handle &args)
const
{
	return call(*this, that, args);
}

template<lifetime L>
function<L>::operator string<L>()
const
{
	return decompile(*this, true);
}

template<lifetime L>
function<L>::operator script<L>()
const
{
	return JS_GetFunctionScript(*cx, *this);
}

template<lifetime L>
function<L>::operator JSObject *()
const
{
	const auto ret(JS_GetFunctionObject(this->get()));
	if(unlikely(!ret))
		throw type_error("function cannot cast to Object");

	return ret;
}

} // namespace basic

inline bool
is_ctor(const JSFunction &f)
{
	return JS_IsConstructor(const_cast<JSFunction *>(&f));
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
