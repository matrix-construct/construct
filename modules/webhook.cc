// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Webhook Handler"
};

conf::item<std::string>
webhook_secret
{
	{ "name", "webhook.secret" }
};

conf::item<std::string>
webhook_user
{
	{ "name", "webhook.user" }
};

conf::item<std::string>
webhook_room
{
	{ "name", "webhook.room" }
};

conf::item<std::string>
webhook_url
{
	{ "name", "webhook.url" },
	{ "default", "/webhook" }
};

resource
webhook_resource
{
	string_view{webhook_url},
	{
		"Webhook Resource",
		webhook_resource.DIRECTORY
	}
};

static resource::response
post__webhook(client &,
              const resource::request &);

resource::method
webhook_post
{
	webhook_resource, "POST", post__webhook
};

static void
github_handle(client &,
              const resource::request &);

resource::response
post__webhook(client &client,
              const resource::request &request)
{
	if(has(http::headers(request.head.headers), "X-GitHub-Event"_sv))
		github_handle(client, request);

	return resource::response
	{
		client, http::OK
	};
}

static bool
github_validate(const string_view &sig,
                const const_buffer &content,
                const string_view &secret);

static string_view
github_find_commit_hash(const json::object &content);

static string_view
github_find_issue_number(const json::object &content);

static std::pair<string_view, string_view>
github_find_party(const json::object &content);

static std::ostream &
github_handle__push(std::ostream &,
                    const json::object &content);

static std::ostream &
github_handle__ping(std::ostream &,
                    const json::object &content);

static std::ostream &
github_heading(std::ostream &,
               const string_view &type,
               const json::object &content);

void
github_handle(client &client,
              const resource::request &request)
{
	const http::headers &headers
	{
		request.head.headers
	};

	const auto sig
	{
		headers.at("X-Hub-Signature")
	};

	if(!github_validate(sig, request.content, webhook_secret))
		throw http::error
		{
			http::UNAUTHORIZED, "X-Hub-Signature verification failed"
		};

	const string_view &type
	{
		headers.at("X-GitHub-Event")
	};

	const string_view &delivery
	{
		headers.at("X-GitHub-Delivery")
	};

	const unique_buffer<mutable_buffer> buf
	{
		48_KiB
	};

	std::stringstream out;
	pubsetbuf(out, buf);

	github_heading(out, type, request.content);

	if(type == "ping")
		github_handle__ping(out, request.content);
	else if(type == "push")
		github_handle__push(out, request.content);

	if(!string_view(webhook_room))
		return;

	const auto room_id
	{
		m::room_id(string_view(webhook_room))
	};

	if(!string_view(webhook_user))
		return;

	const m::user::id::buf user_id
	{
		string_view(webhook_user), my_host()
	};

	const auto evid
	{
		m::msghtml(room_id, user_id, view(out, buf), "No alt text")
	};

	log::info
	{
		"Webhook [%s] '%s' delivered to %s %s",
		delivery,
		type,
		string_view{room_id},
		string_view{evid}
	};
}

static std::ostream &
github_heading(std::ostream &out,
               const string_view &type,
               const json::object &content)
{
	const json::object repository
	{
		content["repository"]
	};

	out << "<a href=\\\"" << unquote(repository["html_url"]) << "\\\">"
	    << unquote(repository["full_name"])
	    << "</a>";

	const string_view commit_hash
	{
		github_find_commit_hash(content)
	};

	if(commit_hash && type == "push")
		out << " <b><font color=\\\"#FF5733\\\">";
	else if(commit_hash && type == "pull_request")
		out << " <b><font color=\\\"#CC00CC\\\">";
	else if(commit_hash)
		out << " <b>";

	if(commit_hash)
		out << commit_hash.substr(0, 8)
		    << "</font></b>";

	const string_view issue_number
	{
		github_find_issue_number(content)
	};

	if(issue_number)
		out << " <b>#" << issue_number << "</b>";
	else
		out << " " << type;

	const auto party
	{
		github_find_party(content)
	};

	out << " by "
	    << "<a href=\\\""
	    << party.second
	    << "\\\">"
	    << party.first
	    << "</a>";

	return out;
}

static std::ostream &
github_handle__push(std::ostream &out,
                    const json::object &content)
{
	const json::array commits
	{
		content["commits"]
	};

	const auto count
	{
		size(commits)
	};

	if(!count)
	{
		out << " <font color=\\\"#FF0000\\\">";
		if(content["ref"])
			out << " " << unquote(content["ref"]);

		out << " deleted</font>";
		return out;
	}

	if(content["ref"])
	{
		const auto ref(unquote(content["ref"]));
		out << " "
		    << " "
		    << token_last(ref, '/');
	}

	out << " <a href=\\\"" << unquote(content["compare"]) << "\\\">"
	    << "<b>" << count << " commits</b>"
	    << "</a>";

	if(content["forced"] == "true")
		out << " (rebase)";

	out << "<pre>";
	for(const json::object &commit : commits)
	{
		const auto url(unquote(commit["url"]));
		const auto id(unquote(commit["id"]));
		const auto sid(id.substr(0, 8));
		out << " <a href=\\\"" << url << "\\\">"
		    << "<b>" << sid << "</b>"
		    << "</a>";

		const json::object author(commit["author"]);
		out << " " << unquote(author["name"]);

		const json::object committer(commit["committer"]);
		if(committer["email"] != author["email"])
			out << " via " << unquote(committer["name"]);

		const auto message(unquote(commit["message"]));
		const auto summary
		{
			split(message, "\\n").first
		};

		out << " <u>"
		    << summary
		    << "</u>";

		out << "<br />";
	}

	out << "</pre>";
	return out;
}

static std::ostream &
github_handle__ping(std::ostream &out,
                    const json::object &content)
{
	out << "<pre>"
	    << unquote(content["zen"])
	    << "</pre>";

	return out;
}

/// Researched from yestifico bot
static std::pair<string_view, string_view>
github_find_party(const json::object &content)
{
	const json::object pull_request
	{
		content["pull_request"]
	};

	const json::object user
	{
		pull_request["user"]
	};

	if(!empty(user))
		return
		{
			unquote(user["login"]),
			unquote(user["html_url"])
		};

	const json::object sender
	{
		content["sender"]
	};

	return
	{
		unquote(sender["login"]),
		unquote(sender["html_url"])
	};
}

/// Researched from yestifico bot
static string_view
github_find_issue_number(const json::object &content)
{
	const json::object issue(content["issue"]);
	if(!empty(issue))
		return unquote(issue["number"]);

	if(content["number"])
		return unquote(content["number"]);

	return {};
}

/// Researched from yestifico bot
static string_view
github_find_commit_hash(const json::object &content)
{
	if(content["sha"])
		return unquote(content["sha"]);

	const json::object commit(content["commit"]);
	if(!empty(commit))
		return unquote(commit["sha"]);

	const json::object head(content["head"]);
	if(!empty(head))
		return unquote(head["commit"]);

	const json::object head_commit(content["head_commit"]);
	if(!empty(head_commit))
		return unquote(head_commit["id"]);

	const json::object comment(content["comment"]);
	if(!empty(comment))
		return unquote(comment["commit_id"]);

	if(content["commit"])
		return unquote(content["commit"]);

	return {};
}

static bool
github_validate(const string_view &sigheader,
                const const_buffer &content,
                const string_view &secret)
try
{
	const auto sig
	{
		split(sigheader, "=")
	};

	crh::hmac hmac
	{
		sig.first, secret
	};

	hmac.update(content);

	char ubuf[32];
	assert(sizeof(ubuf) >= hmac.length());
	const const_buffer hmac_bin
	{
		hmac.finalize(ubuf)
	};

	char abuf[64];
	static_assert(sizeof(abuf) >= sizeof(ubuf) * 2);
	const string_view hmac_hex
	{
		u2a(abuf, hmac_bin)
	};

	return hmac_hex == sig.second;
}
catch(const crh::error &e)
{
	throw http::error
	{
		http::NOT_IMPLEMENTED, "The signature algorithm is not supported"
	};
}
