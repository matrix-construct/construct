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

/******************************************************************************
 *
 * Main
 *
 */

/**
 * Main synchronous loop. This drives the client by receiving updates from
 * /sync or tries to figure out why it can't, fix it, and then continues to
 * poll /sync. Otherwise if there is no hope that /sync can ever be called,
 * the client is cleanly shut down and the function returns. Call mc.run()
 * after this happens.
 *
 * Any exceptions out of here are abnormal and the window should be reloaded.
 */
mc.main = async function()
{
	var ret = 0;
	let sopts = {};
	if(await mc.main.init()) while(1) try
	{
		// longpolls and processes data from a /sync request
		mc.ng.apply.later();
		await mc.sync(sopts);
	}
	catch(error)
	{
		// When sync() throws, the error is evalulated for whether or not
		// to call sync() again or to clean shutdown. Throwing another exception
		// here (double fault) results in program abort.
		if((await mc.main.fault(error)) === true)
			continue;
		else
			break;
	}

	// The destruction sequence should always be reached except on abort.
	await mc.main.fini();
	return ret;
};

/**
 * Place for things that need to happen before the main loop.
 */
mc.main.init = async function()
{
	// WebStorage related
	console.log("Loading WebStorage...");
	mc.storage.init();

	// Angular related
	console.log("Configuring Angular...");
	mc.ng.init();

	// Local room interfaces
	console.log("Creating local pseudo-rooms...");
	mc.settings.init();
	mc.console.init();

	// This event will break the main loop and allow a clean shutdown.
	window.addEventListener("beforeunload", mc.main.beforeunload,
	{
		passive: false,
		once: true,
	});

	// Fault this manually to ensure authenticity for now.
	console.log("Logging in...");
	let errors = {};
	return await mc.main.fault["M_MISSING_TOKEN"](errors);
};

/**
 * Place for things that need to happen after the main loop.
 */
mc.main.fini = async function()
{
	console.log("Stopping remaining tasks..."); try
	{
		await mc.main.interrupt();
	}
	catch(error)
	{
		console.error("Error synchronizing account data: " + error);
	}

	console.log("Synchronizing account_data..."); try
	{
		await mc.storage.account.sync();
	}
	catch(error)
	{
		console.error("Error synchronizing account data: " + error);
	}

	console.log("Logging out..."); try
	{
		await mc.auth.logout();
	}
	catch(error)
	{
		console.error("Error logging out: " + error);
	}
	finally
	{
		mc.main.on_logout();
	}

	console.log("Synchronizing WebStorage..."); try
	{
		mc.storage.sync();
	}
	catch(error)
	{
		console.error("Error synchronizing WebStorage: " + error);
	}
};

/**
 */
mc.main["beforeunload"] = function(event)
{
	mc.main.interrupt();
	event.preventDefault();
	event.stopPropagation();
	return false;
};

mc.main.interrupt = function()
{
	return Promise.all(
	[
		mc.io.cancel.all("interrupt"),
	]);
};

/**
 * @returns boolean whether the sync loop can continue. If false,
 * the client has a clean exit. Throws to crash with exception.
 */
mc.main.fault = async function(error)
{
	if(maybe(() => error.m.errcode))
		if((error.m.errcode in mc.main.fault))
			return mc.main.fault[error.m.errcode](error);

	switch(error.status)
	{
		case "Client Side":
			if(error.name == "abort")
				return false;

		default:
			throw error;
	}
};

/**
 * Called when main() faults with an authentication issue. This function
 * invokes the mc.auth system to try and get credentials for the user to
 * allow sync() to continue. A guest registration may occur in desperation.
 * If no access token is obtained then false is returned.
 */
mc.main.fault["M_MISSING_TOKEN"] = async function(error)
{
	error.auth = {};
	if((await mc.auth(error.auth)) === true)
	{
		await mc.main.on_login();
		return true;
	}

	for(let i in error.auth)
	{
		let flow = error.auth[i];
		let type = flow.type;
		let result = flow.result;
		console.error("Authentication via " + type + ": " + result);
	}

	mc.main.on_logout();
	mc.ng.apply();
	return false;
};


/* Most of this logic can probably be moved into Angular directives
 */
mc.main.on_login = async function()
{
	await mc.filter.init();

	mc.main.menu["ROOMS"].hide = false;
	mc.main.menu["MENU"].hide = false;
	mc.show["#charybdis_rooms"] = true;

	if(!mc.session.guest)
	{
		mc.show["#charybdis_login"] = false;
		mc.main.menu["LOGOUT"].hide = false;
		mc.main.menu["LOGIN"].hide = true;
	}

	if(empty(maybe(() => mc.local.rooms.current)))
	{
		mc.show["#charybdis_menu"] = true;
		if(mc.session.guest)
			mc.show["#charybdis_motd"] = true;
		else
			mc.show["#charybdis_motd"] = false;
	} else {
		mc.show["#charybdis_menu"] = false;
		mc.show["#charybdis_motd"] = false;
	}
};

/* Most of this logic can probably be moved into Angular directives
 */
mc.main.on_logout = function()
{
	mc.main.menu["ACCOUNT"].hide = true;
	mc.main.menu["ROOMS"].hide = true;
	mc.main.menu["USERS"].hide = true;
	mc.main.menu["LOGIN"].hide = false;
	mc.main.menu["LOGOUT"].hide = true;
	mc.main.menu["MENU"].hide = true;
	mc.show["#charybdis_menu"] = true;
	mc.show["#charybdis_login"] = true;
	mc.show["#charybdis_rooms"] = false;
};

/**
 * Main menu
 */
mc.main.menu =
{
	"MENU":
	{
		icon: "fa-bars",
		target: "#charybdis_menu",
		hide: true,
	},

	"IRCd":
	{
		icon: "fa-home",
	},

	"ROOMS":
	{
		icon: "fa-th",
		hide: true,
		sticky: true,
		target: "#charybdis_rooms_main",
	},

	"USERS":
	{
		icon: "fa-users",
		hide: true,
		sticky: true,
		target: "#charybdis_users",
	},

	"ACCOUNT":
	{
		icon: "fa-user",
		hide: true,
		sticky: true,
		target: "#charybdis_account",
	},

	"SETTINGS":
	{
		icon: "fa-cogs",
		selected: () => mc.rooms.current("!settings:localhost"),
		click: function(event)
		{
			if(this.selected())
				mc.rooms.current.del("!settings:localhost");
			else
				mc.rooms.current.add("!settings:localhost");
		},
	},

	"HELP":
	{
		icon: "fa-question-circle",
	},

	"LOGIN":
	{
		icon: "fa-sign-in",
		target: "#charybdis_login",
		click: (event) =>
		{
			let login = $(this.target);
			$(login).find("#charybdis_login_form input[name=username]").focus();
		},
	},

	"LOGOUT":
	{
		icon: "fa-sign-out",
		hide: true,
		click: (event) =>
		{
			mc.auth.logout.event = event;
		},
	},
};
