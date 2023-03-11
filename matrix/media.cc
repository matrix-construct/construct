// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::media::log)
ircd::m::media::log
{
	"m.media"
};

decltype(ircd::m::media::events_prefetch)
ircd::m::media::events_prefetch
{
	{ "name",     "ircd.m.media.file.prefetch.events" },
	{ "default",  16L                                 },
};

decltype(ircd::m::media::journal_threshold)
ircd::m::media::journal_threshold
{
	{ "name",     "ircd.m.media.journal.threshold" },
	{ "default",  0L                               },
};

decltype(ircd::m::media::downloading)
ircd::m::media::downloading;

decltype(ircd::m::media::downloading_dock)
ircd::m::media::downloading_dock;

//
// media::file
//

ircd::m::room::id::buf
ircd::m::media::file::download(const mxc &mxc,
                               const m::user::id &user_id,
                               const string_view &remote)
{
	const m::room::id::buf room_id
	{
		file::room_id(mxc)
	};

	if(remote && my_host(remote))
		return room_id;

	if(!remote && my_host(mxc.server))
		return room_id;

	download(mxc, user_id, room_id, remote);
	return room_id;
}

ircd::m::room
ircd::m::media::file::download(const mxc &mxc,
                               const m::user::id &user_id,
                               const m::room::id &room_id,
                               string_view remote)
try
{
	auto iit
	{
		downloading.emplace(room_id)
	};

	if(!iit.second)
	{
		downloading_dock.wait([&room_id]
		{
			return !downloading.count(room_id);
		});

		return room_id;
	}

	const unwind uw{[&iit]
	{
		downloading.erase(iit.first);
		downloading_dock.notify_all();
	}};

	if(exists(room_id))
		return room_id;

	if(!remote)
		remote = mxc.server;

	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	const auto &[head, content]
	{
		download(buf, mxc, remote)
	};

	char mime_type_buf[64];
	const auto &content_type
	{
		magic::mime(mime_type_buf, content)
	};

	if(content_type != head.content_type)
		log::dwarning
		{
			log, "Server %s claims thumbnail %s is '%s' but we think it is '%s'",
			remote,
			mxc.mediaid,
			head.content_type,
			content_type,
		};

	m::vm::copts vmopts;

	// Disable the WAL for file rooms
	if(size(content) >= size_t(journal_threshold))
		vmopts.wopts.sopts.journal = false;

	const ctx::uninterruptible::nothrow ui;
	auto room
	{
		create(room_id, user_id, "file")
	};

	room.copts = &vmopts;
	const size_t written
	{
		file::write(room, user_id, content, content_type)
	};

	return room;
}
catch(const ircd::server::unavailable &e)
{
	throw m::error
	{
		http::BAD_GATEWAY, "M_MEDIA_UNAVAILABLE",
		"Server '%s' is not available for media for '%s/%s' :%s",
		remote,
		mxc.server,
		mxc.mediaid,
		e.what()
	};
}

decltype(ircd::m::media::download_timeout)
ircd::m::media::download_timeout
{
	{ "name",     "ircd.m.media.download.timeout" },
	{ "default",  30L                             },
};

std::pair
<
	ircd::http::response::head,
	ircd::unique_buffer<ircd::mutable_buffer>
>
ircd::m::media::file::download(const mutable_buffer &buf_,
                               const mxc &mxc,
                               string_view remote,
                               server::request::opts *const opts)
{
	assert(remote || !my_host(mxc.server));
	assert(!remote || !my_host(remote));

	mutable_buffer buf{buf_};
	fed::request::opts fedopts;
	fedopts.remote = remote?: mxc.server;
	json::get<"method"_>(fedopts.request) = "GET";
	thread_local char mxc_buf[2][2048];
	json::get<"uri"_>(fedopts.request) = fmt::sprintf
	{
		buf, "/_matrix/media/r0/download/%s/%s",
		url::encode(mxc_buf[0], mxc.server),
		url::encode(mxc_buf[1], mxc.mediaid),
	};
	consume(buf, size(json::get<"uri"_>(fedopts.request)));

	//TODO: --- This should use the progress callback to build blocks
	fed::request remote_request
	{
		buf, std::move(fedopts)
	};

	if(!remote_request.wait(seconds(download_timeout), std::nothrow))
		throw m::error
		{
			http::GATEWAY_TIMEOUT, "M_MEDIA_DOWNLOAD_TIMEOUT",
			"Server '%s' did not respond with media for '%s/%s' in time",
			remote,
			mxc.server,
			mxc.mediaid
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
	return std::pair<http::response::head, unique_buffer<mutable_buffer>>
	{
		pc, std::move(remote_request.in.dynamic)
	};
}

size_t
ircd::m::media::file::write(const m::room &room,
                            const m::user::id &user_id,
                            const const_buffer &content,
                            const string_view &content_type,
                            const string_view &name)
try
{
	static const size_t BLK_SZ
	{
		32_KiB
	};

	static const size_t BLK_ENCODE_BUF_SZ
	{
		48_KiB
	};

	static const size_t BLK_ENCODE_BUF_ALIGN
	{
		64
	};

	static_assert(BLK_ENCODE_BUF_SZ >= b64::encode_size(BLK_SZ));
	const unique_mutable_buffer blk_encode_buf
	{
		BLK_ENCODE_BUF_SZ,
		BLK_ENCODE_BUF_ALIGN,
	};

	send(room, user_id, "ircd.file.stat.size", "", json::members
	{
		{ "bytes", long(size(content)) }
	});

	send(room, user_id, "ircd.file.stat.type", "", json::members
	{
		{ "mime_type", content_type }
	});

	if(name)
		send(room, user_id, "ircd.file.stat.name", "", json::members
		{
			{ "name", name }
		});

	size_t off{0}, wrote{0};
	while(off < size(content))
	{
		const size_t blk_sz
		{
			std::min(size(content) - off, BLK_SZ)
		};

		const const_buffer blk_raw
		{
			content + off, blk_sz
		};

		const string_view blk
		{
			b64::encode(blk_encode_buf, blk_raw)
		};

		assert(size(blk) == b64::encode_size(blk_raw));
		const auto event_id
		{
			send(room, user_id, "ircd.file.block.b64", json::members
			{
				{ "data", blk },
			})
		};

		off += size(blk_raw);
		wrote += size(blk);
	}

	log::logf
	{
		log, log::level::DEBUG,
		"File written %s by %s type:%s len:%zu pos:%zu wrote:%zu",
		string_view{room.room_id},
		string_view{user_id},
		content_type,
		size(content),
		off,
		wrote,
	};

	//assert(wrote == b64::encode_size(off));
	assert(off == size(content));
	return off;
}
catch(const std::exception &e)
{
	const ctx::exception_handler eh;
	log::error
	{
		log, "File writing %s by %s type:%s len:%zu :%s",
		string_view{room.room_id},
		string_view{user_id},
		content_type,
		size(content),
		e.what(),
	};

	m::room::purge
	{
		room.room_id
	};

	std::rethrow_exception(eh);
}

size_t
ircd::m::media::file::read(const m::room &room,
                           const closure &closure)
{
	static const size_t BLK_DECODE_BUF_SZ
	{
		64_KiB
	};

	static const size_t BLK_DECODE_BUF_ALIGN
	{
		64
	};

	const unique_mutable_buffer blk_decode_buf
	{
		BLK_DECODE_BUF_SZ,
		BLK_DECODE_BUF_ALIGN,
	};

	static const event::fetch::opts fopts
	{
		event::keys::include
		{
			"content", "type"
		}
	};

	room::events it
	{
		room, 1, &fopts
	};

	if(!it)
		return 0;

	room::events epf
	{
		room, 1, &fopts
	};

	size_t
	decoded_bytes(0),
	encoding_bytes(0),
	events_fetched(0),
	events_prefetched(0);
	m::event::fetch event;
	for(; it; ++it) try
	{
		for(; epf && events_prefetched < events_fetched + events_prefetch; ++epf)
			events_prefetched += epf.prefetch();

		seek(event, it.event_idx());
		++events_fetched;

		if(json::get<"type"_>(event) != "ircd.file.block.b64")
			continue;

		const json::object content
		{
			json::get<"content"_>(event)
		};

		const json::string &blk_encoded
		{
			content["data"]
		};

		const const_buffer blk
		{
			b64::decode(blk_decode_buf, blk_encoded)
		};

		if constexpr(debug_read)
			log::debug
			{
				log, "File %s read event_idx:%lu events[fetched:%zu prefetched:%zu] encoded:%zu decoded:%zu total_encoded:%zu total_decoded:%zu",
				string_view{room.room_id},
				it.event_idx(),
				events_fetched,
				events_prefetched,
				size(blk_encoded),
				size(blk),
				encoding_bytes,
				decoded_bytes,
			};

		assert(size(blk) == b64::decode_size(blk_encoded));

		closure(blk);
		decoded_bytes += size(blk);
		encoding_bytes += size(blk_encoded);
	}
	catch(const ctx::interrupted &)
	{
		throw;
	}
	catch(const std::exception &e)
	{
		log::error
		{
			log, "File %s block:%zu decoded:%zu :%s",
			string_view{room.room_id},
			events_fetched,
			decoded_bytes,
			e.what()
		};

		throw;
	}

	//assert(decoded_bytes == b64::decode_size(encoding_bytes));
	return decoded_bytes;
}

//
// media::file
//

ircd::m::room::id::buf
ircd::m::media::file::room_id(const mxc &mxc)
{
	m::room::id::buf ret;
	room_id(ret, mxc);
	return ret;
}

ircd::m::room::id
ircd::m::media::file::room_id(room::id::buf &out,
                              const mxc &mxc)
{
	thread_local char buf[512];
	const auto path
	{
		mxc.path(buf)
	};

	const sha256::buf hash
	{
		sha256{path}
	};

	out =
	{
		b64::encode_unpadded(buf, hash, b64::urlsafe), my_host()
	};

	return out;
}

//
// media::mxc
//

ircd::m::media::mxc::mxc(const string_view &server,
                         const string_view &mediaid)
:server
{
	split(lstrip(server, "mxc://"), '/').first
}
,mediaid
{
	mediaid?: rsplit(server, '/').second
}
{
	if(unlikely(empty(server)))
		throw m::BAD_REQUEST
		{
			"Invalid MXC: missing server parameter."
		};

	if(unlikely(empty(mediaid)))
		throw m::BAD_REQUEST
		{
			"Invalid MXC: missing mediaid parameter."
		};
}

ircd::m::media::mxc::mxc(const string_view &uri)
:server
{
	split(lstrip(uri, "mxc://"), '/').first
}
,mediaid
{
	rsplit(uri, '/').second
}
{
	if(unlikely(empty(server)))
		throw m::BAD_REQUEST
		{
			"Invalid MXC: missing server parameter."
		};

	if(unlikely(empty(mediaid)))
		throw m::BAD_REQUEST
		{
			"Invalid MXC: missing mediaid parameter."
		};
}

ircd::string_view
ircd::m::media::mxc::uri(const mutable_buffer &out)
const
{
	return fmt::sprintf
	{
		out, "mxc://%s/%s",
		server,
		mediaid
	};
}

ircd::string_view
ircd::m::media::mxc::path(const mutable_buffer &out)
const
{
	return fmt::sprintf
	{
		out, "%s/%s",
		server,
		mediaid
	};
}
