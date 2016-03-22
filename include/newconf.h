/* This code is in the public domain.
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


#define CF_QSTRING	0x01 /* quoted string */
#define CF_INT		0x02
#define CF_STRING	0x03 /* unquoted string */
#define CF_TIME		0x04
#define CF_YESNO	0x05

#define CF_MTYPE	0xFF /* mask for type */

/* CF_FLIST is used to allow specifying that an option accepts a list of (type)
 * values. conf_parm_t.type will never actually have another type & CF_FLIST;
 * it's only used as a true flag in newconf.c (which only consumes conf_parm_t
 * structures and doesn't create them itself).
 */
#define CF_FLIST	0x0100 /* flag for list */
#define CF_MFLAG	0xFF00 /* mask for flags */

/* conf_parm_t.type must be either one type OR one flag. this is pretty easy to
 * enforce because lists always contain nested conf_parm_t structures whose
 * .type is the real type, so it doesn't need to be stored in the top-level one
 * anyway.
 */
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
int conf_call_set(struct TopConf *, char *, conf_parm_t *);
void conf_report_error(const char *, ...);
void conf_report_warning(const char *, ...);
void newconf_init(void);
int add_conf_item(const char *topconf, const char *name, int type, void (*func) (void *));
int remove_conf_item(const char *topconf, const char *name);
int add_top_conf(const char *name, int (*sfunc) (struct TopConf *), int (*efunc) (struct TopConf *), struct ConfEntry *items);
int remove_top_conf(char *name);
struct TopConf *find_top_conf(const char *name);
struct ConfEntry *find_conf_item(const struct TopConf *top, const char *name);

#endif
