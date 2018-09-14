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
 */
mc.io.request = class
{
	/** Create a new IO request. The url is normally undefined so it can
	 * be composed properly from the ctx. If the URL is defined it will
	 * override any composition.
	 */
	constructor(ctx = {}, callback = undefined)
	{
		if(callback)
			ctx.callback = callback;

		mc.io.request.constructor.call(this, ctx);
	}

	/** Multi-purpose function dealing with HTTP headers for both directions.
	 * - Note: It's better to set headers in the ctx.headers dictionary.
	 * + If key and value are defined, a request header is set.
	 * + If only key is defined, a response header is returned for that key.
	 * + If neither is defined, all response headers are returned as a string.
	 */
	header(key = undefined, val = undefined)
	{
		return mc.io.request.header.call(this, key, val);
	}

	/** Receive a promise which will resolve to the next XHR event.type.
	 *
	 * @param type - If a string, an event type name which can be registered
	 * with xhr i.e "progress". If a number, an XHR readyState.
	 *
	 * @returns Promise - await for the event 
	 */
	promise(type)
	{
		return mc.io.request.promise.call(this, type);
	}

	/** Abort this request.
	 *
	 * @returns Promise - await for the request to be aborted;
	 * - promise resolves to the abort event or rejects with mc.error
	 */
	abort(reason = "")
	{
		mc.io.request.abort.call(this, reason);
		return this.promise("abort");
	}

	/** Convenience to get a promise resolving to the final result
	 * data of this request, or rejecting with the final error.
	 */
	get response()
	{
		return this.promise("response");
	}

	/** Convenience to current xhr state
	 */
	get state()
	{
		return this.xhr.readyState;
	}

	/** Convenience to get the composed url stored in the ctx
	 */
	get url()
	{
		return this.ctx.url;
	}
};

/** Default options for a request.
 */
mc.io.request.ctx =
{
	// default response type expectation for xhr,
	responseType: "json",

	// default timeout in milliseconds
	timeout: 30 * 1000,

	// HTTP query string dictionary `?key=value&`
	query: {},

	headers:
	{
		// default request type submission, added to headers
		"content-type": "application/json; charset=utf-8",
	},

	// Content object is turned into JSON when content-type is
	// set for json (by default), otherwise assign content anything
	// which xhr.send() can take.
	content: {},

	// defaulting
	method: "GET",
	prefix: "",

	digest: false,
};

mc.io.request.constructor = function(ctx = {})
{
	// Setup the context for this request.
	this.ctx = ctx;

	// Default structure for anything required and not included by the user.
	Object.defaults(this.ctx, mc.io.request.ctx);

	// Remove keys with values that won't be sent, but leave the potentially
	// empty objects to maintain the structure.
	Object.clean(this.ctx.query);
	Object.clean(this.ctx.headers);
	Object.clean(this.ctx.content);

	// Dictionary holding the state of promises registered to resolve for events by type.
	// To register a promise use the request.promise(type). The format of this structure:
	// type => { reject: [], resolve: [] }.
	this.promised = {};

	// The top of the request stack is saved here to debug the request later.
	this.stack = (new Error).stack;

	// The URL is unconditionally regenerated (the url in ctx is overwritten); this is because
	// the components may change between calls and we don't require the user to know how to
	// manipulate the cached url value. The user should manipulate components in ctx only.
	this.ctx.url = mc.io.request.constructor.url.call(this);

	// Setup IO for this request.
	this.xhr = mc.io.request.constructor.xhr.call(this);

	// Insert request into active table. The request can be aborted hereafter.
	mc.io.requests.insert(this);

	this.started = mc.now();

	// This should be done here for now
	this.xhr.open(this.ctx.method, this.ctx.url);

	console.log(this.ctx.method
	           + " " + this.ctx.resource
	           + " " + this.ctx.url
	           + " sending " + maybe(() => this.ctx.content.length) + " bytes"
	           );
}

/** Construct XHR related for the request.
 *
 */
mc.io.request.constructor.xhr = function()
{
	let xhr = new XMLHttpRequest();
	xhr.timeout = this.ctx.timeout;
	xhr.responseType = this.ctx.responseType;
	mc.io.request.constructor.xhr.bindings.call(this, xhr);
	return xhr;
};

/** Walks the mc.io.request.on tree and binds those handlers to the request
 * and registers them with xhr for both uploading and downloading.
 */
mc.io.request.constructor.xhr.bindings = function(xhr)
{
	let options =
	{
		//passive: true,
	};

	let upload = function(handler, event)
	{
		event.upload = true;
		handler.call(this, event);
		mc.io.request.promise.resolve.call(this, event.type, event);
	};

	let download = function(handler, event)
	{
		handler.call(this, event);
		mc.io.request.promise.resolve.call(this, event.type, event);
	};

	Object.each(mc.io.request.on, (name, handler) =>
	{
		xhr.addEventListener(name, download.bind(this, handler), options);
		xhr.upload.addEventListener(name, upload.bind(this, handler), options);
	});
};

/** Generates a full URL for this request based on the amalgam of
 * context components. 
 */
mc.io.request.constructor.url = function(base_url = mc.opts.base_url)
{
	let ctx = this.ctx;
	let query = !empty(ctx.query)? "?" + $.param(ctx.query, false) : "";
	let prefix = ctx.prefix !== undefined? ctx.prefix : "";
	let resource = ctx.resource !== undefined? ctx.resource : "/";
	let version = mc.io.request.constructor.url.version.call(this, ctx);
	let url = base_url + prefix + version + resource + query;
	return url;
};

mc.io.request.constructor.url.version = function(ctx)
{
	if(typeof(ctx.version) == "number")
		return "r" + ctx.version + "/";

	if(ctx.version === null)
		return "";

	if(!empty(ctx.version))
		return ctx.version;

	return "r" + mc.server.version + "/";
};

/** Called when the request reaches the xhr.DONE state
 */
mc.io.request.destructor = function()
{
	mc.io.requests.remove(this);
	mc.io.request.destructor.stats.call(this);

	let xhr = this.xhr;
	let ctx = this.ctx;
	let type = xhr.responseType;
	let bytes_up = this.uploaded();
	let bytes_down = this.loaded();
	console.log(xhr.status
	           + " " + ctx.method
	           + " " + ctx.resource
	           + " sent:" + bytes_up
	           + " recv:" + bytes_down
	           + " " + type
	           );
};

/**
 * TODO: byte counting should probably be done incrementally on progress
 */
mc.io.request.destructor.stats = function()
{
	let bytes_up = this.uploaded();
	let bytes_down = this.loaded();
	mc.io.stats.sent.bytes += bytes_up;
	mc.io.stats.recv.bytes += bytes_down;
};

/** Interrupt the request
 */
mc.io.request.abort = function(reason = "")
{
	this.reason = reason;
	if(this.xhr !== undefined)
		this.xhr.abort();
}

mc.io.request.prototype.loaded = function()
{
	return 0;
}

mc.io.request.prototype.uploaded = function()
{
	return 0;
}

mc.io.request.prototype.total = function()
{
	return 0;
}

mc.io.request.prototype.uptotal = function()
{
	return 0;
}

/** User promise generation
 *
 * Receive a promise for an xhr event type which is resolved when this request
 * gets a callback for that type. The resolution data will be the xhr event.
 *
 * Additionally, numerical keys promise to be resolved when the xhr.readyState
 * meets that state.
 *
 * Additionally, the key "response" will return a promise which is settled when the
 * request has finalized, resolving to the result data or rejecting with error.
 */
mc.io.request.promise = function(type)
{
	if(type != "response" && typeof(type) != "number" && !(type in mc.io.request.on))
		console.warn("Trying to register an unrecognized xhr event '" + type + "'");

	if(!(type in this.promised))
		mc.io.request.promise.init.call(this, type);

	return new Promise((resolve, reject) =>
	{
		this.promised[type].resolve.push(resolve);
		this.promised[type].reject.push(reject);
	});
};

/** Called to resolve promises for an event.
 */
mc.io.request.promise.resolve = function(type, event)
{
	if(!(type in this.promised))
		return;

	let promised = this.promised[type];
	promised.resolve.forEach((resolve) => resolve(event));
	mc.io.request.promise.clear.call(this, type);
}

/** Called to reject promises registered for a type with the error.
 */
mc.io.request.promise.reject = function(type, error)
{
	if(!(type in this.promised))
		return;

	let promised = this.promised[type];
	promised.reject.forEach((reject) => reject(error));
	mc.io.request.promise.clear.call(this, type);
}

/** Called when a new type must be added to the promised collection.
 */
mc.io.request.promise.init = function(type)
{
	this.promised[type] =
	{
		resolve: [],
		reject: [],
	};
};

/** Called when a promises for a type have been fulfilled
 */
mc.io.request.promise.clear = function(type)
{
	if(!(type in this.promised))
		return;

	this.promised[type].resolve = [];
	this.promised[type].reject = [];
};

/** Called to reject any promises registered.
 */
mc.io.request.promise.reject.all = function(error)
{
	Object.each(this.promised, (type) =>
	{
		mc.io.request.promise.reject.call(this, type, error);
	});
}

mc.io.request.on = {};

mc.io.request.on.readystatechange = function(event)
{
	let state = this.xhr.readyState;
	let handler = mc.io.request.on.readystatechange[state];
	this.event = event;

	// The promise is resolved for the user before the lib's handlers are called.
	// This is because the DONE handler has to fulfill all outstanding promises for
	// this request entirely -- that would include the promise on DONE itself.
	mc.io.request.promise.resolve.call(this, state, event);

	if(handler !== undefined)
		handler.call(this, event, state);
}

mc.io.request.on.readystatechange[XMLHttpRequest.OPENED] = function(event)
{
	// When headers are preset in the context they are auto registered.
	if(!empty(this.ctx.headers))
		mc.io.request.header.set.defaults.call(this);

	let content = this.ctx.content;
	let headers = this.ctx.headers;
	let content_type = headers["content-type"];

	// Handle automatic JSON. The ctx's content is always sent in default JSON
	// mode, even when it's empty. The only way to prevent that so you can call
	// send() yourself is to null it.
	if(content != null && (!content_type || content_type.startsWith("application/json")))
	{
		let json = JSON.stringify(content);
		this.xhr.send(json);
		return;
	}

	// Handle automatic other
	if(content)
		this.xhr.send(content);
};

mc.io.request.on.readystatechange[XMLHttpRequest.HEADERS_RECEIVED] = function(event)
{
	//console.log("" + xhr.getAllResponseHeaders());
};

mc.io.request.on.readystatechange[XMLHttpRequest.LOADING] = function(event)
{
};

mc.io.request.on.readystatechange[XMLHttpRequest.DONE] = function(event)
{
	mc.io.request.destructor.call(this);

	let code = parseInt(this.xhr.status);
	switch(Math.floor(code / 100))
	{
		case 2:   // 2xx
			return mc.io.request.success.call(this, event);

		case 3:   // 3xx
		case 4:   // 4xx
		case 5:   // 5xx
		case 0:   // xhr error
		default:  // unknown/unhandled
			return mc.io.request.error.call(this, event);
	}
};

mc.io.request.on.abort = function(event)
{
	//console.log("ABT " + this.ctx.method + " " + this.ctx.resource + " " + this.reason);
	mc.io.request.promise.resolve.call(this, "abort", event);
}

mc.io.request.on.loadstart = function(event)
{
}

mc.io.request.on.progress = function(event)
{
	if(event.upload)
		mc.io.stats.sent.msgs++;
	else
		mc.io.stats.recv.msgs++;

}

mc.io.request.on.load = function(event)
{
}

mc.io.request.on.timeout = function(event)
{
	console.log("TIM " + this.ctx.method + " " + this.ctx.resource + " " + this.ctx.url);
}

mc.io.request.on.loadend = function(event)
{
}

mc.io.request.on.error = function(event)
{
	console.log("ERR " + this.ctx.method + " " + this.ctx.resource + " " + this.ctx.url);
}

mc.io.request.success = function(event)
{
	let error = undefined;
	let data = this.xhr.response;
	mc.io.request.continuation.call(this, error, data);
};

mc.io.request.error = function(event)
{
	let error = new mc.error(
	{
		message:
			!empty(this.reason)?
				this.reason:
			this.response && this.xhr.responseType == "text"?
				this.xhr.responseText:
			this.started + this.ctx.timeout <= mc.now()?
				"timeout":
			maybe(() => this.event.detail)?
				this.event.detail:
			undefined,

		name:
			this.xhr.statusText == "error"?
				"Network Request Error":
			this.xhr.statusText == "abort"?
				"Network Request Canceled":
			!empty(this.xhr.statusText)?
				this.xhr.statusText:
			this.started + this.ctx.timeout <= mc.now()?
				"timeout":
			!empty(this.reason)?
				this.reason:
			!window.navigator.onLine?
				"disconnected":
			this.started + 10 > mc.now()?
				"killed":
			"timeout",

		status:
			this.xhr.status != 0? this.xhr.status : "Client",

		m:
			this.xhr.responseType == "json"? this.xhr.response : undefined,

		request_stack:
			this.stack,

		event:
			this.event,

		element:
			this.ctx.element,
	});

	//if(!empty(error.m))
	//	delete error.message;

	let data = undefined;
	mc.io.request.continuation.call(this, error, data);
};

mc.io.request.continuation = function(error, data)
{
	try
	{
		if(this.ctx.callback)
			this.ctx.callback(error, data);

		if(error)
			mc.io.request.promise.reject.all.call(this, error);
		else
			mc.io.request.promise.resolve.call(this, "response", data);
	}
	catch(exception)
	{
		mc.io.request.continuation.unhandled.call(this, exception);
	}
	finally
	{
		if(this.ctx.digest)
			mc.ng.apply();
	}
};

mc.io.request.continuation.unhandled = function(exception)
{
	Object.defaults(exception,
	{
		element: this.ctx.element,
		response_stack: exception.stack,
		request_stack: this.stack,
	});

	mc.unhandled(exception);
};

mc.io.request.header = function(key = undefined, val = undefined)
{
	if(key && val)
		return mc.io.request.header.set.call(this, key, val);

	if(key)
		return mc.io.request.header.get.call(this, key);

	return this.xhr.getAllResponseHeaders();
};

mc.io.request.header.get = function(key)
{
	return this.xhr.getResponseHeader(key);
};

mc.io.request.header.set = function(key, val)
{
	this.xhr.setRequestHeader(key, val);
};

mc.io.request.header.set.defaults = function()
{
	Object.each(this.ctx.headers, (key, val) =>
	{
		mc.io.request.header.set.call(this, key, val);
	});
};
