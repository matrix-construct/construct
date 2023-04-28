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
	"Web hook Handler"
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

conf::item<bool>
webhook_status_verbose
{
	{ "name",     "webhook.github.status.verbose" },
	{ "default",  true                            },
};

conf::item<bool>
webhook_status_errors
{
	{ "name",     "webhook.github.status.errors" },
	{ "default",  true                           },
};

conf::item<std::string>
webhook_github_token
{
	{ "name", "webhook.github.token" }
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

static void
appveyor_handle(client &,
                const resource::request &);

static void
dockerhub_handle(client &,
                 const resource::request &);

resource::response
post__webhook(client &client,
              const resource::request &request)
{
	const http::headers &headers
	{
		request.head.headers
	};

	if(http::has(headers, "X-GitHub-Event"))
		github_handle(client, request);

	else if(http::has(headers, "X-Appveyor-Secret"))
		appveyor_handle(client, request);

	else if(startswith(request.head.content_type, "application/json"))
		dockerhub_handle(client, request);

	return resource::response
	{
		client, http::OK
	};
}

static bool
github_validate(const string_view &sig,
                const const_buffer &content,
                const string_view &secret);

static std::string
github_url(const json::string &url);

static json::string
github_find_commit_hash(const json::object &content);

static json::string
github_find_issue_number(const json::object &content);

static std::pair<json::string, json::string>
github_find_party(const json::object &content);

static std::pair<json::string, json::string>
github_find_repo(const json::object &content);

static ircd::m::event::id::buf
github_find_push_event_id(const m::room &, const m::user::id &, const string_view &);

static bool
github_handle__dependabot_alert(std::ostream &,
                                const json::object &content);

static bool
github_handle__check_suite(std::ostream &,
                           const json::object &content);

static bool
github_handle__check_run(std::ostream &,
                         const json::object &content);

static bool
github_handle__workflow_job(std::ostream &,
                            std::ostream &,
                            const json::object &content);

static bool
github_handle__workflow_run(std::ostream &,
                            std::ostream &,
                            const json::object &content);

static bool
github_handle__milestone(std::ostream &,
                         const json::object &content);

static bool
github_handle__gollum(std::ostream &,
                      const json::object &content);

static bool
github_handle__push(std::ostream &,
                    const json::object &content);

static bool
github_post_handle__push(const m::event::id &,
                         const json::object &content);

static bool
github_handle__pull_request(std::ostream &,
                            const json::object &content);

static bool
github_handle__issue_comment(std::ostream &,
                             const json::object &content);

static bool
github_handle__commit_comment(std::ostream &,
                              const json::object &content);

static bool
github_handle__issues(std::ostream &,
                      const json::object &content);

static bool
github_handle__watch(std::ostream &,
                     const json::object &content);

static bool
github_handle__star(std::ostream &,
                    const json::object &content);

static bool
github_handle__label(std::ostream &,
                     const json::object &content);

static bool
github_handle__organization(std::ostream &,
                            const json::object &content);

static bool
github_handle__status(std::ostream &,
                      const json::object &content);

static bool
github_handle__repository(std::ostream &,
                          const json::object &content);

static bool
github_handle__delete(std::ostream &,
                      const json::object &content);

static bool
github_handle__create(std::ostream &,
                      const json::object &content);

static bool
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
	if(!string_view(webhook_room))
		return;

	if(!string_view(webhook_user))
		return;

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

	const unique_buffer<mutable_buffer> buf[2]
	{
		{ 48_KiB },
		{ 4_KiB  },
	};

	std::stringstream out, alt;
	pubsetbuf(out, buf[0]);
	pubsetbuf(alt, buf[1]);

	github_heading(out, type, request.content);

	alt
	<< type
	<< " by "
	<< github_find_party(request.content).first
	<< " to "
	<< github_find_repo(request.content).first
	<< " at "
	<< github_find_commit_hash(request.content)
	;

	const bool ok
	{
		type == "ping"?
			github_handle__ping(out, request.content):
		type == "push"?
			github_handle__push(out, request.content):
		type == "pull_request"?
			github_handle__pull_request(out, request.content):
		type == "issues"?
			github_handle__issues(out, request.content):
		type == "issue_comment"?
			github_handle__issue_comment(out, request.content):
		type == "commit_comment"?
			github_handle__commit_comment(out, request.content):
		type == "watch"?
			github_handle__watch(out, request.content):
		type == "star"?
			github_handle__star(out, request.content):
		type == "label"?
			github_handle__label(out, request.content):
		type == "organization"?
			github_handle__organization(out, request.content):
		type == "status"?
			github_handle__status(out, request.content):
		type == "repository"?
			github_handle__repository(out, request.content):
		type == "create"?
			github_handle__create(out, request.content):
		type == "delete"?
			github_handle__delete(out, request.content):
		type == "gollum"?
			github_handle__gollum(out, request.content):
		type == "milestone"?
			github_handle__milestone(out, request.content):
		type == "dependabot_alert"?
			github_handle__dependabot_alert(out, request.content):
		type == "workflow_run"?
			github_handle__workflow_run(out, alt, request.content):
		type == "workflow_job"?
			github_handle__workflow_job(out, alt, request.content):
		type == "check_run"?
			github_handle__check_run(out, request.content):
		type == "check_suite"?
			github_handle__check_suite(out, request.content):

		true // unhandled will just show heading
	};

	if(!ok)
		return;

	const auto room_id
	{
		m::room_id(string_view(webhook_room))
	};

	const m::user::id::buf user_id
	{
		string_view(webhook_user), my_host()
	};

	const auto evid
	{
		m::msghtml(room_id, user_id, view(out, buf[0]), view(alt, buf[1]), "m.notice")
	};

	if(type == "push")
		github_post_handle__push(evid, request.content);

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

	const json::object organization
	{
		content["organization"]
	};

	if(empty(repository))
	{
		const auto url
		{
			github_url(organization["url"])
		};

		out << "<a href=\"" << url << "\">"
		    << json::string(organization["login"])
		    << "</a>";
	}
	else
		out << "<a href=" << repository["html_url"] << ">"
		    << json::string(repository["full_name"])
		    << "</a>";

	const auto commit_hash
	{
		github_find_commit_hash(content)
	};

	if(commit_hash && type == "push")
		out << " <b><font color=\"#FF5733\">";
	else if(commit_hash && type == "pull_request")
		out << " <b><font color=\"#FF5733\">";
	else if(commit_hash)
		out << " <b>";

	if(commit_hash)
		out << commit_hash.substr(0, 8)
		    << "</font></b>";

	const string_view issue_number
	{
		github_find_issue_number(content)
	};

	const json::object workflow
	{
		content.has("workflow_run")?
			content["workflow_run"]:
			content["workflow_job"]
	};

	const json::string
	workflow_name{workflow["workflow_name"]},
	job_name{workflow["name"]};

	if(issue_number)
		out << " <b>#" << issue_number << "</b>";
	else if(workflow_name && job_name)
		out << " job <b>" << workflow_name << "</b>";
	else if(job_name)
		out << " job <b>" << job_name << "</b>";
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

static string_view
github_markdown(unique_const_buffer &buf,
                const string_view &text);

bool
github_handle__dependabot_alert(std::ostream &out,
                                const json::object &content)
{
	const json::string
	action{content["action"]},
	url{content["html_url"]};

	const json::object
	alert{content["alert"]},
	advise{alert["security_advisory"]},
	vuln{alert["security_vulnerability"]},
	dep{alert["dependency"]},
	pkg{dep["package"]};

	const json::string
	ghsa{advise["ghsa_id"]},
	cve{advise["cve_id"]},
	summary{advise["summary"]},
	desc{advise["description"]},
	severity{advise["severity"]},
	name{pkg["name"]},
	path{dep["manifest_path"]};

	out
	<< " <a href=" << alert["html_url"] << ">"
	<< "<b>"
	<< summary
	<< "</b>"
	<< "</a>"
	<< "<br>🚨 "
	<< "<b>"
	<< cve
	<< "</b>"
	<< " "
	<< "<b>"
	<< ghsa
	<< "</b>"
	<< " severity "
	<< severity
	<< " 🚨<br>"
	;

	unique_const_buffer buf;
	const string_view markup
	{
		github_markdown(buf, desc)
	};

	out
	//<< "<blockquote>"
	<< markup
	//<< "</blockquote>"
	<< "<br>"
	;

	if(path)
		out
		<< "<pre>"
		<< path
		<< "</pre>"
		<< "<br>"
		;

	return true;
}

static size_t
clear_reactions(const m::room &room,
                const m::user::id &user_id,
                const m::event::id &event_id)
{
	const m::relates relations
	{
		index(event_id)
	};

	const auto user_match
	{
		[&user_id](const auto &sender)
		{
			return sender == user_id;
		}
	};

	size_t ret(0);
	relations.for_each("m.annotation", [&]
	(const auto &ref_idx, const json::object &content, const m::relates_to &relates)
	{
		if(!m::query(ref_idx, "sender", user_match))
			return true;

		if(m::redacted(ref_idx))
			return true;

		const auto ref_id(m::event_id(ref_idx));
		m::redact(room, user_id, ref_id, "cleared");
		++ret;
		return true;
	});

	return ret;
}

template<class closure>
static ircd::m::event::id::buf
_find_reaction_id(const m::room &room,
                 const m::user::id &user_id,
                 const m::event::id &event_id,
                 closure&& func)
{
	const m::relates relations
	{
		index(event_id)
	};

	const auto user_match
	{
		[&user_id](const auto &sender)
		{
			return sender == user_id;
		}
	};

	m::event::id::buf ret;
	relations.for_each("m.annotation", [&func, &user_match, &ret]
	(const auto &ref_idx, const json::object &content, const m::relates_to &relates)
	{
		if(!m::query(ref_idx, "sender", user_match))
			return true;

		if(m::redacted(ref_idx))
			return true;

		if(func(relates.source))
		{
			ret = m::event_id(ref_idx);
			return false;
		}
		else return true;
	});

	return ret;
}

static ircd::m::event::id::buf
find_reaction_id(const m::room &room,
                 const m::user::id &user_id,
                 const m::event::id &event_id,
                 const string_view &label)
{
	return _find_reaction_id(room, user_id, event_id, [&label]
	(const auto &relates)
	{
		const json::string key
		{
			relates["key"]
		};

		return key == label;
	});
}

static ircd::m::event::id::buf
find_reaction_id_contains(const m::room &room,
                          const m::user::id &user_id,
                          const m::event::id &event_id,
                          const string_view &label)
{
	return _find_reaction_id(room, user_id, event_id, [&]
	(const auto &relates)
	{
		const json::string key
		{
			relates["key"]
		};

		return has(key, label);
	});
}

static bool
clear_reaction(const m::room &room,
               const m::user::id &user_id,
               const m::event::id &event_id,
               const string_view &label)
{
	const auto reaction_id
	{
		find_reaction_id(room, user_id, event_id, label)
	};

	if(!reaction_id)
		return false;

	m::redact(room, user_id, reaction_id, "cleared");
	return true;
}

static bool
clear_reaction_contains(const m::room &room,
                        const m::user::id &user_id,
                        const m::event::id &event_id,
                        const string_view &label)
{
	const auto reaction_id
	{
		find_reaction_id_contains(room, user_id, event_id, label)
	};

	if(!reaction_id)
		return false;

	m::redact(room, user_id, reaction_id, "cleared");
	return true;
}

static ircd::m::event::id::buf
github_find_job_table(const m::room &room,
                      const m::user::id &user_id,
                      const string_view &str)
{
	const auto type_match
	{
		[](const string_view &type) noexcept
		{
			return type == "m.room.message";
		}
	};

	const auto user_match
	{
		[&user_id](const string_view &sender) noexcept
		{
			return sender && sender == user_id;
		}
	};

	const auto content_match
	{
		[&str](const json::object &content)
		{
			const json::string &body
			{
				content["body"]
			};

			return has(body, str);
		}
	};

	// Limit the search to a maximum of recent messages from the
	// webhook user and total messages so we don't run out of control
	// and scan the whole room history.
	int lim[2] { 768, 384 };
	m::room::events it{room};
	for(; it && lim[0] > 0 && lim[1] > 0; --it, --lim[0])
	{
		if(!m::query(std::nothrow, it.event_idx(), "sender", user_match))
			continue;

		--lim[1];
		if(!m::query(std::nothrow, it.event_idx(), "type", type_match))
			continue;

		if(!m::query(std::nothrow, it.event_idx(), "content", content_match))
			continue;

		return m::event_id(std::nothrow, it.event_idx());
	}

	return {};
}

static json::string
github_repopath(const json::object &content)
{
	const json::object repository
	{
		content.at("repository")
	};

	const json::string full_name
	{
		repository.at("full_name")
	};

	return full_name;
}

static json::object
_github_request(unique_const_buffer &out,
                const string_view &method,
                const string_view &url,
                const string_view &content)
{
	char authorization_buf[128];
	const string_view authorization
	{
		fmt::sprintf
		{
			authorization_buf, "Bearer %s",
			string_view{webhook_github_token},
		}
	};

	const http::header headers[]
	{
		{ "Accept", "application/json; charset=utf-8"  },
		{ "X-GitHub-Api-Version", "2022-11-28"         },
		{ "Authorization", authorization               },
	};

	const auto num_headers
	{
		sizeof(headers) / sizeof(http::header)
		- empty(webhook_github_token)
	};

	return string_view
	{
		rest::request
		{
			url,
			{
				.method = method,
				.content = content,
				.content_type = "application/json; charset=utf-8",
				.headers = vector_view(headers, num_headers),
				.out = &out,
			}
		}
	};
}

template<class... args>
static json::object
github_request(const string_view &content,
               unique_const_buffer &out,
               const string_view &method,
               const string_view &repo,
               const string_view &fmt,
               args&&... a)
{
	char path_buf[384];
	const string_view path{fmt::sprintf
	{
		path_buf, fmt,
		std::forward<args>(a)...
	}};

	char url_buf[512];
	const string_view url{fmt::sprintf
	{
		url_buf, "https://api.github.com/repos/%s/%s",
		repo,
		path,
	}};

	return _github_request(out, method, url, content);
}

template<class... args>
static json::object
github_request(unique_const_buffer &out,
               const string_view &method,
               const string_view &repo,
               const string_view &fmt,
               args&&... a)
{
	const auto &content(json::empty_object);
	return github_request(content, out, method, repo, fmt, std::forward<args>(a)...);
}

static string_view
github_markdown(unique_const_buffer &buf,
                const string_view &text)
{
	const json::strung content
	{
		json::members
		{
			{ "text", text }
		}
	};

	return _github_request
	(
		buf, "POST", "https://api.github.com/markdown", content
	);
}

static bool
github_hook_for_each(const string_view &repo,
                     const function_bool<json::object> &closure)
{
	unique_const_buffer buf;
	const json::array response
	{
		github_request
		(
			buf, "GET", repo, "hooks"
		)
	};

	for(const json::object hook : response)
		if(!closure(hook))
			return false;

	return true;
}

static void
github_hook_ping(const string_view &repo,
                 const string_view &hook)
{
	unique_const_buffer buf;
	github_request
	(
		buf, "POST", repo, "hooks/%s/pings",
		hook
	);
}

static void
github_hook_ping(const string_view &repo)
{
	github_hook_for_each(repo, [&repo]
	(const json::object &hook)
	{
		const json::string id
		{
			hook["id"]
		};

		github_hook_ping(repo, id);
	});
}

static bool
github_hook_shot_for_each(const string_view &repo,
                          const string_view &hook,
                          const bool &redelivery,
                          const function_bool<json::object> &closure)
{
	unique_const_buffer buf;
	const json::array response
	{
		github_request
		(
			//TODO: pagination token
			buf, "GET", repo, "hooks/%s/deliveries?per_page=100",
			hook
		)
	};

	for(const json::object shot : response)
		if(!closure(shot))
			return false;

	return true;
}

static void
github_hook_shot_retry(const string_view &repo,
                       const string_view &hook,
                       const string_view &id)
{
	unique_const_buffer buf;
	github_request
	(
		buf, "POST", repo, "hooks/%s/deliveries/%s/attempts",
		hook,
		id
	);
}

static bool
github_hook_shot_for_each_fail(const string_view &repo,
                               const string_view &hook,
                               const function_bool<json::object> &closure)
{
	bool ret {true};
	github_hook_shot_for_each(repo, hook, true, [&ret, &closure]
	(const json::object &object)
	{
		if(object.at<bool>("redelivery"))
			return true;

		const json::string status
		{
			object["status"]
		};

		if(status == "OK")
			return false;

		ret = closure(object);
		return ret;
	});

	return ret;
}

static bool
github_run_for_each_jobs(const string_view &repo,
                         const string_view &run_id,
                         const function_bool<json::object> &closure)
{
	for(size_t page(1), i(50); i >= 50; ++page)
	{
		unique_const_buffer buf;
		const json::object response
		{
			github_request
			(
				buf, "GET", repo, "actions/runs/%s/jobs?per_page=%zu&page=%zu", run_id, i, page
			)
		};

		i = 0;
		for(const json::object job : json::array(response["jobs"]))
			if(!closure(job))
				return false;
			else
				++i;
	}

	return true;
}

static void
github_run_delete(const string_view &repo,
                  const string_view &run_id)
{
	unique_const_buffer buf;
	github_request(buf, "DELETE", repo, "actions/runs/%s", run_id);
}

static void
github_run_cancel(const string_view &repo,
                  const string_view &run_id)
{
	unique_const_buffer buf;
	github_request(buf, "POST", repo, "actions/runs/%s/cancel", run_id);
}

static void
github_run_rerun(const string_view &repo,
                 const string_view &run_id)
{
	unique_const_buffer buf;
	github_request(buf, "POST", repo, "actions/runs/%s/rerun", run_id);
}

static void
github_run_rerun_failed(const string_view &repo,
                        const string_view &run_id)
{
	unique_const_buffer buf;
	github_request(buf, "POST", repo, "actions/runs/%s/rerun-failed-jobs", run_id);
}

static void
github_run_dispatch(const string_view &repo,
                    const string_view &name,
                    const string_view &ref = "master",
                    const json::members inputs = {})
{
	const json::strung content{json::members
	{
		{ "ref",    ref    },
		{ "inputs", inputs },
	}};

	unique_const_buffer buf;
	github_request(content, buf, "POST", repo, "actions/workflows/%s/dispatches", name);
}

static bool
github_flow_for_each(const string_view &repo,
                     const function_bool<json::object> &closure,
                     const bool active_only = true)
{
	unique_const_buffer buf;
	const json::object response
	{
		github_request
		(
			//TODO: pagination token
			buf, "GET", repo, "actions/workflows?per_page=100&page=1"
		)
	};

	const json::array workflows
	{
		response["workflows"]
	};

	for(const json::object flow : workflows)
	{
		const json::string state
		{
			flow["state"]
		};

		if(active_only && state != "active")
			continue;

		if(!closure(flow))
			return false;
	}

	return true;
}

bool
github_handle__workflow_run(std::ostream &out,
                            std::ostream &alt,
                            const json::object &content)
{
	const json::object
	workflow{content["workflow"]},
	workflow_run{content["workflow_run"]};

	const json::string
	action{content["action"]},
	title{workflow_run["display_title"]},
	status{workflow_run["status"]},
	conclusion{workflow_run["conclusion"]},
	url{workflow_run["html_url"]},
	name{workflow_run["name"]},
	head_sha{workflow_run["head_sha"]},
	created_at{workflow_run["created_at"]},
	updated_at{workflow_run["updated_at"]},
	run_started_at{workflow_run["run_started_at"]},
	attempt{workflow_run["run_attempt"]},
	run_id{workflow_run["id"]};

	const auto _webhook_room_id
	{
		m::room_id(string_view(webhook_room))
	};

	const m::user::id::buf _webhook_user
	{
		string_view{webhook_user}, my_host()
	};

	const m::room _webhook_room
	{
		_webhook_room_id
	};

	const auto push_event_id
	{
		github_find_push_event_id(_webhook_room, _webhook_user, head_sha)
	};

	const auto &stage
	{
		workflow_run["conclusion"] == json::literal_null?
			status: conclusion
	};

	string_view annote;
	switch(hash(stage))
	{
		case "queued"_:        annote = "🔵"_sv;  break;
		case "in_progress"_:   annote = "🟡"_sv;  break;
		case "success"_:       annote = "🟢"_sv;  break;
		case "failure"_:       annote = "🔴"_sv;  break;
		case "skipped"_:       annote = "⭕"_sv;  break;
		case "cancelled"_:     annote = "⭕"_sv;  break;
		default:               annote = "❓️"_sv;  break;
	}

	char buf[64] {0};
	annote = ircd::strlcpy(buf, annote);
	annote = ircd::strlcat(buf, " "_sv);
	annote = ircd::strlcat(buf, name);

	if(push_event_id && action != "requested") // skip search on first action
		while(clear_reaction_contains(_webhook_room, _webhook_user, push_event_id, name));

	m::annotate(_webhook_room, _webhook_user, push_event_id, annote);

	if(status == "completed")
	{
		const fmt::bsprintf<128> alt
		{
			"job status table %s %s %s",
			github_repopath(content),
			run_id,
			attempt,
		};

		const auto job_table_id
		{
			github_find_job_table(_webhook_room, _webhook_user, alt)
		};

		if(job_table_id)
			switch(hash(conclusion))
			{
				case "success"_:
				case "skipped"_:
					clear_reactions(_webhook_room, _webhook_user, job_table_id);
					break;

				default:
					clear_reaction(_webhook_room, _webhook_user, job_table_id, "⭕"_sv);
					break;
			}
	}

	bool outputs{false};
	if(action == "requested" && conclusion == "failure" && webhook_status_errors)
	{
		outputs = true;
		out
		<< "<br>"
		<< "<font data-mx-bg-color=\"#CC0000\" color=\"#FFFFFF\">"
		<< "&nbsp;"
		<< "&nbsp;"
		<< "<b>"
		<< name
		<< "</b>"
		<< "&nbsp;"
		<< "&nbsp;"
		<< "</font>"
		<< " failed "
		<< "<a href=\""
		<< url
		<< "\">"
		<< "</a>"
		;

		alt
		<< ' '
		<< name
		<< ' '
		<< "failed"
		;
	}

	return outputs;
}

bool
github_handle__workflow_job(std::ostream &out,
                            std::ostream &alt,
                            const json::object &content)
{
	const json::object workflow_job
	{
		content["workflow_job"]
	};

	const json::string
	action{content["action"]};

	// Ignore queued actions. Instead on the first in_progress we'll pull
	// all jobs from github at once.
	if(action == "queued")
		return false;

	const json::string
	flow_name{workflow_job["workflow_name"]},
	job_name{workflow_job["name"]},
	url{workflow_job["html_url"]},
	status{workflow_job["status"]},
	conclusion{workflow_job["conclusion"]},
	head_sha{workflow_job["head_sha"]},
	started_at{workflow_job["started_at"]},
	completed_at{workflow_job["completed_at"]},
	attempt{workflow_job["run_attempt"]},
	run_id{workflow_job["run_id"]},
	job_id{workflow_job["id"]};

	const json::array steps
	{
		workflow_job["steps"]
	};

	const auto _webhook_room_id
	{
		m::room_id(string_view(webhook_room))
	};

	const m::user::id::buf _webhook_user
	{
		string_view{webhook_user}, my_host()
	};

	const m::room _webhook_room
	{
		_webhook_room_id
	};

	static const auto annote{[]
	(const json::object &workflow_job)
	{
		const json::string stage
		{
			workflow_job["conclusion"] == json::literal_null?
				workflow_job["status"]:
				workflow_job["conclusion"]
		};

		switch(hash(stage))
		{
			case "queued"_:        return "🟦"_sv;
			case "in_progress"_:   return "🟨"_sv;
			case "success"_:       return "🟩"_sv;
			case "failure"_:       return "🟥"_sv;
			case "skipped"_:       return "⬜️"_sv;
			case "cancelled"_:     return "⬛️"_sv;
			default:               return "❓️"_sv;
		}
	}};

	const fmt::bsprintf<128> alt_tab
	{
		"job status table %s %s %s",
		github_repopath(content),
		run_id,
		attempt,
	};

	const fmt::bsprintf<128> alt_up
	{
		"job status update %s %s %s",
		github_repopath(content),
		run_id,
		attempt,
	};

	// slow this bird down
	static ctx::mutex mutex;
	const std::unique_lock lock
	{
		mutex
	};

	const auto orig_table_id
	{
		github_find_job_table(_webhook_room, _webhook_user, alt_tab)
	};

	const auto last_table_id
	{
		github_find_job_table(_webhook_room, _webhook_user, alt_up)
	};

	const unique_mutable_buffer buf
	{
		32_KiB
	};

	char headbuf[512] {0};
	std::stringstream heading;
	pubsetbuf(heading, headbuf);
	github_heading(heading, "push", content);

	if(orig_table_id)
	{
		const auto old_content
		{
			m::get(last_table_id?: orig_table_id, "content")
		};

		const json::string old_tab
		{
			json::object(old_content)["formatted_body"]
		};

		const string_view td
		{
			between(old_tab, "<td>", "</td>")
		};

		const fmt::bsprintf<512> expect
		{
			"<a href=\\\"%s\\\">", url
		};

		const bool exists
		{
			!tokens(td, "​"_sv, [&]
			(const string_view &cell)
			{
				return !startswith(cell, expect); // return false for found
			})
		};

		const fmt::bsprintf<512> expect_unmodified
		{
			"%s%s</a>",
			string_view{expect},
			annote(workflow_job),
		};

		bool modified
		{
			!tokens(td, "​"_sv, [&]
			(const string_view &cell)
			{
				if(!startswith(cell, expect))
					return true;

				return cell == expect_unmodified; // return false for found
			})
		};

		const bool cancelled
		{
			json::string(workflow_job["conclusion"]) == "cancelled"
		};

		string_view tab;
		tab = ircd::strlcpy(buf, view(heading, headbuf));
		tab = ircd::strlcat(buf, "<table><tr><td>");

		if(exists && modified && !cancelled)
			tokens(td, "​"_sv, [&]
			(const string_view &cell)
			{
				if(!startswith(cell, expect))
				{
					tab = ircd::strlcat(buf, cell);
					tab = ircd::strlcat(buf, "​"_sv);
					return;
				}

				tab = ircd::strlcat(buf, "<a href=\"");
				tab = ircd::strlcat(buf, url);
				tab = ircd::strlcat(buf, "\">");
				tab = ircd::strlcat(buf, annote(workflow_job));
				tab = ircd::strlcat(buf, "</a>");
				tab = ircd::strlcat(buf, "​"_sv);
			});

		if(!exists || (modified && cancelled))
			github_run_for_each_jobs(github_repopath(content), run_id, [&]
			(const json::object &workflow_job)
			{
				const json::string
				url{workflow_job["html_url"]};

				tab = ircd::strlcat(buf, "<a href=");
				tab = ircd::strlcat(buf, workflow_job["html_url"]);
				tab = ircd::strlcat(buf, ">");
				tab = ircd::strlcat(buf, annote(workflow_job));
				tab = ircd::strlcat(buf, "</a>");
				tab = ircd::strlcat(buf, "​"_sv);
				modified = true;
				return true;
			});

		if(modified)
		{
			tab = ircd::strlcat(buf, "</td></tr></table>");

			m::message(_webhook_room, _webhook_user, json::members
			{
				{ "body", alt_up },
				{ "msgtype", "m.notice" },
				{ "format", "org.matrix.custom.html" },
				{ "formatted_body", tab },
				{ "m.new_content", json::members
				{
					{ "body", alt_up },
					{ "msgtype", "m.notice" },
					{ "format", "org.matrix.custom.html" },
					{ "formatted_body", tab },
				}},
				{ "m.relates_to", json::members
				{
					{ "event_id", orig_table_id },
					{ "rel_type", "m.replace" },
				}}
			});
		}
	}
	else if(json::string(workflow_job["conclusion"]) != "skipped")
	{
		string_view tab;
		tab = ircd::strlcpy(buf, view(heading, headbuf));
		tab = ircd::strlcat(buf, "<table><tr><td>");

		github_run_for_each_jobs(github_repopath(content), run_id, [&]
		(const json::object &workflow_job)
		{
			const json::string
			url{workflow_job["html_url"]};

			tab = ircd::strlcat(buf, "<a href=");
			tab = ircd::strlcat(buf, workflow_job["html_url"]);
			tab = ircd::strlcat(buf, ">");
			tab = ircd::strlcat(buf, annote(workflow_job));
			tab = ircd::strlcat(buf, "</a>");
			tab = ircd::strlcat(buf, "​"_sv);
			return true;
		});

		tab = ircd::strlcat(buf, "</td></tr></table>");

		const auto table_event_id
		{
			m::msghtml(_webhook_room, _webhook_user, tab, alt_tab)
		};

		if(table_event_id)
		{
			m::annotate(_webhook_room, _webhook_user, table_event_id, "⭕"_sv);
			m::annotate(_webhook_room, _webhook_user, table_event_id, "🔄"_sv);
			m::annotate(_webhook_room, _webhook_user, table_event_id, "↪️"_sv);
			m::annotate(_webhook_room, _webhook_user, table_event_id, "🚮"_sv);
		}

		if(lex_cast<uint>(attempt) > 1)
		{
			const fmt::bsprintf<128> prior_alt
			{
				"job status table %s %s %u",
				github_repopath(content),
				run_id,
				lex_cast<uint>(attempt) - 1,
			};

			const auto prior_table_id
			{
				github_find_job_table(_webhook_room, _webhook_user, prior_alt)
			};

			if(prior_table_id)
				clear_reactions(_webhook_room, _webhook_user, prior_table_id);
		}
	}

	bool outputs{false};
	if(conclusion == "failure" && webhook_status_errors)
	{
		outputs = true;
		out
		<< "<br>"
		<< "<font data-mx-bg-color=\"#CC0000\" color=\"#FFFFFF\">"
		<< "&nbsp;"
		<< "&nbsp;"
		<< "<b>"
		<< flow_name
		<< "</b>"
		<< "&nbsp;"
		<< "&nbsp;"
		<< "</font>"
		<< " failed "
		<< "<a href=\""
		<< url
		<< "\">"
		<< "<b>"
		<< job_name
		<< "</b>"
		<< "</a>"
		;

		alt
		<< ' '
		<< flow_name
		<< ':'
		<< job_name
		<< ' '
		<< "failed"
		;
	}

	return outputs;
}

static void
github_react_handle(const m::event &event,
                    m::vm::eval &)
try
{
	if(!webhook_room)
		return;

	// XXX alias?
	if(json::get<"room_id"_>(event) != webhook_room)
		return;

	const m::room room
	{
		at<"room_id"_>(event)
	};

	const m::user::id user_id
	{
		at<"sender"_>(event)
	};

	const m::room::power power
	{
		room
	};

	// XXX ???
	if(power.level_user(user_id) < 50)
		return;

	const json::object relates_to
	{
		json::get<"content"_>(event).get("m.relates_to")
	};

	const json::string relates_event_id
	{
		relates_to["event_id"]
	};

	const json::string key
	{
		relates_to["key"]
	};

	const auto relates_content
	{
		m::get(relates_event_id, "content")
	};

	const json::string relates_body
	{
		json::object{relates_content}.get("body")
	};

	if(startswith(relates_body, "job status table "))
	{
		const auto suffix
		{
			lstrip(relates_body, "job status table ")
		};

		string_view token[3];
		ircd::tokens(suffix, ' ', token);
		const auto &[repopath, run_id, attempt] {token};
		assert(repopath);
		assert(run_id);
		if(!repopath || !run_id)
			return;

		switch(hash(key))
		{
			case hash("🚮"_sv):
				github_run_delete(repopath, run_id);
				m::redact(room, user_id, relates_event_id, "deleted");
				break;

			case hash("⭕"_sv):
				github_run_cancel(repopath, run_id);
				break;

			case hash("🔄"_sv):
				github_run_rerun_failed(repopath, run_id);
				break;

			case hash("↪️"_sv):
				github_run_rerun(repopath, run_id);
				break;
		}
	}
	else if(startswith(relates_body, "push by "))
	{
		const auto suffix
		{
			lstrip(relates_body, "push by ")
		};

		string_view token[5];
		ircd::tokens(suffix, ' ', token);
		const auto &[pusher, _by_, repo, _at_, commit] {token};
		assert(pusher);
		assert(repo);
		assert(commit);
		if(!repo)
			return;

		if(startswith(key, "▶️"))
		{
			const auto id
			{
				between(key, '(', ')')
			};

			const auto ref
			{
				// commit // hash not supported by github
				"master"
			};

			github_run_dispatch(repo, id, ref, json::members
			{

			});

			return;
		}
	}
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		"github react handle hook :%s",
		e.what(),
	};
}

static m::hookfn<m::vm::eval &>
github_react_hook
{
	github_react_handle,
	{
		{ "_site",   "vm.effect"   },
		{ "type",    "m.reaction"  },
	}
};

bool
github_handle__check_run(std::ostream &out,
                         const json::object &content)
{
	const json::string action
	{
		content["action"]
	};

	const json::object check_run
	{
		content["check_run"]
	};

	const json::object check_suite
	{
		check_run["check_suite"]
	};

	return false;
}

bool
github_handle__check_suite(std::ostream &out,
                           const json::object &content)
{
	const json::string action
	{
		content["action"]
	};

	const json::object check_suite
	{
		content["check_suite"]
	};

	return false;
}

bool
github_handle__gollum(std::ostream &out,
                      const json::object &content)
{
	const json::array pages
	{
		content["pages"]
	};

	const auto count
	{
		size(pages)
	};

	out
	<< " to "
	<< "<b>"
	<< count
	<< "</b>"
	<< " page" << (count != 1? "s" : "")
	<< ":"
	;

	for(const json::object page : pages)
	{
		const json::string &action
		{
			page["action"]
		};

		const json::string sha
		{
			page["sha"]
		};

		out
		<< "<br />"
		<< "<b>"
		<< sha.substr(0, 8)
		<< "</b>"
		<< " " << action << " "
		<< "<a href=" << page["html_url"] << ">"
		<< "<b>"
		<< json::string(page["title"])
		<< "</b>"
		<< "</a>"
		;

		if(page["summary"] && page["summary"] != "null")
		{
			out
			<< " "
			<< "<blockquote>"
			<< "<pre>"
			;

			static const auto delim("\\r\\n");
			const json::string body(page["summary"]);
			ircd::tokens(body, delim, [&out]
			(const string_view &line)
			{
				out << line << "<br />";
			});

			out
			<< ""
			<< "</pre>"
			<< "</blockquote>"
			;
		}
	}

	return true;
}

bool
github_handle__milestone(std::ostream &out,
                         const json::object &content)
{
	const json::string &action
	{
		content["action"]
	};

	const json::object milestone
	{
		content["milestone"]
	};

	out
	<< " "
	<< action
	<< " "
	<< "<a href="
	<< milestone["html_url"]
	<< ">"
	<< "<b>"
	<< json::string(milestone["title"])
	<< "</b>"
	<< "</a>"
	<< ' '
	;

	const json::string &state
	{
		milestone["state"]
	};

	if(state == "open")
		out
	    << "<font color=\"#FFFFFF\""
	    << "data-mx-bg-color=\"#2cbe4e\">"
		;
	else if(state == "closed")
		out
	    << "<font color=\"#FFFFFF\""
	    << "data-mx-bg-color=\"#cb2431\">"
	    ;

	out
	<< "&nbsp;<b>"
	<< state
	<< "</b>&nbsp;"
	<< "</font>"
	;

	out
	<< ' '
	<< "<pre><code>"
	<< json::string(milestone["description"])
	<< "</code></pre>"
	;

	out
	<< ' '
	<< "Issues"
	<< ' '
	<< "open"
	<< "&nbsp;"
    << "<font color=\"#2cbe4e\">"
	<< "<b>"
	<< milestone["open_issues"]
	<< "</b>"
	<< "</font>"
	<< ' '
	<< "closed"
	<< ' '
    << "<font color=\"#cb2431\">"
	<< "<b>"
	<< milestone["closed_issues"]
	<< "</b>"
	<< "</font>"
	;

	return true;
}

bool
github_handle__push(std::ostream &out,
                    const json::object &content)
{
	const bool created
	{
		content.get("created", false)
	};

	const bool deleted
	{
		content.get("deleted", false)
	};

	const bool forced
	{
		content.get("forced", false)
	};

	const json::array commits
	{
		content["commits"]
	};

	const auto count
	{
		size(commits)
	};

	if(!count && deleted)
	{
		out << " <font color=\"#FF0000\">";
		if(content["ref"])
			out << " " << json::string(content["ref"]);

		out << " deleted</font>";
		return true;
	}

	if(!count && !webhook_status_verbose)
		return false;

	if(content["ref"])
	{
		const json::string ref(content["ref"]);
		out << " "
		    << " "
		    << token_last(ref, '/');
	}

	out << " <a href=\"" << json::string(content["compare"]) << "\">"
	    << "<b>" << count << " commits</b>"
	    << "</a>";

	if(content["forced"] == "true")
		out << " (rebase)";

	out << "<pre>";
	for(ssize_t i(count - 1); i >= 0; --i)
	{
		const json::object &commit(commits.at(i));
		const json::string url(commit["url"]);
		const json::string id(commit["id"]);
		const auto sid(id.substr(0, 8));
		out << " <a href=\"" << url << "\">"
		    << "<b>" << sid << "</b>"
		    << "</a>";

		const json::object author(commit["author"]);
		out << " <b>"
		    << json::string(author["name"])
		    << "</b>"
		    ;

		const json::object committer(commit["committer"]);
		if(committer["email"] != author["email"])
			out << " via <b>"
			    << json::string(committer["name"])
			    << "</b>"
			    ;

		const json::string message(commit["message"]);
		const auto summary
		{
			split(message, "\\n").first
		};

		out << " "
		    << summary
		    ;

		out << "\n";
	}

	out << "</pre>";
	return true;
}

static bool
github_post_handle__push(const m::event::id &push_event_id,
                         const json::object &content)
{
	const m::user::id::buf _webhook_user
	{
		string_view{webhook_user}, my_host()
	};

	const auto _webhook_room
	{
		m::room_id(string_view(webhook_room))
	};

	const auto repo
	{
		github_repopath(content)
	};

	github_flow_for_each(repo, [&]
	(const json::object &flow)
	{
		const json::string name
		{
			flow["name"]
		};

		const json::string id
		{
			flow["id"]
		};

		const fmt::bsprintf<128> key
		{
			"▶️ %s (%s)", name, id
		};

		const auto annote_id
		{
			m::annotate(_webhook_room, _webhook_user, push_event_id, string_view(key))
		};

		return true;
	});

	return true;
}

bool
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
		    << json::string(content["action"])
		    << "</b>"
		    ;

	if(pr["title"])
		out << " "
		    << "<a href="
		    << pr["html_url"]
		    << ">"
		    << json::string(pr["title"])
		    << "</a>"
		    << "&nbsp;"
		    << ' '
		    ;

	const json::object head
	{
		pr["head"]
	};

	const json::object base
	{
		pr["base"]
	};

	for(const json::object label : json::array(pr["labels"]))
	{
		out << "&nbsp;";
		out << "<font color=\"#FFFFFF\""
		    << "data-mx-bg-color=\"#"
		    << json::string(label["color"])
		    << "\">";

		out << "<b>";
		out << "&nbsp;";
			out << json::string(label["name"]);
		out << "&nbsp;";
		out << "</b>";

		out << "</font>";
	}

	if(pr["merged"] == "true")
		out << ' '
		    << "<font color=\"#FFFFFF\""
		    << "data-mx-bg-color=\"#6f42c1\">"
		    << "&nbsp;<b>"
		    << "merged"
		    << "</b>&nbsp;"
		    << "</font>"
		    ;

	if(pr.has("merged_by") && pr["merged_by"] != "null")
	{
		const json::object merged_by{pr["merged_by"]};
		out << " "
		    << "by "
		    << "<a href=\""
		    << json::string(merged_by["html_url"])
		    << "\">"
		    << json::string(merged_by["login"])
		    << "</a>"
		    ;
	}

	const json::string &body
	{
		pr["body"]
	};

	if(!empty(body))
		out << ' '
		    << "<pre>"
		    << body
		    << "</pre>"
		    << ' '
		    ;
	else
		out << ' '
		    << "<br />"
		    ;

	if(pr.has("commits"))
		out << ' '
		    << "&nbsp;"
		    << "<b>"
		    << pr["commits"]
		    << ' '
		    << "<a href="
		    << github_url(pr["commits_url"])
		    << ">"
		    << "commits"
		    << "</a>"
		    << "</b>"
		    ;

	if(pr.has("comments"))
		out << ' '
		    << "&nbsp;"
		    << "<b>"
		    << pr["comments"]
		    << ' '
		    << "<a href="
		    << github_url(pr["comments_url"])
		    << ">"
		    << "comments"
		    << "</a>"
		    << "</b>"
		    ;

	if(pr.has("changed_files"))
		out << ' '
		    << "&nbsp;"
		    << "<b>"
		    << pr["changed_files"]
		    << ' '
		    << "<a href=\""
		    << json::string(pr["html_url"])
		    << "/files"
		    << "\">"
		    << "files"
		    << "</a>"
		    << "</b>"
		    ;

	if(pr.has("additions"))
		out << ' '
		    << "&nbsp;"
		    << "<b>"
		    << "<font color=\"#33CC33\">"
		    << "++"
		    << "</font>"
		    << pr["additions"]
		    << "</b>"
		    ;

	if(pr.has("deletions"))
		out << ' '
		    << "<b>"
		    << "<font color=\"#CC0000\">"
		    << "--"
		    << "</font>"
		    << pr["deletions"]
		    << "</b>"
		    ;

	if(pr["merged"] == "false") switch(hash(pr["mergeable"]))
	{
		default:
		case "null"_:
			break;

		case "true"_:
			out << ' '
			    << "<font color=\"#FFFFFF\""
			    << "data-mx-bg-color=\"#03B381\">"
			    << "<b>"
			    << "&nbsp;"
			    << "NO CONFLICTS"
			    << "&nbsp;"
			    << "</b>"
			    << "</font>"
			    ;
			break;

		case "false"_:
			out << ' '
			    << "<font color=\"#FFFFFF\""
			    << "data-mx-bg-color=\"#CC0000\">"
			    << "<b>"
			    << "&nbsp;"
			    << "MERGE CONFLICT"
			    << "&nbsp;"
			    << "</b>"
			    << "</font>"
			    ;
			break;
	}

	return true;
}

bool
github_handle__issues(std::ostream &out,
                      const json::object &content)
{
	const json::string &action
	{
		content["action"]
	};

	out << " "
	    << "<b>"
	    << action
	    << "</b>"
	    ;

	const json::object issue
	{
		content["issue"]
	};

	switch(hash(action))
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
			    << json::string(assignee["html_url"])
			    << "\">"
			    << json::string(assignee["login"])
			    << "</a>"
			    ;

			break;
		}
	}

	out << " "
	    << "<a href=\""
	    << json::string(issue["html_url"])
	    << "\">"
	    << "<b><u>"
	    << json::string(issue["title"])
	    << "</u></b>"
	    << "</a>"
	    ;

	for(const json::object label : json::array(issue["labels"]))
	{
		out << "&nbsp;";
		out << "<font color=\"#FFFFFF\""
		    << "data-mx-bg-color=\"#"
		    << json::string(label["color"])
		    << "\">";

		out << "<b>";
		out << "&nbsp;";
			out << json::string(label["name"]);
		out << "&nbsp;";
		out << "</b>";

		out << "</font>";
	}

	if(action == "opened")
	{
		out << " "
		    << "<blockquote>"
		    << "<pre>"
		    ;

		static const auto delim("\\r\\n");
		const json::string body(issue["body"]);
		ircd::tokens(body, delim, [&out]
		(const string_view &line)
		{
			out << line << "<br />";
		});

		out << ""
		    << "</pre>"
		    << "</blockquote>"
		    ;
	}
	else if(action == "labeled")
	{
		// quiet these messages for now until we can figure out how to reduce
		// noise around issue opens.
		return false;

		const json::object label
		{
			content["label"]
		};

		out << "<ul>";
		out << "<li>added: ";
		out << "<font color=\"#FFFFFF\""
		    << "data-mx-bg-color=\"#"
		    << json::string(label["color"])
		    << "\">";

		out << "<b>";
		out << "&nbsp;";
		out << json::string(label["name"]);
		out << "&nbsp;";
		out << "</b>";

		out << "</font>";
		out << "</li>";
		out << "</ul>";
	}
	else if(action == "unlabeled")
	{
		// quiet these messages for now until we can figure out how to reduce
		// noise around issue opens.
		return false;

		const json::object label
		{
			content["label"]
		};

		out << "<ul>";
		out << "<li>removed: ";
		out << "<font color=\"#FFFFFF\""
		    << "data-mx-bg-color=\"#"
		    << json::string(label["color"])
		    << "\">";

		out << "<b>";
		out << "&nbsp;";
		out << json::string(label["name"]);
		out << "&nbsp;";
		out << "</b>";

		out << "</font>";
		out << "</li>";
		out << "</ul>";
	}
	else if(action == "milestoned")
	{
		const json::object &milestone
		{
			content["milestone"]
		};

		out
		<< "<ul>"
		<< "<li>"
		<< "<a href="
		<< milestone["html_url"]
		<< ">"
		<< json::string(milestone["title"])
		<< "</a>"
		<< ' '
		;

		const json::string &state{milestone["state"]};
		if(state == "open")
			out
			<< "<font color=\"#FFFFFF\""
			<< "data-mx-bg-color=\"#2cbe4e\">"
			;
		else if(state == "closed")
			out
			<< "<font color=\"#FFFFFF\""
			<< "data-mx-bg-color=\"#cb2431\">"
			;

		out
		<< "&nbsp;<b>"
		<< state
		<< "</b>&nbsp;"
		<< "</font>"
		;

		out
		<< ' '
		<< "&nbsp;"
		<< "Issues"
		<< ' '
		<< "<font color=\"#2cbe4e\">"
		<< "<b>"
		<< milestone["open_issues"]
		<< "</b>"
		<< "</font>"
		<< ' '
		<< "open"
		<< ' '
		<< "<font color=\"#cb2431\">"
		<< "<b>"
		<< milestone["closed_issues"]
		<< "</b>"
		<< "</font>"
		<< ' '
		<< "closed"
		<< "</li>"
		<< "</ul>"
		;
	}

	return true;
}

bool
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
	    << json::string(issue["html_url"])
	    << "\">"
	    << "<b><u>"
	    << json::string(issue["title"])
	    << "</u></b>"
	    << "</a>"
	    ;

	if(action == "created")
	{
		out << " "
		    << "<blockquote>"
		    << "<pre>"
		    ;

		static const auto delim("\\r\\n");
		const json::string body(comment["body"]);
		ircd::tokens(body, delim, [&out]
		(const string_view &line)
		{
			out << line << "<br />";
		});

		out << ""
		    << "</pre>"
		    << "</blockquote>"
		    ;
	}

	for(const json::object label : json::array(issue["labels"]))
		out
		<< "<font color=\"#FFFFFF\""
		<< "data-mx-bg-color=\"#"
		<< json::string(label["color"])
		<< "\">"
		<< "<b>"
		<< "&nbsp;"
		<< json::string(label["name"])
		<< "&nbsp;"
		<< "</b>"
		<< "</font>"
		<< "&nbsp;"
		;

	return true;
}

bool
github_handle__commit_comment(std::ostream &out,
                              const json::object &content)
{
	const json::object comment
	{
		content["comment"]
	};

	const json::string action
	{
		content["action"]
	};

	const json::string commit
	{
		comment["commit_id"]
	};

	const json::string assoc
	{
		comment["author_association"]
	};

	char assoc_buf[32];
	if(assoc && assoc != "NONE")
		out
		<< " ["
		<< tolower(assoc_buf, assoc)
		<< "]"
		;

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
	    << json::string(comment["html_url"])
	    << "\">"
	    << "<b><u>"
	    << trunc(commit, 8)
	    << "</u></b>"
	    << "</a>"
	    ;

	if(action == "created")
	{
		out << " "
		    << "<blockquote>"
		    ;

		const json::string body
		{
			comment["body"]
		};

		static const auto delim("\\r\\n");
		ircd::tokens(body, delim, [&out]
		(const string_view &line)
		{
			out << line << "<br />";
		});

		out << ""
		    << "</blockquote>"
		    ;
	}

	return true;
}

bool
github_handle__label(std::ostream &out,
                     const json::object &content)
{
	const json::string &action
	{
		content["action"]
	};

	out << " "
	    << "<b>"
	    << action
	    << "</b>"
	    ;

	const json::object &label
	{
		content["label"]
	};

	out << "<ul>";
	out << "<li>";
	out << "<font color=\"#FFFFFF\""
	    << "data-mx-bg-color=\"#"
	    << json::string(label["color"])
	    << "\">";

	out << "<b>";
	out << " &nbsp; ";
	out << json::string(label["name"]);
	out << " &nbsp; ";
	out << "</b>";
	out << "</font>";
	out << "</li>";
	out << "</ul>";

	if(action == "edited")
	{
		const json::object &changes
		{
			content["changes"]
		};

		const json::object &color_obj
		{
			changes["color"]
		};

		const json::string &color
		{
			empty(color_obj)?
				label["color"]:
				color_obj["from"]
		};

		const json::object &name_obj
		{
			changes["name"]
		};

		const json::string &name
		{
			empty(name_obj)?
				label["name"]:
				name_obj["from"]
		};

		out << "from: ";
		out << "<ul>";
		out << "<li>";
		out << "<font color=\"#FFFFFF\""
		    << "data-mx-bg-color=\"#"
		    << color
		    << "\">";

		out << "<b>";
		out << " &nbsp; ";
		out << name;
		out << " &nbsp; ";
		out << "</b>";
		out << "</font>";
		out << "</li>";
		out << "</ul>";
	}

	return true;
}

bool
github_handle__organization(std::ostream &out,
                            const json::object &content)
{
	const json::string &action
	{
		content["action"]
	};

	const auto &action_words
	{
		split(action, '_')
	};

	out << " " << "<b>";

	if(action_words.second)
		out
		<< split(action, '_').second
		<< " ";

	out << action_words.first << "</b>";

	if(action == "member_added")
	{
		const json::object &membership
		{
			content["membership"]
		};

		const json::object &user
		{
			membership["user"]
		};

		out << " "
		    << "<a href=" << user["html_url"] << ">"
		    << json::string(user["login"])
		    << "</a>"
		    ;

		out << " with role "
		    << json::string(membership["role"])
		    ;
	}
	else if(action == "member_removed")
	{
		const json::object &membership
		{
			content["membership"]
		};

		const json::object &user
		{
			membership["user"]
		};

		out << " "
		    << "<a href=" << user["html_url"] << ">"
		    << json::string(user["login"])
		    << "</a>"
		    ;
	}
	else if(action == "member_invited")
	{
		const json::object &invitation
		{
			content["invitation"]
		};

		const json::object &user
		{
			invitation["user"]
		};

		out << " "
		    << "<a href=" << user["html_url"] << ">"
		    << json::string(user["login"])
		    << "</a>"
		    ;
	}

	return true;
}

bool
github_handle__status(std::ostream &out,
                      const json::object &content)
{
	const m::user::id::buf _webhook_user
	{
		string_view{webhook_user}, my_host()
	};

	const auto _webhook_room_id
	{
		m::room_id(string_view(webhook_room))
	};

	const m::room _webhook_room
	{
		_webhook_room_id
	};

	const json::string &state
	{
		content["state"]
	};

	const json::string &commit_hash
	{
		content["sha"]
	};

	const auto push_event_id
	{
		github_find_push_event_id(_webhook_room, _webhook_user, commit_hash)
	};

	if(push_event_id) switch(hash(state))
	{
		case "error"_:
			m::annotate(_webhook_room, _webhook_user, push_event_id, "⭕");
			break;

		case "failure"_:
			m::annotate(_webhook_room, _webhook_user, push_event_id, "🔴");
			break;

		case "pending"_:
			m::annotate(_webhook_room, _webhook_user, push_event_id, "🟡");
			break;

		case "success"_:
			m::annotate(_webhook_room, _webhook_user, push_event_id, "🟢");
			break;
	}

	if(!webhook_status_verbose) switch(hash(state))
	{
		case "error"_:
			return false;

		case "failure"_:
			break;

		case "pending"_:
			return false;

		case "success"_:
			return false;

		default:
			return false;
	}

	const json::string &description
	{
		content["description"]
	};

	const string_view &url
	{
		content["target_url"]
	};

	if(state == "success")
		out << " "
		    << "<font data-mx-bg-color=\"#03B381\">"
		    ;

	else if(state == "failure")
		out << " "
		    << "<font data-mx-bg-color=\"#CC0000\">"
		    ;

	else if(state == "error")
		out << " "
		    << "<font data-mx-bg-color=\"#280000\">"
		    ;

	out << "&nbsp;"
	    << "<a href="
	    << url
	    << ">";

	out << ""
	    << "<font color=\"#FFFFFF\">"
	    << "<b>"
	    << description
	    << "</b>"
	    << "</font>"
	    << "</a>"
	    << "&nbsp;"
	    << "</font>"
	    ;

	return true;
}

bool
github_handle__watch(std::ostream &out,
                     const json::object &content)
{
	const json::string &action
	{
		content["action"]
	};

	if(action != "started")
		return false;

	// There appears to be no way to distinguish between a genuine watch
	// button click and just a star; the watch event is sent for both.
	// Returning false just disables this event so there's no double-message.
	return false;
}

bool
github_handle__star(std::ostream &out,
                    const json::object &content)
{
	const json::string &action
	{
		content["action"]
	};

	if(action != "created")
		return false;

	return true;
}

bool
github_handle__repository(std::ostream &out,
                          const json::object &content)
{
	const json::string &action
	{
		content["action"]
	};

	out << ' ' << action;
	out << "<pre><code>"
	    << json::string(content["description"])
	    << "</code></pre>";

	return true;
}

bool
github_handle__create(std::ostream &out,
                      const json::object &content)
{
	const json::string &ref
	{
		content["ref"]
	};

	const json::string &ref_type
	{
		content["ref_type"]
	};

	out
	<< ' '
	<< ref_type
	<< ' '
	<< "<b>"
	<< ref
	<< "</b>"
	;

	if(ref_type == "tag")
		out << ' ' << "🎉";

	return true;
}

bool
github_handle__delete(std::ostream &out,
                      const json::object &content)
{
	const json::string &ref
	{
		content["ref"]
	};

	const json::string &ref_type
	{
		content["ref_type"]
	};

	out
	<< ' '
	<< ref_type
	<< ' '
	<< "<b>"
	<< ref
	<< "</b>"
	;

	return true;
}

bool
github_handle__ping(std::ostream &out,
                    const json::object &content)
{
	out << "<pre><code>"
	    << json::string(content["zen"])
	    << "</code></pre>";

	return true;
}

// Find the message resulting from the push and react with the status.
ircd::m::event::id::buf
github_find_push_event_id(const m::room &room,
                          const m::user::id &user_id,
                          const string_view &commit_hash)
{
	const auto type_match
	{
		[](const string_view &type) noexcept
		{
			return type == "m.room.message";
		}
	};

	const auto user_match
	{
		[&user_id](const string_view &sender) noexcept
		{
			return sender && sender == user_id;
		}
	};

	const auto content_match
	{
		[&commit_hash](const json::object &content)
		{
			const json::string &body
			{
				content["body"]
			};

			return has(body, "push") && has(body, commit_hash);
		}
	};

	// Limit the search to a maximum of recent messages from the
	// webhook user and total messages so we don't run out of control
	// and scan the whole room history.
	int lim[2] { 768, 384 };
	m::room::events it{room};
	for(; it && lim[0] > 0 && lim[1] > 0; --it, --lim[0])
	{
		if(!m::query(std::nothrow, it.event_idx(), "sender", user_match))
			continue;

		--lim[1];
		if(!m::query(std::nothrow, it.event_idx(), "type", type_match))
			continue;

		if(!m::query(std::nothrow, it.event_idx(), "content", content_match))
			continue;

		return m::event_id(std::nothrow, it.event_idx());
	}

	return {};
}

std::pair<json::string, json::string>
github_find_repo(const json::object &content)
{
	const json::object repository
	{
		content["repository"]
	};

	if(!empty(repository))
		return
		{
		    repository["full_name"], repository["html_url"]
		};

	const json::object organization
	{
		content["organization"]
	};

	return
	{
	    organization["login"], organization["url"]
	};
}

/// Researched from yestifico bot
std::pair<json::string, json::string>
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
			user["login"], user["html_url"]
		};

	const json::object sender
	{
		content["sender"]
	};

	return
	{
		sender["login"], sender["html_url"]
	};
}

/// Researched from yestifico bot
json::string
github_find_issue_number(const json::object &content)
{
	const json::object issue(content["issue"]);
	if(!empty(issue))
		return issue["number"];

	if(content["number"])
		return content["number"];

	return {};
}

/// Researched from yestifico bot
json::string
github_find_commit_hash(const json::object &content)
{
	if(content["sha"])
		return content["sha"];

	const json::object commit(content["commit"]);
	if(!empty(commit))
		return commit["sha"];

	const json::object head(content["head"]);
	if(!empty(head))
		return head["commit"];

	const json::object head_commit(content["head_commit"]);
	if(!empty(head_commit))
		return head_commit["id"];

	const json::object comment(content["comment"]);
	if(!empty(comment))
		return comment["commit_id"];

	if(content["commit"])
		return content["commit"];

	const json::object pr{content["pull_request"]};
	const json::object prhead{pr["head"]};
	if(prhead["sha"])
		return prhead["sha"];

	const json::object workflow_run{content["workflow_run"]};
	if(workflow_run["head_sha"])
		return workflow_run["head_sha"];

	const json::object workflow_job{content["workflow_job"]};
	if(workflow_job["head_sha"])
		return workflow_job["head_sha"];

	const json::object check_run{content["check_run"]};
	if(check_run["head_sha"])
		return check_run["head_sha"];

	const json::object check_suite{content["check_suite"]};
	if(check_suite["head_sha"])
		return check_suite["head_sha"];

	return {};
}

std::string
github_url(const json::string &url)
{
	std::string base("https://");
	return base + std::string(lstrip(url, "https://api."));
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

///////////////////////////////////////////////////////////////////////////////
//
// appveyor
//

void
appveyor_handle(client &client,
                const resource::request &request)
{
	const http::headers &headers
	{
		request.head.headers
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// dockerhub
//

static bool
dockerhub_handle_push(std::ostream &out,
                      std::ostream &alt,
                      client &client,
                      const resource::request &request);

void
dockerhub_handle(client &client,
                 const resource::request &request)
try
{
	if(!string_view(webhook_room))
		return;

	if(!string_view(webhook_user))
		return;

	const auto callback_url
	{
		request["callback_url"]
	};

	const unique_buffer<mutable_buffer> buf[2]
	{
		{ 48_KiB },
		{ 4_KiB  },
	};

	std::stringstream out, alt;
	pubsetbuf(out, buf[0]);
	pubsetbuf(alt, buf[1]);

	bool output {true};
	if(request.has("push_data"))
		output = dockerhub_handle_push(out, alt, client, request);

	const auto room_id
	{
		m::room_id(string_view(webhook_room))
	};

	const m::user::id::buf user_id
	{
		string_view(webhook_user), my_host()
	};

	const auto msg
	{
		view(out, buf[0])
	};

	const auto alt_msg
	{
		view(alt, buf[1])
	};

	const auto evid
	{
		output?
			m::msghtml(room_id, user_id, msg, alt_msg, "m.notice"):
			m::event::id::buf{}
	};

	log::info
	{
		"Webhook '%s' delivered to %s %s",
		"push"_sv,
		string_view{room_id},
		string_view{evid},
	};
}
catch(const std::exception &e)
{
	log::error
	{
		"dockerhub webhook :%s",
		e.what(),
	};

	throw;
}

bool
dockerhub_handle_push(std::ostream &out,
                      std::ostream &alt,
                      client &client,
                      const resource::request &request)
{
	const json::object push_data
	{
		request["push_data"]
	};

	const json::object repository
	{
		request["repository"]
	};

	const json::string pusher
	{
		push_data["pusher"]
	};

	const string_view pushed_at
	{
		push_data["pushed_at"]
	};

	const json::string tag
	{
		push_data["tag"]
	};

	const json::array images
	{
		push_data["images"]
	};

	out
	<< "<a href=" << repository["repo_url"] << ">"
	<< json::string(repository["repo_name"])
	<< "</a> push by <b>"
	<< pusher
	<< "</b> to <b>"
	<< tag
	<< "</b>"
	;

	alt
	<< json::string(repository["repo_name"])
	<< " push by "
	<< pusher
	<< " to "
	<< tag
	;

	return bool(webhook_status_verbose);
}
