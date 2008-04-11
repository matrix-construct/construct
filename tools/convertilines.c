/* tools/convertilines.c
 * Copyright (c) 2002 Hybrid Development Team
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1.Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  2.Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  3.The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 *  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: convertilines.c 6 2005-09-10 01:02:21Z nenolod $
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define BUFSIZE 512

#define FLAGS_RESTRICTED	0x001
#define FLAGS_EXCEEDLIMIT	0x002
#define FLAGS_KLINEEXEMPT	0x004
#define FLAGS_NEEDIDENT		0x010
#define FLAGS_NOTILDE		0x020

struct flag_table_struct
{
	const char *name;
	int flag;
};
static struct flag_table_struct flag_table[] =
{
	{ "restricted",		FLAGS_RESTRICTED	},
	{ "exceed_limit",	FLAGS_EXCEEDLIMIT	},
	{ "kline_exempt",	FLAGS_KLINEEXEMPT	},
	{ "need_ident",		FLAGS_NEEDIDENT		},
	{ "no_tilde",		FLAGS_NOTILDE		},
	{ NULL, 0 }
};

struct AuthBlock
{
    struct AuthBlock *next;

    char **hostname;
    int hostnum;
    
    char *spoof;
    char *passwd;
    int class;
    int flags;

    /* indicates one of above */
    int special;
    int specialk;
};

static struct AuthBlock *auth_spoof = NULL;
static struct AuthBlock *auth_special = NULL;
static struct AuthBlock *auth_passwd = NULL;
static struct AuthBlock *auth_general = NULL;
static struct AuthBlock *auth_restricted = NULL;

static void ConvertConf(FILE* file,FILE *out);
static void set_flags(struct AuthBlock *, const char *, const char *);
static void usage(void);
static char *getfield(char *);
static struct AuthBlock *find_matching_conf(struct AuthBlock *);
static void ReplaceQuotes(char *out, char *in);
static void oldParseOneLine(FILE *out, char *in);
static void write_auth_entries(FILE *out);
static void write_specific(FILE *out, struct AuthBlock *);
static int match(struct AuthBlock *, struct AuthBlock *);

int main(int argc,char *argv[])
{
  FILE *in;
  FILE *out;

  if(argc < 3)
    usage();

  if((in = fopen(argv[1],"r")) == NULL)
  {
      fprintf(stderr, "Can't open %s for reading\n", argv[1]);
      usage();
  }

  if((out = fopen(argv[2],"w")) == NULL)
  {
      fprintf(stderr, "Can't open %s for writing\n", argv[2]);
      usage();
  }
  
  ConvertConf(in, out);

  return 0;
}

void usage()
{
    fprintf(stderr, "convertilines conf.old conf.new\n");
    exit(-1);
}

/*
 * ConvertConf() 
 *    Read configuration file.
 *
 *
 * Inputs        - FILE* to config file to convert
 *		 - FILE* to output for new style conf
 *
 */

#define MAXCONFLINKS 150

static void ConvertConf(FILE* file, FILE *out)
{
  char             line[BUFSIZE];
  char             quotedLine[BUFSIZE];
  char*            p;

  while (fgets(line, sizeof(line), file))
  {
      if ((p = strchr(line, '\n')))
          *p = '\0';

      ReplaceQuotes(quotedLine,line);

      if(!*quotedLine || quotedLine[0] == '#' || quotedLine[0] == '\n' ||
        quotedLine[0] == ' ' || quotedLine[0] == '\t')
          continue;

      if(quotedLine[0] == '.')
      {
          char *filename;
          char *back;

          if(!strncmp(quotedLine+1,"include ",8))
          {
              if( (filename = strchr(quotedLine+8,'"')) )
                  filename++;
              else
              {
                  fprintf(stderr, "Bad config line: %s", quotedLine);
                  continue;
              }

              if((back = strchr(filename,'"')))
                  *back = '\0';
              else
              {
                  fprintf(stderr, "Bad config line: %s", quotedLine);
                  continue;
              }

	  }
      }

      /* Could we test if it's conf line at all?        -Vesa */
      if (quotedLine[1] == ':')
          oldParseOneLine(out,quotedLine);

  }

  fclose(file);

  write_auth_entries(out);
  fclose(out);
}

/*
 * ReplaceQuotes
 * Inputs       - input line to quote
 * Output       - quoted line
 * Side Effects - All quoted chars in input are replaced
 *                with quoted values in output, # chars replaced with '\0'
 *                otherwise input is copied to output.
 */
static void ReplaceQuotes(char* quotedLine,char *inputLine)
{
  char *in;
  char *out;
  static char  quotes[] = {
    0,    /*  */
    0,    /* a */
    '\b', /* b */
    0,    /* c */
    0,    /* d */
    0,    /* e */
    '\f', /* f */
    0,    /* g */
    0,    /* h */
    0,    /* i */
    0,    /* j */
    0,    /* k */
    0,    /* l */
    0,    /* m */
    '\n', /* n */
    0,    /* o */
    0,    /* p */
    0,    /* q */
    '\r', /* r */
    0,    /* s */
    '\t', /* t */
    0,    /* u */
    '\v', /* v */
    0,    /* w */
    0,    /* x */
    0,    /* y */
    0,    /* z */
    0,0,0,0,0,0 
    };

  /*
   * Do quoting of characters and # detection.
   */
  for (out = quotedLine,in = inputLine; *in; out++, in++)
    {
      if (*in == '\\')
	{
          in++;
          if(*in == '\\')
            *out = '\\';
          else if(*in == '#')
            *out = '#';
	  else
	    *out = quotes[ (unsigned int) (*in & 0x1F) ];
	}
      else if (*in == '#')
        {
	  *out = '\0';
          return;
	}
      else
        *out = *in;
    }
  *out = '\0';
}

/*
 * oldParseOneLine
 * Inputs       - pointer to line to parse
 *		- pointer to output to write
 * Output       - 
 * Side Effects - Parse one old style conf line.
 */

static void oldParseOneLine(FILE *out,char* line)
{
  char conf_letter;
  char* tmp;
  const char* host_field=NULL;
  const char* passwd_field=NULL;
  const char* user_field=NULL;
  const char* port_field = NULL;
  const char* classconf_field = NULL;
  int class_field = 0;

  tmp = getfield(line);

  conf_letter = *tmp;

  for (;;) /* Fake loop, that I can use break here --msa */
  {
      /* host field */
      if ((host_field = getfield(NULL)) == NULL)
	return;
      
      /* pass field */
      if ((passwd_field = getfield(NULL)) == NULL)
	break;

      /* user field */
      if ((user_field = getfield(NULL)) == NULL)
	break;

      /* port field */
      if ((port_field = getfield(NULL)) == NULL)
	break;

      /* class field */
      if ((classconf_field = getfield(NULL)) == NULL)
	break;

      break;
  }

  if (!passwd_field)
    passwd_field = "";
  if (!user_field)
    user_field = "";
  if (!port_field)    
    port_field = "";
  if (classconf_field)
    class_field = atoi(classconf_field);

  switch( conf_letter )
  {
    case 'i': 
    case 'I': 
    {
        struct AuthBlock *ptr;
	struct AuthBlock *tempptr;

	tempptr = malloc(sizeof(struct AuthBlock));
	memset(tempptr, 0, sizeof(*tempptr));

	if(conf_letter == 'i')
	{
	    tempptr->flags |= FLAGS_RESTRICTED;
	    tempptr->specialk = 1;
	}

        if(passwd_field && *passwd_field)
	    tempptr->passwd = strdup(passwd_field);

	tempptr->class = class_field;

	set_flags(tempptr, user_field, host_field);

	/* dont add specials/passworded ones to existing auth blocks */
        if((ptr = find_matching_conf(tempptr)))
	{
            int authindex;

	    authindex = ptr->hostnum;
	    ptr->hostnum++;

	    ptr->hostname = realloc((void *)ptr->hostname, ptr->hostnum * sizeof(void *));

	    ptr->hostname[authindex] = strdup(tempptr->hostname[0]);

	    free(tempptr->hostname[0]);
	    free(tempptr->hostname);
	    free(tempptr);
	}
	else
	{
	    ptr = tempptr;

            if(ptr->spoof)
  	    {
	        ptr->next = auth_spoof;
	        auth_spoof = ptr;
 	    }
	    else if(ptr->special)
	    {
	        ptr->next = auth_special;
	        auth_special = ptr;
	    }
	    else if(ptr->passwd)
	    {
	        ptr->next = auth_passwd;
	        auth_passwd = ptr;
	    }
	    else if(ptr->specialk)
	    {
	        ptr->next = auth_restricted;
	        auth_restricted = ptr;
	    }
	    else
 	    {
	        ptr->next = auth_general;
	        auth_general = ptr;
	    }
	}
    }
    break;
      
    default:
      break;
  }
}

static void write_auth_entries(FILE *out)
{
    struct AuthBlock *ptr;

    for(ptr = auth_spoof; ptr; ptr = ptr->next)
	write_specific(out, ptr);

    for(ptr = auth_special; ptr; ptr = ptr->next)
	write_specific(out, ptr);

    for(ptr = auth_passwd; ptr; ptr = ptr->next)
	write_specific(out, ptr);

    for(ptr = auth_general; ptr; ptr = ptr->next)
	write_specific(out, ptr);

    for(ptr = auth_restricted; ptr; ptr = ptr->next)
	write_specific(out, ptr);
}

    
static void write_specific(FILE *out, struct AuthBlock *ptr)
{
    int i;
    int prev = 0;

    fprintf(out, "auth {\n");

    for(i = 0; i < ptr->hostnum; i++)
        fprintf(out, "\tuser = \"%s\";\n", ptr->hostname[i]);

    if(ptr->spoof)
	fprintf(out, "\tspoof = \"%s\";\n", ptr->spoof);

    if(ptr->passwd)
	fprintf(out, "\tpassword = \"%s\";\n", ptr->passwd);

    if(ptr->flags)
    {
	    fprintf(out, "\tflags = ");

	    for(i = 0; flag_table[i].flag; i++)
	    {
		    if(ptr->flags & flag_table[i].flag)
		    {
			    fprintf(out, "%s%s", 
					prev ? ", " : "", 
					flag_table[i].name);
			    prev = 1;
		    }
	    }

	    fprintf(out, ";\n");
    }

    fprintf(out, "\tclass = \"%d\";\n", ptr->class);
    fprintf(out, "};\n");
}

/*
 * field breakup for ircd.conf file.
 */
static char *getfield(char *newline)
{
  static char *line = NULL;
  char  *end, *field;
        
  if (newline)
    line = newline;

  if (line == NULL)
    return(NULL);

  field = line;
  if ((end = strchr(line,':')) == NULL)
    {
      line = NULL;
      if ((end = strchr(field,'\n')) == NULL)
        end = field + strlen(field);
    }
  else
    line = end + 1;
  *end = '\0';
  return(field);
}

struct AuthBlock *find_matching_conf(struct AuthBlock *acptr)
{
    struct AuthBlock *ptr;

    for(ptr = auth_spoof; ptr; ptr = ptr->next)
    {
	if(match(ptr, acptr))
	    return ptr;
    }

    for(ptr = auth_special; ptr; ptr = ptr->next)
    {
	if(match(ptr, acptr))
	    return ptr;
    }

    for(ptr = auth_passwd; ptr; ptr = ptr->next)
    {
	if(match(ptr, acptr))
	    return ptr;
    }

    for(ptr = auth_restricted; ptr; ptr = ptr->next)
    {
	if(match(ptr, acptr))
	    return ptr;
    }
	
    for(ptr = auth_general; ptr; ptr = ptr->next)
    {
        if(match(ptr, acptr))
	    return ptr;
    }


    return NULL;
}

static int match(struct AuthBlock *ptr, struct AuthBlock *acptr)
{
    if((ptr->class == acptr->class) &&
       (ptr->flags == acptr->flags))
    {
	const char *p1, *p2;
	
	/* check the spoofs match.. */
	if(ptr->spoof)
	   p1 = ptr->spoof;
	else
	   p1 = "";

	if(acptr->spoof)
	   p2 = acptr->spoof;
	else
	   p2 = "";

	if(strcmp(p1, p2))
	    return 0;

	/* now check the passwords match.. */
	if(ptr->passwd)
	    p1 = ptr->passwd;
	else
	    p1 = "";

	if(acptr->passwd)
	    p2 = acptr->passwd;
	else
	    p2 = "";

	if(strcmp(p1, p2))
	    return 0;

	return 1;
    }

    return 0;
}

void set_flags(struct AuthBlock *ptr, const char *user_field, const char *host_field)
{
  for(; *user_field; user_field++)
  {
      switch(*user_field)
      {
          case '=':
	      if(host_field)
	          ptr->spoof = strdup(host_field);

	      ptr->special = 1;
              break;

          case '-':
	      ptr->flags |= FLAGS_NOTILDE;
	      ptr->special = 1;
              break;

          case '+':
	      ptr->flags |= FLAGS_NEEDIDENT;
	      ptr->specialk = 1;
              break;

          case '^':        /* is exempt from k/g lines */
	      ptr->flags |= FLAGS_KLINEEXEMPT;
	      ptr->special = 1;
              break;

	  case '>':
	      ptr->flags |= FLAGS_EXCEEDLIMIT;
	      ptr->special = 1;
	      break;

	  case '!':
          case '$':
          case '%':
          case '&':
	  case '<':
              break;

          default:
	  {
	      int authindex;
	      authindex = ptr->hostnum;
	      ptr->hostnum++;

              ptr->hostname = realloc((void *)ptr->hostname, ptr->hostnum * sizeof(void *));

	      /* if the IP field contains something useful, use that */
	      if(strcmp(host_field, "NOMATCH") && (*host_field != 'x') && 
	        strcmp(host_field, "*") && !ptr->spoof)
		  ptr->hostname[authindex] = strdup(host_field);
	      else
	          ptr->hostname[authindex] = strdup(user_field);

	      return;
	  }
      }
  }
}

