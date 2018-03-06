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
#define HAVE_IRCD_M_V1_SEND_H

namespace ircd::m::v1
{
	struct send;
};

/// A federation/v1/send request. This sends the provided transaction and
/// receives a response via the server::request ctx::future. This object
/// must stay in scope to complete the request until the future is resolved.
///
struct ircd::m::v1::send
:server::request
{
	struct opts;
	struct response;

	operator json::object() const
	{
		return { in.content };
	}

	send(const string_view &txnid,        // transaction ID (goes in URL)
	     const const_buffer &content,     // full transaction (HTTP content out)
	     const mutable_buffer &head,      // buffer for HTTP head in and out
	     opts);                           // options structure

	send() = default;
};

/// Options for a federation send request.
///
struct ircd::m::v1::send::opts
{
	/// The remote server to contact. Must be specified for this request.
	net::hostport remote;

	/// The m::request structure which helps compose this request. The fields
	/// of this object are eventually used to sign the request for [Fed. 12.1]
	/// Request Authentication. User does not have to fill anything in here;
	/// anything not provided is derived automatically, but providing these
	/// fields will override that derivation.
	m::request request;

	/// The lower-level server::out structure used by server:: when transmitting
	/// data; providing anything here is optional and will override things.
	server::out out;

	/// The lower-level server::in structure used by server:: when receiving
	/// data; providing anything here is optional and will override things.
	server::in in;

	/// The lower-level server::request::opts configuration to attach to
	/// this request.
	const struct server::request::opts *sopts {nullptr};
};

/// Helper for dealing with response content from a /send/.
struct ircd::m::v1::send::response
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
