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
 * I/O
 *
 * Lower-level protocol suite. 
 */

/**
 ******************************************************************************
 * Lower-level network related subsystems
 *
 *****************************************************************************
 */
mc.io = {};

/** Global IO stats
 */
mc.io.stats =
{
	aborts: 0,
	errors: 0,
};

/** Global stats for data transmitted.
 */
mc.io.stats.sent =
{
	bytes: 0,
	msgs: 0,
};

/** Global stats for data received.
 */
mc.io.stats.recv =
{
	bytes: 0,
	msgs: 0,
};

/** Active requests table.
 *
 * This is a collection to provide a way to iterate through all pending requests.
 * You should not manipulate this collection; requests add and remove themselves.
 * To cancel a request or all requests, use the io.cancel() suite.
 */
mc.io.requests =
{
	// URL => request
};

/** Cancel a request by URL.
 *
 * @returns a promise which can be awaited on.
 */
mc.io.cancel = function(url, reason = "Aborted")
{
	let request = mc.io.requests[url];
	let promise = request.abort(reason);
	mc.io.stats.aborts++;
	return promise;
}

/** Cancel all requests.
 *
 * @returns a promise of all() the cancelations which can be awaited on.
 */
mc.io.cancel.all = function(reason = "Aborted")
{
	let cancel = (url) => mc.io.cancel(url, reason);
	let urls = Object.keys(mc.io.requests);

	let count = urls.length;
	console.warn("Interrupting " + count + " pending IO requests (reason: " + reason + ")");

	let futures = urls.map(cancel);
	return Promise.all(futures);
};

/** Add a request to the requests collection. Called automatically by request
 * object constructor. You do not need this.
 */
Object.defineProperty(mc.io.requests, 'insert', {
writable: false,
enumerable: false,
configurable: false,
value: function(request)
{
	let key = request.url;
	if((key in mc.io.requests))
		return false;

	mc.io.requests[key] = request;
	return true;
}});

/** Remove a request from the requests collection. Called automatically by request
 * object destructor. You do not need this.
 */
Object.defineProperty(mc.io.requests, 'remove', {
writable: false,
enumerable: false,
configurable: false,
value: function(request)
{
	let key = request.url;
	delete mc.io.requests[key];
}});
