/*
 * Copyright (c) 2016 Charybdis development team
 * Copyright (c) 2012 - 2016 William Pitcock <nenolod@dereferenced.org>.
 * Copyright (c) 2016 Jason Volk <jason@zemos.net>
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
#define HAVE_IRCD_CAPABILITY_H

#ifdef __cplusplus
namespace ircd       {
namespace capability {

struct entry
{
	std::string cap;
	uint32_t value;
	bool require;
	bool orphan;
	void *ownerdata;   //TODO: inheritance model

	entry(const std::string &cap, const uint32_t &value, void *const &ownerdata);
};

struct index
{
	std::string name;
	uint32_t highest_bit;
	std::list<index *>::iterator indexes_it;
	std::map<std::string, std::shared_ptr<entry>, case_insensitive_less> caps;

	void stats(void (*cb)(const std::string &line, void *privdata), void *privdata) const;
	std::string list(const uint32_t &cap_mask);
	uint32_t required();
	uint32_t mask();

	bool orphan(const std::string &cap_name);
	bool require(const std::string &cap_name);
	uint32_t get(const std::string &cap_name, void **ownerdata);
	uint32_t put(const std::string &cap_name = "", void *ownerdata = nullptr);
	uint32_t put_anonymous();

	index(const std::string &name);
	~index();
};

void stats(void (*cb)(const std::string &line, void *privdata), void *privdata);

} // namespace capability
} // namespace ircd
#endif // __cplusplus
