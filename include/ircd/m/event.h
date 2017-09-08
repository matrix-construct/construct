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

#pragma once
#define HAVE_IRCD_M_EVENT_H

///////////////////////////////////////////////////////////////////////////////
// Protocol notes
//
// 10.4
// The total size of any event MUST NOT exceed 65 KB.
//
// There are additional restrictions on sizes per key:
// sender MUST NOT exceed 255 bytes (including domain).
// room_id MUST NOT exceed 255 bytes.
// state_key MUST NOT exceed 255 bytes.
// type MUST NOT exceed 255 bytes.
// event_id MUST NOT exceed 255 bytes.
//
// Some event types have additional size restrictions which are specified in
// the description of the event. Additional keys have no limit other than that
// implied by the total 65 KB limit on events.
//

namespace ircd::m
{
	struct event;
}

namespace ircd::m::name
{
	constexpr const char *const event_id {"event_id"};
	constexpr const char *const content {"content"};
	constexpr const char *const origin_server_ts {"origin_server_ts"};
	constexpr const char *const sender {"sender"};
	constexpr const char *const type {"type"};
	constexpr const char *const room_id {"room_id"};
	constexpr const char *const state_key {"state_key"};
	constexpr const char *const prev_ids {"prev_ids"};
	constexpr const char *const unsigned_ {"unsigned"};
	constexpr const char *const signatures {"signatures"};
}

struct ircd::m::event
:json::tuple
<
	json::member<name::event_id, string_view>,
	json::member<name::content, string_view>,
	json::member<name::origin_server_ts, time_t>,
	json::member<name::sender, string_view>,
	json::member<name::type, string_view>,
	json::member<name::room_id, string_view>,
	json::member<name::state_key, string_view>,
	json::member<name::prev_ids, string_view>,
	json::member<name::unsigned_, string_view>,
	json::member<name::signatures, string_view>
>
{
	using super_type::tuple;
	using super_type::operator=;
};
