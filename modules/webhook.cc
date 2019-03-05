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
github_handle__pull_request(std::ostream &,
                            const json::object &content);

static std::ostream &
github_handle__issue_comment(std::ostream &,
                             const json::object &content);

static std::ostream &
github_handle__issues(std::ostream &,
                      const json::object &content);

static std::ostream &
github_handle__watch(std::ostream &,
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
	else if(type == "pull_request")
		github_handle__pull_request(out, request.content);
	else if(type == "issues")
		github_handle__issues(out, request.content);
	else if(type == "issue_comment")
		github_handle__issue_comment(out, request.content);
	else if(type == "watch")
		github_handle__watch(out, request.content);

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

	out << "<a href=\"" << unquote(repository["html_url"]) << "\">"
	    << unquote(repository["full_name"])
	    << "</a>";

	const string_view commit_hash
	{
		github_find_commit_hash(content)
	};

	if(commit_hash && type == "push")
		out << " <b><font color=\"#FF5733\">";
	else if(commit_hash && type == "pull_request")
		out << " <b><font color=\"#CC00CC\">";
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
	    << "<a href=\""
	    << party.second
	    << "\">"
	    << party.first
	    << "</a>";

	return out;
}

std::ostream &
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
		out << " <font color=\"#FF0000\">";
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

	out << " <a href=\"" << unquote(content["compare"]) << "\">"
	    << "<b>" << count << " commits</b>"
	    << "</a>";

	if(content["forced"] == "true")
		out << " (rebase)";

	out << "<pre><code>";
	for(ssize_t i(count - 1); i >= 0; --i)
	{
		const json::object &commit(commits.at(i));
		const auto url(unquote(commit["url"]));
		const auto id(unquote(commit["id"]));
		const auto sid(id.substr(0, 8));
		out << " <a href=\"" << url << "\">"
		    << "<b>" << sid << "</b>"
		    << "</a>";

		const json::object author(commit["author"]);
		out << " <b>"
		    << unquote(author["name"])
		    << "</b>"
		    ;

		const json::object committer(commit["committer"]);
		if(committer["email"] != author["email"])
			out << " via <b>"
			    << unquote(committer["name"])
			    << "</b>"
			    ;

		const auto message(unquote(commit["message"]));
		const auto summary
		{
			split(message, "\\n").first
		};

		out << " "
		    << summary
		    ;

		out << "<br />";
	}

	out << "</code></pre>";
	return out;
}

static std::ostream &
github_handle__pull_request(std::ostream &out,
                            const json::object &content)
{
	const json::object pr
	{
		content["pull_request"]
	};

	if(pr["merged"] != "true")
		out << " "
		    << "<b>"
		    << unquote(content["action"])
		    << "</b>"
		    ;

	if(pr["merged"] == "true")
		out << ' '
		    << "<b>"
		    << "<font color=\"#CC00CC\">"
		    << "merged"
		    << "</font>"
		    << "</b>"
		    ;

	if(pr.has("merged_by") && pr["merged_by"] != "null")
	{
		const json::object merged_by{pr["merged_by"]};
		out << " "
		    << "by "
		    << "<a href=\""
		    << unquote(merged_by["html_url"])
		    << "\">"
		    << unquote(merged_by["login"])
		    << "</a>"
		    ;
	}

	if(pr["merged"] == "false") switch(hash(pr["mergeable"]))
	{
		default:
		case hash("null"):
			out << " / "
			    << "<b>"
			    << "<font color=\"#FFCC00\">"
			    << "CHECKING MERGE"
			    << "</font>"
			    << "</b>"
			    ;
			break;

		case hash("true"):
			out << " / "
			    << "<b>"
			    << "<font color=\"#33CC33\">"
			    << "MERGEABLE"
			    << "</font>"
			    << "</b>"
			    ;
			break;

		case hash("false"):
			out << " / "
			    << "<b>"
			    << "<font color=\"#CC0000\">"
			    << "MERGE CONFLICT"
			    << "</font>"
			    << "</b>"
			    ;
			break;
	}

	if(pr.has("additions"))
		out << " / "
		    << "<b>"
		    << "<font color=\"#33CC33\">"
		    << "++"
		    << "</font>"
		    << pr["additions"]
		    << "</b>"
		    ;

	if(pr.has("deletions"))
		out << " / "
		    << "<b>"
		    << "<font color=\"#CC0000\">"
		    << "--"
		    << "</font>"
		    << pr["deletions"]
		    << "</b>"
		    ;

	if(pr.has("changed_files"))
		out << " / "
		    << "<b>"
		    << pr["changed_files"]
		    << ' '
		    << "<font color=\"#476b6b\">"
		    << "files"
		    << "</font>"
		    << "</b>"
		    ;

	const json::object head
	{
		pr["head"]
	};

	out << " "
	    << "<pre><code>"
	    << "<a href=\""
	    << unquote(pr["html_url"])
	    << "\">"
	    << "<b>"
	    << unquote(head["sha"]).substr(0, 8)
	    << "</b>"
	    << "</a>"
	    << " "
	    << "<u>"
	    << unquote(pr["title"])
	    << "</u>"
	    << "</code></pre>"
	    ;

	return out;
}

static std::ostream &
github_handle__issues(std::ostream &out,
                      const json::object &content)
{
	out << " "
	    << "<b>"
	    << unquote(content["action"])
	    << "</b>"
	    ;

	const json::object issue
	{
		content["issue"]
	};

	switch(hash(unquote(content["action"])))
	{
		case "assigned"_:
		case "unassigned"_:
		{
			const json::object assignee
			{
				content["assignee"]
			};

			out << " "
			    << "<a href=\""
			    << unquote(assignee["html_url"])
			    << "\">"
			    << unquote(assignee["login"])
			    << "</a>"
			    ;

			break;
		}
	}

	out << " "
	    << "<a href=\""
	    << unquote(issue["html_url"])
	    << "\">"
	    << "<b><u>"
	    << unquote(issue["title"])
	    << "</u></b>"
	    << "</a>"
	    ;

	if(unquote(content["action"]) == "opened")
	{
		out << " "
		    << "<blockquote>"
		    << "<pre><code>"
		    ;

		static const auto delim("\\r\\n");
		const auto body(unquote(issue["body"]));
		auto lines(split(body, delim)); do
		{
			out << lines.first
			    << "<br />"
			    ;

			lines = split(lines.second, delim);
		}
		while(!empty(lines.second));

		out << ""
		    << "</code></pre>"
		    << "</blockquote>"
		    ;
	}
	else if(unquote(content["action"]) == "labeled")
	{
		const json::array labels
		{
			issue["labels"]
		};

		const json::object label
		{
			content["label"]
		};

		out << "<ul>";
		for(const json::object &label : labels)
		{
			out << "<li>";
			out << "<font color="
			    << label["color"]
			    << ">";

			out << unquote(label["name"]);

			out << "</font>";
			out << "</li>";
		}

		out << "</ul>";
	}

	return out;
}

static std::ostream &
github_handle__issue_comment(std::ostream &out,
                             const json::object &content)
{
	const json::object issue
	{
		content["issue"]
	};

	const json::object comment
	{
		content["comment"]
	};

	const json::string action
	{
		content["action"]
	};

	out << " <b>";
	switch(hash(action))
	{
		case "created"_:
			out << "commented on";
			break;

		default:
			out << action;
			break;
	}
	out << "</b>";

	out << " "
	    << "<a href=\""
	    << unquote(issue["html_url"])
	    << "\">"
	    << "<b><u>"
	    << unquote(issue["title"])
	    << "</u></b>"
	    << "</a>"
	    ;

	if(action == "created")
	{
		out << " "
		    << "<blockquote>"
		    << "<pre><code>"
		    ;

		static const auto delim("\\r\\n");
		const auto body(unquote(comment["body"]));
		auto lines(split(body, delim)); do
		{
			out << lines.first
			    << "<br />"
			    ;

			lines = split(lines.second, delim);
		}
		while(!empty(lines.second));

		out << ""
		    << "</code></pre>"
		    << "</blockquote>"
		    ;
	}

	return out;
}

std::ostream &
github_handle__watch(std::ostream &out,
                     const json::object &content)
{
	const string_view action
	{
		unquote(content["action"])
	};

	if(action == "started")
	{
		out << " with a star";
		return out;
	}

	return out;
}

std::ostream &
github_handle__ping(std::ostream &out,
                    const json::object &content)
{
	out << "<pre><code>"
	    << unquote(content["zen"])
	    << "</code></pre>";

	return out;
}

/// Researched from yestifico bot
std::pair<string_view, string_view>
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
ircd::string_view
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
ircd::string_view
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

bool
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

	char ubuf[64], abuf[sizeof(ubuf) * 2];
	if(unlikely(sizeof(ubuf) < hmac.length()))
		throw ircd::panic
		{
			"HMAC algorithm '%s' digest exceeds buffer size.",
			sig.first
		};

	const const_buffer hmac_bin
	{
		hmac.finalize(ubuf)
	};

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
		http::NOT_IMPLEMENTED, "The signature algorithm is not supported.",
	};
}
