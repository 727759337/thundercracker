/*
 * unlimTPSI.c for the OpenBWT project
 * Copyright (c) 2010 Yuta Mori. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif
#include <assert.h>
#include <stdio.h>
#if defined(HAVE_STDLIB_H)
# include <stdlib.h>
#else
# if defined(HAVE_MEMORY_H)
#  include <memory.h>
# endif
#endif
#ifdef _OPENMP
# include <omp.h>
#endif
#include "openbwt.h"


/*
  Unlimited Merged TPSI
             space: 4n+4*256 bytes
    best-case time: O(n)
   worst-case time: O(n lg 256)
*/

int
obwt_unbwt_unlimTPSI(const unsigned char *T, unsigned char *U, int n, int pidx) {
  int C[256];
  unsigned int *TPSI;
  unsigned int u;
  int i, t;
  int c, mask, len, half;
  int shift;

  /* Check arguments. */
  if((T == NULL) || (U == NULL) || (n < 0) || (pidx < 0) ||
     (n < pidx) || ((0 < n) && (pidx == 0))) { return -1; }
  if(n <= 1) { if(T != U) { U[0] = T[0]; } return 0; }

  /* Allocate 4n bytes of memory. */
  TPSI = (unsigned int *)malloc(n * sizeof(unsigned int));
  if(TPSI == NULL) { return -2; }

  /* Inverse BW-transform. */
  for(shift = 0; (n - 1) >> shift; ++shift) { }
  shift = 32 - shift; if(8 < shift) { shift = 8; }
  mask = (1U << shift) - 1;
  for(c = 0; c < 256; ++c) { C[c] = 0; }
  for(i = 0; i < n; ++i) { ++C[T[i]]; }
  for(c = 0, i = 0; c < 256; ++c) { t = C[c]; C[c] = i; i += t; }
  for(i = 0; i < pidx; ++i) {
    c = T[i];
    u = i - 1;
    TPSI[C[c]++] = (u << shift) | (c & mask);
  }
  for(; i < n; ++i) {
    c = T[i];
    u = i;
    TPSI[C[c]++] = (u << shift) | (c & mask);
  }
  for(i = 0, t = pidx - 1; i < n; ++i) {
    assert(0 <= t);
    assert(t < n);
    for(c = 0, len = 256 - 1, half = len >> 1; mask < len; len = half, half >>= 1) {
      if(C[c + half] <= t) { c += half + 1; }
    }
    u = TPSI[t];
    c |= u & mask;
    U[i] = (unsigned char)c;
    t = u >> shift;
  }

  /* Deallocate memory. */
  free(TPSI);

  return 0;
}

int
obwt_unbwt_unlimTPSI_omp(const obwt_ISAs_t *ISAs, const unsigned char *T, unsigned char *U, int n, int numthreads) {
#ifdef _OPENMP
  int C[256];
  unsigned int *TPSI;
  unsigned int u;
  int i, s, t, pidx;
  int c, mask;
  int shift;

  /* Check arguments. */
  if((ISAs == NULL) || (ISAs->samples == NULL) || (ISAs->numsamples <= 0)) { return -1; }
  pidx = ISAs->samples[0];
  if((ISAs->numsamples == 1) || (numthreads <= 1)) { return obwt_unbwt_unlimTPSI(T, U, n, pidx); }
  if((T == NULL) || (U == NULL) || (n < 0) || (pidx < 0) ||
     (n < pidx) || ((0 < n) && (pidx == 0))) { return -1; }
  if(n <= 1) { if(T != U) { U[0] = T[0]; } return 0; }

  /* Allocate 4n bytes of memory. */
  TPSI = (unsigned int *)malloc(n * sizeof(unsigned int));
  if(TPSI == NULL) { return -2; }

  /* Inverse BW-transform. */
  for(shift = 0; (n - 1) >> shift; ++shift) { }
  shift = 32 - shift; if(8 < shift) { shift = 8; }
  mask = (1U << shift) - 1;
  for(c = 0; c < 256; ++c) { C[c] = 0; }
  for(i = 0; i < n; ++i) { ++C[T[i]]; }
  for(c = 0, i = 0; c < 256; ++c) { t = C[c]; C[c] = i; i += t; }
  for(i = 0; i < pidx; ++i) {
    c = T[i];
    u = i - 1;
    TPSI[C[c]++] = (u << shift) | (c & mask);
  }
  for(; i < n; ++i) {
    c = T[i];
    u = i;
    TPSI[C[c]++] = (u << shift) | (c & mask);
  }
# pragma omp parallel for schedule(guided) default(none) shared(ISAs, U, n, C, TPSI, mask, shift) private(i, s, t, u, c) num_threads(numthreads)
  for(s = 0; s < ISAs->numsamples; ++s) {
    int first, last;
    int len, half;
    first = s << ISAs->lgsamplerate;
    last = ((s + 1) < ISAs->numsamples) ? (s + 1) << ISAs->lgsamplerate : n;
    t = ISAs->samples[s] - 1;
    for(i = first; i < last; ++i) {
      assert(0 <= t);
      assert(t < n);
      for(c = 0, len = 256 - 1, half = len >> 1; mask < len; len = half, half >>= 1) {
        if(C[c + half] <= t) { c += half + 1; }
      }
      u = TPSI[t];
      c |= u & mask;
      U[i] = (unsigned char)c;
      t = u >> shift;
    }
  }

  /* Deallocate memory. */
  free(TPSI);

  return 0;
#else
  (void)numthreads;
  if((ISAs == NULL) || (ISAs->samples == NULL) || (ISAs->numsamples <= 0)) { return -1; }
  return obwt_unbwt_unlimTPSI(T, U, n, ISAs->samples[0]);
#endif
}
