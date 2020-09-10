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
#define HAVE_IRCD_M_ROOM_MESSAGE_H

/// This is a json::tuple describing the common m.room.message content schema
/// intended for fast and convenient observation of message content. Note that
/// more properties will likely exist and they can be accessed using the
/// json::tuple.source member which points to the json::object this tuple
/// was constructed with.
///
struct ircd::m::room::message
:json::tuple
<
	/// Required. The body of the message.
	json::property<name::body, json::string>,

	/// Required. enum.
	json::property<name::msgtype, json::string>,

	/// The format used in the formatted_body.
	json::property<name::format, json::string>,

	/// The formatted version of the body. This is required if format
	/// is specified.
	json::property<name::formatted_body, json::string>
>
{
	using super_type::tuple;
};
