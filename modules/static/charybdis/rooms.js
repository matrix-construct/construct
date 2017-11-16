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

/**
 ******************************************************************************
 * Rooms
 *
 */
mc.rooms = {};

if(!mc.session.rooms)
	mc.session.rooms = {};

if(!mc.session.rooms.history)
	mc.session.rooms.history = [];

// Collection (room_id => mc.room)
mc.rooms.joined = {};

// Collection (room_id => mc.room)
mc.rooms.invited = {};

// Collection (room_id => mc.room)
mc.rooms.left = {};

// Primary rooms mode
Object.defineProperty(mc.rooms, 'mode',
{
	get: function()
	{
		return this.current_mode;
	},

	set: function(current_mode)
	{
		if(this.current_mode == "INFO" && current_mode != "INFO")
			mc.rooms.info.clear();

		if(this.current_mode == "SEARCH")
			if(current_mode != "SEARCH" && current_mode != "PUBLIC" && current_mode != "GLOBAL")
				mc.rooms.search.clear();

		if(empty(current_mode))
		{
			this.current_mode = this.previous_mode;
			this.previous_mode = undefined;
		} else {
			this.previous_mode = this.current_mode;
			this.current_mode = current_mode;
		}

		return true;
	}
});

mc.rooms.sort = {};

mc.rooms.sort.focused = function(rooms)
{
	return rooms.sort((a, b) =>
	{
		let af = maybe(() => a.focus.last);
		let bf = maybe(() => b.focus.last);
		return af > bf? -1: af == bf? 0: 1;
	});
};

mc.rooms.menu = {};

mc.rooms.menu["JOINED"] =
{
	order: 1,
	icon: "fa-thumbs-up",
	selected: () => mc.rooms.mode == "JOINED",
	tooltip: "List the rooms you have joined.",
};

mc.rooms.menu["JOINED"].click = function($event)
{
	let rooms = Object.values(mc.rooms.joined);
	mc.rooms.list = mc.rooms.sort.focused(rooms);
	mc.rooms.mode = "JOINED";
};

mc.rooms.menu["LEFT"] =
{
	order: 2,
	icon: "fa-thumbs-down",
	selected: () => mc.rooms.mode == "LEFT",
	tooltip: "List the rooms from which you have parted company.",
};

mc.rooms.menu["LEFT"].click = function($event)
{
	let rooms = Object.values(mc.rooms.left);
	mc.rooms.list = mc.rooms.sort.focused(rooms);
	mc.rooms.mode = "LEFT";
};

mc.rooms.menu["FEED"] =
{
	order: 3,
	icon: "fa-newspaper-o",
	selected: () => mc.rooms.mode == "FEED",
};

mc.rooms.menu["CREATE"] =
{
	order: 4,
	icon: "fa-lightbulb-o",
	selected: () => mc.rooms.mode == "CREATE",
};

mc.rooms.menu["CREATE"].click = async function($event)
{
	mc.rooms.mode = "CREATE";

	let opts =
	{
		element: $event.delegateTarget,
	};

	let room = await mc.rooms.create(undefined, undefined, opts);
	mc.rooms.current.add(room.id);
	mc.rooms.mode = "";
};

mc.rooms.focus = function(room_id, $event)
{
	let room = this.joined[room_id];
	return room? room.focus() : false;
};

mc.rooms.get = function(summary, opts = {})
{
	let room_id = typeswitch(summary,
	{
		"object": summary.room_id,
		"string": summary,
	});

	let room = mc.rooms[room_id];
	if(room !== undefined && !opts.recreate)
		return room;

	// Ensures the summary is valid when the argument
	// to this function is just a quick room_id string.
	summary = typeof(summary) == "object"? summary:
	{
		room_id: room_id,
	};

	return mc.rooms.create.room(summary, opts);
};
