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
	struct resolve extern resolve;
}

/// DNS resolution suite.
///
/// This is a singleton class; public usage is to make calls on the singleton
/// object like `ircd::net::resolve()` etc.
///
struct ircd::net::resolve
{
	// Internal resolver service instance
	struct resolver static resolver;

  public:
	using callback_one = std::function<void (std::exception_ptr, const ipport &)>;
	using callback_many = std::function<void (std::exception_ptr, std::vector<ipport>)>;
	using callback_reverse = std::function<void (std::exception_ptr, std::string)>;

	// Callback-based interface
	void operator()(const hostport &, callback_one);
	void operator()(const hostport &, callback_many);
	void operator()(const ipport &, callback_reverse);

	// Future-based interface
	ctx::future<ipport> operator()(const hostport &);
	ctx::future<std::string> operator()(const ipport &);
};
