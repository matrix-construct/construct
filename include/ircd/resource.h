/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
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

#pragma once
#define HAVE_IRCD_RESOURCE_H

namespace ircd {

struct resource
{
	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, not_found)
	IRCD_EXCEPTION(error, already_exists)

	struct member;
	struct method;
	struct request;
	struct response;

	static std::map<string_view, resource *> resources;

	const char *const name;
	const char *const description;
	std::map<string_view, method *> methods;

  protected:
	decltype(resources)::const_iterator resources_it;

  public:
	void operator()(client &, parse::capstan &, const http::request::head &) const;

	resource(const char *const &name,
	         const char *const &description = "");

	virtual ~resource() noexcept;
};

struct resource::response
{
	response(client &, const json::doc &doc, const http::code &code = http::OK);
	response(client &, const json::obj &obj, const http::code &code = http::OK);
	response() = default;
	~response() noexcept;
};

struct resource::request
{
	const http::request::head &head;
	http::request::content &content;
};

struct resource::method
:std::function<response (client &, request &)>
{
	using handler = std::function<response (client &, request &)>;

  protected:
  public:
	const char *const name;
	struct resource *resource;
	decltype(resource::methods)::const_iterator methods_it;
	std::map<string_view, member *> members;

  public:
	method(struct resource &,
	       const char *const &name,
	       const handler &,
	       const std::initializer_list<member *> & = {});

	~method() noexcept;
};

struct resource::member
{
	IRCD_EXCEPTION(resource::error, error)

	using validator = std::function<void (const string_view &)>;

	const char *name;
	enum json::type type;
	validator valid;

	member(const char *const &name, const enum json::type &, validator = {});
};

} // namespace ircd
