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
 *******************************************************************************
 * State Function
 *
 * Dictionary of events considered "state events." The state provides context for the
 * event horizon of a timeline (the termination point at the start) to prevent the
 * need for querying indeterminately over the horizon.
 *
 * This class allows the timeline to be the single source of
 * authority for events in the room. So long as the timeline is
 * maintained in order, the state object will always provide
 * an accurate result as to the state of a room. This proxy
 * prevents any race between the timeline and the state object.
 */
room.state = class
{
	constructor(timeline)
	{
		// The state function holds an internal reference to the timeline to make queries
		this.timeline = timeline;

		// The results of queries to the timeline are cached in the backing-object here.
		this.cache = Object.copy(room.state.defaults);

		// Cache timestamps (type => timestamp) is used to find out if the cache data is stale with
		// respect to when the timeline was last modified. This is partitioned by type even though
		// the timeline only has one TS value for now so all types are invalidated at the same time.
		this.ts = {};

		this.content = new room.state.content(this);
	}
};

room.state.prototype.set = function(state, type, val)
{
	mc.abort({message: "illegal"});
	return false;
};

room.state.prototype.get = function(state, type)
{
	if(this.valid(type))
		if((type in state))
			return state[type];

	let query = mc.event.is_type.bind(null, type);
	let idx = this.timeline.query(query);
	let event = this.timeline[idx[0]];
	if(!event || event.state_key === undefined)
		return state[type];

	this.update(type);
	return this._get_aggreg(state, type, idx);
};

room.state.prototype._get_aggreg = function(state, type, idx)
{
	if(typeof(state[type]) != "object")
		state[type] = {};

	// Ensure true chronological order here so events that happen
	// after other events can simply overwrite them in this iteration.
	let present = this.timeline[0];
	idx.reverse().forEach((i) =>
	{
		let event = this.timeline[i];
		state[type][event.state_key] = event;
	});

	return state[type];
};

/** Determine if the state function has an up to date cache.
 */
room.state.prototype.valid = function(type)
{
	let time = this.ts[type];
	return time && time >= this.timeline.modified;
};

/** Updates the cache timestamp for type
 */
room.state.prototype.update = function(type)
{
	return this.ts[type] = mc.now();
};

/** Clears the cache for type
 *
 * After this, access to state will require new queries into the timeline.
 */
room.state.prototype.invalidate = function(type)
{
	Object.clear(this.cache[type]);
	Object.defaults(this.cache[type], room.state.defaults[type]);
	this.ts[type] = 0;
};

/**
 **************************************
 *
 * State content tree
 *
 */
room.state.content = class
{
	constructor(state, depth = 0)
	{
		this.state = state;
	}
};

room.state.content.prototype.set = function(state, key, val)
{
	mc.abort({message: "unimplemented"});
	return false;
};

room.state.content.prototype.get = function(state, key)
{
	let ret = {};
	for(let state_key in state[key])
		ret[state_key] = state[key][state_key].content;

	return ret;
};

Object.defineProperty(room.state, 'defaults', {
writable: false,
enumerable: false,
configurable: false,
value:
{
	// 10.5.1: m.room.aliases - state_key'ed dictionary of aliases
	"m.room.aliases":
	{
		// "server_hostname": content.aliases
	},

	// 10.5.2: m.room.canonical_alias - main event
	"m.room.canonical_alias":
	{
		"":
		{
			content:
			{
				alias: undefined,
			},
		},
	},

	// 10.5.3: m.room.create - main event
	"m.room.create":
	{
		"":
		{
			content:
			{
				creator: undefined,
			},
		},
	},

	// 10.5.4: m.room.join_rules - main event
	"m.room.join_rules":
	{
		"":
		{
			content:
			{
				join_rule: undefined,
			},
		},
	},

	// 10.5.5: m.room.member - state_key'ed dictionary of members
	"m.room.member":
	{
		// "mxid": { membership: "join", displayname: "foo" },
	},

	// 10.5.6: m.room.power_levels - main event
	"m.room.power_levels":
	{
		"":
		{
			content:
			{
				// Defaults
				events_default: 0,
				state_default: 0,
				users_default: 0,

				// Actions (10.5.6: defaults to 50 if unspecified)
				ban: 50,
				invite: 50,
				kick: 50,
				redact: 50,

				// Access maps
				events: {},           // Power level required for event type; { "m.room.type": power }
				users: {},            // Power level available to user; { mxid: power }
			},
		},
	},

	// 11.2.1.3: m.room.name - main event
	"m.room.name":
	{
		"":
		{
			content:
			{
				name: undefined,
			},
		},
	},

	// 11.2.1.4: m.room.topic - main event
	"m.room.topic":
	{
		"":
		{
			content:
			{
				topic: undefined,
			},
		},
	},

	// 11.2.1.5: m.room.avatar - main event
	"m.room.avatar":
	{
		"":
		{
			content:
			{
				info: {},
				url: undefined,
			},
		},
	},

	// 11.9.1.1: m.room.history_visibility - main event
	"m.room.history_visibility":
	{
		"":
		{
			content:
			{
				history_visibility: "shared", // 11.9.3: defaults to 'shared'
			},
		},
	},

	// 11.13.1.1: m.room.guest_access - main event
	"m.room.guest_access":
	{
		"":
		{
			content:
			{
				guest_access: "forbidden",
			}
		},
	},
}});


/**
 * State -> Summary
 *
 * Digests a room's state into the format of the matrix rooms
 * directory (i.e. public state).
 *
 * Usage: room.state.summary(myroom.state)
 *
 * This is a static function, as the room's state object proxies the
 * timeline and state.public() will then query the timeline.
 */
room.state.summary = function(state)
{
	let members = state['m.room.member'];
	let is_join = (mxid) => maybe(() => members[mxid].content.membership == "join");

	let aliases = [];
	Object.each(state['m.room.aliases'], (state_key, event) =>
	{
		aliases = aliases.concat(event.content.aliases);
	});

	return {
		room_id: state['m.room.create'][''].room_id,
		name: state['m.room.name'][''].content.name,
		topic: state['m.room.topic'][''].content.topic,
		canonical_alias: state['m.room.canonical_alias'][''].content.alias,
		world_readable: state['m.room.history_visibility'][''].content.history_visibility == "world_readable",
		guest_can_join: state['m.room.guest_access'][''].content.guest_access == "can_join",
		num_joined_members: Array.count(Object.keys(members), is_join),
		aliases: aliases,
	};
};

/** Summary -> State (events)
 *
 * Translate summary data into an array of events. The events will not
 * have authentic data other than the essential content but we specify
 * an origin_server_ts of 0 indicating they should be replaced by any
 * timeline algorithm.
 */
room.state.summary.parse = function(summary)
{
	let events = [];
	Object.each(summary, (key, val) =>
	{
		let handler = room.state.summary.parse.on[key];
		if(typeof(handler) == "function") try
		{
			let ret = handler(val);
			if(typeof(ret) != "object")
				return;

			if(Array.isArray(ret))
				events = events.concat(ret);
			else
				events.push(ret);
		}
		catch(error)
		{
			console.error("Room summary parse error: key[" + key + "] " + error);
		}
		else
		{
			console.warn("Unhandled room summary key [" + key + "]");
		}
	});

	return events;
};

// Collection of handlers.
room.state.summary.parse.on = {};

// 'opts' is not part of the matrix room summary;
// this stub prevents a console warning for it being unhandled.
room.state.summary.parse.on["opts"] = function()
{
	return;
};

room.state.summary.parse.on["room_id"] = function(room_id)
{
	return;
};

room.state.summary.parse.on["name"] = function(name)
{
	return {
		type: "m.room.name",
		origin_server_ts: 0,
		state_key: "",
		content:
		{
			name: name,
		},
	};
};

room.state.summary.parse.on["topic"] = function(topic)
{
	return {
		type: "m.room.topic",
		origin_server_ts: 0,
		state_key: "",
		content:
		{
			topic: topic,
		}
	};
};

room.state.summary.parse.on["canonical_alias"] = function(canonical_alias)
{
	return {
		type: "m.room.canonical_alias",
		origin_server_ts: 0,
		state_key: "",
		content:
		{
			alias: canonical_alias,
		}
	};
};

room.state.summary.parse.on["aliases"] = function(aliases)
{
	let contents = {};
	aliases.forEach((alias) =>
	{
		let [local, domain] = alias.split(":");
		if(!(domain in contents))
			contents[domain] = [];

		contents[domain].push(alias);
	});

	let events = [];
	Object.each(contents, (domain, aliases) =>
	{
		events.push(
		{
			type: "m.room.aliases",
			origin_server_ts: 0,
			state_key: domain,
			content:
			{
				aliases: aliases,
			},
		})
	});

	return events;
};

room.state.summary.parse.on["guest_can_join"] = function(bool)
{
	return {
		type: "m.room.guest_access",
		origin_server_ts: 0,
		state_key: "",
		content:
		{
			guest_access: bool? "can_join" : "forbidden",
		},
	};
};

room.state.summary.parse.on["world_readable"] = function(bool)
{
	return {
		type: "m.room.history_visibility",
		origin_server_ts: 0,
		state_key: "",
		content:
		{
			history_visibility: bool? "world_readable" : undefined,
		},
	};
};

room.state.summary.parse.on["num_joined_members"] = function(num_joined_members)
{
	// TODO: XXX
	return
};

/* TODO: XXX
room.state.summary.parse.on["avatar_url"] = function(avatar_url)
{
};
*/
