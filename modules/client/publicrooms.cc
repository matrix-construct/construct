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

resource publicrooms_resource
{
	"/_matrix/client/r0/publicRooms",
	"Lists the public rooms on the server. "
	"This API returns paginated responses. (7.5)"
};

const ircd::m::room::id::buf
public_room_id
{
    "public", ircd::my_host()
};

m::room public_
{
	public_room_id
};

//TODO: not a vm witness; use event release state update hook
struct witness
:m::vm::witness
{
	int add(m::vm::accumulator *const &a, const m::event &event) override final
	{
		if(json::get<"type"_>(event) != "m.room.join_rules")
			return -1;

		const json::object &content{at<"content"_>(event)};
		const auto &join_rule{content.at("join_rule")};
		return -1;
	}

	int del(m::vm::accumulator *const &a, const m::event &event) override final
	{
		if(json::get<"type"_>(event) != "m.room.join_rules")
			return -1;

		return -1;
	}
};

resource::response
get_publicrooms(client &client, const resource::request &request)
{
	return resource::response
	{
		client, http::OK
	};
}

resource::method post
{
	publicrooms_resource, "GET", get_publicrooms
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/publicrooms' to manage Matrix rooms"
};
