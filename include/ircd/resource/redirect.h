// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_RESOURCE_REDIRECT_H

struct ircd::resource::redirect
{
	struct permanent; // 308
};

/// This is a pseudo-resource that listens on `old_path` and responds with
/// a 308 Permanent Redirect. The Location header replaces old_path with
/// new_path, preserving anything in the URI after the replacement. This
/// resource responds to all of the methods listed as instances.
struct ircd::resource::redirect::permanent
:resource
{
	string_view new_path;
	method _options;
	method _trace;
	method _head;
	method _get;
	method _put;
	method _post;
	method _patch;
	method _delete;

	response handler(client &, const request &);

	permanent(const string_view &old_path,
	          const string_view &new_path,
	          struct opts);
};
