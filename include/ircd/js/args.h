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
#define HAVE_IRCD_JS_ARGS_H

namespace ircd {
namespace js   {

struct args
:JS::CallArgs
{
	bool empty() const                           { return length() == 0;                           }
	size_t size() const                          { return length();                                }
	bool has(const size_t &at) const             { return size() > at;                             }

	value at(const size_t &at) const;
	value operator[](const size_t &at) const;

	args(const unsigned &argc, JS::Value *const &argv);
};

inline
args::args(const unsigned &argc,
           JS::Value *const &argv)
:JS::CallArgs{JS::CallArgsFromVp(argc, argv)}
{
}

inline value
args::operator[](const size_t &at)
const
{
	return length() > at? JS::CallArgs::operator[](at) : value{};
}

inline value
args::at(const size_t &at)
const
{
	if(unlikely(length() <= at))
		throw range_error("Missing required argument #%zu", at);

	return JS::CallArgs::operator[](at);
}

} // namespace js
} // namespace ircd
