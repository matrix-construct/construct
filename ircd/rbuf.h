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

#ifdef __cplusplus
namespace ircd {

struct rbuf
{
	unique_buffer<mutable_buffer> buf;
	tape reel;
	std::exception_ptr eptr;
	uint16_t checked;
	uint16_t length;

	bool terminated() const;                     // rbuf has LF termination
	uint16_t remaining() const;                  // bytes remaining in rbuf

	size_t handle_pck(const error_code &, const size_t) noexcept;
	void reset();

	rbuf(const size_t &size = BUFSIZE);
};

inline
rbuf::rbuf(const size_t &size)
:buf{size}
,checked{0}
,length{0}
{
}

inline void
rbuf::reset()
{
	checked = 0;
	length = 0;
}

inline size_t
rbuf::handle_pck(const error_code &ec,
                 const size_t bytes)
noexcept try
{
	if(ec)
		return 0;

	length += bytes;
	if(reel.append(data<const char *>(buf), length))
		return 0;

	if(terminated())
		throw rfc1459::syntax_error("invalid syntax"); //TODO: eps + ERR_

	checked = length;
	return remaining()?: throw rfc1459::syntax_error("message length exceeded"); //TODO: ERR_
}
catch(...)
{
	eptr = std::current_exception();
	return 0;
}

inline uint16_t
ircd::rbuf::remaining()
const
{
	return size(buf) - length;
}

inline bool
rbuf::terminated()
const
{
	const auto b(std::next(rbegin(buf), remaining()));
	const auto e(std::next(rbegin(buf), size(buf) - checked));
	return std::find(b, e, '\n') != e;
}

}      // namespace ircd
#endif // __cplusplus
