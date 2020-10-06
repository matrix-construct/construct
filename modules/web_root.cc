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

resource::response
non_get_root(client &client, const resource::request &request);

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

resource::method
root_put
{
	root_resource, "PUT", non_get_root
};

resource::method
root_post
{
	root_resource, "POST", non_get_root
};

resource::method
root_delete
{
	root_resource, "DELETE", non_get_root
};

/// LEGACY
conf::item<std::string>
webroot_path
{
	{ "name",       "ircd.webroot.path" },
	{ "default",    ""                  },
};

conf::item<std::string>
root_path
{
	{ "name",       "ircd.web.root.path"      },
	{ "default",    string_view{webroot_path} },
};

conf::item<bool>
root_cache_control_immutable
{
	{ "name",       "ircd.web.root.cache_control.immutable" },
	{ "default",    true                                    },
};

void
init_files()
{
	const string_view &path
	{
		root_path
	};

	if(empty(path))
	{
		log::warning
		{
			"Conf item 'ircd.web.root.path' is empty; not serving static assets."
		};

		return;
	}

	if(!fs::exists(path))
	{
		log::error
		{
			"Configured ircd.webroot.path at `%s' does not exist.", path
		};

		return;
	}

	for(const auto &absolute : fs::ls_r(path))
	{
		// fs::ls_r() gives us full absolute paths on the system, but we need
		// to locate resources relative to webroot. The system path below
		// the configured webroot is stripped here.
		auto relative
		{
			lstrip(absolute, path)
		};

		// The configured webroot is a directory string entered by the admin, it
		// may or may not contain a trailing slash. This the strip above may have
		// left a leading slash on this name; which is bad.
		relative = lstrip(relative, '/');

		// Add the mapping of the relative resource path to the absolute path
		// on the system.
		files.emplace(relative, absolute);
	}

	if(files.empty())
	{
		log::dwarning
		{
			"No files or directories found at `%s'; not serving any static assets."
		};

		return;
	}

	log::info
	{
		"Web root loaded %zu file and directory resources for service under `%s'",
		files.size(),
		path,
	};
}

/// This handler exists because the root resource on path "/" catches
/// everything rejected by all the other registered resources; after that
/// happens if the method was not GET the client always gets a 405 even if
/// the path they specified truly does not exist. This handler allows us to
/// give them a 404 first instead by checking the path's existence; then a 405
/// if it does exist and they did not use GET.
resource::response
non_get_root(client &client,
             const resource::request &request)
{
	const auto &request_head_path
	{
		!request.head.path?
			"index.html":
		request.head.path == "/"?
			"index.html":
			request.head.path
	};

	const auto &path
	{
		lstrip(request_head_path, '/')
	};

	const http::code code
	{
		files.count(path)?
			http::METHOD_NOT_ALLOWED:
			http::NOT_FOUND
	};

	return resource::response
	{
		client, code
	};
}

resource::response
get_root(client &client,
         const resource::request &request)
try
{
	const auto &request_head_path
	{
		!request.head.path?
			"index.html":
		request.head.path == "/"?
			"index.html":
			request.head.path
	};

	const auto &path
	{
		lstrip(request_head_path, '/')
	};

	auto it
	{
		files.find(path)
	};

	if(it == end(files))
		return resource::response
		{
			client, http::NOT_FOUND
		};

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

	static const string_view &cache_control_immutable
	{
		"Cache-Control: public, max-age=31536000, immutable\r\n"_sv
	};

	// Responses from this handler are assumed to be static content by
	// default. Developers or applications with mutable static content
	// can disable the header at runtime with this conf item.
	const string_view &addl_headers
	{
		// Don't add this header for index.html otherwise firefox makes really
		// aggressive assumptions on page-load which are fantastic right until
		// you upgrade Riot and then all hell breaks loose as your client
		// straddles between two versions at the same time.
		path == "index.html"?
			string_view{}:

		// Add header if conf item allows
		root_cache_control_immutable?
			cache_control_immutable:

		// Else don't add header
			string_view{}
	};

	char content_type_buf[64];
	resource::response
	{
		client,
		http::OK,
		content_type(content_type_buf, file_name, chunk),
		file_size,
		addl_headers,
	};

	const unwind_exceptional terminate{[&client]
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
		case hash("wasm"):   content_type = "application/wasm"; break;
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
