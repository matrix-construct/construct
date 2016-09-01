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
#define HAVE_IRCD_NEWCONF_H

#ifdef __cplusplus
namespace ircd    {
namespace conf    {
namespace newconf {

using key = std::string;                             // before the equals sign in an item
using val = std::vector<std::string>;                // Either one or more elems after '='
using item = std::pair<key, val>;                    // Pairing of key/vals
using block = std::pair<key, std::vector<item>>;     // key is optional "label" { items };
using topconf = std::multimap<key, block>;           // key is type of block i.e admin { ... };

/* Notes:
 * Some topconf entries are not blocks, but just key/values like "loadmodule." For this, the
 * topconf multimap contains keys of "loadmodule," and a block entry containing an empty key,
 * a vector of one item, with the item key also being "loadmodule" and the value being the
 * module to load.
 */

topconf parse(const std::string &str);
topconf parse(std::ifstream &file);
topconf parse_file(const std::string &path);

}      // namespace newconf
}      // namespace conf
}      // namespace ircd
#endif // __cplusplus
