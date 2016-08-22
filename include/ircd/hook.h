/*
 * Copyright (C) 2004-2005 Lee Hardy <lee -at- leeh.co.uk>
 * Copyright (C) 2004-2005 ircd-ratbox development team
 */

#pragma once
#define HAVE_IRCD_HOOK_H

#ifdef __cplusplus
namespace ircd {

typedef struct
{
	char *name;
	rb_dlink_list hooks;
} hook;

typedef void (*hookfn) (void *data);

extern int h_iosend_id;
extern int h_iorecv_id;
extern int h_iorecvctrl_id;

extern int h_burst_client;
extern int h_burst_channel;
extern int h_burst_finished;
extern int h_server_introduced;
extern int h_server_eob;
extern int h_client_exit;
extern int h_umode_changed;
extern int h_new_local_user;
extern int h_new_remote_user;
extern int h_introduce_client;
extern int h_can_kick;
extern int h_privmsg_channel;
extern int h_privmsg_user;
extern int h_conf_read_start;
extern int h_conf_read_end;
extern int h_outbound_msgbuf;
extern int h_rehash;

void init_hook(void);
int register_hook(const char *name);
void add_hook(const char *name, hookfn fn);
void remove_hook(const char *name, hookfn fn);
void call_hook(int id, void *arg);

typedef struct
{
	client::client *client;
	void *arg1;
	void *arg2;
} hook_data;

typedef struct
{
	client::client *client;
	const void *arg1;
	const void *arg2;
} hook_cdata;

typedef struct
{
	client::client *client;
	const void *arg1;
	int arg2;
	int result;
} hook_data_int;

typedef struct
{
	client::client *client;
	client::client *target;
	chan::chan *chptr;
	int approved;
} hook_data_client;

typedef struct
{
	client::client *client;
	chan::chan *chptr;
	int approved;
} hook_data_channel;

typedef struct
{
	client::client *client;
	chan::chan *chptr;
	char *key;
} hook_data_channel_activity;

typedef struct
{
	client::client *client;
	chan::chan *chptr;
	chan::membership *msptr;
	client::client *target;
	int approved;
	int dir;
	const char *modestr;
} hook_data_channel_approval;

typedef struct
{
	client::client *client;
	client::client *target;
	int approved;
} hook_data_client_approval;

typedef struct
{
	client::client *local_link; /* local client originating this, or NULL */
	client::client *target; /* dying client */
	client::client *from; /* causing client (could be &me or target) */
	const char *comment;
} hook_data_client_exit;

typedef struct
{
	client::client *client;
	unsigned int oldumodes;
	unsigned int oldsnomask;
} hook_data_umode_changed;

enum message_type {
	MESSAGE_TYPE_NOTICE,
	MESSAGE_TYPE_PRIVMSG,
	MESSAGE_TYPE_PART,
	MESSAGE_TYPE_COUNT
};

typedef struct
{
	enum message_type msgtype;
	client::client *source_p;
	chan::chan *chptr;
	const char *text;
	int approved;
	const char *reason;
} hook_data_privmsg_channel;

typedef struct
{
	enum message_type msgtype;
	client::client *source_p;
	client::client *target_p;
	const char *text;
	int approved;
} hook_data_privmsg_user;

typedef struct
{
	bool signal;
} hook_data_rehash;

}      // namespace ircd
#endif // __cplusplus
