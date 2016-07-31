/*
 * Copyright (c) 2012 - 2016 William Pitcock <nenolod@dereferenced.org>.
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

#ifndef __CAPABILITY_H__
#define __CAPABILITY_H__

#include <ircd/stdinc.h>
#include <ircd/util.h>

struct CapabilityEntry {
	std::string cap;
	unsigned int value;
	bool require;
	bool orphan;
	void *ownerdata;

	CapabilityEntry(std::string cap_, unsigned int value_, void *ownerdata_)
		: cap(cap_), value(value_), require(false), orphan(false), ownerdata(ownerdata_) { }
};

struct CapabilityIndex {
	std::string name;
	std::map<std::string, std::shared_ptr<CapabilityEntry>, case_insensitive_less> cap_dict;
	unsigned int highest_bit;

	CapabilityIndex(std::string name_);
	~CapabilityIndex();

	std::shared_ptr<CapabilityEntry> find(std::string cap_name);

	unsigned int get(std::string cap_name, void **ownerdata);
	unsigned int put(std::string cap_name, void *ownerdata);
	unsigned int put_anonymous();

	void orphan(std::string cap_name);
	void require(std::string cap_name);

	void stats(void (*cb)(std::string &line, void *privdata), void *privdata);

	const char *list(unsigned int cap_mask);
	unsigned int mask();
	unsigned int required();
};

extern void capability_stats(void (*cb)(std::string &line, void *privdata), void *privdata);

#endif
