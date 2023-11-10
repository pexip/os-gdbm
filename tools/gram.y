%{
/* This file is part of GDBM, the GNU data base manager.
   Copyright (C) 1990-2022 Free Software Foundation, Inc.

   GDBM is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GDBM is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GDBM. If not, see <http://www.gnu.org/licenses/>.    */

#include <autoconf.h>
#include "gdbmtool.h"

struct dsegm *dsdef[DS_MAX];
  
%}

%define parse.error verbose
%locations
     
%token <type> T_TYPE
%token T_OFF "off"
       T_PAD "pad"
       T_DEF "define"
       T_SET "set"
       T_UNSET "unset"
       T_BOGUS

%token <cmd> T_CMD "command verb"
%token <cmd> T_SHELL "shell"
%token <num> T_NUM "number"
%token <string> T_IDENT "identifier" T_WORD "word"
%type <cmd> command
%type <string> string 
%type <arg> arg
%type <arglist> arglist arg1list
%type <dsegm> def defbody
%type <dsegmlist> deflist
%type <num> defid
%type <kvpair> kvpair compound value
%type <kvlist> kvlist
%type <slist> slist

%union {
  char *string;
  struct kvpair *kvpair;
  struct { struct kvpair *head, *tail; } kvlist;
  struct { struct slist *head, *tail; } slist;
  struct gdbmarg *arg;
  struct gdbmarglist arglist;
  int num;
  struct datadef *type;
  struct dsegm *dsegm;
  struct { struct dsegm *head, *tail; } dsegmlist;
  struct command *cmd;
}

%destructor { gdbmarglist_free (&$$); } <arglist>
%destructor { gdbmarg_free ($$); } <arg>
%destructor { kvlist_free ($$.head); } <kvlist>
%destructor { kvlist_free ($$); } <kvpair>
%destructor { slist_free ($$.head); } <slist>
%destructor { free ($$); } <string>
%destructor { dsegm_list_free ($$); } <dsegm>
%destructor { dsegm_list_free ($$.head); } <dsegmlist>

%%

input     : /* empty */
          | stmtlist
	  ;

stmtlist  : stmt
          | stmtlist stmt
          ;

stmt      : /* empty */ eol
            {
	      if (run_last_command ())
		{
		  YYABORT;
		}
	    }
          | command arglist eol
            {
	      if (run_command ($1, &$2))
		{
		  YYABORT;
		}
	    }
          | set eol
          | defn eol
	  | T_BOGUS eol
	    {
	      if (interactive ())
		{
		  yyclearin;
		  yyerrok;
		}
	      else
		YYERROR;
	    }
          | error { end_def(); } eol
            {
	      if (interactive ())
		{
		  yyclearin;
		  yyerrok;
		}
	      else
		YYERROR;
	    }
          ;

command   : T_CMD
          | T_SHELL
          ;

eol       : '\n'
          | ';'
          ;

arglist   : /* empty */
            {
	      gdbmarglist_init (&$$, NULL);
	    }
          | arg1list
	  ;

arg1list  : arg
            {
	      gdbmarglist_init (&$$, $1);
	    }
          | arg1list arg
	    {
	      gdbmarglist_add (&$1, $2);
	      $$ = $1;
	    }
          ;

arg       : string
            {
	      $$ = gdbmarg_string ($1, &@1);
	    }
          | compound
	    {
	      $$ = gdbmarg_kvpair ($1, &@1);
	    }
	  ;

compound  : '{' kvlist '}'
            {
	      $$ = $2.head;
	    }
          ;

kvlist    : kvpair
            {
	      $$.head = $$.tail = $1;
	    }
          | kvlist ',' kvpair
	    {
	      if (kvlist_find ($1.head, $3->key))
		{
		  lerror (&@3, _("duplicate tag: %s"), $3->key);
		  kvlist_free ($1.head);
		  $1.head = $1.tail = NULL;
		  YYERROR;
		}
	      $1.tail->next = $3;
	      $1.tail = $3;
	      $$ = $1;
	    }
          ;

kvpair    : value
          | T_IDENT '=' value
	    {
	      $3->key = $1;
	      $$ = $3;
	    }
          ;

value     : string
            {
	      $$ = kvpair_string (&@1, $1);
	    }
          | '{' slist '}'
	    {
	      $$ = kvpair_list (&@1, $2.head);
	    }
          ;

slist     : string
            {
	      $$.head = $$.tail = slist_new_s ($1);
	    }
          | slist ',' string
	    {
	      struct slist *s = slist_new_s ($3);
	      slist_insert (&$1.tail, s);
	      $$ = $1;
	    }
          ;

string    : T_IDENT
          | T_WORD
          ;

defn      : T_DEF defid { begin_def (); } defbody
            {
	      end_def ();
	      dsegm_list_free (dsdef[$2]);
	      dsdef[$2] = $4;
	    }
          ;

defbody   : '{' deflist optcomma '}'
            {
	      $$ = $2.head;
	    }
          | T_TYPE
            {
	      $$ = dsegm_new_field ($1, NULL, 1);
	    }
          ;

optcomma  : /* empty */
          | ','
          ;

defid     : T_IDENT
            {
	      if (strcmp ($1, "key") == 0)
		{
		  $$ = DS_KEY;
		  free ($1);
		}
	      else if (strcmp ($1, "content") == 0)
		{
		  $$ = DS_CONTENT;
		  free ($1);
		}
	      else
		{
		  terror (_("expected \"key\" or \"content\", "
			    "but found \"%s\""), $1);
		  free ($1);
		  YYERROR;
		}
	    }
          ;

deflist   : def
            {
	      $$.head = $$.tail = $1;
	    }
          | deflist ',' def
	    {
	      $1.tail->next = $3;
	      $1.tail = $3;
	      $$ = $1;
	    }
          ;

def       : T_TYPE T_IDENT
            {
	      $$ = dsegm_new_field ($1, $2, 1);
	    }
          | T_TYPE T_IDENT '[' T_NUM ']'
            {
	      $$ = dsegm_new_field ($1, $2, $4);
	    }
          | T_OFF T_NUM
	    {
	      $$ = dsegm_new (FDEF_OFF);
	      $$->v.n = $2;
	    }
          | T_PAD T_NUM
	    {
	      $$ = dsegm_new (FDEF_PAD);
	      $$->v.n = $2;
	    }
          ;

set       : T_SET
            {
	      variable_print_all (stdout);
            }
          | T_SET asgnlist
	  | T_UNSET varlist
          ;

asgnlist  : asgn
          | asgnlist asgn
          ;

asgn      : T_IDENT
            {
	      int t = 1;
	      int rc;
	      char *varname = $1;
	      
	      rc = variable_set (varname, VART_BOOL, &t);
	      if (rc == VAR_ERR_NOTDEF && strncmp (varname, "no", 2) == 0)
		{
		  t = 0;
		  varname += 2;
		  rc = variable_set (varname, VART_BOOL, &t);
		}

	      switch (rc)
		{
		case VAR_OK:
		  break;
		  
		case VAR_ERR_NOTDEF:
		  lerror (&@1, _("no such variable: %s"), varname);
		  break;

		case VAR_ERR_BADTYPE:
		  lerror (&@1, _("%s is not a boolean variable"), varname);
		  break;

		case VAR_ERR_BADVALUE:
		  lerror (&@1, _("%s: setting is not allowed"), $1);
		  break;

		case VAR_ERR_GDBM:
		  dberror ("%s", _("can't set variable"));
		  break;
		  
		default:
		  lerror (&@1, _("unexpected error setting %s: %d"), $1, rc);
		}
	      free ($1);
	    }
          | T_IDENT '=' string
	    {
	      int rc = variable_set ($1, VART_STRING, $3);
	      switch (rc)
		{
		case VAR_OK:
		  break;
		  
		case VAR_ERR_NOTDEF:
		  lerror (&@1, _("no such variable: %s"), $1);
		  break;

		case VAR_ERR_BADTYPE:
		  lerror (&@1, _("%s: bad variable type"), $1);
		  break;

		case VAR_ERR_BADVALUE:
		  lerror (&@1, _("%s: value %s is not allowed"), $1, $3);
		  break;

		default:
		  lerror (&@1, _("unexpected error setting %s: %d"), $1, rc);
		}
	      free ($1);
	      free ($3);
	    }
          ;

varlist   : var
          | varlist var
          ;

var       : T_IDENT
            {
	      int rc = variable_unset ($1);
	      switch (rc)
		{
		case VAR_OK:
		  break;
		  
		case VAR_ERR_NOTDEF:
		  lerror (&@1, _("no such variable: %s"), $1);
		  break;

		case VAR_ERR_BADVALUE:
		  lerror (&@1, _("%s: variable cannot be unset"), $1);
		  break;
		}
	      free ($1);
	    }
          ;

%%

void
terror (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  vlerror (&yylloc, fmt, ap);
  va_end (ap);
}

/* gdbmshell-specific version of gdbm_perror */
void
dberror (char const *fmt, ...)
{
  int ec = errno;
  va_list ap;

  if (gdbm_error_is_masked (gdbm_errno))
    return;
  if (!interactive ())
    fprintf (stderr, "%s: ", progname);
  YY_LOCATION_PRINT (stderr, yylloc);
  fprintf (stderr, ": ");
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fprintf (stderr, ": %s", gdbm_strerror (gdbm_errno));
  if (gdbm_check_syserr (gdbm_errno))
    fprintf (stderr, ": %s", strerror (ec));
  fputc ('\n', stderr);
}

void
yyerror (char const *s)
{
  terror ("%s", s);
}

void
gram_trace (int n)
{
#if GDBMTOOL_DEBUG  
  yydebug = 1;
#endif
}
