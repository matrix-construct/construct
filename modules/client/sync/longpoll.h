// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::sync::longpoll
{
	struct accepted;

	size_t polling {0};
	std::deque<accepted> queue;
	ctx::dock dock;

	static bool handle(data &, const args &, const accepted &, const mutable_buffer &scratch);
	static bool poll(data &, const args &);
	static void handle_notify(const m::event &, m::vm::eval &);
	extern m::hookfn<m::vm::eval &> notified;
}

struct ircd::m::sync::longpoll::accepted
:m::event
{
	json::strung strung;
	std::string client_txnid;
	event::idx event_idx;

	accepted(const m::vm::eval &eval)
	:strung
	{
		*eval.event_
	}
	,client_txnid
	{
		eval.copts?
			eval.copts->client_txnid:
			string_view{}
	}
	,event_idx
	{
		eval.sequence
	}
	{
		const json::object object{this->strung};
		static_cast<m::event &>(*this) = m::event{object};
	}

	accepted(accepted &&) = default;
	accepted(const accepted &) = delete;
};
