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
#define HAVE_IRCD_M_FED_SEND_H

namespace ircd::m::fed
{
	struct send;
};

/// A federation/v1/send request. This sends the provided transaction and
/// receives a response via the server::request ctx::future. This object
/// must stay in scope to complete the request until the future is resolved.
///
struct ircd::m::fed::send
:request
{
	struct response;

	explicit operator json::object() const
	{
		return json::object
		{
			in.content
		};
	}

	send(const string_view &txnid,        // transaction ID (goes in URL)
	     const const_buffer &content,     // full transaction (HTTP content out)
	     const mutable_buffer &head,      // buffer for HTTP head in and out
	     opts);                           // options structure

	explicit
	send(const txn::array &pdu,
	     const txn::array &edu,
	     const mutable_buffer &buf,
	     opts);

	send() = default;
};

/// Helper for dealing with response content from a /send/.
struct ircd::m::fed::send::response
:json::object
{
	/// A member of the response object is "pdus" and this helps iterate that object
	/// which is used to convey errors keyed by the event_id; value is matrix error obj.
	using pdus_closure = std::function<void (const id::event &, const json::object &)>;
	void for_each_pdu(const pdus_closure &) const;

	response(const json::object &object)
	:json::object{object}
	{}
};
