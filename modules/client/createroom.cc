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

resource createroom
{
	"/_matrix/client/r0/createRoom",
	"Create a new room with various configuration options. (7.1.1)"
};

resource::response
room_create(client &client, const resource::request &request)
try
{
	const auto name
	{
		unquote(request["name"])
	};

	const auto visibility
	{
		unquote(request["visibility"])
	};

	const m::id::user sender_id
	{
		request.user_id
	};

	const m::id::room::buf room_id
	{
		m::id::generate, my_host()
	};

	json::iov event;
	json::iov content;
	const json::iov::push push[]
	{
		{ event,    { "sender",   sender_id  }},
		{ content,  { "creator",  sender_id  }},
	};

	m::room room
	{
		room_id
	};

	room.create(event, content);

	return resource::response
	{
		client, http::CREATED,
		{
			{ "room_id", string_view{room_id} }
		}
	};
}
catch(const db::not_found &e)
{
	throw m::error
	{
		http::CONFLICT, "M_ROOM_IN_USE", "The desired room name is in use."
	};

	throw m::error
	{
		http::CONFLICT, "M_ROOM_ALIAS_IN_USE", "An alias of the desired room is in use."
	};
}

resource::method post_method
{
	createroom, "POST", room_create,
	{
		post_method.REQUIRES_AUTH
	}
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/createRoom' to handle requests"
};
