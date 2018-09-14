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
		// Calling apply() before and after the await is insufficient because
		// we want an application of what mc.sync() also did before waiting a
		// long time for the response. The apply.later() pushes an async apply
		// to the js queue which paints once after this stack starts to wait.
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

		console.error("Unable to recover from main fault: shutting down...");
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
	mc.rooms.init();
	mc.settings.init();
	mc.console.init();

	// This event will break the main loop and allow a clean shutdown.
	window.addEventListener("beforeunload", mc.main.beforeunload,
	{
		passive: false,
		once: true,
	});

	// Fault this manually to ensure authenticity for now.
	console.log("Initial login attempt...");
	let errors =
	{
		m:
		{
			errcode: "M_MISSING_TOKEN",
		},
	};

	mc.apply.later();
	if(!(await mc.main.fault(errors)))
	{
		let catch_elem = (idx, flow) =>
			flow.result.element = $("#charybdis_login_form");

		Object.each(errors.auth, catch_elem);
		return false;
	}

	mc.storage.sync();
	mc.apply();
	return true;
};

/**
 * Place for things that need to happen after the main loop.
 */
mc.main.fini = async function()
{
	console.log("Synchronizing WebStorage..."); try
	{
		mc.storage.sync();
	}
	catch(error)
	{
		console.error("Error synchronizing WebStorage: " + error);
	}

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

	console.log("Resynchronizing WebStorage..."); try
	{
		mc.storage.sync();
	}
	catch(error)
	{
		console.error("Error synchronizing WebStorage: " + error);
	}

	console.log("Final angular repaint..."); try
	{
		delete mc.ng.root().error;
		delete mc.ng.mc().error;
		mc.ng.apply();
	}
	catch(error)
	{
		console.error("Error repainting: " + error);
	}
};

/**
 */
mc.main["beforeunload"] = function(event)
{
	mc.main.interrupt();
	event.preventDefault();
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
		case "Client":
			if(error.name == "disconnected")
			{
				console.warn("client disconnected");
				mc.unhandled(error);
				return false;
			}

			if(error.name == "killed")
			{
				console.error("client fatal");
				mc.unhandled(error);
				return false;
			}

			if(error.name == "timeout")
			{
				console.warn("client timeout");
				mc.ng.root().error = undefined;
				mc.ng.mc().error = undefined;
				mc.ng.apply.later();
				await new Promise((res, rej) =>
				{
					mc.timeout(5000, () =>
					{
						res();
					});
				});

				return true;
			}

			console.warn("client unhandled " + error);
			mc.unhandled(error);
			return false;

		default:
			console.error("fault unhandled");
			throw error;
	}

	return false;
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
	mc.rooms.current.add("!rooms:mc");

	if(!mc.session.guest)
	{
		mc.show["#charybdis_login"] = false;
		mc.main.menu["LOGOUT"].hide = false;
		mc.main.menu["LOGIN"].hide = true;
	}

	if(!maybe(() => mc.rooms.current.list.length))
	{
		mc.show["#charybdis_menu"] = true;
	} else {
		mc.show["#charybdis_menu"] = false;
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
	mc.rooms.current.clear();
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

	"ROOMS":
	{
		icon: "fa-th",
		hide: true,
		sticky: true,
		selected: () => mc.rooms.current("!rooms:mc"),
		click: function(event)
		{
			if(this.selected())
				mc.rooms.current.del("!rooms:mc");
			else
				mc.rooms.current.add("!rooms:mc");
		},
	},

	"IRCd":
	{
		icon: "fa-home",
		selected: () => mc.rooms.current("!home:mc"),
		click: function(event)
		{
			if(this.selected())
				mc.rooms.current.del("!home:mc");
			else
				mc.rooms.current.add("!home:mc");
		},
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
		selected: () => mc.rooms.current("!settings:mc"),
		click: function(event)
		{
			if(this.selected())
				mc.rooms.current.del("!settings:mc");
			else
				mc.rooms.current.add("!settings:mc");
		},
	},

	"HELP":
	{
		icon: "fa-question-circle",
		selected: () => mc.rooms.current("!help:mc"),
		sticky: true,
		click: function(event)
		{
			if(this.selected())
				mc.rooms.current.del("!help:mc");
			else
				mc.rooms.current.add("!help:mc");
		},
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
			mc.auth.logout();
		},
	},
};
