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
#define HAVE_IRCD_JS_SCRIPT_H

namespace ircd  {
namespace js    {

ctx::future<void *> compile_async(const JS::ReadOnlyCompileOptions &, const std::u16string &);
string decompile(const JS::Handle<JSScript *> &, const char *const &name, const bool &pretty = false);
size_t bytecodes(const JS::Handle<JSScript *> &, uint8_t *const &buf, const size_t &max);
bool compilable(const char *const &str, const size_t &len, const object &stack = {});
bool compilable(const std::string &str, const object &stack = {});

struct script
:root<JSScript *>
{
	IRCD_OVERLOAD(yielding)

	value operator()(JS::AutoObjectVector &stack) const;
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
	JS_DecodeScript(*cx, bytecode, size)
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

inline
script::script(yielding_t,
               const JS::ReadOnlyCompileOptions &opts,
               const std::u16string &src)
:script::root::type{[&opts, &src]
{
	// This constructor compiles the script concurrently by yielding this ircd::ctx.
	// The compilation occurs on another thread entirely, so other ircd contexts will
	// still be able to run.
	auto future(compile_async(opts, src));
	void *const token(future.get());
	return token? JS::FinishOffThreadScript(*cx, *rt, token):
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
script::operator()(JS::AutoObjectVector &stack)
const
{
	value ret;
	if(!JS_ExecuteScript(*cx, stack, *this, &ret))
		throw jserror(jserror::pending);

	return ret;
}

} // namespace js
} // namespace ircd
