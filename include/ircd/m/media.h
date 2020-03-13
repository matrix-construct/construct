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
#define HAVE_IRCD_M_MEDIA_H

namespace ircd::m::media
{
	struct mxc;
}

namespace ircd::m::media::file
{
	using closure = std::function<void (const const_buffer &)>;

	room::id room_id(room::id::buf &out, const mxc &);
	room::id::buf room_id(const mxc &);

	size_t read(const room &, const closure &);
	size_t write(const room &, const user::id &, const const_buffer &content, const string_view &content_type);

	room::id::buf
	download(const mxc &,
	         const m::user::id &,
	         const string_view &remote = {});

	std::pair<http::response::head, unique_buffer<mutable_buffer>>
	download(const mutable_buffer &head_buf,
	         const mxc &mxc,
	         string_view remote = {},
	         server::request::opts *const opts = nullptr);

	m::room
	download(const mxc &mxc,
	         const m::user::id &user_id,
	         const m::room::id &room_id,
	         string_view remote = {});
};

namespace ircd::m::media::block
{
	using closure = std::function<void (const const_buffer &)>;

	bool prefetch(const string_view &hash);
	bool get(const string_view &hash, const closure &);
	const_buffer get(const mutable_buffer &out, const string_view &hash);

	void set(const string_view &hash, const const_buffer &block);
	string_view set(const mutable_buffer &hashbuf, const const_buffer &block);
	m::event::id::buf set(const room &, const user::id &, const const_buffer &block);
}

struct ircd::m::media::mxc
{
	string_view server;
	string_view mediaid;

  public:
	string_view path(const mutable_buffer &out) const;
	string_view uri(const mutable_buffer &out) const;

	// construct from server and file; note that if mediaid is empty then
	// server is fed to the uri constructor.
	mxc(const string_view &server, const string_view &mediaid);

	// construct from "server/file" or "mxc://server/file"
	mxc(const string_view &uri);
};
