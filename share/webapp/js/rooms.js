/*
// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.
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
mc.rooms.invite = {};
mc.rooms.join = {};
mc.rooms.leave = {};

mc.rooms.init = function()
{
	mc.rooms.room = mc.rooms.get(
	{
		name: "Rooms",
		room_id: "!rooms:mc",
		topic: "My rooms manager.",
	});

	mc.rooms.room.sync.one(
	{
		type: "mc.menu",
		state_key: "rooms",
	});

	mc.home = mc.rooms.get(
	{
		name: "Home",
		room_id: "!home:mc",
		canonical_alias: "#home:mc",
		topic: "Home Room"
	});

	mc.help = mc.rooms.get(
	{
		name: "Help",
		room_id: "!help:mc",
		topic: "Help Room"
	});

	mc.help.sync.one(
	{
		type: "mc.room.config",
		state_key: "show",
		content:
		{
			frieze: true,
			control: false,
			player: false,
			input: false,
		},
	});

	mc.help.sync.one(
	{
		type: "m.room.message",
		content:
		{
			msgtype: "m.text",
			body: "Matrix Construct Client",
		},
	});

	mc.help.sync.one(
	{
		type: "m.room.message",
		content:
		{
			msgtype: "m.text",
			body: "(C) 2017 Matrix Construct Developers",
		},
	});

	mc.help.sync.one(
	{
		type: "m.room.message",
		content:
		{
			msgtype: "m.text",
			body: "Jason Volk <jason@zemos.net>",
		},
	});

	mc.help.sync.one(
	{
		type: "mc.help.chapter",
		state_key: "introduction",
		content:
		{
		},
	});
};

mc.rooms.set = function(room_id, action, room = undefined)
{
	if((room.id in mc.rooms[action]))
		return;

	if(room_id == "!rooms:mc" || maybe(() => room.id) == "!rooms:mc")
		return;

	let event = new mc.event(
	{
		type: "mc.room",
		state_key: room_id,
		membership: action,
	});

	mc.rooms[action][room.id] = room;
	mc.rooms.room.sync(event, action);
};

mc.rooms.get = function(summary)
{
	if(typeof(summary.opts) != "object")
		summary.opts = {};

	let is_array = Array.isArray(summary);
	let room_id = typeswitch(summary,
	{
		"string": summary,
		"object": () => is_array? summary[0].room_id : summary.room_id,
	});

	let room = mc.rooms[room_id];
	if(room !== undefined && !summary.opts.recreate)
		return room;

	// Ensures the summary is valid when the argument
	// to this function is just a quick room_id string.
	return mc.rooms.create.room(summary);
};

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
		let at = maybe(() => a.timeline.modified);
		let bt = maybe(() => b.timeline.modified);
		if(at && bt)
			return at > bt? -1: at == bt? 0: 1;

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
	let rooms = Object.values(mc.rooms.join);
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
	let rooms = Object.values(mc.rooms.leave);
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

mc.rooms.create = async function(id = undefined, room_opts = {}, request_opts = {})
{
	let request = mc.m.createRoom.post(request_opts); try
	{
		let data = await request.response;
		let room = mc.rooms.create.room(data, room_opts);
		mc.rooms.join[room.id] = room;
		mc.rooms.list[room.id] = room;
		return room;
	}
	catch(error)
	{
		debug.error(error);
		throw error;
	}
};

mc.rooms.create.room = function(summary, opts = {})
{
	Object.update(summary,
	{
		opts: opts,
	});

	let room = new mc.room(summary);
	if(room.opts.local === true)
		mc.rooms.set(room.id, "join", room);

	if(room.opts.register !== false)
		mc.rooms[room.id] = room;

	return room;
};

mc.rooms.current = function(room_id)
{
	let index = mc.rooms.current.index(room_id);
	return mc.rooms.current.list[index];
};

// Currently running rooms by ID
mc.rooms.current.list = [];

mc.rooms.current.empty = function()
{
	return mc.rooms.current.list.length == 0;
};

mc.rooms.current.index = function(room_id)
{
	return mc.rooms.current.list.findIndex((room) => room.id == room_id);
};

mc.rooms.current.clear = function()
{
	mc.rooms.current.list = [];
};

// Multi-use function called whenever a room should be displayed
// which also makes it the currently focused room div.
mc.rooms.current.add = function(room_id)
{
	let room = mc.rooms.current(room_id);
	if(room !== undefined)
		return;

	room = mc.rooms[room_id];
	if(room === undefined)
		return;

	while(mc.rooms.current.list.length >= mc.rooms.current.max())
		mc.rooms.current.del.oldest();

	mc.rooms.current.list.push(room);
	mc.session.rooms.history.unshift(room.id);
	room.focus();
	return room;
};

// Multi-use function called whenever a room should be displayed
// which also makes it the currently focused room div.
mc.rooms.current.del = function(room_id)
{
	let idx = mc.rooms.current.index(room_id);
	if(idx >= 0)
	{
		let room = mc.rooms.current.list.splice(idx, 1)[0];
		room.focus.defocus();
	}

	if(Array.isArray(maybe(() => mc.session.rooms.history)))
	{
		let idx = mc.session.rooms.history.indexOf(room_id);
		if(idx >= 0)
			mc.session.rooms.history.splice(idx, 1);
	}
};

mc.rooms.current.del.oldest = function()
{
	let sorted = mc.rooms.current.list.sort((a, b) =>
	{
		return a.focus.last < b.focus.last? -1:
		       a.focus.last > b.focus.last? 1:
		                                    0;
	});

	let room = sorted[0];
	return room? mc.rooms.current.del(room.id) : undefined;
};

mc.rooms.current.max = function()
{
	let ret = maybe(() => mc.opts.rooms.current.max);
	return ret? ret : 1;
};

mc.rooms.info = function(room_id, $event)
{
	if(mc.rooms.info.model == room_id)
	{
		mc.rooms.mode = "";
		return;
	}

	if(!mc.rooms.current(room_id))
		return;

	mc.rooms.info.model = room_id;
	mc.rooms.mode = "INFO";
};

mc.rooms.info.clear = function()
{
	delete mc.rooms.info.model;
};

mc.rooms.info.menu =
{
	"JOIN":
	{
		icon: "fa-thumbs-up",
		classes: { join: true, },
		hide: (room_id) => mc.rooms.join[room_id] !== undefined,
	},

	"KNOCK":
	{
		icon: "fa-hand-rock-o",
		classes: { knock: true, disabled: true, },
		hide: (room_id) => mc.rooms.join[room_id] !== undefined,
	},

	"HOLD TO LEAVE":
	{
		icon: "fa-thumbs-down",
		classes: { leave: true, },
		hide: (room_id) => mc.rooms.join[room_id] === undefined,
		holds: 1000,
	},
};

mc.rooms.info.menu["JOIN"].click = async function($event, room_id)
{
	if(!room_id)
		room_id = mc.rooms.search.value.split(" ")[0];

	if(!(room_id in mc.rooms))
		mc.rooms[room_id] = new mc.room({room_id: room_id});

	let room = mc.rooms[room_id];

	// Try to join room by canon alias first
	let alias = room.state['m.room.canonical_alias'].content.alias;

	// Fallback to any alias
	if(empty(alias)) try
	{
		let aliases = room.state['m.room.aliases'];
		let state_key = Object.keys(aliases)[0];
		alias = aliases[state_key].content.aliases[0];
	}
	catch(e) {}

	// Fallback to room id
	if(empty(alias))
		alias = room_id;

	let request = mc.m.join.post(alias); try
	{
		let data = await request.response;
		mc.rooms.join[room_id] = room;
		debug.object(data);
		return true;
	}
	catch(error)
	{
		error.element = $event.delegateTarget;
		throw error;
	}
};

mc.rooms.info.menu["HOLD TO LEAVE"].click = async function($event, room_id)
{
	let request = mc.m.rooms.leave.post(room_id,); try
	{
		let data = await request.response;
		debug.object(data);

		if(mc.rooms.current(room_id))
			mc.rooms.current.del(room_id);

		delete mc.rooms.join[room_id];
		return true;
	}
	catch(error)
	{
		error.element = $event.delegateTarget;
		throw error;
	}
};

mc.ng.app.controller('rooms.list', class extends mc.ng.controller
{
	constructor($scope)
	{
		super("rooms.list", $scope);
		this.loading = true;
	}

	constructed()
	{
		this.loading = false;
	}

	destructor()
	{
	}

	scroll(event)
	{
		let e = $(event.delegateTarget);
		if(!this.edge && mc.scroll.at.top(e))
		{
			this.edge = "top";
			return this.scroll_up();
		}

		if(!this.edge && mc.scroll.at.bottom(e))
		{
			this.edge = "bottom";
			return this.scroll_down();
		}

		// By setting this.edge we prevent repeat calls to the async scroll functions
		// which will result in a digest loop even if no IO is done.
		this.edge = null;
	}

	async scroll_up()
	{
	}

	async scroll_down()
	{
		await fetch();
	}

	async fetch()
	{
		if(!maybe(() => mc.rooms.list.opts))
			return;

		let opts = mc.rooms.list.opts;
		delete mc.rooms.list.opts;
		let rooms = await mc.rooms.public.fetch(opts);
		mc.rooms.list = mc.rooms.list.concat(rooms.map(mc.rooms.get));
		mc.rooms.list.opts = opts;
	}
});

mc.rooms.public = {};

mc.rooms.menu["PUBLIC"] =
{
	icon: "fa-rss",
	selected: () => mc.rooms.mode == "PUBLIC",
	choice: () => mc.rooms.search.feedback == "BY SERVER",
	order: 5,
};

mc.rooms.menu["GLOBAL"] =
{
	order: 6,
	icon: "fa-globe",
	selected: () => mc.rooms.mode == "GLOBAL",
	choice: () => mc.rooms.search.feedback == "BY SERVER",
};

mc.rooms.menu["GLOBAL"].click = async function($event)
{
	let opts =
	{
		query:
		{
			server: mc.rooms.search.value,
		},

		content:
		{
			include_all_networks: true,
			third_party_instance_id: undefined,
		},
	};

	mc.rooms.mode = "GLOBAL";
	return mc.rooms.public.click($event, opts);
};

mc.rooms.menu["PUBLIC"].click = async function($event)
{
	let opts =
	{
		query:
		{
			server: mc.rooms.search.value,
		},
	};

	mc.rooms.mode = "PUBLIC";
	return mc.rooms.public.click($event, opts);
};

mc.rooms.public.click = async function($event, opts = {})
{
	mc.rooms.loading = true;
	let rooms = await mc.rooms.public.fetch(opts);
	mc.rooms.list = rooms.map(mc.rooms.get);
	mc.rooms.list.opts = opts;
	mc.rooms.loading = false;
};

/** IO to fetch public rooms.
 * The results come in chunks of an array. One chunk is returned from this
 * function per call. To receieve additional chunks of the same result, call
 * this function again with the same ctx.
 */
mc.rooms.public.fetch = async function(ctx = {})
{
	Object.defaults(ctx,
	{
		estimate: 0,
		count: 0,
		query:
		{
			limit: 64,
		},
	});

	let request = mc.m.publicRooms.get(ctx);
	let data = await request.response;
	let rooms = data.chunk;

	ctx.estimate = data.total_room_count_estimate;
	ctx.query.since = data.next_batch;
	ctx.count += rooms.length;

	return rooms;
};

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
	let tests = mc.rooms.search.type.tests;

	if(empty(value))
		return null;

	// Short circuit optimization on the widest test first.
	for(let type in tests)
		if(tests[type].test(value))
			return type;

	return null;
};

mc.rooms.search.type.tests =
{
	id:       /^\!(\w|\-)+\:(\w|\-)+\.\w+/,
	alias:    /^\#(\w|\-|\#)+\:(\w|\-)+\.\w+/,
	unknown:  /^(\w|\-)+\:(\w|\-)+\.\w+/,
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
	if(typeof(handler) != "function")
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
	let event_id = room_id.split(' ')[1];
	room_id = room_id.split(' ')[0];

	return [await mc.rooms.search.by.state(room_id, undefined, undefined, event_id)];
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

		mc.rooms.search.feedback = "ERROR: " + error.name;
		mc.apply.later();
		return;
		//error.element = $("#charybdis_rooms_list");
		//throw error;
	}
	finally
	{
		delete mc.rooms.search.request;
	}
};

mc.rooms.search.by.state = async function(room_id, type = undefined, state_key = undefined, event_id = undefined)
{
	let opts =
	{
		query:
		{
			event_id: event_id
		},
	};

	mc.rooms.search.request = mc.m.rooms.state.get(room_id, type, state_key, opts); try
	{
		let summary = await mc.rooms.search.request.response;
		return mc.rooms.get(summary);
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
