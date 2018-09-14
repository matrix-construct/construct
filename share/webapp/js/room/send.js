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


/**************************************
 * Input send interface.
 *
 * Handles the transmission process for input.
 */

/**
 * Any text from the input area which is intended for transmission lands here
 * and is dispatched. This function may be skipped to call another send
 * sub-method directly.
 */
room.prototype.send = function(text)
{
	let emote_prefix = "/me ";
	if(text.startsWith(emote_prefix))
	{
		let msg = text.slice(emote_prefix.length);
		return room.send.message.emote.call(this, msg);
	}

	if(text.endsWith(".jpg") || text.endsWith(".png"))
		return room.send.message.image.call(this, text);

	if(text.startsWith("https://www.youtube.com/watch?v="))
	{
		let a = text.split(" ", 2);
		let url = a[0];
		let desc = a[1];
		return room.send.message.video.call(this, url, desc);
	}

	return room.send.message.text.call(this, text);
};

/**
 * TODO
 */
room.prototype.restate = function($event, type)
{
	return room.send.state.call(this, type);
};

/**
 * The composed event content is transmitted out of here.
 * Should not be called directly. Use a helper below.
 *
 * this = room
 */
room.send = function(type, opts = {})
{
	let handler = (error, data) => callback(arguments, error, data);
	return client.m.rooms.send.put(this.id, type, opts, handler);
};

/**
 * TODO
 */
room.send.state = function(type, state_key = "")
{
	let event = this.state[type];
	if(state_key.length > 0)
		event = event[state_key];

	let pending = this.pending[type];
	if(state_key.length > 0)
		pending = pending[state_key];

	if(pending.content === undefined)
		return;

	let opts = {};
	return client.m.rooms.state.put(this.id, type, state_key, pending.content, opts, (error, data) =>
	{
		if(error)
			throw new client.error(error);

		pending.event_id = data.event_id;
		pending.type = type;
		pending.state_key = state_key;
		pending.sender = client.session.user_id;

		this.timeline.insert(pending);
		if(state_key.length > 0)
			delete this.pending[type][state_key];
		else
			delete this.pending[type];
	});
};

/**
 * The composed m.room.message event content is transmitted out of here.
 * Should not be called directly. Use or create a helper below.
 *
 * this = room
 */
room.send.message = function(opts = {})
{
	let handler = (error, data) =>
	{
		if(error !== undefined)
			this.scroll.to.bottom("fast");

		return callback(arguments, error, data);
	};

	return room.send.call(this, "m.room.message", opts, handler);
};

/** Compose a text message
 */
room.send.message.text = function(text, opts = {})
{
	Object.update(opts,
	{
		content:
		{
			body: text,
			msgtype: "m.text",
		},
	});

	let handler = (error, data) => callback(arguments, error, data);
	return room.send.message.call(this, opts, handler);
};

/** Compose an emote message
 */
room.send.message.emote = function(text, opts = {})
{
	Object.update(opts,
	{
		content:
		{
			body: text,
			msgtype: "m.emote",
		}
	});

	let handler = (error, data) => callback(arguments, error, data);
	return room.send.message.call(this, opts, handler);
};

/** Compose an image message
 */
room.send.message.image = function(url, body = url, opts = {})
{
	Object.update(opts,
	{
		content:
		{
			body: body,
			msgtype: "m.image",
			url: url,
		},
	});

	let handler = (error, data) => callback(arguments, error, data);
	return room.send.message.call(this, opts, handler);
};

/** Compose a video message.
 */
room.send.message.video = function(url, body = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", opts = {})
{
	Object.update(opts,
	{
		content:
		{
			body: body,
			info:
			{
				mimetype: "text/plain"
			},
			msgtype: "m.video",
			url: url,
		},
	});

	let handler = (error, data) => callback(arguments, error, data);
	return room.send.message.call(this, opts, handler);
};
