/* This code is in the public domain.
 * $Nightmare: nightmare/include/config.h,v 1.32.2.2.2.2 2002/07/02 03:41:28 ejb Exp $
 * $Id: newconf.h 1735 2006-07-19 02:35:40Z nenolod $
 */

#ifndef _NEWCONF_H_INCLUDED
#define _NEWCONF_H_INCLUDED

struct ConfEntry
{
	const char *cf_name;
	int cf_type;
	void (*cf_func) (void *);
	int cf_len;
	void *cf_arg;
};

struct TopConf
{
	const char *tc_name;
	int (*tc_sfunc) (struct TopConf *);
	int (*tc_efunc) (struct TopConf *);
	rb_dlink_list tc_items;
	struct ConfEntry *tc_entries;
};


#define CF_QSTRING	0x01
#define CF_INT		0x02
#define CF_STRING	0x03
#define CF_TIME		0x04
#define CF_YESNO	0x05
#define CF_LIST		0x06
#define CF_ONE		0x07

#define CF_MTYPE	0xFF

#define CF_FLIST	0x1000
#define CF_MFLAG	0xFF00

typedef struct conf_parm_t_stru
{
	struct conf_parm_t_stru *next;
	int type;
	union
	{
		char *string;
		int number;
		struct conf_parm_t_stru *list;
	}
	v;
}
conf_parm_t;

extern struct TopConf *conf_cur_block;

extern char *current_file;

int read_config(char *);
int conf_start_block(char *, char *);
int conf_end_block(struct TopConf *);
int conf_call_set(struct TopConf *, char *, conf_parm_t *, int);
void conf_report_error(const char *, ...);
void newconf_init(void);
int add_conf_item(const char *topconf, const char *name, int type, void (*func) (void *));
int remove_conf_item(const char *topconf, const char *name);
int add_top_conf(const char *name, int (*sfunc) (struct TopConf *), int (*efunc) (struct TopConf *), struct ConfEntry *items);
int remove_top_conf(char *name);
struct TopConf *find_top_conf(const char *name);
struct ConfEntry *find_conf_item(const struct TopConf *top, const char *name);

#endif
