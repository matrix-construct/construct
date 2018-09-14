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
 * Input history (bash style history)
 *
 * this = room
 */
room.input.history = {};

room.input.history.history = function()
{
	if(!Array.isArray(this.session.history))
		this.session.history = [];

	return this.session.history;
};

/** Add text to the history
 * (use enter() for official user input)
 */
room.input.history.add = function(text)
{
	let history = this.input.history.history();
	history.push(text);
};

/** Get text from the history
 */
room.input.history.get = function(pos = 0)
{
	let history = this.input.history.history();
	return history[history.length - pos - 1];
};

/** The number of lines in the history
 */
room.input.history.len = function()
{
	let history = this.input.history.history();
	return maybe(() => history.length)? history.length : 0;
};

/** Enter text into the history and clear the input
 */
room.input.history.enter = function(text)
{
	this.input.history.add(text);
	this.input.history.pos = undefined;
	this.control.input = "";
};

/** Handle an up-arrow event by restoring text from
 * history into the input.
 */
room.input.history.up = function(event)
{
	let pos = this.input.history.pos;
	if(pos === undefined || pos < 0)
	{
		this.input.history.add(this.control.input);
		this.input.history.pos = pos = 1;
	}

	if(pos > this.input.history.len() - 1)
		return;

	this.control.input = this.input.history.get(pos);

	if(pos < this.input.history.len() - 1)
		++this.input.history.pos;
};

/** Handle a down-arrow event by restoring text from
 * history into the input.
 */
room.input.history.down = function(event)
{
	let pos = this.input.history.pos;
	if(pos === undefined)
		return;

	if(pos < 0)
	{
		this.input.history.add(this.control.input);
		this.control.input = "";
	}
	else this.control.input = this.input.history.get(pos);

	if(pos >= 0)
		--this.input.history.pos;
};
