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
 *
 *   MATRIX PROTOCOL
 *
 */
mc.m = {}

/**************************************
 *
 * Matrix utilities
 *
 */

/** local part of id
 */
mc.m.local = (mxid) =>
	typeif(mxid, "string", () => mxid.split(':')[0]);

/** domain part of id - Chops off the domain and other non-name characters from an mxid
 */
mc.m.domid = (mxid) =>
	typeif(mxid, "string", () => mxid.split(':')[1]);

/** short id - Chops off the domain and other non-name characters from an mxid
 */
mc.m.sid = (mxid) =>
	typeif(mxid, "string", () => mc.m.local(mxid).replace(/^[\@\!\$]/, ''));

/** pretty_errcode
 */
mc.m.pretty_errcode = (errcode) =>
	typeif(errcode, "string", () => errcode.replace(/^M_/, "").replace(/_/g, " "));

/** Randomness
 *
 * This is a library to generate various elements of the matrix protocol
 * when they need to have random content.
 */
mc.m.random = {}

/** Generate a random mxid */
mc.m.random.mxid = (prefix = "$", domain = "localhost") =>
	prefix + (new Date).getTime() + mc.random.string(5) + "@" + domain;

/** Sigil specification
 */
mc.m.sigil =
{
	'@': "user_id",
	'!': "room_id",
	'$': "event_id",
	'#': "room_alias",
};

/** Find sigil reverse lookup
 */
Object.defineProperty(mc.m.sigil, 'find', {
enumerable: false,
valud: function(what)
{
	return Object.keys(this).find((key) =>
	{
		return (this)[key] == what;
	});
}});

/** Validators
 *
 * This is a library of various validator functions for the matrix protocol
 */
mc.m.valid = {};

mc.m.valid["user_id"] = function(mxid)
{
	return mc.m.valid.mxid(mxid, "@");
};

mc.m.valid["room_id"] = function(mxid)
{
	return mc.m.valid.mxid(mxid, "!");
};

mc.m.valid["event_id"] = function(mxid)
{
	return mc.m.valid.mxid(mxid, "!");
};

mc.m.valid["room_alias"] = function(mxid)
{
	return mc.m.valid.mxid(mxid, "#");
};

mc.m.valid.mxid = function(mxid, sigil = undefined, opts = {})
{
	if(!this.sigil.valid(mxid))
		return false;

	let [local, domain] = mxid.split(':');
	if(local.length <= 1)
		return false;

	if(sigil !== undefined)
		if(local[0] != sigil)
			return false;

	if(opts.valid_domain === true)
		if(!valid_domain(domain))
			return false;

	return true;
};

mc.m.valid.sigil = function(string)
{
	if(typeof(string) != "string")
		return false;

	if(!this.expr.test(string))
		return false;

	return true;
};

mc.m.valid.sigil.expr = /^(\@|\!|\$|\@)/;

/** Translates an mxc:// when the registered window protocol handler isn't relevant
 */
mc.m.xc = (url) =>
{
	if(typeof(url) != "string")
		return url;

	if(!url.startsWith("mxc://"))
		return url;

	let mxc_protocol_path = mc.opts.base_url + "/_matrix/media/r0/download/";
	return url.replace("mxc://", mxc_protocol_path);
};

/** ???
 *
 */
mc.m.error = async function(path)
{
	let ret =
	{
		error: await path.error,
	};

	if(!ret.error)
		return false;

	ret.errcode = await path.errcode;
	return ret;
};


/**************************************
 *
 * Matrix Protocol Specification / IO
 *
 */

/**
 ******************************************************************************
 * 2 API Standards
 *
 */

/** 2.1 Versions
 *
 * Gets the versions of the specification supported by the server. Values will take
 * the form rX.Y.Z. Only the latest Z value will be reported for each supported X.Y
 * value. i.e. if the server implements r0.0.0, r0.0.1, and r1.2.0, it will report
 * r0.0.1 and r1.2.0.
 */
mc.m.versions = {};

mc.m.versions.get = function(opts = {})
{
	Object.update(opts,
	{
		method: "GET",
		resource: "versions",
		prefix: "/_matrix/client/",
		version: null,
	});

	return new mc.io.request(opts, (error, data) =>
	{
		return callback(arguments, error, data);
	});
};

/**
 ******************************************************************************
 * 3 Client Authentication
 *
 */

/**
 * 3.2 Login
 */
mc.m.login = {};

/** 3.2 Login Flows
 */
mc.m.login.get = function(opts = {})
{
	Object.update(opts,
	{
		method: "GET",
		resource: "login",
		prefix: "/_matrix/client/",
		version: 0,
	});

	return new mc.io.request(opts, (error, data) =>
	{
		return callback(arguments, error, data);
	});
};

/** 3.2.1 Login
 */
mc.m.login.post = function(opts = {})
{
	Object.defaults(opts,
	{
		content:
		{
			type: "m.login.dummy"
		},
	});

	Object.update(opts,
	{
		method: "POST",
		resource: "login",
		prefix: "/_matrix/client/",
		version: 0,
	});

	return new mc.io.request(opts, (error, data) => callback(arguments, error, data));
};

/** 3.1.2.1 Password-based
 *
 */
mc.m.login.password = function(username, password, opts = {})
{
	Object.defaults(opts,
	{
		content:
		{
			user: username,
			password: password,
			type: "m.login.password",
		},
	});

	return mc.m.login.post(opts, (error, data) => callback(arguments, error, data));
};

/** 3.1.2.3 Token-based
 *
 */
mc.m.login.token = function(username, token, opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},

		content:
		{
			user: username,
			token: token,
			type: "m.login.token",
			txn_id: mc.local.txnid++,
		},
	});

	return mc.m.login.post(opts, (error, data) => callback(arguments, error, data));
};

/** 3.2.2 tokenrefresh
 *
 */
mc.m.tokenrefresh = {};

mc.m.tokenrefresh.post = function(opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},

		content:
		{
			refresh_token: mc.local.refresh_token,
		},
	});

	Object.update(opts,
	{
		method: "POST",
		resource: "tokenrefresh",
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts, (error, data) => callback(arguments, error, data));
};

/** 3.2.3 logout
 *
 */
mc.m.logout = {};

mc.m.logout.post = function(opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	Object.update(opts,
	{
		method: "POST",
		resource: "logout",
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts, (error, data) => callback(arguments, error, data));
};

/** 3.3 Account registration and management
 *
 */

mc.m.register = {};
mc.m.account = {};

/** 3.3.1 register
 *
 */
mc.m.register.post = function(opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			// The kind of account to register. Defaults to user. One of: ["guest", "user"]
			kind: "guest",
		},

		content:
		{
			// The local part of the desired Matrix ID. If omitted, the homeserver MUST
			// generate a Matrix ID local part.
			username: undefined,

			// If true, the server binds the email used for authentication to the Matrix ID
			// with the ID Server.
			bind_email: undefined,

			// Required. The desired password for the account.
			password: undefined,

			// Additional authentication information for the user-interactive authentication API.
			auth:
			{
				// The value of the session key given by the homeserver.
				session: undefined,

				// Required. The login type that the client is attempting to complete.
				type: "m.login.dummy",
			}
		},
	});

	Object.update(opts,
	{
		method: "POST",
		resource: "register",
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts, (error, data) => callback(arguments, error, data));
};

/** 3.4 Account Administrative contact information
 *
 */
mc.m.account['3pid'] = {};

/** 3.4.1 POST 3pid
 *
 */

/** 3.4.2 GET 3pid
 *
 */
mc.m.account['3pid'].get = function(opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	Object.update(opts,
	{
		method: "GET",
		resource: "account/3pid",
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts, (error, data) => callback(arguments, error, data));
};

/**
 ******************************************************************************
 * 5 Filtering
 *
 * Filters can be created on the server and can be passed as as a parameter to APIs
 * which return events. These filters alter the data returned from those APIs. Not
 * all APIs accept filters.
 *
 */

// Protocol suite
mc.m.filter = {};

/** 5.1 GET Filter
 *
 * Download a filter
 */
mc.m.filter.get = function(filter_id, opts = {})
{
	let user_id = mc.uri(mc.session.user_id);
	let resource = "user/" + user_id + "/filter/" + mc.uri(filter_id);
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	Object.update(opts,
	{
		method: "GET",
		resource: resource,
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts, (error, data) => callback(arguments, error, data));
};

/** 5.2 POST Filter
 *
 * Uploads a new filter definition to the homeserver. Returns a filter ID that may be used in
 * future requests to restrict which events are returned to the mc.
 */
mc.m.filter.post = function(content = {}, opts = {})
{
	if(!mc.instance.authentic)
		throw new mc.error("Posting a filter requires authentication");

	let filter =
	{
		// A list of event types to exclude. If this list is absent then no event types are
		// excluded. A matching type will be excluded even if it is listed in the 'types'
		// filter. A '*' can be used as a wildcard to match any sequence of characters.
		not_types: [],

		// The maximum number of events to return.
		limit: undefined,

		// A list of senders IDs to include. If this list is absent then all senders are included.
		senders: [],

		// A list of event types to include. If this list is absent then all event types are
		// included. A '*' can be used as a wildcard to match any sequence of characters.
		types: [],

		// A list of sender IDs to exclude. If this list is absent then no senders are excluded.
		// A matching sender will be excluded even if it is listed in the 'senders' filter.
		not_senders: [],
	};

	let room_event_filter =
	{
		// A list of event types to exclude. If this list is absent then no event types are
		// excluded. A matching type will be excluded even if it is listed in the 'types'
		// filter. A '*' can be used as a wildcard to match any sequence of characters.
		not_types: [],

		// A list of room IDs to exclude. If this list is absent then no rooms are excluded.
		// A matching room will be excluded even if it is listed in the 'rooms' filter.
		not_rooms: [],

		// The maximum number of events to return.
		limit: undefined,

		// A list of room IDs to include. If this list is absent then all rooms are included.
		rooms: [],

		// A list of sender IDs to exclude. If this list is absent then no senders are excluded.
		// A matching sender will be excluded even if it is listed in the 'senders' filter.
		not_senders: [],

		// A list of senders IDs to include. If this list is absent then all senders are included.
		senders: [],

		// A list of event types to include. If this list is absent then all event types are
		// included. A '*' can be used as a wildcard to match any sequence of characters.
		types: [],
	};

	let room_filter =
	{
		// Include rooms that the user has left in the sync, default false
		include_leave: undefined,

		// The per user account data to include for rooms.
		account_data: undefined, //filter,

		// The message and state update events to include for rooms.
		timeline: undefined, //room_event_filter,

		// The events that aren't recorded in the room history, e.g. typing and receipts,
		// to include for rooms.
		ephemeral: undefined, //room_event_filter,

		// The state events to include for rooms.
		state: undefined, //room_event_filter,

		// A list of room IDs to exclude. If this list is absent then no rooms are
		// excluded. A matching room will be excluded even if it is listed in the
		// 'rooms' filter. This filter is applied before the filters in ephemeral,
		// state, timeline or account_data
		not_rooms: undefined, //[],

		// A list of room IDs to include. If this list is absent then all rooms are
		// included. This filter is applied before the filters in ephemeral, state,
		// timeline or account_data
		rooms: undefined, //[],
	};

	Object.defaults(content,
	{
		// List of event fields to include. If this list is absent then all fields are included.
		// The entries may include '.' charaters to indicate sub-fields. So ['content.body'] will
		// include the 'body' field of the 'content' object. A literal '.' character in a field
		// name may be escaped using a '\'. A server may include more fields than were requested.
		event_fields: undefined, //[],

		// The format to use for events. 'client' will return the events in a format suitable for
		// clients. 'federation' will return the raw event as receieved over federation. The default
		// is 'client'. One of: ["client", "federation"]
		event_format: undefined,

		// The user account data that isn't associated with rooms to include.
		account_data: undefined, //filter,

		// Filters to be applied to room data.
		room: undefined, //room_filter,

		// The presence updates to include.
		presence: undefined, //filter,
	});

	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
		content: content,
	});

	Object.update(opts,
	{
		method: "POST",
		resource: "user/" + mc.uri(mc.my.mxid) + "/filter",
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts, (error, data) => callback(arguments, error, data));
};

/**
 ******************************************************************************
 * 6 Events
 *
 */

/** 6.2 Syncing
 *
 */
mc.m.sync = {};

/** 6.2.1 get sync
 *
 */
mc.m.sync.get = function(opts = {})
{
	Object.defaults(opts, mc.m.sync.get.opts);
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		}
	});

	Object.update(opts,
	{
		method: "GET",
		resource: "sync",
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts, (error, data) => callback(arguments, error, data));
};

mc.m.sync.get.opts =
{
	// This timeout is our XHR timeout, not the longpoll timeout sent to the server in the
	// query string. This only fires if there's a real connection problem. Set this to
	// something higher than the longpoll timeout.
	timeout: 305 * 1000,
};

mc.m.sync.get.opts.query =
{
	// The maximum time to poll in milliseconds before returning this request.
	timeout: 300 * 1000,

	// The ID of a filter created using the filter API or a filter JSON object
	// encoded as a string. The server will detect whether it is an ID or a JSON
	// object by whether the first character is a "{" open brace. Passing the JSON
	// inline is best suited to one off requests. Creating a filter using the filter
	// API is recommended for clients that reuse the same filter multiple times, for
	// example in long poll requests.
	filter: undefined,

	// Controls whether the client is automatically marked as online by polling this API.
	// If this parameter is omitted then the client is automatically marked as online
	// when it uses this API. Otherwise if the parameter is set to "offline" then the
	// client is not marked as being online when it uses this API. One of: ["offline"]
	set_presence: undefined, //"online",

	// Controls whether to include the full state for all rooms the user is a member of.
	// If this is set to true, then all state events will be returned, even if since is
	// non-empty. The timeline will still be limited by the since parameter. In this case,
	// the timeout parameter will be ignored and the query will return immediately,
	// possibly with an empty timeline.
	// If false, and since is non-empty, only state which has changed since the point
	// indicated by since will be returned. By default, this is false.
	full_state: false,
};

/**
 * 6.3 Getting events for a room
 * 6.4 Sending events to a room
 * 6.5 Redactions
 */
mc.m.rooms = {};
mc.m.rooms.state = {};
mc.m.rooms.members = {};
mc.m.rooms.messages = {};
mc.m.rooms.send = {};
mc.m.rooms.redact = {};

// abstract
mc.m.rooms.request = function(room_id, resource, opts = {})
{
	Object.defaults(opts,
	{
		method: "GET",
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	let uri = mc.uri(room_id);
	Object.update(opts,
	{
		resource: "rooms/" + uri + "/" + resource,
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts, (error, data) => callback(arguments, error, data));
};

// abstract
mc.m.rooms.state.request = function(room_id, type = undefined, state_key = undefined, opts = {})
{
	let resource = "state";
	if(type !== undefined)
	{
		resource += "/" + type;
		if(state_key !== undefined && state_key.length)
			resource += "/" + state_key;
	}

	let handler = (error, data) => callback(arguments, error, data);
	return mc.m.rooms.request(room_id, resource, opts, handler);
};

/** 6.3 Getting events for a room
 */

/** 6.3.1 GET state
 *+ 6.3.2 GET state eventType
 *+ 6.3.3 GET state eventType stateKey
 */
mc.m.rooms.state.get = function(room_id, type = undefined, state_key = undefined, opts = {})
{
	Object.update(opts,
	{
		method: "GET",
	});

	let handler = (error, data) => callback(arguments, error, data);
	return mc.m.rooms.state.request(room_id, type, state_key, opts, handler);
};

/** 6.3.4 GET members
 */
mc.m.rooms.members.get = function(room_id, opts = {})
{
	let resource = "members";
	Object.update(opts,
	{
		method: "GET",
	});

	Object.defaults(opts,
	{
		query:
		{
			filter: mc.filter("members"),
		},
	});

	let handler = (error, data) => callback(arguments, error, data);
	return mc.m.rooms.request(room_id, resource, opts, handler);
};

/** 6.3.5 GET messages
 */
mc.m.rooms.messages.get = function(room_id, opts = {})
{
	let resource = "messages";
	Object.update(opts,
	{
		method: "GET",
	});

	Object.defaults(opts,
	{
		query:
		{
			//from: undefined,
			//to: undefined,
			//dir: "b",
			limit: 128,
		},
	});

	let handler = (error, data) =>
	{
		if(error || !data)
			return callback(arguments, error, data);

		switch(opts.query.dir)
		{
			case 'b':
				opts.query.from = data.start;
				opts.query.to = undefined;
				break;

			case 'f':
				opts.query.from = data.end;
				opts.query.to = undefined;
				break;
		}

		return callback(arguments, error, data);
	};

	return mc.m.rooms.request(room_id, resource, opts, handler);
};


/** 6.4 Sending events to a room
 */

/** 6.4.1 PUT state eventType
 *+ 6.4.2 PUT state eventType stateKey
 */
mc.m.rooms.state.put = function(room_id, type, state_key = "", content = {}, opts = {})
{
	Object.defaults(opts,
	{
		content: content,
	});

	Object.update(opts,
	{
		method: "PUT",
	});

	let handler = (error, data) => callback(arguments, error, data);
	return mc.m.rooms.state.request(room_id, type, state_key, opts, handler);
}

/** 6.4.3 PUT send eventType txnId
 *
 * Content of the event passed in opts.content
 */
mc.m.rooms.send.put = function(room_id, type, opts = {})
{
	Object.update(opts,
	{
		method: "PUT",
		txnid: mc.local.txnid++,
	});

	let resource = "send/" + type + "/" + opts.txnid;
	let handler = (error, data) => callback(arguments, error, data);
	return mc.m.rooms.request(room_id, resource, opts, handler);
};

/**
 ******************************************************************************
 * 7 Rooms
 *
 */

/** 7.1 Creation
 *
 */
mc.m.createRoom = {};

mc.m.createRoom.post = function(opts = {})
{
	Object.defaults(opts, mc.m.createRoom.post.opts);
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	Object.update(opts,
	{
		method: "POST",
		resource: "createRoom",
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts, (error, data) => callback(arguments, error, data));
};

mc.m.createRoom.post.opts = {};

mc.m.createRoom.post.opts.content =
{
	// A list of user IDs to invite to the room. This will tell the server to invite everyone in the list
	// to the newly created room.
	invite: undefined,

	// If this is included, an m.room.name event will be sent into the room to indicate the name of the
	// room. See Room Events for more information on m.room.name.
	name: undefined,

	// A public visibility indicates that the room will be shown in the published room list. A private
	// visibility will hide the room from the published room list. Rooms default to private visibility if this key is not included. NB: This should not be confused with join_rules which also uses the word public.
	// One of: ["public", "private"]
	visibility: undefined,

	// A list of objects representing third party IDs to invite into the room.
	invite_3pid: undefined,

	// If this is included, an m.room.topic event will be sent into the room to indicate the topic for
	// the room. See Room Events for more information on m.room.topic.
	topic: undefined,

	// Extra keys to be added to the content of the m.room.create. The server will clober the following
	// keys: creator. Future versions of the specification may allow the server to clobber other keys.
	creation_content: undefined,

	// A list of state events to set in the new room. This allows the user to override the default
	// state events set in the new room. The expected format of the state events are an object with
	// 'type', 'state_key' and 'content' keys set. Takes precedence over events set by presets, but gets
	// overriden by name and topic keys.
	initial_state: undefined,

	// The desired room alias local part. If this is included, a room alias will be created and mapped to
	// the newly created room. The alias will belong on the same homeserver which created the room. For
	// example, if this was set to "foo" and sent to the homeserver "example.com" the complete room alias
	// would be #foo:example.com.
	room_alias_name: undefined,
};


/** 7.2 Room aliases
 *
 */
mc.m.directory = {};

/** 7.2.1 PUT
 *
 */
mc.m.directory.put = function(alias, room_id, opts = {})
{
	Object.update(opts,
	{
		method: "PUT",
		content:
		{
			room_id: room_id,
		},
	});

	return mc.m.directory.request(alias, opts, (error, data) => callback(arguments, error, data));
};

/** 7.2.2 DELETE
 *
 */
mc.m.directory.del = function(alias, opts = {})
{
	Object.update(opts,
	{
		method: "DELETE",
	});

	return mc.m.directory.request(alias, opts, (error, data) => callback(arguments, error, data));
};

/** 7.2.3 GET
 *
 */
mc.m.directory.get = function(alias, opts = {})
{
	Object.update(opts,
	{
		method: "GET",
	});

	return mc.m.directory.request(alias, opts, (error, data) => callback(arguments, error, data));
};

/** Abstract
 */
mc.m.directory.request = function(alias, opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	Object.update(opts,
	{
		resource: "directory/room/" + mc.uri(alias),
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts, (error, data) => callback(arguments, error, data));
};

/** 7.4 Room Membership
 *
 */

/** 7.4.1 Joining Rooms
 *
 */

/** 7.4.1.1 POST invite
 */
mc.m.rooms.invite = {};
mc.m.rooms.invite.post = function(room_id, user_id, opts = {})
{
	let resource = "invite";
	Object.defaults(opts,
	{
		content:
		{
			user_id: user_id,
		},
	});

	Object.update(opts,
	{
		method: "POST",
	});

	let handler = (error, data) => callback(arguments, error, data);
	return mc.m.rooms.request(room_id, resource, opts, handler);
}

/** 7.4.1.2 POST join
 */
mc.m.join = {};
mc.m.join.post = function(room_id_or_alias, opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},

		content:
		{
			third_party_signed: undefined,
		},
	});

	Object.update(opts,
	{
		method: "POST",
		resource: "join/" + mc.uri(room_id_or_alias),
		prefix: "/_matrix/client/",
	});

	let handler = (error, data) => callback(arguments, error, data);
	return new mc.io.request(opts, handler);
}

/** 7.4.1.3 POST join
 */
mc.m.rooms.join = {};
mc.m.rooms.join.post = function(room_id, opts = {})
{
	let resource = "join";
	Object.defaults(opts,
	{
		content:
		{
			third_party_signed: undefined,
		},
	});

	Object.update(opts,
	{
		method: "POST",
	});

	let handler = (error, data) => callback(arguments, error, data);
	return mc.m.rooms.request(room_id, resource, opts, handler);
}

/** 7.4.2 Leaving rooms
 *
 */

/** 7.4.2.1 POST forget
 */
mc.m.rooms.forget = {};
mc.m.rooms.forget.post = function(room_id, opts = {})
{
	let resource = "forget";
	Object.update(opts,
	{
		method: "POST",
	});

	let handler = (error, data) => callback(arguments, error, data);
	return mc.m.rooms.request(room_id, resource, opts, handler);
}

/** 7.4.2.2 POST leave
 */
mc.m.rooms.leave = {};
mc.m.rooms.leave.post = function(room_id, opts = {})
{
	let resource = "leave";
	Object.update(opts,
	{
		method: "POST",
	});

	let handler = (error, data) => callback(arguments, error, data);
	return mc.m.rooms.request(room_id, resource, opts, handler);
}

/** 7.4.2.3 POST kick
 */
mc.m.rooms.kick = {};
mc.m.rooms.kick.post = function(room_id, user_id, reason = undefined, opts = {})
{
	let resource = "kick";
	Object.defaults(opts,
	{
		content:
		{
			user_id: user_id,
			reason: reason,
		},
	});

	Object.update(opts,
	{
		method: "POST",
	});

	let handler = (error, data) => callback(arguments, error, data);
	return mc.m.rooms.request(room_id, resource, opts, handler);
}

/** 7.4.3 Banning users in a room
 *
 */

/** 7.4.3.1 POST unban
 */
mc.m.rooms.unban = {};
mc.m.rooms.unban.post = function(room_id, user_id, opts = {})
{
	let resource = "unban";
	Object.defaults(opts,
	{
		content:
		{
			user_id: user_id,
		},
	});

	Object.update(opts,
	{
		method: "POST",
	});

	let handler = (error, data) => callback(arguments, error, data);
	return mc.m.rooms.request(room_id, resource, opts, handler);
}

/** 7.4.3.2 POST ban
 */
mc.m.rooms.ban = {};
mc.m.rooms.ban.post = function(room_id, user_id, reason = undefined, opts = {})
{
	let resource = "ban";
	Object.defaults(opts,
	{
		content:
		{
			user_id: user_id,
			reason: reason,
		},
	});

	Object.update(opts,
	{
		method: "POST",
	});

	let handler = (error, data) => callback(arguments, error, data);
	return mc.m.rooms.request(room_id, resource, opts, handler);
}

/** 7.5 Listing rooms
 *
 */
mc.m.publicRooms = {};

/** 7.5.1
 *
 */
mc.m.publicRooms.get = function(opts = {})
{
	Object.update(opts,
	{
		method: "GET",
		resource: "publicRooms",
		prefix: "/_matrix/client/",
	});

	Object.defaults(opts,
	{
		query:
		{
			limit: 64,
			access_token: mc.session.access_token,
		},
	});

	return new mc.io.request(opts);
};

/** EXPERIMENTAL
 *
 */
mc.m.publicRooms.post = function(opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			server: undefined,
			access_token: mc.session.access_token,
		},

		content:
		{
			// limit param (content?)
			limit: 128,

			// since param (content?)
			since: undefined,

			// filter_id (content?)
			filter: undefined,

			// boolean
			include_all_networks: undefined,

			third_party_instance_id: undefined,
		},
	});

	Object.update(opts,
	{
		method: "POST",
		resource: "publicRooms",
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts);
};

/**
 ******************************************************************************
 * 8 Profiles
 *
 */
mc.m.profile = {};
mc.m.profile.displayname = {};
mc.m.profile.avatar_url = {};

/** 8.1 PUT displayname
 */
mc.m.profile.displayname.put = function(user_id, displayname, opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	Object.update(opts,
	{
		content:
		{
			displayname: displayname,
		},
	});

	Object.update(opts,
	{
		method: "PUT",
		resource: "profile/" + mc.uri(user_id) + "/displayname",
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts);
};

/** 8.2 GET displayname
 */
mc.m.profile.displayname.get = function(user_id, opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	Object.update(opts,
	{
		method: "GET",
		resource: "profile/" + mc.uri(user_id) + "/displayname",
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts);
};

/** 8.3 PUT avatar_url
 */
mc.m.profile.avatar_url.put = function(user_id, avatar_url, opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	Object.update(opts,
	{
		content:
		{
			avatar_url: avatar_url,
		},
	});

	Object.update(opts,
	{
		method: "PUT",
		resource: "profile/" + mc.uri(user_id) + "/avatar_url",
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts);
};

/** 8.4 GET avatar_url
 */
mc.m.profile.avatar_url.get = function(user_id, opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	Object.update(opts,
	{
		method: "GET",
		resource: "profile/" + mc.uri(user_id) + "/avatar_url",
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts);
};

/** 8.5 GET profile
 *
 */
mc.m.profile.get = function(user_id, opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	Object.update(opts,
	{
		method: "GET",
		resource: "profile/" + mc.uri(user_id),
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts, (error, data) => callback(arguments, error, data));
};

/**
 ******************************************************************************
 * 11 Modules
 *
 */

/**
 **************************************
 * 11.4 Typing notifications
 *
 */
mc.m.rooms.typing = {};

/** 11.4.2.1 PUT typing
 */
mc.m.rooms.typing.put = function(room_id, user_id, opts = {})
{
	Object.defaults(opts,
	{
		content:
		{
			timeout: 15 * 1000,
			typing: true,
		},
	});

	Object.update(opts,
	{
		method: "PUT",
		resource: "typing" + "/" + mc.uri(user_id),
		prefix: "/_matrix/client/",
	});

	let handler = (error, data) => callback(arguments, error, data);
	return mc.m.rooms.request(room_id, opts.resource, opts, handler);
}

/**
 **************************************
 * 11.5 Receipts
 *
 */
mc.m.rooms.receipt = {};

/** 11.5.2.1 POST receipt
 */
mc.m.rooms.receipt.post = function(room_id, event_id, type = "m.read", opts = {})
{
	Object.update(opts,
	{
		method: "POST",
		resource: "receipt/" + mc.uri(type) + "/" + mc.uri(event_id),
		prefix: "/_matrix/client/",
	});

	return mc.m.rooms.request(room_id, opts.resource, opts);
}

/**
 **************************************
 * 11.6 Presence
 *
 */
mc.m.presence = {};

mc.m.presence.list = {};
mc.m.presence.status = {};

/** 11.6.2
 */

/** 11.6.2.1 PUT status
 */
mc.m.presence.status.put = function(user_id, presence, status_msg = undefined, opts = {})
{
	if(user_id === undefined)
		user_id = mc.session.user_id;

	if(presence === undefined)
		presence = mc.instance.presence;

	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},

		content:
		{
			// The status message to attach to this state.
			status_msg: status_msg,

			// Required. The new presence state. One of: ["online", "offline", "unavailable"]
			presence: presence,
		},
	});

	let resource = "presence";
	resource += "/" + mc.uri(user_id);
	resource += "/" + "status";

	Object.update(opts,
	{
		method: "PUT",
		resource: resource,
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts);
};

/** 11.6.2.2 GET status
 */
mc.m.presence.status.get = function(user_id, opts = {})
{
	if(user_id === undefined)
		user_id = mc.session.user_id;

	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	Object.update(opts,
	{
		method: "GET",
		resource: "presence/" + mc.uri(user_id) + "/status",
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts);
};

/** 11.6.2.3 POST list
 *
 */
mc.m.presence.list.post = function(user_id, delta = { drop: [], invite: [] }, opts = {})
{
	if(user_id === undefined)
		user_id = mc.session.user_id;

	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	Object.update(opts,
	{
		content:
		{
			drop: delta.drop,
			invite: delta.invite,
		},
	});

	Object.update(opts,
	{
		method: "POST",
		resource: "presence/list/" + mc.uri(user_id),
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts);
};

/** 11.6.2.4 GET list
 */
mc.m.presence.list.get = function(user_id, opts = {})
{
	if(user_id === undefined)
		user_id = mc.session.user_id;

	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	Object.update(opts,
	{
		method: "GET",
		resource: "presence/list/" + mc.uri(user_id),
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts);
};

/**
 **************************************
 * 11.7 Content Repository
 *
 */
mc.m.media = {};

/**
 * Register the mxc:// protocol with the browser for href's.
 */
//let mxc_protocol_path = mc.opts.base_url + "/_matrix/media/r0/download/%s";
//window.navigator.registerProtocolHandler("web+mxc", mxc_protocol_path, "Matrix Content");

/**
 * 11.7.1 Client Behavior
 *
 */
mc.m.media.download = {};
mc.m.media.upload = {};

/** 11.7.1.1 GET media
 *
 */
mc.m.media.download.get = function(server_name, media_id, opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},

		// This IO request is a corner-case. Setting these is required
		// to override the mc.io.request()'s defaulting to JSON.
		responseType: "arraybuffer",
	});

	//XXX: have to restore prefix clobber
	let their_prefix = opts.prefix;
	Object.update(opts,
	{
		method: "GET",
		prefix: "/_matrix/media/",
		resource: "download/" + server_name + "/" + media_id,
	});

	let handler = (error, data, xhr) =>
	{
		opts.prefix = their_prefix;

		if(error)
			return callback(arguments, error, data, undefined);

		let type = xhr.getResponseHeader("content-type");
		return callback(arguments, error, data, type);
	};

	return new mc.io.request(opts, handler);
}

/** 11.7.1.2 POST media
 *
 * All arguments in opts.
 * opts.contentType
 * opts.content -> xhr.send()
 * 
 */
mc.m.media.upload.post = function(opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	//XXX: have to restore prefix clobber
	let their_prefix = opts.prefix;
	Object.update(opts,
	{
		method: "POST",
		resource: "upload",
		prefix: "/_matrix/media/",
	});

	return new mc.io.request(opts, (error, data) =>
	{
		opts.prefix = their_prefix;
		callback(arguments, error, data);
	});
}

/**
 **************************************
 * 11.10 Push Notifications
 *
 */

/** 11.10.1 pushers
 *
 */
mc.m.pushers = {};

/** 11.10.1.2 GET pushers
 *
 */
mc.m.pushers.get = function(opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	Object.update(opts,
	{
		method: "GET",
		resource: "pushers",
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts, (error, data) => callback(arguments, error, data));
};

/**
 **************************************
 * 11.12 Server Side Search
 *
 */
mc.m.search = {};

/** 11.12.1.1 search
 *
 */
mc.m.search.post = function(opts = {})
{
	Object.defaults(opts, mc.m.search.post.opts);
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	Object.update(opts,
	{
		method: "POST",
		resource: "search",
		prefix: "/_matrix/client/",
	});

	let remain = opts.query.limit;
	opts.query.limit = Math.min(remain, opts.chunk_size);

	///TODO: XXX
	// We continue to paginate automatically until finished or the user
	// callback returns false.
	let handler = (error, data) =>
	{
		if(callback(arguments, error, data) === false)
			return;

		if(maybe(() => data.search_categories.room_events.next_batch) === undefined)
			return;

		//remain -= data.search_categories.room_events.
		//opts.query.limit = Math.min(remain, opts.chunk_size);
		opts.query.since = data.search_categories.room_events.next_batch;
		return new mc.io.request(opts, handler);
	};

	return new mc.io.request(opts, handler);
};

mc.m.search.post.opts =
{
	chunk_size: 10,
	query:
	{
		limit: 10,
	},
};

mc.m.search.post.opts.content =
{
	search_categories:
	{
		room_events:
		{
			// Required. The string to search events for.
			search_term: "",

			// Takes a section 5 filter
			filter: undefined,

			// The keys to search. Defaults to all.
			// One of: ["content.body", "content.name", "content.topic"]
			keys: undefined,

			// Requests that the server partitions the result set based on the
			// provided list of keys.
			groupings:
			{
				// List of groups to request.
				group_by: undefined,
			},

			// The order in which to search for results. One of: ["recent", "rank"]
			order_by: undefined,

			// Requests the server return the current state for each room returned.
			include_state: undefined,

			// Configures whether any context for the events returned are included
			// in the response.
			event_context:
			{
				// How many events before the result are returned.
				before_limit: undefined,      // integer

				// How many events after the result are returned.
				after_limit: undefined,       // integer

				// Requests that the server returns the historic profile
				// information for the users that sent the events that were
				// returned.
				include_profile: undefined,   // boolean
			},
		},
	},
};


/**
 **************************************
 * 11.14 Room Previews
 *
 */

/** 11.14.1
 *
 */

/** 11.14.1.1 GET events
 *
 */
mc.m.events = {};
mc.m.events.get = function(room_id, opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			room_id: room_id,
			from: undefined,
			timeout: 60 * 1000,
			access_token: mc.session.access_token,
		},
	});

	Object.update(opts,
	{
		method: "GET",
		resource: "events",
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts);
};

/**
 **************************************
 * 11.16 Client Config
 *
 */

/** 11.16.2
 *
 */
mc.m.user = {};
mc.m.user.account_data = {};
mc.m.user.rooms = {};
mc.m.user.rooms.account_data = {};

/** 11.16.2.1 PUT per room account data
 *
 */
mc.m.user.rooms.account_data.put = function(user_id, room_id, type, content, opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	let resource;
	resource = "user/" + mc.uri(user_id);
	resource +="/rooms/" + mc.uri(room_id);
	resource += "/account_data/" + mc.uri(type);

	Object.update(opts,
	{
		method: "PUT",
		resource: resource,
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts);
};

mc.m.user.rooms.account_data.del = function(user_id, room_id, type, opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	let resource;
	resource = "user/" + mc.uri(user_id);
	resource +="/rooms/" + mc.uri(room_id);
	resource += "/account_data/" + mc.uri(type);

	Object.update(opts,
	{
		method: "DELETE",
		resource: resource,
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts);
};

/** 11.16.2.2 PUT per room account data
 *
 */
mc.m.user.account_data.put = function(user_id, type, content, opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	let resource;
	resource = "user/" + mc.uri(user_id);
	resource += "/account_data/" + mc.uri(type);

	Object.update(opts,
	{
		method: "PUT",
		resource: resource,
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts);
};

mc.m.user.account_data.del = function(user_id, type, opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	let resource;
	resource = "user/" + mc.uri(user_id);
	resource += "/account_data/" + mc.uri(type);

	Object.update(opts,
	{
		method: "DELETE",
		resource: resource,
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts);
};

/**
 **************************************
 * 11.17 Server Administration
 *
 */
mc.m.admin = {};
mc.m.admin.whois = {};

/** 11.17.1
 *
 */

/** 11.17.1.1 GET whois
 *
 */
mc.m.admin.whois.get = function(user_id, opts = {})
{
	Object.defaults(opts,
	{
		query:
		{
			access_token: mc.session.access_token,
		},
	});

	Object.update(opts,
	{
		method: "GET",
		resource: "admin/whois/" + mc.uri(user_id),
		prefix: "/_matrix/client/",
	});

	return new mc.io.request(opts);
};

/**
 **************************************
 * 11.18 Event Context
 *
 */

/** 11.18.1 GET context
 *
 */
mc.m.rooms.context = {};
mc.m.rooms.context.get = function(room_id, event_id, opts = {})
{
	let resource = "context/" + event_id;
	Object.defaults(opts,
	{
		query:
		{
			//from: undefined,
			//to: undefined,
			limit: 0,
		},
	});

	let handler = (error, data) => callback(arguments, error, data);
	return mc.m.rooms.request(room_id, resource, opts, handler);
};
