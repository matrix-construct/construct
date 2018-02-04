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
#define HAVE_IRCD_JS_SCRIPT_H

namespace ircd  {
namespace js    {

ctx::future<void *> compile_async(const JS::ReadOnlyCompileOptions &, const std::u16string &, const bool &module = false);
string decompile(const JS::Handle<JSScript *> &, const char *const &name, const bool &pretty = false);
size_t bytecodes(const JS::Handle<JSScript *> &, uint8_t *const &buf, const size_t &max);
bool compilable(const char *const &str, const size_t &len, const object &stack = {});
bool compilable(const std::string &str, const object &stack = {});

struct script
:root<JSScript *>
{
	IRCD_OVERLOAD(yielding)

	value operator()(JS::AutoObjectVector &environment) const;
	value operator()(const object &environment) const;
	value operator()() const;

	using root<JSScript *>::root;
	script(yielding_t, const JS::ReadOnlyCompileOptions &opts, const std::u16string &src);
	script(const JS::ReadOnlyCompileOptions &opts, const std::u16string &src);
	script(const JS::ReadOnlyCompileOptions &opts, const std::string &src);
	script(const uint8_t *const &bytecode, const size_t &size);
	script(JSScript *const &);
	script(JSScript &);
};

inline
script::script(JSScript &val)
:script::root::type{&val}
{
}

inline
script::script(JSScript *const &val)
:script::root::type{val}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL script");
}

inline
script::script(const uint8_t *const &bytecode,
               const size_t &size)
:script::root::type
{
	//JS_DecodeScript(*cx, bytecode, size)
}
{
	if(unlikely(!this->get()))
		throw jserror(jserror::pending);
}

inline
script::script(const JS::ReadOnlyCompileOptions &opts,
               const std::string &src)
:script::root::type{}
{
	if(!JS::Compile(*cx, opts, src.data(), src.size(), &(*this)))
		throw jserror(jserror::pending);
}

inline
script::script(const JS::ReadOnlyCompileOptions &opts,
               const std::u16string &src)
:script::root::type{}
{
	if(!JS::Compile(*cx, opts, src.data(), src.size(), &(*this)))
		throw jserror(jserror::pending);
}

// This constructor compiles the script concurrently by yielding this ircd::ctx.
// The compilation occurs on another thread entirely, so other ircd contexts will
// still be able to run, but this ircd context will block until the script is
// compiled at which point this constructor will complete.
inline
script::script(yielding_t,
               const JS::ReadOnlyCompileOptions &opts,
               const std::u16string &src)
:script::root::type{[this, &opts, &src]
{
	auto future(compile_async(opts, src));
	void *const token(future.get());
	return token? JS::FinishOffThreadScript(*cx, token):
	              script(opts, src).get();
}()}
{
	if(unlikely(!this->get()))
		throw jserror(jserror::pending);
}

inline value
script::operator()()
const
{
	value ret;
	if(!JS_ExecuteScript(*cx, *this, &ret))
		throw jserror(jserror::pending);

	return ret;
}

inline value
script::operator()(const object &environment)
const
{
	JS::AutoObjectVector env(*cx);
	env.append(object());
	return operator()(env);
}

inline value
script::operator()(JS::AutoObjectVector &environment)
const
{
	value ret;
	if(!JS_ExecuteScript(*cx, environment, *this, &ret))
		throw jserror(jserror::pending);

	return ret;
}

} // namespace js
} // namespace ircd
