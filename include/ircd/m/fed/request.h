// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_FED_REQUEST_H

namespace ircd::m::fed
{
	struct request;
}

/// Abstract request; everything goes through here.
struct ircd::m::fed::request
:server::request
{
	struct opts;

	request(const mutable_buffer &buf,
	        opts &&);

	request() = default;
};

struct ircd::m::fed::request::opts
{
	/// The remote server to contact. Must be specified for this request.
	string_view remote;

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

	/// Custom options to pass when resolving a server name with the well-known
	/// system.
	well_known::opts wkopts;

	/// Whether dynamic content buffering for incoming data will be used.
	/// if false, the user supplied buffer handles all data sent from the
	/// remote server; this is faster, but if it runs out the request is
	/// canceled and an exception is thrown.
	bool dynamic {true};
};
