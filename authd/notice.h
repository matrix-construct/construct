/* authd/notice.h - send notices back to the ircd and to clients
 * Copyright (c) 2016 Elizabeth Myers <elizabeth@interlinked.me>
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

#ifndef __CHARYBDIS_AUTHD_NOTICE_H__
#define __CHARYBDIS_AUTHD_NOTICE_H__

typedef enum
{
	L_DEBUG = 'D',
	L_INFO = 'I',
	L_WARN = 'W',
	L_CRIT ='C',
} notice_level_t;

void notice_client(uint32_t cid, const char *fmt, ...);
void warn_opers(notice_level_t level, const char *fmt, ...);
void stats_result(uint32_t cid, char letter, const char *fmt, ...);
void stats_error(uint32_t cid, char letter, const char *fmt, ...);
void stats_done(uint32_t cid, char letter);

#endif /* __CHARYBDIS_AUTHD_NOTICE_H__ */
