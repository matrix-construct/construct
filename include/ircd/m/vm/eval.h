// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_VM_EVAL_H

namespace ircd::m::vm
{
	struct eval;

	const event *find_pdu(const eval &, const event::id &) noexcept;
	eval *find_parent(const eval &, const ctx::ctx & = ctx::cur()) noexcept;
	eval *find_root(const eval &, const ctx::ctx & = ctx::cur()) noexcept;

	string_view loghead(const mutable_buffer &, const eval &);
	string_view loghead(const eval &);    // single tls buffer

	size_t prefetch_refs(const eval &);
	size_t fetch_keys(const eval &);
}

/// Event Evaluation Device
///
/// This object conducts the evaluation of an event or a tape of multiple
/// events. An event is evaluated in an attempt to execute it. Events which
/// fail during evaluation won't be executed; such is the case for events which
/// have already been executed, or events which are invalid or lead to invalid
/// transitions or actions of the machine etc.
///
struct ircd::m::vm::eval
:instance_list<eval>
{
	static uint64_t id_ctr;
	static uint executing;
	static uint injecting;

	const vm::opts *opts {&default_opts};
	const vm::copts *copts {nullptr};
	ctx::ctx *ctx {ctx::current};
	vm::eval *parent {nullptr};
	vm::eval *child {nullptr};
	uint64_t id {++id_ctr};
	uint64_t sequence {0};
	std::shared_ptr<db::txn> txn;
	unique_mutable_buffer buf;
	size_t evaluated {0};
	size_t accepted {0};
	size_t faulted {0};

	vector_view<const m::event> pdus;
	const json::iov *issue {nullptr};
	const event *event_ {nullptr};
	string_view room_id;
	event::id::buf event_id;
	event::conforms report;
	string_view room_version;
	hook::base *hook {nullptr};
	vm::phase phase {vm::phase(0)};
	bool room_internal {false};

  public:
	operator const event::id::buf &() const
	{
		return event_id;
	}

	eval(const vm::opts &);
	eval(const vm::copts &);
	eval(const event &, const vm::opts & = default_opts);
	eval(const vector_view<const m::event> &, const vm::opts & = default_opts);
	eval(const json::array &, const vm::opts & = default_opts);
	eval(json::iov &event, const json::iov &content, const vm::copts & = default_copts);
	eval() = default;
	eval(eval &&) = delete;
	eval(const eval &) = delete;
	eval &operator=(eval &&) = delete;
	eval &operator=(const eval &) = delete;
	~eval() noexcept;

	// Tools for all evals
	static bool for_each(const std::function<bool (eval &)> &);
	static bool for_each_pdu(const std::function<bool (const event &)> &);

	// Tools for all evals sharing this ircd::context
	static bool for_each(const ctx::ctx *const &, const std::function<bool (eval &)> &);
	static size_t count(const ctx::ctx *const &);

	// Event snoop interface
	static const event *find_pdu(const event::id &);
	static size_t count(const event::id &);
	static eval *find(const event::id &);
	static eval &get(const event::id &);

	// Sequence related interface
	static bool sequnique(const uint64_t &seq);
	static eval *seqnext(const uint64_t &seq);
	static eval *seqmax();
	static eval *seqmin();
	static void seqsort();
};
