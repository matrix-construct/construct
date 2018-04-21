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

m::room::id::buf
file_room_id(const string_view &server,
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

	return m::room::id::buf
	{
		b58encode(buf, hash), my_host()
	};
}

size_t
write_file(const m::room &room,
           const string_view &content,
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

		if(!fs::exists(path))
			wrote += size(fs::write(path, block));

		off += blksz;
	}

	assert(off == size(content));
	assert(wrote <= off);
	return wrote;
}

size_t
read_each_block(const m::room &room,
                const std::function<void (const string_view &)> &closure)
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
	m::room::messages it{room, 0};
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

		const string_view &block
		{
			fs::read(path, buf)
		};

		assert(size(block) == blksz);
		ret += size(block);
		closure(block);
	}

	return ret;
}
