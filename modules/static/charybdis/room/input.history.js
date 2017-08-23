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
