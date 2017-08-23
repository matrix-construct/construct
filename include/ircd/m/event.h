/*
 * charybdis: 21st Century IRC++d
 *
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
 *
 */

#include <ircd/json/object.h>

#pragma once
#define HAVE_IRCD_M_EVENT_H

namespace ircd {
namespace m    {

struct event
:json::object
<
	string_view,
	time_t,
	string_view,
	string_view,
	string_view,
	string_view
>
{
	IRCD_MEMBERS
	(
		"content",
		"origin_server_ts",
		"sender",
		"type",
		"unsigned",
		"state_key"
	)

	template<class... A>
	event(A&&... a)
	:object{std::make_tuple(std::forward<A>(a)...)}
	{}
};

struct sync
:json::object
<
	string_view,
	string_view,
	string_view,
	string_view
>
{
	IRCD_MEMBERS
	(
		"account_data",
		"next_batch",
		"rooms",
		"presence",
	)

	sync(const json::doc &doc)
	{
	
	}

	template<class... A>
	sync(A&&... a)
	:object{std::make_tuple(std::forward<A>(a)...)}
	{}
};

} // namespace m
} // namespace ircd
