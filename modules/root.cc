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

	const unique_buffer<mutable_buffer> buffer
	{
		24_KiB
	};

	const auto &file_name{it->second};
	const size_t file_size
	{
		fs::size(file_name)
	};

	string_view chunk
	{
		fs::read(file_name, buffer)
	};

	char content_type_buf[64];
	resource::response
	{
		client,
		http::OK,
		content_type(content_type_buf, file_name, chunk),
		file_size
	};

	const unwind::exceptional terminate{[&client]
	{
		client.close(net::dc::RST, net::close_ignore);
	}};

	size_t written
	{
		client.write_all(chunk)
	};

	size_t offset
	{
		size(chunk)
	};

	while(offset < file_size)
	{
		chunk = fs::read(file_name, buffer, offset);
		assert(!empty(chunk));
		written += client.write_all(chunk);
		offset += size(chunk);
		assert(written == offset);
	}

	assert(offset == file_size);
	assert(written == offset);
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
