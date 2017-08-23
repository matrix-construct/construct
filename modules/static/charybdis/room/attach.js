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

/** Handles an input[type=file] onchange event to read files
 * from the user and store it in the model.
 *
 * this = room
 */
room.prototype.attach = function(event)
{
	return room.attach.call(this, event);
};

/**
 * this = room
 */
room.attach = function(event)
{
	let e = event.target;
	let files = e.files;
	Object.each(files, (i, file) =>
	{
		if((file.name in this.control.files))
			return;

		this.control.files[file.name] = file;

		// this opts eventually becomes the context of client.io.request()
		file.opts = {};

		file.reader = new FileReader();
		file.reader.onloadstart = room.attach.event.bind(this, file);
		file.reader.onprogress = room.attach.event.bind(this, file);
		file.reader.onloadend = room.attach.onloadend.bind(this, file);
		file.reader.onabort = room.attach.onabort.bind(this, file);
		file.reader.readAsArrayBuffer(file);
	});

	// Once the files are grabbed the <input type="file"> must be cleared for reuse.
	$(e).val("");
};

/** Handles the user X'ing an attachment.
 *
 * this = room
 */
room.attach.cancel = function(event, file)
{
	file = this.control.files[file.name];
	if(!file)
		return;

	switch(file.reader.readyState)
	{
		case FileReader.EMPTY:
		case FileReader.DONE:
			delete this.control.files[file.name];
			break;

		case FileReader.LOADING:
			file.reader.abort();
			break;
	}
};

/**
 *
 * this = room
 */
room.attach.upload = function(event, file)
{
	if(file.reader.readyState != file.reader.DONE)
		return;

	let input = this;
	let opts = file.opts;
	Object.update(opts,
	{
		on:
		{
			progress: (event) => input.attach.event.call(input, file, event),
			abort: (event) => input.attach.onabort.call(input, file, event),
		},

		headers:
		{
			"content-type": file.type,
		},

		digest: true,
	});

	opts.content = new Uint8Array(file.reader.result);
	file.request = mc.m.media.upload.post(opts, (error, data) =>
	{
		if(error)
		{
			file.error = error;
			return;
		}

		delete opts.headers;
		opts.content =
		{
			url: data.content_uri,
			body: file.name,
		};
	});
};

/** Passes event data for digest by angular
 *
 * this = room
 */
room.attach.event = function(file, event)
{
	file.progress = event;
	client.apply();
};

/** Passes event data for digest by angular
 *
 * this = room
 */
room.attach.onloadend = function(file, event)
{
	file.progress = event;
	room.attach.upload.call(this, event, file);
	client.apply();
};

/** 
 * this = room
 */
room.attach.onabort = function(file, event)
{
	delete this.control.files[file.name];
	client.apply();
};
