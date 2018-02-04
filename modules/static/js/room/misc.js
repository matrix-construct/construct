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

room.prototype.realias = function($event, server, index)
{
	let event = this.state['m.room.aliases'][server];
	let pending = this.pending['m.room.aliases'][server];
	let alias = pending.content.aliases[index];
	delete pending.content.aliases;

	if(alias.length)
	{
		if(!alias.startsWith("#"))
			alias = "#" + alias;

		if(!client.m.domid(alias))
			alias += ":" + client.my.domain;

		pending.content.aliases = [alias,];
	}

	if(event.content !== undefined && event.content.aliases !== undefined)
		for(let i = 0; i < event.content.aliases.length; i++)
			if(i != index)
				pending.content.aliases.push(event.content.aliases[i]);

	return this.restate($event, "m.room.aliases", server);
};

/**************************************
 * Misc interface
 *
 * Room functions below here are not yet categorized in a sub-object
 */

room.prototype.query = function(event, name)
{

};

room.prototype.kick = function(event, name)
{
	if(this.opts.local)
		return;

	let reason = name;
};

room.prototype.ban = function(event, name)
{
	if(this.opts.local)
		return;

	let reason = "Thus do I counsel you, my friends: distrust all in whom the impulse to punish is powerful";
};

room.prototype._invite_autocomplete = function(request, response)
{
	let max = 30;
	let ids = []; //client.users.find(request.term);
	ids.slice(0, max);
	response(ids);
};

room.prototype.invite = function(event)
{
	if(this.opts.local)
		return;

	let form = event.delegateTarget;
	let user = $(form).find("input").val();
	let ids = [];//client.users.find(user);
	if(ids.length != 1)
		return;

	let id = ids[0];
	/*
	client.client.invite(this.id, id, (error, data)
	{
		if(error)
			return client.unhandled(error);

		debug.object({invite: data});
	});
	*/
};

room.prototype.tab_completion_find = function(term)
{
	let ids = [];
	for(let key in this.state['m.room.member'])
	{
		let sid = client.m.sid(key.toLowerCase());
		if(sid.startsWith(term.toLowerCase()))
			ids.push([sid, key]);
	}

	return ids;
};

room.prototype.tab_completion = function(e)
{
	let term = $(e).val();
	let ids = this.tab_completion_find(term);
	if(ids.length == 0)
		return;

	// Once a single id is left we can present the MXID to user
	if(ids.length == 1)
	{
		$(e).val(ids[0][1] + ": ");
		return;
	}

	// Best common string of multiple matching ID's is presented for further invocations
	let common = ids[0][0];
	for(let i = 1; i < ids.length; i++)
	{
		common = common.slice(0, ids[i][0].length);
		for(let j = 0; j < common.length && j < ids[i][0].length; j++)
			if(common[j] != ids[i][0][j])
				common = common.slice(0, j);
	}

	$(e).val(common);
};

room.prototype.console_clear = function()
{
	let filter = (event) =>
		event.sender != "@console:mc";

	//this.timeline = this.timeline.filter(filter);
};

room.prototype.console_push = function(text)
{
	this.timeline.insert(new client.event(
	{
		type: "m.room.message",
		sender: "@console:mc",
		event_id: client.m.random.mxid("$", "mc"),
		origin_server_ts: (new Date).getTime(),
		content:
		{
			msgtype: "m.notice",
			body: text,
		},
	}));

	if(this.focused() && this.control.mode == "LIVE")
		this.scroll.to.bottom();
};

room.prototype.search = function($event)
{
	// Searchbox input
	let query = this.control.search_query;

	// Call this to destroy results and return
	// the room back to normal from search mode
	let clear = () =>
		this.control.content.event_id = {};

	//
	// Conditions for exiting search mode
	//

	if(!query)
		return clear();

	if(query.length == 0)
		return clear();

	if(!this.control.show_search)
		return clear();


	//
	// Search mode
	//

	let re = new RegExp(query);

	// Search for an event
	if(client.m.valid.mxid(query, "event"))
	{
		this.control.content.event_id[query] = true;
		return;
	}

	// Search for a user
	if(client.m.valid.mxid(query, "user", { valid_domain: false }))
	{
		this.timeline.each((event) =>
		{
			let matching = re.test(event.sender);
			this.control.content.event_id[event.event_id] = matching;
		});

		return;
	}

	this.timeline.each((event) =>
	{
		for(let key in event)
		{
			if(typeof(event[key]) != "string")
				continue;

			let matching = re.test(event[key]);
			this.control.content.event_id[event.event_id] = matching;
			if(matching)
				break;
		}
	});
};

room.prototype.seek = function(event_id)
{
	this.control.show_event_timeline = true;
	this.control.show_event_info[event_id] = true;
};

Object.defineProperty(room.prototype, 'event_menu', {
writable: false,
enumerable: false,
configurable: false,
value:
{
	"REDACT":
	{
		icon: "fa-gavel",
		click: (event, room) =>
		{
		},
	},

	"MARK AS READ":
	{
		icon: "fa-envelope-open",
		click: (event, room) =>
		{
		},
	},

	"RECEIPTS":
	{
		icon: "fa-users",
		click: (event, room) =>
		{
		},
	},

	"HIDE":
	{
		icon: "fa-close",
		click: (event, room) =>
		{
		},
	},
}});

Object.defineProperty(room.prototype, 'timeline_player', {
writable: false,
enumerable: false,
configurable: false,
value:
{
	"STOP":
	{
		icon: "fa-stop",
		click: (event, room) =>
		{
			room.control.mode = "STOP";
		},
	},

	"PREV":
	{
		icon: "fa-step-backward",
		click: (event, room) =>
		{
			room.control.mode = "STOP";
			room.timeline.pop();
		},
	},

	"NEXT":
	{
		icon: "fa-step-forward",
		click: (event, room) =>
		{
			room.control.mode = "STOP";
		},
	},

	"PAST":
	{
		icon: "fa-pause-circle",
		click: (event, room) =>
		{
			room.control.mode = "PAST";
		},
	},

	"LIVE":
	{
		icon: "fa-play-circle",
		click: (event, room) =>
		{
			room.control.mode = "LIVE";
			room.scroll.to.event(room.current_event_id());
		},
	},
}});

Object.defineProperty(room.prototype, 'detail_scrollback', {
writable: false,
enumerable: false,
configurable: false,
value:
{
	"joined":
	{
		icon: "fa-lock",
	},

	"invited":
	{
		icon: "fa-handshake-o",
	},

	"shared":
	{
		icon: "fa-feed",
	},

	"world_readable":
	{
		icon: "fa-globe",
		name: "PUBLIC"
	},
}});

Object.defineProperty(room.prototype, 'detail_join_rules', {
writable: false,
enumerable: false,
configurable: false,
value:
{
	"private":
	{
		icon: "fa-lock",
	},

	"invite":
	{
		icon: "fa-handshake-o",
	},

	"knock":
	{
		icon: "fa-hand-rock-o",
	},

	"public":
	{
		icon: "fa-globe",
	},
}});

Object.defineProperty(room.prototype, 'detail_guest_access', {
writable: false,
enumerable: false,
configurable: false,
value:
{
	"forbidden":
	{
		icon: "fa-lock",
	},

	"can_join":
	{
		icon: "fa-globe",
	},
}});

Object.defineProperty(room.prototype, 'detail_feedback', {
writable: false,
enumerable: false,
configurable: false,
value:
{
	"DELIVERED":
	{
		icon: "fa-envelope"
	},

	"READ":
	{
		icon: "fa-envelope-open"
	},
}});

Object.defineProperty(room.prototype, 'detail_presence', {
writable: false,
enumerable: false,
configurable: false,
value:
{
	"ONLINE":
	{
		icon: "fa-eye"
	},

	"UNAVAILABLE":
	{
		icon: "fa-bed"
	},

	"OFFLINE":
	{
		icon: "fa-sign-out"
	},
}});

Object.defineProperty(room.prototype, 'detail_order', {
writable: false,
enumerable: false,
configurable: false,
value:
{
	"RECENT":
	{
	},

	"RANK":
	{
	},
}});

Object.defineProperty(room.prototype, 'search_menu', {
writable: false,
enumerable: false,
configurable: false,
value:
{
	"SERVER SIDE SEARCH":
	{
		icon: "fa-download",
		selected: true,
	},

	"HIDE NON-MATCHING":
	{
		icon: "fa-eye-slash",
		selected: false,
	},
}});

/** TODO: very temp
 */
client.embed = function(url)
{
	let ret = url.replace("https://www.youtube.com/watch?v=", "https://www.youtube.com/embed/");
	return ret;
}
