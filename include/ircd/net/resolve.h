/*
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
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
#define HAVE_IRCD_NET_RESOLVE_H

namespace ircd::net
{
	struct resolve;
}

/// DNS resolution suite.
///
/// There are plenty of ways to resolve plenty of things. Still more to come.
struct ircd::net::resolve
{
	using callback_one = std::function<void (std::exception_ptr, const ipport &)>;
	using callback_many = std::function<void (std::exception_ptr, vector_view<ipport>)>;
	using callback_reverse = std::function<void (std::exception_ptr, std::string)>;

	resolve(const hostport &, callback_one);
	resolve(const hostport &, callback_many);
	resolve(const ipport &, callback_reverse);

	resolve(const hostport &, ctx::future<ipport> &);
	resolve(const hostport &, ctx::future<std::vector<ipport>> &);

	resolve(const vector_view<hostport> &in, const vector_view<ipport> &out);
	resolve(const vector_view<ipport> &in, const vector_view<std::string> &out);
};
