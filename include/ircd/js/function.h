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
#define HAVE_IRCD_JS_FUNCTION_H

namespace ircd {
namespace js   {

string decompile(const JS::Handle<JSFunction *> &, const bool &pretty = false);
string display_name(const JSFunction *const &);
string name(const JSFunction *const *);
uint16_t arity(const JSFunction *const &);
bool is_ctor(const JSFunction *const &);
object enclosing_scope(JSFunction *const &);

struct function
:root<JSFunction *>
{
	struct native;      // Use this instead to specify a function in C
	struct literal;     // Use this instead to supply the JS as a C string literal.

	IRCD_OVERLOAD(outermost_enclosing)

	operator JSObject *() const;
	explicit operator script() const;
	explicit operator string() const;

	value operator()(const object::handle &, const vector<value>::handle &) const;
	template<class... args> value operator()(const object::handle &, args&&...) const;

	// new function
	template<class string_t>
	function(JS::AutoObjectVector &stack,
	         const JS::CompileOptions &opts,
	         const char *const &name,
	         const std::vector<string_t> &args,
	         const string_t &src);

	using root<JSFunction *>::root;
	function(outermost_enclosing_t);             // GetOutermostEnclosingFunctionOfScriptedCaller
	function(const value::handle &);
	function(const value &);
	function(JSFunction *const &);
	function(JSFunction &);
};

inline
function::function(JSFunction &func)
:function::root::type{&func}
{
}

inline
function::function(JSFunction *const &func)
:function::root::type{func}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL function");
}

inline
function::function(const value &val)
:function{static_cast<value::handle>(val)}
{
}

inline
function::function(const value::handle &val)
:function::root::type
{
	JS_ValueToFunction(*cx, val)
}
{
	if(!this->get())
		throw type_error("value is not a function");
}

inline
function::function(outermost_enclosing_t)
:function::root::type
{
	::js::GetOutermostEnclosingFunctionOfScriptedCaller(*cx)
}
{
	if(!this->get())
		throw internal_error("Caller has no enclosing function");
}

template<class string_t>
function::function(JS::AutoObjectVector &stack,
                   const JS::CompileOptions &opts,
                   const char *const &name,
                   const std::vector<string_t> &args,
                   const string_t &src)
:function::root::type{}
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

template<class... args>
value
function::operator()(const object::handle &that,
                     args&&... a)
const
{
	const vector<value> argv
	{
		std::forward<args>(a)...
	};

	const vector<value>::handle handle(argv);
	return operator()(that, handle);
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
	const auto ret(JS_GetFunctionObject(this->get()));
	if(unlikely(!ret))
		throw type_error("function cannot cast to Object");

	return ret;
}

inline object
enclosing_scope(JSFunction *const &f)
{
	return ::js::GetNearestEnclosingWithEnvironmentObjectForFunction(f);
}

inline bool
is_ctor(const JSFunction *const &f)
{
	return JS_IsConstructor(const_cast<JSFunction *>(f));
}

inline uint16_t
arity(const JSFunction *const &f)
{
	return JS_GetFunctionArity(const_cast<JSFunction *>(f));
}

inline string
name(const JSFunction *const &f)
{
	const auto ret(JS_GetFunctionId(const_cast<JSFunction *>(f)));
	return ret? string(ret) : string("<unnamed>");
}

inline string
display_name(const JSFunction *const &f)
{
	const auto ret(JS_GetFunctionDisplayId(const_cast<JSFunction *>(f)));
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
