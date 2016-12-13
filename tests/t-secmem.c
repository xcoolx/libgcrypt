/* t-secmem.c - Test the secmem memory allocator
 * Copyright (C) 2016 g10 Code GmbH
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define PGMNAME "t-secmem"

#include "t-common.h"
#include "../src/gcrypt-testapi.h"


static void
test_secmem (void)
{
  void *a[28];
  void *b;
  int i;

  memset (a, 0, sizeof a);

  /* Allocating 28*512=14k should work in the default 16k pool even
   * with extrem alignment requirements.  */
  for (i=0; i < DIM(a); i++)
    a[i] = gcry_xmalloc_secure (512);

  /* Allocating another 2k should fail for the default 16k pool.  */
  b = gcry_malloc_secure (2048);
  if (b)
    fail ("allocation did not fail as expected\n");

  for (i=0; i < DIM(a); i++)
    xfree (a[i]);
  xfree (b);
}


static void
test_secmem_overflow (void)
{
  void *a[150];
  int i;

  memset (a, 0, sizeof a);

  /* Allocating 150*512=75k should require more than one overflow buffer.  */
  for (i=0; i < DIM(a); i++)
    {
      a[i] = gcry_xmalloc_secure (512);
      if (verbose && !(i %40))
        gcry_control (GCRYCTL_DUMP_SECMEM_STATS, 0 , 0);
    }

  if (debug)
    gcry_control (PRIV_CTL_DUMP_SECMEM_STATS, 0 , 0);
  if (verbose)
    gcry_control (GCRYCTL_DUMP_SECMEM_STATS, 0 , 0);
  for (i=0; i < DIM(a); i++)
    xfree (a[i]);
}


/* This function is called when we ran out of core and there is no way
 * to return that error to the caller (xmalloc or mpi allocation).  */
static int
outofcore_handler (void *opaque, size_t req_n, unsigned int flags)
{
  static int been_here;  /* Used to protect against recursive calls. */

  (void)opaque;

  /* Protect against a second call.  */
  if (been_here)
    return 0; /* Let libgcrypt call its own fatal error handler.  */
  been_here = 1;

  info ("outofcore handler invoked");
  gcry_control (PRIV_CTL_DUMP_SECMEM_STATS, 0 , 0);
  fail ("out of core%s while allocating %lu bytes",
       (flags & 1)?" in secure memory":"", (unsigned long)req_n);

  die ("stopped");
  /*NOTREACHED*/
  return 0;
}


int
main (int argc, char **argv)
{
  int last_argc = -1;

  if (argc)
    { argc--; argv++; }

  while (argc && last_argc != argc )
    {
      last_argc = argc;
      if (!strcmp (*argv, "--"))
        {
          argc--; argv++;
          break;
        }
      else if (!strcmp (*argv, "--help"))
        {
          fputs ("usage: " PGMNAME " [options]\n"
                 "Options:\n"
                 "  --verbose       print timings etc.\n"
                 "  --debug         flyswatter\n"
                 , stdout);
          exit (0);
        }
      else if (!strcmp (*argv, "--verbose"))
        {
          verbose++;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--debug"))
        {
          verbose += 2;
          debug++;
          argc--; argv++;
        }
      else if (!strncmp (*argv, "--", 2))
        die ("unknown option '%s'", *argv);
    }

  if (!gcry_check_version (GCRYPT_VERSION))
    die ("version mismatch; pgm=%s, library=%s\n",
         GCRYPT_VERSION, gcry_check_version (NULL));
  if (debug)
    gcry_control (GCRYCTL_SET_DEBUG_FLAGS, 1u , 0);
  gcry_control (GCRYCTL_ENABLE_QUICK_RANDOM, 0);
  gcry_control (GCRYCTL_INIT_SECMEM, 16384, 0);
  gcry_set_outofcore_handler (outofcore_handler, NULL);
  gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);

  /* Libgcrypt prints a warning when the first overflow is allocated;
   * we do not want to see that.  */
  if (!verbose)
    gcry_control (GCRYCTL_DISABLE_SECMEM_WARN, 0);


  test_secmem ();
  test_secmem_overflow ();
  /* FIXME: We need to improve the tests, for example by registering
   * our own log handler and comparing the output of
   * PRIV_CTL_DUMP_SECMEM_STATS to expected pattern.  */

  if (verbose)
    {
      gcry_control (PRIV_CTL_DUMP_SECMEM_STATS, 0 , 0);
      gcry_control (GCRYCTL_DUMP_SECMEM_STATS, 0 , 0);
    }
  info ("All tests completed.  Errors: %d\n", error_count);
  gcry_control (GCRYCTL_TERM_SECMEM, 0 , 0);
  return !!error_count;
}
