// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

extern mapi::header IRCD_MODULE;
extern log::log media_log;

extern "C" m::room::id
file_room_id(m::room::id::buf &out,
             const string_view &server,
             const string_view &file);

m::room::id::buf
file_room_id(const string_view &server,
             const string_view &file);

extern "C" size_t
read_each_block(const m::room &,
                const std::function<void (const const_buffer &)> &);

extern "C" size_t
write_file(const m::room &room,
           const const_buffer &content,
           const string_view &content_type);

std::pair<http::response::head, unique_buffer<mutable_buffer>>
download(const mutable_buffer &head_buf,
         const string_view &server,
         const string_view &mediaid,
         net::hostport remote = {},
         server::request::opts *const opts = nullptr);

m::room
download(const string_view &server,
         const string_view &mediaid,
         const net::hostport &remote,
         const m::room::id &room_id);

extern "C" m::room::id::buf
download(const string_view &server,
         const string_view &mediaid,
         const net::hostport &remote = {});
