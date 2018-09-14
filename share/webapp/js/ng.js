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
 *
 * Client's Angular application
 *
 */

mc.ng = {};

mc.opts.ng =
{
	resource_url_whitelist:
	[
		/* Allow same origin resource loads. */
		"self",

		/* Allow loading from our assets domain.  Notice the difference between * and **. */
		"https://*.youtube.com/**"

		/**/
	],

	resource_url_blacklist:
	[
	],

	// tell angular to allow mxc:// URL's for images
	img_src_sanitation_whitelist: /^\s*(https?|ftp|file|mxc):/,
};

mc.ng.app = angular.module('ircd',
[
	// 'ngAnimate',
]);

mc.ng.app.config(['$rootScopeProvider', function($rootScopeProvider)
{
	$rootScopeProvider.digestTtl(16);
}]);

mc.ng.app.config(['$compileProvider', function($compileProvider)
{
	$compileProvider.imgSrcSanitizationWhitelist(mc.opts.ng.img_src_sanitation_whitelist);
}]);

mc.ng.app.config(['$sceDelegateProvider', function($sceDelegateProvider)
{
	$sceDelegateProvider.resourceUrlWhitelist(mc.opts.ng.resource_url_whitelist);
	$sceDelegateProvider.resourceUrlBlacklist(mc.opts.ng.resource_url_blacklist);
}]);

/**
 * Things in here can't take place at the global init; they take place after
 * the document/main is ready.
 */
mc.ng.init = function()
{
	let root = mc.ng.root();
	root.$watch(() =>
	{
		mc.instance.digests++;
		return null;
	});
};

/**
 * @returns the formal root scope of the app
 */
mc.ng.root = function()
{
	let body = angular.element(document.body);
	let root = body.scope().$root;
	return root;
};

/**
 * @returns the mc scope which is our root scope for the app. This is directly under
 * the real root, and all our client scopes are children of the mc scope. Other children
 * of the real root may exist but are only indirectly related.
 */
mc.ng.mc = function()
{
	let charybdis = angular.element("#charybdis");
	return charybdis.scope();
};

/**
 * Trigger Angular manually
 */
mc.ng.apply = function(closure = undefined)
{
	let root = mc.ng.root();
	if(root.$$phase == "$apply")
	{
		console.warn("Unnecessary mc.ng.apply(); " + (new Error));
		return;
	}

	return root.$apply(closure);
};

// Alias
mc.apply = mc.ng.apply;

/**
 * Trigger Angular manually
 */
mc.ng.apply.later = function(closure = () => {})
{
	let root = mc.ng.root();
	root.$applyAsync(closure);
};

/**
 * Trigger Angular manually
 */
mc.ng.apply.animate = function(callback)
{
	let handler = () =>
	{
		if(callback() !== false)
			mc.ng.apply.later();
	};

	return window.requestAnimationFrame(handler);
};

/**
 * Trigger Angular manually
 */
mc.ng.apply.idle = function(timeout, callback)
{
	if(typeof(timeout) == "function")
	{
		callback = timeout;
		timeout = undefined;
	}

	let handler = (...params) =>
	{
		if(callback(...params) !== false)
			mc.ng.apply.later();
	};

	let opts =
	{
		timeout: timeout,
	};

	return window.requestIdleCallback(handler, opts);
};

/**
 * Trigger Angular manually
 */
mc.ng.apply.defer = function()
{
	let root = mc.ng.root();
	let watchers = root.$$watchers;
	root.$$watchers = [];
	root.$applyAsync(() =>
	{
		root.$$watchers = watchers;
	});
};

/**
 * Use mc.ng.timeout instead of window.setTimeout(), as the latter
 * issues an event which is unknown to Angular.
 */
mc.ng.timeout = function(timeout, closure)
{
	if(typeof(timeout) == "function")
	{
		closure = timeout;
		timeout = undefined;
	}

	//TODO: XXX: arbitrary
	let $timeout = mc.ng.timeout.$timeout;
	return $timeout(closure, timeout);
};

// Alias
mc.timeout = mc.ng.timeout;

/**
 * Use mc.ng.timeout instead of window.setTimeout(), as the latter
 * issues an event which is unknown to Angular.
 */
mc.ng.timeout.cancel = function(promise)
{
	//TODO: XXX: arbitrary
	let $timeout = mc.ng.timeout.$timeout;
	$timeout.cancel(promise);
};

/**
 * Non-angular event so we send a notify to handle the resize changing things
 * that need to trigger that model<->view dialectic.
 */
 /*
window.addEventListener("resize", (event) =>
{
	maybe(() => mc.ng.apply());
});
*/

/** Abstract Controller
 *
 * Should be on the prototype chain of all of the controllers in client.
 * This allows us to DRY things across all controllers as well as provide
 * a suite of virtual convenience functions.
 *
 * Overrides can be async. This abstraction provides a digest for that at
 * the base of this virtual call stack. Note: overrides for digesting() will
 * never be waited on.
 */
mc.ng.controller = class
{
	constructor(name, $scope)
	{
		// DEBUG
		//let argc = arguments.length;
		//console.log("init controller [" + name + "] " + typeof($scope));

		this.$scope = $scope;
		this.$apply = $scope.$apply;

		// Register dtors
		this.$scope.$on("$destroy", mc.ng.controller.virtual.bind(this, this.destructor));

		// NOTE: $applyAsync MUST be used here rather than $evalAsync.
		// $evalAsync from a directive runs after DOM update.
		// $evalAsync from a controller runs before the DOM has been manipulated. <---
		this.$scope.$applyAsync(mc.ng.controller.virtual.bind(this, this.constructed));

		// Register digest loop
		this.$scope.$watch(this.digesting.bind(this));

		// DEBUG
		//this.$scope.$watch(() => debug.digest(name, $scope));
	}

	/**
	 * Override to get called when the controller gets $destroy'ed
	 */
	destructor() {}

	/**
	 * Override to get called after the DOM element has been inserted.
	 * This will be on the next digest cycle after construction.
	 */
	constructed() {}

	/**
	 * Ovrride convenience to execute code during a digest
	 */
	digesting() {}
};

/**
 *
 */
mc.ng.controller.virtual = async function(override)
{
	let ret = override.call(this);
	if(ret instanceof Promise)
	{
		ret = await ret;
		this.$scope.$apply();
	}

	return ret;
};

/** Async Angular directives.
 *
 * These ang- prefixed directives are identical to the ng- directives except they
 * add support for ES6 async usage up the stack. This allows the evaluation of a
 * directive to digest after a promise it returns is fulfilled.
 *
 * Regular ng- directives run the digest loop when the evaluation returns -- even if
 * it returns a promise, which happens on the first await up the stack. We need it to
 * happen after the code on the other side of the await too.
 */
mc.ng.ang = function(directive_name, event_name, opts = {})
{
	mc.ng.app.directive(directive_name, function()
	{
		return {
			restrict: 'A',
			link: mc.ng.ang.link.bind(null, directive_name, event_name, opts),
		};
	});
};

mc.ng.ang.link = function(name, on, opts, $scope, element, attr, controller)
{
	let expression = attr[name];
	element.on(on, async function($event)
	{
		let locals =
		{
			$event: $event,
		};

		let onerror = (error) =>
		{
			if(!error.element)
				error.element = $event.delegateTarget;

			mc.unhandled(error);
		};

		let promise; try
		{
			promise = $scope.$eval(expression, locals);
		}
		catch(error)
		{
			onerror(error);
		}

		if(promise instanceof Promise) try
		{
			await promise;
		}
		catch(error)
		{
			onerror(error);
		}
		finally
		{
			if(opts.apply_async !== false)
				$scope.$apply();
		}
		else
		{
			if(opts.apply_sync !== false)
				$scope.$apply();
		}
	});
};

mc.ng.ang.directives =
[
	mc.ng.ang("angClick", "click"),
	mc.ng.ang("angDblclick", "dblclick"),
	mc.ng.ang("angMouseup", "mouseup"),
	mc.ng.ang("angMousedown", "mousedown"),

	// The scroll event is optioned to not digest unless the handler returns a
	// promise. Note that this means a scroll event handler should not be an
	// `async function()` if it has efficient return cases (and it should).
	mc.ng.ang("angScroll", "scroll",
	{
		apply_sync: false,
	}),
];

/**
 * ng-repeat="foo in bar | reverse"
 */
mc.ng.app.filter('reverse', () => (items) => items.slice().reverse());
