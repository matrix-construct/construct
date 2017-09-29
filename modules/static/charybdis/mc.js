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
 ****************************************************************************
 ****************************************************************************
 *
 * IRCd (Charybdis) Client
 *
 *
 * The mission of this client is to provide a fully functioning Matrix chat experience and
 * drive the server administration functions of IRCd.
 *
 * This is a singleton object; it takes unique ownership of the window/session and not reused.
 * It is composed during the initial script execution itself.
 *
 */
ircd.mc = {};

// alias
let mc = ircd.mc;
let client = mc;

/**************************************
 *
 * Primary Client Options
 *
 */
mc.opts =
{
	// Base URL to use if there is no real window location (i.e browsing from file://)
	//base_url: "https://matrix.org:8448",
	base_url: String(window.location.origin),

	// The root element for the application in the DOM.
	root: "#charybdis",

	// We can reapply the stylesheet dynamically if its path is specified here. This is
	// useful for developers, and accomplished using the mc.ctrl system which may hook
	// ctrl+r to refresh the style rather than the whole application.
	style: "charybdis.css",

	// Quick setting to debug incoming sync messages on the console
	sync_debug: 10,

	account_data:
	{
		key: "ircd_storage",
	},
};

/**
 * This object is synchronized with window.localStorage. Data in this object
 * will be associated with the mc.opts.base_url and preserved indefinitely.
 */
mc.local =
{
	// mxid (copied from mc.session when the session is not a guest)
	user_id: undefined,

	// password (when the user_id uses a password we save it here in nakedtext for now)
	wasspord: undefined,

	// Monotonic counter for non-random ID generation, for events etc.
	txnid: 0,

	// Room specific local data; room_id => storage object
	room: {},
};

/**
 * This object is synchronized with window.sessionStorage. Data in this object
 * will be associated with the mc.opts.base_url and preserved across navigation
 * and reloads but not when the window closes.
 */
mc.session =
{
	// mxid (applied by server)
	user_id: undefined,

	// last access_token (applied by server)
	access_token: undefined,

	// If the session has guest status (derived by client)
	guest: true,

	// filter_id => string; string is a key in mc.filter[]
	filter: {},

	// Room specific local data; room_id => storage object
	room: {},

	show: {},
};

//TODO: xxx
mc.show = mc.session.show;

/**
 * This object is synchronized with the server via matrix account data. Sync
 * will occur automatically on window unload unless a manual sync is done.
 *
 * Matrix account data event type charybdis_storage
 */
mc.account =
{

};

/**
 * This object is not synchronized and defaults every time. Purpose is a place
 * to organize ephemeral data on the same plane as the other two synchronized objects.
 */
mc.instance =
{
	// True when the client is ready for main service. This is after the initial script
	// execution, angular controller initializations, and finally the jQuery/document
	// ready callbacks. Setting it to false has effects and shuts down the mc.
	ready: false,

	// True when the auth-related state of the mc.session object is usable as the
	// current session. On load when the session is fetched from local storage,
	// 'authentic' remains false until a login/refresh with the server is made proving
	// the authentication data in mc.session is still valid.
	authentic: false,

	// True when the user is considered present. Inputs drawn below and outputs
	// probably on m.presence state for matrix among others. ["online", "offline", "unavailable"]
	presence: "online",

	// Counts the number of Angular digests seen by the application root. This data is useful
	// when identifying microtask-slicing with JS caused by async/await which we have carefully
	// integrated together with Angular. Seeing the digest counter increment or remain the same
	// across a call to await *MAY* be an indication for contiguity across an await. This is
	// only possible because digests tend to run after every event -- which includes hitting
	// the first await of a handler.
	digests: 0,

	room: {},
};

window.addEventListener('focus', () => mc.instance.presence = "online");
window.addEventListener('blur', () => mc.instance.presence = "offline");

/**
 * Collection of information about the server this client is connected to.
 */
mc.server =
{
	get host()
	{
		return String(window.location.host);
	},

	get domain()
	{
		return String(window.location.hostname);
	},

	get version()
	{
		return 0;
	},
};
