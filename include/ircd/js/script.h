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
namespace basic {

template<lifetime L>
struct script
:root<JSScript *, L>
{
	value<lifetime::stack> operator()(JS::AutoObjectVector &stack) const;
	value<lifetime::stack> operator()() const;

	using root<JSScript *, L>::root;
	script(const JS::CompileOptions &opts, const std::string &src);    // new script
	script(JSScript *const &);
	script(JSScript &);
};

} // namespace basic

using script = basic::script<lifetime::stack>;
using heap_script = basic::script<lifetime::heap>;

//
// Implementation
//
namespace basic {

template<lifetime L>
script<L>::script(JSScript &val)
:script<L>::root::type{&val}
{
}

template<lifetime L>
script<L>::script(JSScript *const &val)
:script<L>::root::type{val}
{
	if(unlikely(!this->get()))
		throw internal_error("NULL script");
}

template<lifetime L>
script<L>::script(const JS::CompileOptions &opts,
                  const std::string &src)
:script<L>::root::type{}
{
	if(!JS::Compile(*cx, opts, src.data(), src.size(), &(*this)))
		throw syntax_error("Failed to compile script");
}

template<lifetime L>
value<lifetime::stack>
script<L>::operator()()
const
{
	value<lifetime::stack> ret;
	if(!JS_ExecuteScript(*cx, *this, &ret))
		throw jserror(jserror::pending);

	return ret;
}

template<lifetime L>
value<lifetime::stack>
script<L>::operator()(JS::AutoObjectVector &stack)
const
{
	value<lifetime::stack> ret;
	if(!JS_ExecuteScript(*cx, stack, *this, &ret))
		throw jserror(jserror::pending);

	return ret;
}

} // namespace basic
} // namespace js
} // namespace ircd
