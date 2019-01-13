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
	"Federation 2.2.1.1 :Publishing Keys"
};

resource
server_resource
{
	"/_matrix/key/v2/server/",
	{
		"federation 2.2.1.1: Publishing Keys",
		resource::DIRECTORY,
	}
};

resource::response
handle_get(client &client,
           const resource::request &request)
{
	char key_id_buf[256];
	const auto key_id
	{
		url::decode(key_id_buf, request.parv[0])
	};

	m::keys::get(my_host(), key_id, [&client]
	(const json::object &keys)
	{
		resource::response
		{
			client, http::OK, keys
		};
	});

	return {};
}

resource::method
method_get
{
	server_resource, "GET", handle_get
};

#ifdef RB_DEBUG
__attribute__((constructor))
#endif
static void
_test_ed25519_()
noexcept
{
	using namespace ircd;

	char seed_buf[ed25519::SEED_SZ + 10];
	const auto seed
	{
		b64decode(seed_buf, "YJDBA9Xnr2sVqXD9Vj7XVUnmFZcZrlw8Md7kMW+3XA1")
	};

	ed25519::pk pk;
	ed25519::sk sk{&pk, seed};

	const auto SERVER_NAME {"domain"};
	const auto KEY_ID {"ed25519:1"};

	const auto test{[&]
	(const std::string &object) -> bool
	{
		const auto sig
		{
			sk.sign(const_buffer{object})
		};

		char sigb64_buf[128];
		const auto sigb64
		{
			b64encode_unpadded(sigb64_buf, sig)
		};

		ed25519::sig unsig; const auto unsigb64
		{
			b64decode(unsig, sigb64)
		};

		return pk.verify(const_buffer{object}, unsig);
	}};

	const bool tests[]
	{
		test(std::string{json::object
		{
			"{}"
		}}),

		test(json::strung(json::members
		{
			{ "one", 1L },
			{ "two", "Two" }
		})),
	};

	if(!std::all_of(begin(tests), end(tests), [](const bool &b) { return b; }))
		throw ircd::panic
		{
			"Seeded ed25519 test failed"
		};
}
