// Downloaded from: https://github.com/CTrabant/teeny-sha1/

#if 0& License_for_teeny_sha1

MIT License

Copyright (c) 2016 

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

#endif

/*******************************************************************************
 * Teeny SHA-1
 *
 * The below sha1digest() calculates a SHA-1 hash value for a
 * specified data buffer and generates a hex representation of the
 * result.  This implementation is a re-forming of the SHA-1 code at
 * https://github.com/jinqiangshou/EncryptionLibrary.
 *
 * Copyright (c) 2017 CTrabant
 *
 * License: MIT, see included LICENSE file for details.
 *
 * To use the sha1digest() function either copy it into an existing
 * project source code file or include this file in a project and put
 * the declaration (example below) in the sources files where needed.
 ******************************************************************************/

#include "teeny-sha1.h"

/* Declaration:
extern int sha1digest(uint8_t *digest, char *hexdigest, const uint8_t *data, size_t databytes);
*/

/*******************************************************************************
 * sha1digest: https://github.com/CTrabant/teeny-sha1
 *
 * Calculate the SHA-1 value for supplied data buffer and generate a
 * text representation in hexadecimal.
 *
 * Based on https://github.com/jinqiangshou/EncryptionLibrary, credit
 * goes to @jinqiangshou, all new bugs are mine.
 *
 * @input:
 *    data      -- data to be hashed
 *    databytes -- bytes in data buffer to be hashed
 *
 * @output:
 *    digest    -- the result, MUST be at least 20 bytes
 *    hexdigest -- the result in hex, MUST be at least 41 bytes
 *
 * At least one of the output buffers must be supplied.  The other, if not 
 * desired, may be set to NULL.
 *
 * @return: 0 on success and non-zero on error.
 ******************************************************************************/
int
sha1digest(uint8_t *digest, char *hexdigest, const uint8_t *data, size_t databytes)
{
#define SHA1ROTATELEFT(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

  uint32_t W[80];
  uint32_t H[] = {0x67452301,
                  0xEFCDAB89,
                  0x98BADCFE,
                  0x10325476,
                  0xC3D2E1F0};
  uint32_t a;
  uint32_t b;
  uint32_t c;
  uint32_t d;
  uint32_t e;
  uint32_t f = 0;
  uint32_t k = 0;

  uint32_t idx;
  uint32_t lidx;
  uint32_t widx;
  uint32_t didx = 0;

  int32_t wcount;
  uint32_t temp;
  uint64_t databits = ((uint64_t)databytes) * 8;
  uint32_t loopcount = (databytes + 8) / 64 + 1;
  uint32_t tailbytes = 64 * loopcount - databytes;
  uint8_t datatail[128] = {0};

  if (!digest && !hexdigest)
    return -1;

  if (!data)
    return -1;

  /* Pre-processing of data tail (includes padding to fill out 512-bit chunk):
     Add bit '1' to end of message (big-endian)
     Add 64-bit message length in bits at very end (big-endian) */
  datatail[0] = 0x80;
  datatail[tailbytes - 8] = (uint8_t) (databits >> 56 & 0xFF);
  datatail[tailbytes - 7] = (uint8_t) (databits >> 48 & 0xFF);
  datatail[tailbytes - 6] = (uint8_t) (databits >> 40 & 0xFF);
  datatail[tailbytes - 5] = (uint8_t) (databits >> 32 & 0xFF);
  datatail[tailbytes - 4] = (uint8_t) (databits >> 24 & 0xFF);
  datatail[tailbytes - 3] = (uint8_t) (databits >> 16 & 0xFF);
  datatail[tailbytes - 2] = (uint8_t) (databits >> 8 & 0xFF);
  datatail[tailbytes - 1] = (uint8_t) (databits >> 0 & 0xFF);

  /* Process each 512-bit chunk */
  for (lidx = 0; lidx < loopcount; lidx++)
  {
    /* Compute all elements in W */
    memset (W, 0, 80 * sizeof (uint32_t));

    /* Break 512-bit chunk into sixteen 32-bit, big endian words */
    for (widx = 0; widx <= 15; widx++)
    {
      wcount = 24;

      /* Copy byte-per byte from specified buffer */
      while (didx < databytes && wcount >= 0)
      {
        W[widx] += (((uint32_t)data[didx]) << wcount);
        didx++;
        wcount -= 8;
      }
      /* Fill out W with padding as needed */
      while (wcount >= 0)
      {
        W[widx] += (((uint32_t)datatail[didx - databytes]) << wcount);
        didx++;
        wcount -= 8;
      }
    }

    /* Extend the sixteen 32-bit words into eighty 32-bit words, with potential optimization from:
       "Improving the Performance of the Secure Hash Algorithm (SHA-1)" by Max Locktyukhin */
    for (widx = 16; widx <= 31; widx++)
    {
      W[widx] = SHA1ROTATELEFT ((W[widx - 3] ^ W[widx - 8] ^ W[widx - 14] ^ W[widx - 16]), 1);
    }
    for (widx = 32; widx <= 79; widx++)
    {
      W[widx] = SHA1ROTATELEFT ((W[widx - 6] ^ W[widx - 16] ^ W[widx - 28] ^ W[widx - 32]), 2);
    }

    /* Main loop */
    a = H[0];
    b = H[1];
    c = H[2];
    d = H[3];
    e = H[4];

    for (idx = 0; idx <= 79; idx++)
    {
      if (idx <= 19)
      {
        f = (b & c) | ((~b) & d);
        k = 0x5A827999;
      }
      else if (idx >= 20 && idx <= 39)
      {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      }
      else if (idx >= 40 && idx <= 59)
      {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      }
      else if (idx >= 60 && idx <= 79)
      {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }
      temp = SHA1ROTATELEFT (a, 5) + f + e + k + W[idx];
      e = d;
      d = c;
      c = SHA1ROTATELEFT (b, 30);
      b = a;
      a = temp;
    }

    H[0] += a;
    H[1] += b;
    H[2] += c;
    H[3] += d;
    H[4] += e;
  }

  /* Store binary digest in supplied buffer */
  if (digest)
  {
    for (idx = 0; idx < 5; idx++)
    {
      digest[idx * 4 + 0] = (uint8_t) (H[idx] >> 24);
      digest[idx * 4 + 1] = (uint8_t) (H[idx] >> 16);
      digest[idx * 4 + 2] = (uint8_t) (H[idx] >> 8);
      digest[idx * 4 + 3] = (uint8_t) (H[idx]);
    }
  }

  /* Store hex version of digest in supplied buffer */
  if (hexdigest)
  {
    snprintf (hexdigest, 41, "%08x%08x%08x%08x%08x",
              H[0],H[1],H[2],H[3],H[4]);
  }

  return 0;
}  /* End of sha1digest() */


#ifdef TESTSHA1
/******************************************************************************
 * A test program for the SHA-1 function in teeny-sha1.c
 *
 * The tests are conducted by calculating the SHA-1 values using
 * sha1digest() and comparing the result to the known digest value.
 *
 * Included are some data values and known hashes from
 * http://www.di-mgt.com.au/sha_testvectors.html, a few examples from
 * https://en.wikipedia.org/wiki/SHA-1 and the ability to use test
 * vectors from NIST's National Software Reference Library (NSRL).
 *
 * Command line options:
 *   -l : Perform a large (1GB) test
 *   -nsrl <directory>
 *        Perform tests with vectors provided in:
 *        http://www.nsrl.nist.gov/testdata/NSRLvectors.zip
 *        The test vectors must be downloaded and unzipped manually.
 *
 * Build test program:
 *   cc -g -O2 -Wall teeny-sha1.c teeny-sha1-test.c -o teeny-sha1-test
 ******************************************************************************/

#include <sys/stat.h>
#include <errno.h>

/* Declare sha1digest and use directly from teeny-sha1.c */
extern int sha1digest(uint8_t *digest, char *hexdigest, const uint8_t *data, size_t databytes);

/* Generate hexadecimal version of a 20-byte digest value */
void
generatehex (char *hexdigest, uint8_t *digest)
{
  int idx;

  for (idx = 0; idx < 20; idx++)
  {
    sprintf (hexdigest + (idx * 2), "%02x", digest[idx]);
  }
  hexdigest[40] = '\0';
}

/* Run hash function and print comparison results.
 * Return positive mismatch count with known digest, 0 when all match. */
int
testhash (char *data, size_t databytes, char *knowndigest)
{
  uint8_t digest[20];
  char hexdigest[41];
  char binhexdigest[41];
  char *hexstatus;
  char *binstatus;
  int mismatch = 0;

  /* Generate digest, both binary and hex versions */
  if (sha1digest (digest, hexdigest, (uint8_t *)data, databytes))
  {
    fprintf (stderr, "Error with sha1digest()\n");
    return 1;
  }

  /* Generate hex version from binary digest */
  generatehex (binhexdigest, digest);

  /* Compare hex versions to know digest in hex */
  if (strcasecmp (hexdigest, knowndigest))
  {
    hexstatus = "does NOT match";
    mismatch++;
  }
  else
  {
    hexstatus = "matches";
  }
  if (strcasecmp (binhexdigest, knowndigest))
  {
    binstatus = "does NOT match";
    mismatch++;
  }
  else
  {
    binstatus = "matches";
  }

  printf ("Known digest:  '%s'  data length: %zu\n", knowndigest, databytes);
  printf ("  Hex digest:  '%s'  %s\n", hexdigest, hexstatus);
  printf ("  Bin digest:  '%s'  %s\n", binhexdigest, binstatus);
  printf ("\n");

  return mismatch;
}

int
main (int argc, char **argv)
{
  char *data;
  char *knowndigest;
  char *nsrldir = NULL;
  int failurecount = 0;
  int largetest = 0;
  int idx;

  /* Process command line options */
  for (idx = 0; idx < argc; idx++)
  {
    /* A "-l" option turns on the large test */
    if (!strcmp (argv[idx], "-l"))
      largetest = 1;

    /* A "-nsrl <directory>" option does testing with the NSRL Sample Vectors subset */
    if (!strcmp (argv[idx], "-nsrl"))
    {
      if ((idx + 1) == argc)
      {
        printf ("-nsrl option requires a directory argument\n");
        return 1;
      }
      else
        nsrldir = argv[idx + 1];
    }
  }

  /* Test vectors from http://www.di-mgt.com.au/sha_testvectors.html */
  data = "abc";
  knowndigest = "a9993e364706816aba3e25717850c26c9cd0d89d";
  failurecount += testhash (data, strlen(data), knowndigest);

  data = "";
  knowndigest = "da39a3ee5e6b4b0d3255bfef95601890afd80709";
  failurecount += testhash (data, strlen(data), knowndigest);

  data = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  knowndigest = "84983e441c3bd26ebaae4aa1f95129e5e54670f1";
  failurecount += testhash (data, strlen(data), knowndigest);

  data = "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
  knowndigest = "a49b2446a02c645bf419f995b67091253a04a259";
  failurecount += testhash (data, strlen(data), knowndigest);

  /* One million repetitions of 'a' */
  data = (char *)malloc (1000000);
  memset (data, 'a', 1000000);
  knowndigest = "34aa973cd4c4daa4f61eeb2bdbad27316534016f";
  failurecount += testhash (data, 1000000, knowndigest);
  free (data);

  /* Large test (~1GB), 16,777,216 repetitions of base */
  if (largetest)
  {
    char *base = "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno";
    size_t baselen = strlen(base);
    data = (char *) malloc (16777216 * baselen);
    for (idx = 0; idx < 16777216; idx++)
    {
      memcpy (data + (idx * baselen), base, baselen);
    }

    knowndigest = "7789f0c9ef7bfc40d93311143dfbe69e2017f592";
    failurecount += testhash (data, baselen * 16777216, knowndigest);
    free(data);
  }

  /* Example hashes from Wikipedia */
  data = "The quick brown fox jumps over the lazy dog";
  knowndigest = "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12";
  failurecount += testhash (data, strlen(data), knowndigest);

  data = "The quick brown fox jumps over the lazy cog";
  knowndigest = "de9f2c7fd25e1b3afad3e85a0bd17d9b100db4b3";
  failurecount += testhash (data, strlen(data), knowndigest);

  /* Read test vectors from the NIST NSRL Sample Vectors subset:
     http://www.nsrl.nist.gov/testdata/
     http://www.nsrl.nist.gov/testdata/NSRLvectors.zip */
  if (nsrldir)
  {
    FILE *infile;
    struct stat sb;
    char *nsrlhashes[200] = {0};
    char path[100];
    char line[100];
    int hashcount = 0;
    int idx;

    /* Open byte-hashes.sha1 and store known hash values (in hex) */
    snprintf (path, sizeof(path), "%s/byte-hashes.sha1", nsrldir);
    if (!(infile = fopen(path, "r")))
    {
      printf ("Error opening %s: %s\n", path, strerror(errno));
      return 1;
    }
    while (fgets(line, sizeof(line), infile))
    {
      /* Store known hash values, e.g. "DA39A3EE5E6B4B0D3255BFEF95601890AFD80709 ^" */
      if (strlen(line) >= 42 && line[40] == ' ' && line[41] == '^')
      {
        line[40] = '\0';
        nsrlhashes[hashcount] = strdup(line);
        hashcount++;
      }
    }
    fclose (infile);

    fprintf (stderr, "hash count: %d\n", hashcount);

    /* Open byte####.dat file that match the order of the hashes */
    for (idx = 0; idx < hashcount; idx++)
    {
      snprintf (path, sizeof(path), "%s/byte%04d.dat", nsrldir, idx);
      if (!(infile = fopen(path, "r")))
      {
        printf ("Error opening %s: %s\n", path, strerror(errno));
        return 1;
      }

      /* Read file contents into buffer */
      fstat (fileno(infile), &sb);
      data = (char *) malloc ((size_t)sb.st_size);
      if (sb.st_size > 0 && fread (data, (size_t)sb.st_size, 1, infile) != 1)
      {
        if (ferror(infile))
          printf ("Error reading %s: %s\n", path, strerror(errno));
        else
          printf ("Short read of %s, expected %zu bytes\n", path, (size_t)sb.st_size);

        return 1;
      }
      fclose (infile);

      printf ("File: %s\n", path);
      failurecount += testhash (data, (size_t)sb.st_size, nsrlhashes[idx]);

      free (data);
    }
  } /* Done with NSRL test vectors */


  printf ("Failures: %d\n", failurecount);

  return failurecount;
}
#endif
