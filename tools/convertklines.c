/************************************************************************
 *   IRC - Internet Relay Chat, tools/convertklines.c
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: convertklines.c 6 2005-09-10 01:02:21Z nenolod $
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define BUFSIZE 512

static void ConvertConf(FILE* file,FILE *outkline, FILE *outdline);
static void usage(void);
static char *getfield(char *);
static void ReplaceQuotes(char *out, char *in);
static void parse(FILE *outkline, FILE *outdline, char *in);

int main(int argc,char *argv[])
{
  FILE *in;
  FILE *outkline;
  FILE *outdline;

  if(argc < 4)
    usage();

  if (( in = fopen(argv[1],"r")) == NULL )
    {
      fprintf(stderr,"Can't open %s for reading\n", argv[1]);
      usage();
    }

  if (( outkline = fopen(argv[2],"w")) == NULL )
    {
      fprintf(stderr,"Can't open %s for writing\n", argv[2]);
      usage();
    }
  
  if(( outdline = fopen(argv[3], "w")) == NULL )
    {
      fprintf(stderr, "Can't open %s for writing\n", argv[3]);
      usage();
    }
    
  ConvertConf(in, outkline, outdline);

  fprintf(stderr, "The kline file has been converted and should be renamed to\n");
  fprintf(stderr, "the config.h options (normally kline.conf and dline.conf) and\n");
  fprintf(stderr, "placed in your etc/ dir\n");
  return 0;
}

static void usage()
{
  fprintf(stderr, "klines and dlines now go in separate files:\n");
  fprintf(stderr,"convertklines kline.conf.old kline.conf.new dline.conf.new\n");
  exit(-1);
}


/*
 * ConvertConf() 
 *    Read configuration file.
 *
 *
 * inputs	- FILE* to config file to convert
 *		- FILE* to output for new klines
 *		- FILE* to output for new dlines
 * outputs	- -1 if the file cannot be opened
 *		- 0 otherwise
 */

static void ConvertConf(FILE* file, FILE *outkline, FILE *outdline)
{
  char             line[BUFSIZE];
  char             quotedLine[BUFSIZE];
  char*            p;

  while (fgets(line, sizeof(line), file))
    {
      if ((p = strchr(line, '\n')))
        *p = '\0';

      ReplaceQuotes(quotedLine,line);

      if (!*quotedLine || quotedLine[0] == '#' || quotedLine[0] == '\n' ||
          quotedLine[0] == ' ' || quotedLine[0] == '\t')
        continue;

      /* Could we test if it's conf line at all?        -Vesa */
      if (quotedLine[1] == ':')
      {
        parse(outkline, outdline, quotedLine);
      }
    }

  fclose(file);
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
 * parse()
 * Inputs       - pointer to line to parse
 *		- pointer to output to write
 * Output       - 
 * Side Effects - Parse one old style conf line.
 */

static void parse(FILE *outkline, FILE *outdline, char* line)
{
  char conf_letter;
  char *tmp;
  const char *user_field = NULL;
  const char *passwd_field = NULL;
  const char *host_field = NULL;
  const char *operpasswd_field = NULL;

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
    else
    {
      /* if theres a password, try splitting the operreason out */
      char *p;

      if((p = strchr(passwd_field, '|')))
      {
        *p++ = '\0';
        operpasswd_field = p;
      }
      else
        operpasswd_field = "";
    }

    /* user field */
    if ((user_field = getfield(NULL)) == NULL)
      break;

    /* what could possibly be after a userfield? */
    break;
  }

  if (!passwd_field)
  {
    passwd_field = "";
    operpasswd_field = "";
  }
   
  if (!user_field)
    user_field = "";
    
  switch( conf_letter )
  {
    case 'd':
      fprintf(stderr, "exempt in old file, ignoring.\n");
      break;

    case 'D': /* Deny lines (immediate refusal) */
      if(host_field && passwd_field)
        fprintf(outdline, "\"%s\",\"%s\",\"%s\",\"\",\"Unknown\",0\n",
	        host_field, passwd_field, operpasswd_field);
      break;

    case 'K': /* Kill user line on irc.conf           */
    case 'k':
      if(host_field && passwd_field && user_field)
        fprintf(outkline, "\"%s\",\"%s\",\"%s\",\"%s\",\"\",\"Unknown\",0\n",
	        user_field, host_field, passwd_field, operpasswd_field);
      break;

    default:
      fprintf(stderr, "Error in config file: %s", line);
      break;
  }
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
  {
    fprintf(stderr, "returned null!\n");
    return NULL;
  }
  
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
  
  return field;
}


