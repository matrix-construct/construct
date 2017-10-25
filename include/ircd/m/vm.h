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
#define HAVE_IRCD_M_VM_H

namespace ircd::m::vm
{
	using closure = std::function<void (const event &)>;
	using closure_bool = std::function<bool (const event &)>;

	bool query(const event::query<> &, const closure_bool &);
	bool query(const closure_bool &);

	void for_each(const event::query<> &, const closure &);
	void for_each(const closure &);

	size_t count(const event::query<> &, const closure_bool &);
	size_t count(const event::query<> &);

	bool test(const event::query<> &, const closure_bool &);
	bool test(const event::query<> &);
};

namespace ircd::m::vm
{
	extern uint64_t current_sequence;
	extern ctx::view<const event> inserted;

	// Synchronous fetch and eval
	size_t acquire(const vector_view<id::event> &, const vector_view<mutable_buffer> &);
	json::object acquire(const id::event &, const mutable_buffer &);
    void state(const room::id &, const event::id &);
    void backfill(const room::id &, const event::id &v, const size_t &limit);

	using tracer = std::function<bool (const event &, event::id::buf &)>;
	void trace(const id::event &, const tracer &);

	// Hypostasis
	void eval(const vector_view<event> &);
	void eval(const json::array &);
	void eval(const event &);

	event::id::buf commit(json::iov &event);
    event::id::buf join(const room::id &, json::iov &iov);
}
