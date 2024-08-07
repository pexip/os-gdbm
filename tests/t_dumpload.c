/* This file is part of GDBM test suite.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.

   GDBM is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GDBM is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GDBM. If not, see <http://www.gnu.org/licenses/>.
*/
#include "autoconf.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include "gdbm.h"

#if __STDC_VERSION__ < 199901L
# if __GNUC__ >= 2
#  define __func__ __FUNCTION__
# else
#  define __func__ "<unknown>"
# endif
#endif

char a_name[] = "a.gdbm";
char b_name[] = "b.gdbm";
char orig_name[] = "orig.gdbm";
char bin_dumpname[] = "a.bin.dump";
char ascii_dumpname[] = "a.ascii.dump";

void
db_perror (char const *fmt, ...)
{
  int en = errno;
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fprintf (stderr, ": %s", gdbm_strerror (gdbm_errno));
  if (gdbm_check_syserr (gdbm_errno))
    fprintf (stderr, ": %s", strerror (en));
  fputc ('\n', stderr);
}

GDBM_FILE
createdb (char const *name)
{
  int rc;
  char *cmdstub = "num2word 1:2000 | gtload -clear ";
  char *cmd;
  size_t cmdlen;
  GDBM_FILE db;

  cmdlen = strlen (cmdstub) + strlen (name);
  cmd = malloc (cmdlen + 1);
  assert (cmd != NULL);
  strcat (strcpy (cmd, cmdstub), name);

  rc = system (cmd);
  if (rc == -1)
    {
      perror (cmd);
      exit (1);
    }
  else if (WIFEXITED (rc))
    {
      rc = WEXITSTATUS (rc);
      if (rc)
	{
	  fprintf (stderr, "%s: terminated with code %d\n", cmd, rc);
	  exit (1);
	}
    }
  else if (WIFSIGNALED (rc))
    {
      fprintf (stderr, "%s: terminated on signal %d", cmd, WTERMSIG (rc));
      if (WCOREDUMP (rc))
	fprintf (stderr, " (core dumped)");
      fputc ('\n', stderr);
      exit (1);
    }
  else
    {
      fprintf (stderr, "%s: terminated with unrecognized status: %d", cmd, rc);
      exit (1);
    }

  db = gdbm_open (name, 0, GDBM_READER, 0, NULL);
  if (db == NULL)
    {
      db_perror ("can't open %s", name);
      exit (1);
    }

  return db;
}

int
db_cmp (GDBM_FILE a, GDBM_FILE b)
{
  datum key, adata, bdata;
  gdbm_count_t an, bn;

  if (gdbm_count (a, &an))
    {
      fprintf (stderr, "a: gdbm_count: %s\n", gdbm_db_strerror (a));
      return -1;
    }
  if (gdbm_count (b, &bn))
    {
      fprintf (stderr, "b: gdbm_count: %s\n", gdbm_db_strerror (b));
      return -1;
    }

  if (an != bn)
    {
      fprintf (stderr, "key counts differ: a=%lu, b=%lu\n",
	       (unsigned long)an, (unsigned long)bn);
      return -1;
    }

  key = gdbm_firstkey (a);
  while (key.dptr)
    {
      datum nextkey;

      adata = gdbm_fetch (a, key);
      if (adata.dptr == NULL)
	{
	  fprintf (stderr, "a: can't get %*.*s: %s\n",
		   key.dsize, key.dsize, key.dptr,
		   gdbm_db_strerror (a));
	  free (key.dptr);
	  return -1;
	}

      bdata = gdbm_fetch (b, key);
      if (bdata.dptr == NULL)
	{
	  fprintf (stderr, "b: can't get %*.*s: %s\n",
		   key.dsize, key.dsize, key.dptr,
		   gdbm_db_strerror (b));
	  free (key.dptr);
	  free (adata.dptr);
	  return -1;
	}

      if (adata.dsize != bdata.dsize ||
	  memcmp (adata.dptr, bdata.dptr, adata.dsize))
	{
	  fprintf (stderr, "data differ for %*.*s\n",
		   key.dsize, key.dsize, key.dptr);
	  free (key.dptr);
	  free (adata.dptr);
	  free (bdata.dptr);
	  return -1;
	}

      free (adata.dptr);
      free (bdata.dptr);

      nextkey = gdbm_nextkey (a, key);
      free (key.dptr);
      key = nextkey;
    }

  return 0;
}

/*
 * Load binary dump into a non-existing (NULL) database.  This should
 * fail with GDBM_NO_DBNAME.
 */
void
test_bindump_0 (GDBM_FILE dbf)
{
  GDBM_FILE b = NULL;

  if (gdbm_load (&b, bin_dumpname, GDBM_INSERT, 0, NULL))
    {
      if (gdbm_errno != GDBM_NO_DBNAME)
	{
	  db_perror ("%s: loading binary dump to non-existing database failed with unexpected error", __func__);
	  exit (1);
	}
    }
  else
    {
      fprintf (stderr, "%s: loading binary dump to non-existing database succeeded when it should not\n", __func__);
      exit (1);
    }
}

/*
 * Load binary dump into an existing empty database.
 */
void
test_bindump_1 (GDBM_FILE dbf)
{
  GDBM_FILE b;

  /*
   * Create an empty database and retry loading into it.
   */
  b = gdbm_open (b_name, 0, GDBM_NEWDB, 0644, NULL);
  if (b == NULL)
    {
      db_perror ("%s: can't open %s", __func__, b_name);
      exit (1);
    }

  if (gdbm_load (&b, bin_dumpname, GDBM_INSERT, 0, NULL))
    {
      db_perror ("%s: failed to load database from binary dump", __func__);
      exit (1);
    }

  /* Compare two databases. */
  if (db_cmp (dbf, b))
    {
      fprintf (stderr, "%s: databases differ\n", __func__);
      exit (1);
    }

  gdbm_close (b);
}

/*
 * Load binary dump to a non-empty database.
 * When tried with GDBM_INSERT replace parameter, it is expected to
 * fail with GDBM_CANNOT_REPLACE.
 * When run with GDBM_REPLACE, it should succeed.
 */
void
test_bindump_2 (GDBM_FILE dbf)
{
  GDBM_FILE b;
  datum key, dat;

  b = gdbm_open (b_name, 0, GDBM_NEWDB, 0644, NULL);
  if (b == NULL)
    {
      db_perror ("%s: can't open %s", __func__, b_name);
      exit (1);
    }

  key.dptr = "99";
  key.dsize = 2;
  dat.dptr = "99";
  dat.dsize = 2;
  if (gdbm_store (b, key, dat, 0))
    {
      db_perror ("%s: can't store datum", __func__);
      exit (1);
    }
  gdbm_sync (b);
  
  if (gdbm_load (&b, bin_dumpname, GDBM_INSERT, 0, NULL))
    {
      if (gdbm_errno != GDBM_CANNOT_REPLACE)
	{
	  db_perror ("%s: expected GDBM_CANNOT_REPLACE, but got", __func__);
	  exit (1);
	}
    }

  if (gdbm_load (&b, bin_dumpname, GDBM_REPLACE, 0, NULL))
    {
      db_perror ("%s: failed to load from ASCII dump", __func__);
      exit (1);
    }

  if (db_cmp (dbf, b))
    {
      fprintf (stderr, "%s: databases differ\n", __func__);
      exit (1);
    }
  
  gdbm_close (b);
}
  
/*
 * Test dumping to and restoring from binary dump format.
 */
void
test_bindump (GDBM_FILE dbf)
{
  /*
   * Create a binary dump.
   */
  if (gdbm_dump (dbf, bin_dumpname, GDBM_DUMP_FMT_BINARY, GDBM_NEWDB, 0600))
    {
      fprintf (stderr, "%s: failed to dump: %s\n", __func__,
	       gdbm_db_strerror (dbf));
      exit (1);
    }

  test_bindump_0 (dbf);
  test_bindump_1 (dbf);
  test_bindump_2 (dbf);
}

/*
 * Load ASCII dump into a non-existing (NULL) database.
 */
void
test_asciidump_0 (GDBM_FILE dbf)
{
  GDBM_FILE b = NULL;

  /*
   * Loading into a non-existing database should create it.
   */
  if (gdbm_load (&b, ascii_dumpname, GDBM_INSERT,
		 GDBM_META_MASK_MODE|GDBM_META_MASK_OWNER, NULL))
    {
      db_perror ("%s: can't load from ascii dump", __func__);
      exit (1);
    }

  /*
   * The loaded database should have the same name as the original.
   */
  if (access (a_name, F_OK))
    {
      fprintf (stderr, "%s: %s: %s\n", __func__, a_name, strerror (errno));
      exit (1);
    }

  /* Compare the two databases. */
  if (db_cmp (dbf, b))
    {
      fprintf (stderr, "%s: databases differ\n", __func__);
      exit (1);
    }
  
  gdbm_close (b);

  /* Cleanup */
  unlink (a_name);
}

/*
 * Load ASCII dump into an existing empty database.
 */
void
test_asciidump_1 (GDBM_FILE dbf)
{
  GDBM_FILE b;

  /*
   * Now try loading into an existing database.
   */
  b = gdbm_open (b_name, 0, GDBM_NEWDB, 0644, NULL);
  if (b == NULL)
    {
      db_perror ("%s: can't open %s", __func__, b_name);
      exit (1);
    }

  if (gdbm_load (&b, ascii_dumpname, GDBM_INSERT,
		 GDBM_META_MASK_MODE|GDBM_META_MASK_OWNER, NULL))
    {
      db_perror ("%s: can't load from ascii dump", __func__);
      exit (1);
    }

  /*
   * Just in case: check if a_name was not created on disk.
   */
  if (access (a_name, F_OK) == 0)
    {
      fprintf (stderr, "%s: %s exists when it should not\n", __func__, a_name);
      exit (1);
    }

  if (db_cmp (dbf, b))
    {
      fprintf (stderr, "%s: databases differ\n", __func__);
      exit (1);
    }
  
  gdbm_close (b);
}

/*
 * Load ASCII dump to a non-empty database.
 * When tried with GDBM_INSERT replace parameter, it is expected to
 * fail with GDBM_CANNOT_REPLACE.
 * When run with GDBM_REPLACE, it should succeed.
 */
void
test_asciidump_2 (GDBM_FILE dbf)
{
  GDBM_FILE b;
  datum key, dat;

  b = gdbm_open (b_name, 0, GDBM_NEWDB, 0644, NULL);
  if (b == NULL)
    {
      db_perror ("%s: can't open %s", __func__, b_name);
      exit (1);
    }

  key.dptr = "99";
  key.dsize = 2;
  dat.dptr = "99";
  dat.dsize = 2;
  if (gdbm_store (b, key, dat, 0))
    {
      db_perror ("%s: can't store datum", __func__);
      exit (1);
    }
  gdbm_sync (b);

  if (gdbm_load (&b, ascii_dumpname, GDBM_INSERT,
		 GDBM_META_MASK_MODE|GDBM_META_MASK_OWNER, NULL))
    {
      if (gdbm_errno != GDBM_CANNOT_REPLACE)
	{
	  db_perror ("%s: expected GDBM_CANNOT_REPLACE, but got", __func__);
	  exit (1);
	}
    }

  if (gdbm_load (&b, ascii_dumpname, GDBM_REPLACE,
		 GDBM_META_MASK_MODE|GDBM_META_MASK_OWNER, NULL))
    {
      db_perror ("%s: failed to load from ASCII dump", __func__);
      exit (1);
    }

  if (db_cmp (dbf, b))
    {
      fprintf (stderr, "%s: databases differ\n", __func__);
      exit (1);
    }
  
  gdbm_close (b);
}

/*
 * Test dumping to and restoring from ASCII dump format.
 */
void
test_asciidump (GDBM_FILE dbf)
{
  /*
   * Create a dump in ASCII format.
   */
  if (gdbm_dump (dbf, ascii_dumpname, GDBM_DUMP_FMT_ASCII, GDBM_NEWDB, 0600))
    {
      fprintf (stderr, "%s: failed to dump: %s\n", __func__,
	       gdbm_db_strerror (dbf));
      exit (1);
    }

  /*
   * Rename the original database.
   */
  if (unlink (orig_name) && errno != ENOENT)
    {
      fprintf (stderr, "%s: failed to remove %s: %s\n", __func__,
	       orig_name, strerror (errno));
      exit (1);
    }

  if (rename (a_name, orig_name))
    {
      fprintf (stderr, "%s: can't rename %s to %s: %s\n", __func__,
	       a_name, orig_name, strerror (errno));
      exit (1);
    }

  test_asciidump_0 (dbf);
  test_asciidump_1 (dbf);
  test_asciidump_2 (dbf);
}

int
main (int argc, char **argv)
{
  GDBM_FILE db;

  db = createdb (a_name);
  test_bindump (db);
  test_asciidump (db);
  unlink (a_name);
  unlink (b_name);
  unlink (orig_name);
  unlink (bin_dumpname);
  unlink (ascii_dumpname);
  return 0;
}
