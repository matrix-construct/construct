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

mapi::header
IRCD_MODULE
{
	"Identity Service 7 :Key management"
};

m::resource
pubkey_resource
{
	"/_matrix/identity/api/v1/pubkey/",
	{
		"7.1 Get the public key for the passed key ID.",
		resource::DIRECTORY
	}
};

static m::resource::response
handle_get(client &,
           const m::resource::request &);

m::resource::method
method_get
{
	pubkey_resource, "GET", handle_get
};

m::resource::response
handle_get(client &client,
           const m::resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"keyId path parameter required"
		};

	char buf[256];
	const string_view keyid
	{
		url::decode(buf, request.parv[0])
	};

	const string_view algorithm
	{
		split(keyid, ':').first
	};

	const string_view identifier
	{
		split(keyid, ':').second
	};

	const string_view public_key
	{

	};

	return m::resource::response
	{
		client, http::NOT_FOUND, json::members
		{
			{ "public_key", public_key }
		}
	};
}
