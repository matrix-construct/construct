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
 * Input interface.
 *
 * Functions apropos user input into the form at the bottom
 * of the room to submit messages etc.
 */

room.input = {};

/** Primary input dispatch. This is called when the user is finished entering input
 * into the room's input box (so only when 'enter' or the 'send' button is explicitly
 * pressed).
 *
 * If the input is a command it is evaluated locally. Otherwise if intended for
 * transmission it is passed to this.send suite.
 */
room.input.submit = function(event)
{
	let input = this.control.input;
	if(empty(input))
		return;

	if(this.opts.local || (input.startsWith('/') && !input.startsWith("/me")))
	{
		if(this.input.eval(event, input))
			this.input.history.enter(input);

		return;
	}

	this.send(input);
	this.input.history.enter(input);

	// Unconditionally set typing status to false.
	this.typing.set(false);
}

/** Input model change handler. This is called when there's a change to the input box.
 * It does not indicate a will to submit the input.
 */
room.input.change = function(event)
{
	let input = this.control.input;
	if(input[0] == '/')
		this.control.evalmode = true;
	else
		this.control.evalmode = false;

	// Re-determine typing state based on changed input state.
	this.input.typing();
}

/** Local command handler. This branch is taken for all input which will not be
 * transmitted. This is always taken for local pseudo rooms.
 */
room.input.eval = function(event, input)
{
	if(this.opts.local) try
	{
		console.log("" + eval(text));
	}
	catch(e)
	{
		console.error(e.name + ": " + e.message);
	}
	finally
	{
		return;
	}
	else try
	{
		let res = eval(input.slice(1));
		switch(typeof(res))
		{
			case "undefined":
				return true;

			case "object":
				this.console_push(debug.stringify(res, 10));
				return true;

			default:
				this.console_push(JSON.stringify(res));
				return true;
		}
	}
	catch(e)
	{
		this.console_push(e.name + ": " + e.message);
		return false;
	}
}

/** Intercepts keydown events in the room input box
 */
room.input.keydown = function(event)
{
	switch(event.which)
	{
		case 38:  return this.input.history.up(event);
		case 40:  return this.input.history.down(event);
		case 13:
		{
			this.input.submit(event);
			event.preventDefault();
			break;
		}
	}
}

/** Input box lost focus handler.
 */
room.input.blur = function(event)
{
	if(this.typing.get())
		this.typing.set(false);
};

/** Determines what state the typing indicator should be in
 * based on the input state.
 *
 * this = room.input
 */
room.input.typing = function()
{
	let typing = this.typing.get();  // gets my mxid by default
	let input = this.control.input;

	if(this.control.evalmode)
	{
		// Rescind any typing status if we find ourselves in local
		// mode with typing still set.
		if(typing)
			this.typing.set(false);

		return;
	}

	if(typing && input.length == 0)
	{
		this.typing.set(false);
		return;
	}

	if(!typing && input.length)
	{
		this.typing.set(true);
		return;
	}
};
