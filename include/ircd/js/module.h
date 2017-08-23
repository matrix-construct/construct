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
#define HAVE_IRCD_JS_MODULE_H

namespace ircd  {
namespace js    {

struct module
:object
,script
{
	IRCD_OVERLOAD(yielding)

	struct trap *trap;

	object requested() const;           // GetRequestedModules()
	void instantiate();                 // ModuleDeclarationInstantiation()
	void operator()() const;            // ModuleEvaluation()

	module(yielding_t, const JS::ReadOnlyCompileOptions &, const std::u16string &src, struct trap *const & = nullptr, const bool &instantiate = true);
	module(const JS::ReadOnlyCompileOptions &, const std::u16string &src, struct trap *const & = nullptr, const bool &instantiate = true);

	static module &our(const object &module);    // Get this structure from the record object
};

inline
module::module(const JS::ReadOnlyCompileOptions &opts,
               const std::u16string &src,
               struct trap *const &trap,
               const bool &instantiate)
:object{[this, &opts, &src, &instantiate]
() -> object
{
	static const auto ownership(JS::SourceBufferHolder::NoOwnership);

	object ret(object::uninitialized);
	JS::SourceBufferHolder buf(src.data(), src.size(), ownership);
	if(!JS::CompileModule(*cx, opts, buf, &ret))
		throw jserror(jserror::pending);

	if(unlikely(!ret.get()))
		throw jserror(jserror::pending);

	JS::SetModuleHostDefinedField(ret, value(value::pointer, this));
	return ret;
}()}
,script
{
	JS::GetModuleScript(*cx, static_cast<object &>(*this))
}
,trap{trap}
{
	if(instantiate)
		this->instantiate();
}

// This constructor compiles the module concurrently by yielding this ircd::ctx.
// See the yielding_t overload in script.h for details.
inline
module::module(yielding_t,
               const JS::ReadOnlyCompileOptions &opts,
               const std::u16string &src,
               struct trap *const &trap,
               const bool &instantiate)
:object{[this, &opts, &src]
() -> object
{
	static const auto ownership(JS::SourceBufferHolder::NoOwnership);

	auto future(compile_async(opts, src, true));
	void *const token(future.get());
	object ret(object::uninitialized);
	if(!token)
	{
		JS::SourceBufferHolder buf(src.data(), src.size(), ownership);
		if(!JS::CompileModule(*cx, opts, buf, &ret))
			throw jserror(jserror::pending);
	}
	else ret = JS::FinishOffThreadModule(*cx, token);

	if(unlikely(!ret.get()))
		throw jserror(jserror::pending);

	JS::SetModuleHostDefinedField(ret, pointer_value(this));
	return ret;
}()}
,script
{
	JS::GetModuleScript(*cx, static_cast<object &>(*this))
}
,trap{trap}
{
	if(instantiate)
		this->instantiate();
}

inline void
module::instantiate()
{
	if(!JS::ModuleDeclarationInstantiation(*cx, static_cast<object &>(*this)))
		throw jserror(jserror::pending);
}

inline void
module::operator()()
const
{
	if(!JS::ModuleEvaluation(*cx, static_cast<const object &>(*this)))
		throw jserror(jserror::pending);
}

inline object
module::requested()
const
{
	return JS::GetRequestedModules(*cx, static_cast<const object &>(*this));
}

inline module &
module::our(const object &module)
{
	const value priv(JS::GetModuleHostDefinedField(module));
	return *pointer_value<struct module>(priv);
}

} // namespace js
} // namespace ircd
