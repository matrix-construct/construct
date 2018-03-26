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

std::map<std::string, std::string, iless> files
{};

void
init_files()
{
	// TODO: XXX
	for(const auto &file : fs::ls_recursive("/home/jason/charybdis/charybdis/modules/static"))
	{
		const auto name(tokens_after(file, '/', 5));
		files.emplace(std::string(name), file);
	}
}

static string_view
content_type(const mutable_buffer &out,
             const string_view &filename,
             const string_view &content);

resource::response
get_root(client &client,
         const resource::request &request)
try
{
	const auto &path
	{
		!request.head.path?
			"index.html":
		request.head.path == "/"?
			"index.html":
		request.head.path
	};

	auto it
	{
		files.find(lstrip(path, '/'))
	};

	if(it == end(files))
		throw http::error{http::NOT_FOUND};

	const auto &filename{it->second};

	char content_buffer[24_KiB];
	const string_view first_chunk
	{
		fs::read(filename, content_buffer)
	};

	char content_type_buf[64];
	resource::response
	{
		client,
		http::OK,
		content_type(content_type_buf, filename, first_chunk),
		size(first_chunk) < sizeof(content_buffer)? size(first_chunk) : -1
	};

	if(size(first_chunk) < sizeof(content_buffer))
	{
		client.write_all(first_chunk);
		return {};
	}

	char headbuf[64];
	const unwind::exceptional terminate{[&headbuf, &client]
	{
		//TODO: find out if it's not a client problem so we don't have to eject
		//TODO: them. And write_all() blowing up inside here is bad.
		client.close(net::dc::RST, net::close_ignore);
		//client.write_all("\r\n"_sv);
		//client.write_all(http::writechunk(headbuf, 0));
		//client.write_all("\r\n"_sv);
	}};

	client.write_all(http::writechunk(headbuf, size(first_chunk)));
	client.write_all(first_chunk);
	client.write_all("\r\n"_sv);

	static const size_t max_chunk_length{48_KiB};
	const unique_buffer<mutable_buffer> chunk_buffer
	{
		max_chunk_length
	};

	for(size_t offset(size(first_chunk));;)
	{
		const string_view chunk
		{
			fs::read(filename, chunk_buffer, offset)
		};

		if(empty(chunk))
			break;

		const string_view head
		{
			http::writechunk(headbuf, size(chunk))
		};

		client.write_all(head);
		client.write_all(chunk);
		client.write_all("\r\n"_sv);
		offset += size(chunk);
		if(size(chunk) < max_chunk_length)
			break;
	}

	client.write_all(http::writechunk(headbuf, 0));
	client.write_all("\r\n"_sv);

	return {};
}
catch(const fs::filesystem_error &e)
{
	throw http::error
	{
		http::NOT_FOUND  //TODO: interp error_code
	};
}

string_view
content_type(const mutable_buffer &out,
             const string_view &filename,
             const string_view &content)
{
	const auto extension
	{
		rsplit(filename, '.').second
	};

	string_view content_type; switch(hash(extension))
	{
		case hash("css"):    content_type = "text/css; charset=utf-8";  break;
		case hash("js"):     content_type = "application/javascript; charset=utf-8";  break;
		case hash("html"):   content_type = "text/html; charset=utf-8"; break;
		case hash("ico"):    content_type = "image/x-icon"; break;
		case hash("svg"):    content_type = "image/svg+xml"; break;
		case hash("png"):    content_type = "image/png"; break;
		case hash("gif"):    content_type = "image/gif"; break;
		case hash("jpeg"):
		case hash("jpg"):    content_type = "image/jpeg"; break;
		case hash("woff2"):  content_type = "application/font-woff2"; break;
		case hash("woff"):   content_type = "application/font-woff"; break;
		case hash("eot"):    content_type = "application/vnd.ms-fontobject"; break;
		case hash("otf"):
		case hash("ttf"):    content_type = "application/font-sfnt"; break;
		case hash("ogg"):    content_type = "application/ogg"; break;
		case hash("json"):   content_type = "application/json; charset=utf-8"; break;
		case hash("txt"):    content_type = "text/plain; charset=utf-8"; break;
		default:
		{
			content_type = magic::mime(out, content);
			break;
		}
	}

	return content_type;
}

resource root_resource
{
	"/",
	{
		"Webroot resource",
		root_resource.DIRECTORY
	}
};

resource::method root_get
{
	root_resource, "GET", get_root
};

mapi::header IRCD_MODULE
{
	"Web root content resource",
	init_files
};
