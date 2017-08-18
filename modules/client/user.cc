/* 
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
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
 */

using namespace ircd;

resource user_resource
{
	"_matrix/client/r0/user",
	"User resource",
	{
		resource::DIRECTORY
	}
};

namespace ircd
{
	static string_view extract_user_id(const auto &path, const auto &suffix);
}

ircd::string_view
ircd::extract_user_id(const auto &path,
                      const auto &suffix)
{
	// Extract the user_id from _matrix/client/r0/user/$user/filter
	return lstrip(between(path, user_resource.path, suffix), '/');
}

resource::response
get_filter(client &client, const resource::request &request)
try
{
	const auto user_id
	{
		extract_user_id(request.head.path, "/filter")
	};

	return resource::response
	{
		client, json::index
		{
			{    }
		}
	};
}
catch(...)
{
	throw;
}

resource::method get
{
	user_resource, "GET", get_filter
};

// (5.2) Uploads a new filter definition to the homeserver. Returns a filter ID that
// may be used in future requests to restrict which events are returned to the client.
resource::response
post_filter(client &client, const resource::request &request)
try
{
	const auto user_id
	{
		// (5.2) Required. The id of the user uploading the filter. The access
		// token must be authorized to make requests for this user id.
		extract_user_id(request.head.path, "/filter")
	};

	const auto event_fields
	{
		// (5.2) List of event fields to include. If this list is absent then all fields are
		// included. The entries may include '.' charaters to indicate sub-fields. So
		// ['content.body'] will include the 'body' field of the 'content' object. A literal '.'
		// character in a field name may be escaped using a '\'. A server may include more
		// fields than were requested.
		request["event_fields"]
	};

	const auto event_format
	{
		// (5.2) The format to use for events. 'client' will return the events in a format suitable
		// for clients. 'federation' will return the raw event as receieved over federation.
		// The default is 'client'. One of: ["client", "federation"]
		request["event_format"]
	};

	const auto account_data
	{
		// (5.2) The user account data that isn't associated with rooms to include.
		request["account_data"]
	};

	const auto room
	{
		// (5.2) Filters to be applied to room data.
		request["room"]
	};

	const auto presence
	{
		// (5.2) The presence updates to include.
		request["presence"]
	};

	std::cout << "do filter: " << request.content << std::endl;

	return resource::response
	{
		client, http::CREATED, json::index
		{
			{ "filter_id", "abc321" }
		}
	};
}
catch(...)
{
	throw;
}

resource::method post
{
	user_resource, "POST", post_filter
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/user' to handle requests"
};
