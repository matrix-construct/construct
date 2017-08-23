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
