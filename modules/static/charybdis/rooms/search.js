/*
 * IRCd Charybdis 5/Matrix
 *
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk (jason@zemos.net)
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
 *
 */

'use strict';

mc.rooms.search = {};

mc.rooms.search.value = "";

mc.rooms.search.clear = function()
{
	delete mc.rooms.search.value;
	delete mc.rooms.search.feedback;
};

mc.rooms.search.modechk = function(value)
{
	if(!empty(value) && mc.rooms.mode != "SEARCH")
	{
		mc.rooms.mode = "SEARCH";
		return true;
	}

	if(empty(value) && mc.rooms.mode == "SEARCH")
	{
		mc.rooms.mode = "";
		return false;
	}

	return true;
};

mc.rooms.search.type = function(value)
{
	if(empty(value))
		return null;

	const any = /^\w+:\w+\.\w+/;
	if(any.test(value))
		return null;

	const id = /^(\!)\w+:\w+\.\w+/;
	if(id.test(value))
		return "id";

	const alias = /^(#)(\w|\#)+:\w+\.\w+/;
	if(alias.test(value))
		return "alias";

	if(valid_domain(value))
		return "server";

	return null;
};

mc.rooms.search.on = {};

mc.rooms.search.on.change = function()
{
	let value = mc.rooms.search.value;
	mc.rooms.search.modechk(value);
	if(mc.rooms.search.on.change.bounce)
	{
		mc.timeout.cancel(mc.rooms.search.on.change.bounce);
		delete mc.rooms.search.on.change.bounce;
	}

	let type = mc.rooms.search.type(value);
	if(!type)
		return;

	mc.rooms.search.feedback = "BY " + type.toUpperCase();
	mc.rooms.search.on.change.bounce = mc.timeout(500, () =>
	{
		mc.rooms.search.commit(mc.rooms.search.value);
		delete mc.rooms.search.on.change.bounce;
	});
};

mc.rooms.search.on.keypress = function($event)
{
	switch($event.which)
	{
		case 13:
			mc.rooms.search.commit(mc.rooms.search.value);
			return;
	}
};

mc.rooms.search.on.blur = function($event)
{
	let val = () => mc.rooms.search.value;
	mc.rooms.search.modechk(val());
};

mc.rooms.search.on.focus = function($event)
{
	let val = () => mc.rooms.search.value;
	mc.rooms.search.modechk(val());
};

mc.rooms.search.commit = async function(value)
{
	let type = mc.rooms.search.type(value);
	if(!type)
		return;

	let handler = mc.rooms.search.by[type];
	if(!handler)
		return;

	if(mc.rooms.search.request)
		mc.rooms.search.request.abort("canceled");

	try
	{
		mc.rooms.list = await handler(value);
		mc.apply();
	}
	catch(error)
	{
		error.element = $("#charybdis_rooms_list");
		throw error;
	}
};

mc.rooms.search.by = {};

mc.rooms.search.by.id = async function(room_id)
{
	let summary = await mc.rooms.search.by.state(room_id, undefined, undefined);
	return [mc.rooms.get(summary)];
};

mc.rooms.search.by.alias = async function(alias)
{
	mc.rooms.search.request = mc.m.directory.get(alias); try
	{
		let summary = await mc.rooms.search.request.response;

		if(!Array.isArray(summary.aliases))
			summary.aliases = [];
		summary.aliases.push(alias);

		return [mc.rooms.get(summary)];
	}
	catch(error)
	{
		if(error.message == "canceled")
			return;

		error.element = $("#charybdis_rooms_list");
		throw error;
	}
	finally
	{
		delete mc.rooms.search.request;
	}
};

mc.rooms.search.by.state = async function(room_id, type = undefined, state_key = undefined)
{
	mc.rooms.search.request = mc.m.rooms.state.get(room_id, type, state_key); try
	{
		let summary = await mc.rooms.search.request.response;
		return [mc.rooms.get(summary)];
	}
	catch(error)
	{
		if(error.message == "canceled")
			return;

		error.element = $("#charybdis_rooms_list");
		throw error;
	}
	finally
	{
		delete mc.rooms.search.request;
	}
};

mc.rooms.search.by.server = async function(domain)
{
/*
	let opts =
	{
		query:
		{
			server: domain,
			limit: 64,
		}
	};

	mc.rooms.mode = "PUBLIC";
	mc.rooms.search.request = mc.rooms.public.fetch(opts); try
	{
		let rooms = await mc.rooms.search.request;
		return rooms;
	}
	catch(error)
	{
		if(error.message == "canceled")
			return;

		error.element = $("#charybdis_rooms_list");
		throw error;
	}
	finally
	{
		delete mc.rooms.search.request;
	}
*/
};
