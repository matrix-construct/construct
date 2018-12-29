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

std::map<std::string, std::string, iless> files;

static string_view
content_type(const mutable_buffer &out, const string_view &filename, const string_view &content);

resource::response
get_root(client &client, const resource::request &request);

static void
init_files();

mapi::header
IRCD_MODULE
{
	"Web root content resource",
	init_files
};

resource
root_resource
{
	"/",
	{
		"Webroot resource",
		root_resource.DIRECTORY
	}
};

resource::method
root_get
{
	root_resource, "GET", get_root
};

conf::item<std::string>
webroot_path
{
	{ "name",       "ircd.webroot.path" },
	{ "default",    ""                  },
};

void
init_files()
{
	const std::string &path
	{
		webroot_path
	};

	if(empty(path))
		return;

	if(!fs::exists(path))
	{
		log::error
		{
			"Configured ircd.webroot.path at `%s' does not exist.", path
		};

		return;
	}

	for(const auto &file : fs::ls_r(path))
	{
		const auto name(lstrip(file, path));
		files.emplace(std::string(name), file);
	}
}

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

	const auto &file_name
	{
		it->second
	};

	const fs::fd fd
	{
		file_name
	};

	const size_t file_size
	{
		size(fd)
	};

	const unique_buffer<mutable_buffer> buffer
	{
		24_KiB
	};

	string_view chunk
	{
		fs::read(fd, buffer)
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
		chunk = fs::read(fd, buffer, offset);
		assert(!empty(chunk));
		written += client.write_all(chunk);
		offset += size(chunk);
		assert(written == offset);
	}

	assert(offset == file_size);
	assert(written == offset);
	return {};
}
catch(const fs::error &e)
{
	throw http::error
	{
		e.code() == std::errc::no_such_file_or_directory?
			http::NOT_FOUND:
			http::INTERNAL_SERVER_ERROR,

		"%s", e.what()
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
