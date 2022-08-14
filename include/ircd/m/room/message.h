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

	/// The format used in the formatted_body.
	json::property<name::format, json::string>,

	/// The formatted version of the body. This is required if format
	/// is specified.
	json::property<name::formatted_body, json::string>,

	/// m.relates_to
	json::property<name::m_relates_to, m::relates_to>,

	/// Required. enum.
	json::property<name::msgtype, json::string>,

	/// mxc:// for media.
	json::property<name::url, json::string>
>
{
	/// The event ID of the message being replied to; empty if malformed or
	/// not a reply. If malformed, the message is not considered a reply.
	id::event reply_to_event() const noexcept;

	/// The user who sent the message being replied to; empty if not a reply
	/// or the username was missing or malformed.
	id::user reply_to_user() const noexcept;

	/// The message being replied to, cut to quoted content, which may include
	/// multiple pseudo-newlines and leading '>' interrupting the text; caller
	/// must clean that up if required. Empty if not a reply or malformed.
	string_view reply_to_body() const noexcept;

	/// The event ID of the replaced event; empty if not a replace
	id::event replace_event() const noexcept;

	/// The new content body; empty if not a replace or replace was empty!
	string_view replace_body() const noexcept;

	/// C2S v1.3 11.3.1 message body stripped of any reply fallback. This is
	/// the proper way to read the message rather than reading "body" direct;
	/// returns "body" if not reply.
	string_view body() const noexcept;

	using super_type::tuple;
};
