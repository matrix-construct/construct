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

resource::response
get_root(client &client, const resource::request &request)
{
	const auto &path
	{
		!request.head.path? "index.html":
		request.head.path == "/"? "index.html":
		request.head.path
	};

	auto it(files.find(lstrip(path, '/')));
	if(it == end(files))
		throw http::error{http::NOT_FOUND};

	const auto &filename(it->second);
	const std::string content
	{
		ircd::fs::read(filename)
	};

	string_view content_type; switch(hash(rsplit(filename, '.').second))
	{
		case hash("css"):    content_type = "text/css; charset=utf-8";  break;
		case hash("js"):     content_type = "application/javascript; charset=utf-8";  break;
		case hash("html"):   content_type = "text/html; charset=utf-8"; break;
		case hash("ico"):    content_type = "image/x-icon"; break;
		case hash("svg"):    content_type = "image/svg+xml"; break;
		case hash("png"):    content_type = "image/png"; break;
		case hash("woff2"):  content_type = "application/font-woff2"; break;
		case hash("woff"):   content_type = "application/font-woff"; break;
		case hash("eot"):    content_type = "application/vnd.ms-fontobject"; break;
		case hash("otf"):
		case hash("ttf"):    content_type = "application/font-sfnt"; break;
		default:             content_type = "text/plain; charset=utf-8"; break;
	}

	return resource::response
	{
		client, string_view{content}, content_type
	};
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
