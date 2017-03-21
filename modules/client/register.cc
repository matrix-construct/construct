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

db::handle accounts
{
	"accounts"
};

resource::response
handle_post(client &client,
            resource::request &request)
{
	const auto access_token("abcdef");
	const auto home_server("cdc.z");
	const auto user_id("foo@bar");
	const auto refresh_token(12345);

	return resource::response
	{
		client,
		{
			{ "access_token",    access_token   },
			{ "home_server",     home_server    },
			{ "user_id",         user_id        },
			{ "refresh_token",   refresh_token  },
		}
	};
}

resource register_resource
{
	"/_matrix/client/r0/register",
	"Register for an account on this homeserver. (3.3.1)"
};

resource::method post
{
	register_resource, "POST", handle_post
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/register' to handle requests"
};
