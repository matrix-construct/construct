// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_EVENT_CONFORMS_H

/// Device to evaluate the conformity of an event object. This is an 'in vitro'
/// or 'pure' evaluation: it determines if the event is reasonably sane enough
/// to be evaluated further using only the information in the event itself. It
/// requires nothing external and conducts no IO etc..
///
/// This evaluation does not throw or stop when a check fails: instead it
/// collects the failures allowing the user to further determine how to proceed
/// at their own discretion.
///
struct ircd::m::event::conforms
{
	enum code :uint;

	uint64_t report {0};

	bool clean() const;
	operator bool() const;
	bool operator!() const;
	bool has(const uint &code) const;
	bool has(const code &code) const;
	string_view string(const mutable_buffer &out) const;

	void set(const code &code);
	void del(const code &code);
	void operator|=(const code &) &;

	conforms() = default;
	conforms(const event &);
	conforms(const event &, const uint64_t &skip);

	static code reflect(const string_view &);
	friend string_view reflect(const code &);
	friend std::ostream &operator<<(std::ostream &, const conforms &);
};

/// Report codes corresponding to the checks conducted by event::conforms.
/// Developers: If you add a code here you must also add a string reflection
/// in the definition file.
///
enum ircd::m::event::conforms::code
:uint
{
	INVALID_OR_MISSING_EVENT_ID,       ///< event_id empty or failed mxid grammar check
	INVALID_OR_MISSING_ROOM_ID,        ///< room_id empty or failed mxid grammar check
	INVALID_OR_MISSING_SENDER_ID,      ///< sender empty or failed mxid grammar check
	MISSING_TYPE,                      ///< type empty
	MISSING_ORIGIN,                    ///< origin empty
	INVALID_ORIGIN,                    ///< origin not a proper domain
	INVALID_OR_MISSING_REDACTS_ID,     ///< for m.room.redaction
	MISSING_CONTENT_MEMBERSHIP,        ///< for m.room.member, content.membership
	INVALID_CONTENT_MEMBERSHIP,        ///< for m.room.member, content.membership
	MISSING_PREV_EVENTS,               ///< for non-m.room.create, empty prev_events
	MISSING_PREV_STATE,                ///< for state_key'ed, empty prev_state
	DEPTH_NEGATIVE,                    ///< depth < 0
	DEPTH_ZERO,                        ///< for non-m.room.create, depth=0
	MISSING_SIGNATURES,                ///< no signatures
	MISSING_ORIGIN_SIGNATURE,          ///< no signature for origin
	MISMATCH_ORIGIN_SENDER,            ///< sender mxid host not from origin
	MISMATCH_ORIGIN_EVENT_ID,          ///< event_id mxid host not from origin
	SELF_REDACTS,                      ///< event redacts itself
	SELF_PREV_EVENT,                   ///< event_id self-referenced in prev_events
	SELF_PREV_STATE,                   ///< event_id self-referenced in prev_state
	DUP_PREV_EVENT,                    ///< duplicate references in prev_events
	DUP_PREV_STATE,                    ///< duplicate references in prev_state

	_NUM_
};
