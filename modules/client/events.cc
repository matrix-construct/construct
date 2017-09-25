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

resource events_resource
{
	"_matrix/client/r0/events",
	"Events (6.2.3) (10.x)"
};

const m::id::room::buf accounts_room_id
{
	"accounts", "cdc.z"
};

const m::id::room::buf locops_room_id
{
	"locops", "cdc.z"
};

const m::id::room::buf ircd_room_id
{
	"ircd", "cdc.z"
};

resource::response
get_events(client &client, const resource::request &request)
{
	m::room room
	{
		m::id::room
		{
			unquote(request.at("room_id"))
		}
	};

	const m::room::events events
	{
		room
	};

	size_t i(0);
	events.for_each([&i](const auto &event)
	{
		++i;
	});

	size_t j(0);
	json::value ret[i];
	events.for_each([&i, &j, &ret](const m::event &event)
	{
		if(j < i)
			ret[j++] = event;
	});

	return resource::response
	{
		client, json::members
		{
			{ "chunk", { ret, j } }
		}
	};
}

resource::method method_get
{
	events_resource, "GET", get_events
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/events' and hosts the events database"
};
