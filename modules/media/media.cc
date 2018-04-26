// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "media.h"

mapi::header
IRCD_MODULE
{
	"11.7 :Content respository"
};

decltype(media_log)
media_log
{
	"media"
};

std::set<m::room::id>
downloading;

ctx::dock
downloading_dock;

m::room::id::buf
download(const string_view &server,
         const string_view &mediaid,
         const net::hostport &remote)
{
	const m::room::id::buf room_id
	{
		file_room_id(server, mediaid)
	};

	download(server, mediaid, remote, room_id);
	return room_id;
}

m::room
download(const string_view &server,
         const string_view &mediaid,
         const net::hostport &remote,
         const m::room::id &room_id)
try
{
	auto iit
	{
		downloading.emplace(room_id)
	};

	if(!iit.second)
	{
		do
		{
			downloading_dock.wait();
		}
		while(downloading.count(room_id));
		return room_id;
	}

	const unwind uw{[&iit]
	{
		downloading.erase(iit.first);
		downloading_dock.notify_all();
	}};

	if(exists(m::room{room_id}))
		return room_id;

	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	const auto pair
	{
		download(buf, server, mediaid, remote)
	};

	const auto &head
	{
		pair.first
	};

	const const_buffer &content
	{
		pair.second
	};

	char mime_type_buf[64];
	const auto &content_type
	{
		magic::mime(mime_type_buf, content)
	};

	if(content_type != head.content_type) log::warning
	{
		media_log, "Server %s claims thumbnail %s is '%s' but we think it is '%s'",
		string(remote),
		mediaid,
		head.content_type,
		content_type
	};

	m::vm::opts::commit vmopts;
	vmopts.history = false;
	const m::room room
	{
		room_id, &vmopts
	};

	create(room, m::me.user_id, "file");

	const size_t written
	{
		write_file(room, content, content_type)
	};

	return room;
}
catch(const ircd::server::unavailable &e)
{
	throw http::error
	{
		http::BAD_GATEWAY, e.what()
	};
}

std::pair<http::response::head, unique_buffer<mutable_buffer>>
download(const mutable_buffer &head_buf,
         const string_view &server,
         const string_view &mediaid,
         net::hostport remote,
         server::request::opts *const opts)
{
	if(!remote)
		remote = server;

	window_buffer wb{head_buf};
	thread_local char uri[4_KiB];
	http::request
	{
		wb, host(remote), "GET", fmt::sprintf
		{
			uri, "/_matrix/media/r0/download/%s/%s", server, mediaid
		}
	};

	const const_buffer out_head
	{
		wb.completed()
	};

	// Remaining space in buffer is used for received head
	const mutable_buffer in_head
	{
		data(head_buf) + size(out_head), size(head_buf) - size(out_head)
	};

	//TODO: --- This should use the progress callback to build blocks
	// Null content buffer will cause dynamic allocation internally.
	const mutable_buffer in_content{};
	server::request remote_request
	{
		remote, { out_head }, { in_head, in_content }, opts
	};

	//TODO: conf
	if(!remote_request.wait(seconds(10), std::nothrow))
		throw http::error
		{
			http::REQUEST_TIMEOUT
		};

	const auto &code
	{
		remote_request.get()
	};

	if(code != http::OK)
		return {};

	parse::buffer pb{remote_request.in.head};
	parse::capstan pc{pb};
	pc.read += size(remote_request.in.head);
	return
	{
		http::response::head{pc}, std::move(remote_request.in.dynamic)
	};
}

size_t
write_file(const m::room &room,
           const const_buffer &content,
           const string_view &content_type)
{
	//TODO: TXN
	send(room, m::me.user_id, "ircd.file.stat", "size",
	{
		{ "value", long(size(content)) }
	});

	//TODO: TXN
	send(room, m::me.user_id, "ircd.file.stat", "type",
	{
		{ "value", content_type }
	});

	const auto lpath
	{
		fs::make_path({fs::DPATH, "media"})
	};

	char pathbuf[768];
	size_t pathlen{0};
	pathlen = strlcpy(pathbuf, lpath);
	pathlen = strlcat(pathbuf, "/"_sv); //TODO: fs utils
	const mutable_buffer pathpart
	{
		pathbuf + pathlen, sizeof(pathbuf) - pathlen
	};

	size_t off{0}, wrote{0};
	while(off < size(content))
	{
		const size_t blksz
		{
			std::min(size(content) - off, size_t(32_KiB))
		};

		const const_buffer &block
		{
			data(content) + off, blksz
		};

		const sha256::buf hash_
		{
			sha256{block}
		};

		char b58buf[hash_.size() * 2];
		const string_view hash
		{
			b58encode(b58buf, hash_)
		};

		send(room, m::me.user_id, "ircd.file.block",
		{
			{ "size",  long(blksz) },
			{ "hash",  hash        }
		});

		const string_view path
		{
			pathbuf, pathlen + copy(pathpart, hash)
		};

		wrote += size(fs::overwrite(path, block));
		off += blksz;
	}

	assert(off == size(content));
	assert(wrote == off);
	return wrote;
}

size_t
read_each_block(const m::room &room,
                const std::function<void (const const_buffer &)> &closure)
{
	const auto lpath
	{
		fs::make_path({fs::DPATH, "media"})
	};

	char pathbuf[768];
	size_t pathlen{0};
	pathlen = strlcpy(pathbuf, lpath);
	pathlen = strlcat(pathbuf, "/"_sv); //TODO: fs utils
	const mutable_buffer pathpart
	{
		pathbuf + pathlen, sizeof(pathbuf) - pathlen
	};

	// Block buffer
	const unique_buffer<mutable_buffer> buf
	{
		64_KiB
	};

	size_t ret{0};
	m::room::messages it{room, 1};
	for(; bool(it); ++it)
	{
		const m::event &event{*it};
		if(at<"type"_>(event) != "ircd.file.block")
			continue;

		const auto &hash
		{
			unquote(at<"content"_>(event).at("hash"))
		};

		const auto &blksz
		{
			at<"content"_>(event).get<size_t>("size")
		};

		const string_view path
		{
			pathbuf, pathlen + copy(pathpart, hash)
		};

		const const_buffer &block
		{
			fs::read(path, buf)
		};

		if(unlikely(size(block) != blksz)) throw error
		{
			"File [%s] block [%s] (%s) blksz %zu != %zu",
			string_view{room.room_id},
			string_view{at<"event_id"_>(event)},
			path,
			blksz,
			size(block)
		};

		assert(size(block) == blksz);
		ret += size(block);
		closure(block);
	}

	return ret;
}

m::room::id::buf
file_room_id(const string_view &server,
             const string_view &file)
{
	m::room::id::buf ret;
	file_room_id(ret, server, file);
	return ret;
}

m::room::id
file_room_id(m::room::id::buf &out,
             const string_view &server,
             const string_view &file)
{
	if(empty(server) || empty(file))
		throw m::BAD_REQUEST
		{
			"Invalid MXC: empty server or file parameters..."
		};

	size_t len;
	thread_local char buf[512];
	len = strlcpy(buf, server);
	len = strlcat(buf, "/"_sv);
	len = strlcat(buf, file);
	const sha256::buf hash
	{
		sha256{string_view{buf, len}}
	};

	out =
	{
		b58encode(buf, hash), my_host()
	};

	return out;
}
