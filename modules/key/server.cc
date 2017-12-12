/*
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

using namespace ircd;

mapi::header IRCD_MODULE
{
	"federation 2.2.1.1: Publishing Keys"
};

struct server
:resource
{
	using resource::resource;
}
server_resource
{
	"/_matrix/key/v2/server/", resource::opts
	{
		resource::DIRECTORY,
		"federation 2.2.1.1: Publishing Keys"
	}
};

resource::response
handle_get(client &client,
           const resource::request &request)
{
	char key_id_buf[256];
	const auto key_id
	{
		url::decode(request.parv[0], key_id_buf)
	};

	std::string my_key;
	m::keys::get(my_host(), key_id, [&my_key](const auto &key)
	{
		my_key = json::strung(key);
	});

	return resource::response
	{
		client, json::object{my_key}
	};
}

resource::method method_get
{
	server_resource, "GET", handle_get
};

// __attribute__((constructor))
static
void foop()
{
	using namespace ircd;

	uint8_t seed_buf[ed25519::SEED_SZ + 10];
	const auto seed
	{
		b64decode(seed_buf, "YJDBA9Xnr2sVqXD9Vj7XVUnmFZcZrlw8Md7kMW+3XA1")
	};

	ed25519::pk pk;
	ed25519::sk sk{&pk, seed};

	const auto SERVER_NAME {"domain"};
	const auto KEY_ID {"ed25519:1"};

	const auto test{[&](const std::string &object)
	{
		const auto sig
		{
			sk.sign(const_raw_buffer{object})
		};

		char sigb64_buf[128];
		const auto sigb64
		{
			b64encode_unpadded(sigb64_buf, sig)
		};

		std::cout << "sig: " << sigb64 << std::endl;

		ed25519::sig unsig; const auto unsigb64
		{
			b64decode(unsig, sigb64)
		};

		return pk.verify(const_raw_buffer{object}, unsig);
	}};

	std::cout <<
	test(std::string{json::object
	{
		"{}"
	}})
	<< std::endl;

	std::cout <<
	test(json::strung(json::members
	{
		{ "one", 1 },
		{ "two", "Two" }
	}))
	<< std::endl;
}
