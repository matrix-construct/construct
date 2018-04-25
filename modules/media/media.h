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

size_t read_each_block(const m::room &, const std::function<void (const const_buffer &)> &);
size_t write_file(const m::room &room, const const_buffer &content, const string_view &content_type);
