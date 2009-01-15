/*
 * Forward error correction based on Vandermonde matrices
 *
 * (C) 1997-98 Luigi Rizzo (luigi@iet.unipi.it)
 * (C) 2009 Jack Lloyd (lloyd@randombit.net)
 *
 * Distributed under the terms given in license.txt
 */

#include "fecpp.h"
#include <stdexcept>
#include <vector>
#include <cstring>
#include <sstream>

namespace {

/* Tables for arithetic in GF(2^8) using 1+x^2+x^3+x^4+x^8
 *
 * See Lin & Costello, Appendix A, and Lee & Messerschmitt, p. 453.
 *
 * Generate GF(2**m) from the irreducible polynomial p(X) in p[0]..p[m]
 * Lookup tables:
 *     index->polynomial form           gf_exp[] contains j= \alpha^i;
 *     polynomial form -> index form    gf_log[ j = \alpha^i ] = i
 * \alpha=x is the primitive element of GF(2^m)
 *
 * For efficiency, gf_exp[] has size 2*GF_SIZE, so that a simple
 * multiplication of two numbers can be resolved without calling mod
 */
const byte GF_EXP[510] = {
0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1D, 0x3A, 0x74,
0xE8, 0xCD, 0x87, 0x13, 0x26, 0x4C, 0x98, 0x2D, 0x5A, 0xB4, 0x75,
0xEA, 0xC9, 0x8F, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x9D,
0x27, 0x4E, 0x9C, 0x25, 0x4A, 0x94, 0x35, 0x6A, 0xD4, 0xB5, 0x77,
0xEE, 0xC1, 0x9F, 0x23, 0x46, 0x8C, 0x05, 0x0A, 0x14, 0x28, 0x50,
0xA0, 0x5D, 0xBA, 0x69, 0xD2, 0xB9, 0x6F, 0xDE, 0xA1, 0x5F, 0xBE,
0x61, 0xC2, 0x99, 0x2F, 0x5E, 0xBC, 0x65, 0xCA, 0x89, 0x0F, 0x1E,
0x3C, 0x78, 0xF0, 0xFD, 0xE7, 0xD3, 0xBB, 0x6B, 0xD6, 0xB1, 0x7F,
0xFE, 0xE1, 0xDF, 0xA3, 0x5B, 0xB6, 0x71, 0xE2, 0xD9, 0xAF, 0x43,
0x86, 0x11, 0x22, 0x44, 0x88, 0x0D, 0x1A, 0x34, 0x68, 0xD0, 0xBD,
0x67, 0xCE, 0x81, 0x1F, 0x3E, 0x7C, 0xF8, 0xED, 0xC7, 0x93, 0x3B,
0x76, 0xEC, 0xC5, 0x97, 0x33, 0x66, 0xCC, 0x85, 0x17, 0x2E, 0x5C,
0xB8, 0x6D, 0xDA, 0xA9, 0x4F, 0x9E, 0x21, 0x42, 0x84, 0x15, 0x2A,
0x54, 0xA8, 0x4D, 0x9A, 0x29, 0x52, 0xA4, 0x55, 0xAA, 0x49, 0x92,
0x39, 0x72, 0xE4, 0xD5, 0xB7, 0x73, 0xE6, 0xD1, 0xBF, 0x63, 0xC6,
0x91, 0x3F, 0x7E, 0xFC, 0xE5, 0xD7, 0xB3, 0x7B, 0xF6, 0xF1, 0xFF,
0xE3, 0xDB, 0xAB, 0x4B, 0x96, 0x31, 0x62, 0xC4, 0x95, 0x37, 0x6E,
0xDC, 0xA5, 0x57, 0xAE, 0x41, 0x82, 0x19, 0x32, 0x64, 0xC8, 0x8D,
0x07, 0x0E, 0x1C, 0x38, 0x70, 0xE0, 0xDD, 0xA7, 0x53, 0xA6, 0x51,
0xA2, 0x59, 0xB2, 0x79, 0xF2, 0xF9, 0xEF, 0xC3, 0x9B, 0x2B, 0x56,
0xAC, 0x45, 0x8A, 0x09, 0x12, 0x24, 0x48, 0x90, 0x3D, 0x7A, 0xF4,
0xF5, 0xF7, 0xF3, 0xFB, 0xEB, 0xCB, 0x8B, 0x0B, 0x16, 0x2C, 0x58,
0xB0, 0x7D, 0xFA, 0xE9, 0xCF, 0x83, 0x1B, 0x36, 0x6C, 0xD8, 0xAD,
0x47, 0x8E,

0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1D, 0x3A, 0x74,
0xE8, 0xCD, 0x87, 0x13, 0x26, 0x4C, 0x98, 0x2D, 0x5A, 0xB4, 0x75,
0xEA, 0xC9, 0x8F, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x9D,
0x27, 0x4E, 0x9C, 0x25, 0x4A, 0x94, 0x35, 0x6A, 0xD4, 0xB5, 0x77,
0xEE, 0xC1, 0x9F, 0x23, 0x46, 0x8C, 0x05, 0x0A, 0x14, 0x28, 0x50,
0xA0, 0x5D, 0xBA, 0x69, 0xD2, 0xB9, 0x6F, 0xDE, 0xA1, 0x5F, 0xBE,
0x61, 0xC2, 0x99, 0x2F, 0x5E, 0xBC, 0x65, 0xCA, 0x89, 0x0F, 0x1E,
0x3C, 0x78, 0xF0, 0xFD, 0xE7, 0xD3, 0xBB, 0x6B, 0xD6, 0xB1, 0x7F,
0xFE, 0xE1, 0xDF, 0xA3, 0x5B, 0xB6, 0x71, 0xE2, 0xD9, 0xAF, 0x43,
0x86, 0x11, 0x22, 0x44, 0x88, 0x0D, 0x1A, 0x34, 0x68, 0xD0, 0xBD,
0x67, 0xCE, 0x81, 0x1F, 0x3E, 0x7C, 0xF8, 0xED, 0xC7, 0x93, 0x3B,
0x76, 0xEC, 0xC5, 0x97, 0x33, 0x66, 0xCC, 0x85, 0x17, 0x2E, 0x5C,
0xB8, 0x6D, 0xDA, 0xA9, 0x4F, 0x9E, 0x21, 0x42, 0x84, 0x15, 0x2A,
0x54, 0xA8, 0x4D, 0x9A, 0x29, 0x52, 0xA4, 0x55, 0xAA, 0x49, 0x92,
0x39, 0x72, 0xE4, 0xD5, 0xB7, 0x73, 0xE6, 0xD1, 0xBF, 0x63, 0xC6,
0x91, 0x3F, 0x7E, 0xFC, 0xE5, 0xD7, 0xB3, 0x7B, 0xF6, 0xF1, 0xFF,
0xE3, 0xDB, 0xAB, 0x4B, 0x96, 0x31, 0x62, 0xC4, 0x95, 0x37, 0x6E,
0xDC, 0xA5, 0x57, 0xAE, 0x41, 0x82, 0x19, 0x32, 0x64, 0xC8, 0x8D,
0x07, 0x0E, 0x1C, 0x38, 0x70, 0xE0, 0xDD, 0xA7, 0x53, 0xA6, 0x51,
0xA2, 0x59, 0xB2, 0x79, 0xF2, 0xF9, 0xEF, 0xC3, 0x9B, 0x2B, 0x56,
0xAC, 0x45, 0x8A, 0x09, 0x12, 0x24, 0x48, 0x90, 0x3D, 0x7A, 0xF4,
0xF5, 0xF7, 0xF3, 0xFB, 0xEB, 0xCB, 0x8B, 0x0B, 0x16, 0x2C, 0x58,
0xB0, 0x7D, 0xFA, 0xE9, 0xCF, 0x83, 0x1B, 0x36, 0x6C, 0xD8, 0xAD,
0x47, 0x8E };

const byte GF_LOG[256] = {
0xFF, 0x00, 0x01, 0x19, 0x02, 0x32, 0x1A, 0xC6, 0x03, 0xDF, 0x33,
0xEE, 0x1B, 0x68, 0xC7, 0x4B, 0x04, 0x64, 0xE0, 0x0E, 0x34, 0x8D,
0xEF, 0x81, 0x1C, 0xC1, 0x69, 0xF8, 0xC8, 0x08, 0x4C, 0x71, 0x05,
0x8A, 0x65, 0x2F, 0xE1, 0x24, 0x0F, 0x21, 0x35, 0x93, 0x8E, 0xDA,
0xF0, 0x12, 0x82, 0x45, 0x1D, 0xB5, 0xC2, 0x7D, 0x6A, 0x27, 0xF9,
0xB9, 0xC9, 0x9A, 0x09, 0x78, 0x4D, 0xE4, 0x72, 0xA6, 0x06, 0xBF,
0x8B, 0x62, 0x66, 0xDD, 0x30, 0xFD, 0xE2, 0x98, 0x25, 0xB3, 0x10,
0x91, 0x22, 0x88, 0x36, 0xD0, 0x94, 0xCE, 0x8F, 0x96, 0xDB, 0xBD,
0xF1, 0xD2, 0x13, 0x5C, 0x83, 0x38, 0x46, 0x40, 0x1E, 0x42, 0xB6,
0xA3, 0xC3, 0x48, 0x7E, 0x6E, 0x6B, 0x3A, 0x28, 0x54, 0xFA, 0x85,
0xBA, 0x3D, 0xCA, 0x5E, 0x9B, 0x9F, 0x0A, 0x15, 0x79, 0x2B, 0x4E,
0xD4, 0xE5, 0xAC, 0x73, 0xF3, 0xA7, 0x57, 0x07, 0x70, 0xC0, 0xF7,
0x8C, 0x80, 0x63, 0x0D, 0x67, 0x4A, 0xDE, 0xED, 0x31, 0xC5, 0xFE,
0x18, 0xE3, 0xA5, 0x99, 0x77, 0x26, 0xB8, 0xB4, 0x7C, 0x11, 0x44,
0x92, 0xD9, 0x23, 0x20, 0x89, 0x2E, 0x37, 0x3F, 0xD1, 0x5B, 0x95,
0xBC, 0xCF, 0xCD, 0x90, 0x87, 0x97, 0xB2, 0xDC, 0xFC, 0xBE, 0x61,
0xF2, 0x56, 0xD3, 0xAB, 0x14, 0x2A, 0x5D, 0x9E, 0x84, 0x3C, 0x39,
0x53, 0x47, 0x6D, 0x41, 0xA2, 0x1F, 0x2D, 0x43, 0xD8, 0xB7, 0x7B,
0xA4, 0x76, 0xC4, 0x17, 0x49, 0xEC, 0x7F, 0x0C, 0x6F, 0xF6, 0x6C,
0xA1, 0x3B, 0x52, 0x29, 0x9D, 0x55, 0xAA, 0xFB, 0x60, 0x86, 0xB1,
0xBB, 0xCC, 0x3E, 0x5A, 0xCB, 0x59, 0x5F, 0xB0, 0x9C, 0xA9, 0xA0,
0x51, 0x0B, 0xF5, 0x16, 0xEB, 0x7A, 0x75, 0x2C, 0xD7, 0x4F, 0xAE,
0xD5, 0xE9, 0xE6, 0xE7, 0xAD, 0xE8, 0x74, 0xD6, 0xF4, 0xEA, 0xA8,
0x50, 0x58, 0xAF };

const byte GF_INVERSE[256] = {
0x00, 0x01, 0x8E, 0xF4, 0x47, 0xA7, 0x7A, 0xBA, 0xAD, 0x9D, 0xDD,
0x98, 0x3D, 0xAA, 0x5D, 0x96, 0xD8, 0x72, 0xC0, 0x58, 0xE0, 0x3E,
0x4C, 0x66, 0x90, 0xDE, 0x55, 0x80, 0xA0, 0x83, 0x4B, 0x2A, 0x6C,
0xED, 0x39, 0x51, 0x60, 0x56, 0x2C, 0x8A, 0x70, 0xD0, 0x1F, 0x4A,
0x26, 0x8B, 0x33, 0x6E, 0x48, 0x89, 0x6F, 0x2E, 0xA4, 0xC3, 0x40,
0x5E, 0x50, 0x22, 0xCF, 0xA9, 0xAB, 0x0C, 0x15, 0xE1, 0x36, 0x5F,
0xF8, 0xD5, 0x92, 0x4E, 0xA6, 0x04, 0x30, 0x88, 0x2B, 0x1E, 0x16,
0x67, 0x45, 0x93, 0x38, 0x23, 0x68, 0x8C, 0x81, 0x1A, 0x25, 0x61,
0x13, 0xC1, 0xCB, 0x63, 0x97, 0x0E, 0x37, 0x41, 0x24, 0x57, 0xCA,
0x5B, 0xB9, 0xC4, 0x17, 0x4D, 0x52, 0x8D, 0xEF, 0xB3, 0x20, 0xEC,
0x2F, 0x32, 0x28, 0xD1, 0x11, 0xD9, 0xE9, 0xFB, 0xDA, 0x79, 0xDB,
0x77, 0x06, 0xBB, 0x84, 0xCD, 0xFE, 0xFC, 0x1B, 0x54, 0xA1, 0x1D,
0x7C, 0xCC, 0xE4, 0xB0, 0x49, 0x31, 0x27, 0x2D, 0x53, 0x69, 0x02,
0xF5, 0x18, 0xDF, 0x44, 0x4F, 0x9B, 0xBC, 0x0F, 0x5C, 0x0B, 0xDC,
0xBD, 0x94, 0xAC, 0x09, 0xC7, 0xA2, 0x1C, 0x82, 0x9F, 0xC6, 0x34,
0xC2, 0x46, 0x05, 0xCE, 0x3B, 0x0D, 0x3C, 0x9C, 0x08, 0xBE, 0xB7,
0x87, 0xE5, 0xEE, 0x6B, 0xEB, 0xF2, 0xBF, 0xAF, 0xC5, 0x64, 0x07,
0x7B, 0x95, 0x9A, 0xAE, 0xB6, 0x12, 0x59, 0xA5, 0x35, 0x65, 0xB8,
0xA3, 0x9E, 0xD2, 0xF7, 0x62, 0x5A, 0x85, 0x7D, 0xA8, 0x3A, 0x29,
0x71, 0xC8, 0xF6, 0xF9, 0x43, 0xD7, 0xD6, 0x10, 0x73, 0x76, 0x78,
0x99, 0x0A, 0x19, 0x91, 0x14, 0x3F, 0xE6, 0xF0, 0x86, 0xB1, 0xE2,
0xF1, 0xFA, 0x74, 0xF3, 0xB4, 0x6D, 0x21, 0xB2, 0x6A, 0xE3, 0xE7,
0xB5, 0xEA, 0x03, 0x8F, 0xD3, 0xC9, 0x42, 0xD4, 0xE8, 0x75, 0x7F,
0xFF, 0x7E, 0xFD };

/*
* gf_mul(x,y) multiplies two numbers. If GF_BITS<=8, it is much
* faster to use a multiplication table.
*
* USE_GF_MULC, GF_MULC0(c) and GF_ADDMULC(x) can be used when multiplying
* many numbers by the same constant. In this case the first
* call sets the constant, and others perform the multiplications.
* A value related to the multiplication is held in a local variable
* declared with USE_GF_MULC . See usage in addmul1().
*/
static byte gf_mul_table[0xFF + 1][0xFF + 1];

#define gf_mul(x,y) gf_mul_table[x][y]

void
init_mul_table()
   {
   for(size_t i = 0; i < 256; ++i)
      for(size_t j = 0; j < 256; ++j)
         gf_mul_table[i][j] = GF_EXP[(GF_LOG[i] + GF_LOG[j]) % 255];

   for(size_t i = 0; i < 256; ++i)
      gf_mul_table[0][i] = gf_mul_table[i][0] = 0;
   }

void init_fec()
   {
   static bool fec_initialized = false;

   if(!fec_initialized)
      {
      fec_initialized = true;
      init_mul_table();
      }
   }

/*
* Various linear algebra operations that i use often.
*/

/*
* addmul() computes dst[] = dst[] + c * src[]
* This is used often, so better optimize it! Currently the loop is
* unrolled 16 times, a good value for 486 and pentium-class machines.
* The case c=0 is also optimized, whereas c=1 is not. These
* calls are unfrequent in my typical apps so I did not bother.
*/
void addmul(byte dst[], const byte src[], byte c, int sz)
   {
   if(c == 0)
      return;

   const byte* mul_base = gf_mul_table[c];
   byte *lim = &dst[sz - 16 + 1];

   for(; dst < lim; dst += 16, src += 16)
      {
      dst[0] ^= mul_base[src[0]];
      dst[1] ^= mul_base[src[1]];
      dst[2] ^= mul_base[src[2]];
      dst[3] ^= mul_base[src[3]];
      dst[4] ^= mul_base[src[4]];
      dst[5] ^= mul_base[src[5]];
      dst[6] ^= mul_base[src[6]];
      dst[7] ^= mul_base[src[7]];
      dst[8] ^= mul_base[src[8]];
      dst[9] ^= mul_base[src[9]];
      dst[10] ^= mul_base[src[10]];
      dst[11] ^= mul_base[src[11]];
      dst[12] ^= mul_base[src[12]];
      dst[13] ^= mul_base[src[13]];
      dst[14] ^= mul_base[src[14]];
      dst[15] ^= mul_base[src[15]];
      }

   lim += 16 - 1;
   for(; dst < lim; dst++, src++)
      *dst ^= mul_base[*src];
   }

void addmul_k(byte dst[], byte* srcs[], const byte cs[],
              size_t size, size_t k)
   {
   for(size_t i = 0; i != k; ++i)
      {
      byte c = cs[i];
      if(c == 0)
         continue;

      const byte* mul_c = gf_mul_table[c];
      const byte* src = srcs[i];

      for(size_t j = 0; j != size; ++j)
         dst[j] ^= mul_c[src[j]];
      }
   }

/*
* invert_mat() takes a matrix and produces its inverse k is the size
* of the matrix.  (Gauss-Jordan, adapted from Numerical Recipes in C)
* Return non-zero if singular.
*/
void invert_mat(byte *src, int k)
   {
   int irow, icol;

   std::vector<int> indxc(k);
   std::vector<int> indxr(k);
   std::vector<int> ipiv(k);
   std::vector<byte> id_row(k);
   std::vector<byte> temp_row(k);

   /*
   * ipiv marks elements already used as pivots.
   */
   for(int i = 0; i < k; i++)
      ipiv[i] = 0;

   for(int col = 0; col < k; col++)
      {
      byte *pivot_row;
      /*
      * Zeroing column 'col', look for a non-zero element.
      * First try on the diagonal, if it fails, look elsewhere.
      */
      irow = icol = -1;

      if(ipiv[col] != 1 && src[col*k + col] != 0)
         {
         irow = col;
         icol = col;
         goto found_piv;
         }

      for(int row = 0; row < k; row++)
         {
         if(ipiv[row] != 1)
            {
            int ix;
            for(ix = 0; ix < k; ix++)
               {
               if(ipiv[ix] == 0)
                  {
                  if(src[row*k + ix] != 0)
                     {
                     irow = row;
                     icol = ix;
                     goto found_piv;
                     }
                  }
               else if(ipiv[ix] > 1)
                  throw std::invalid_argument("singlar matrix");
               }
            }
         }

      if(icol == -1)
         throw std::invalid_argument("pivot not found in invert_mat");

      found_piv:

      ++(ipiv[icol]);

      /*
      * swap rows irow and icol, so afterwards the diagonal
      * element will be correct. Rarely done, not worth
      * optimizing.
      */
      if(irow != icol)
         {
         for(int i = 0; i < k; i++)
            std::swap(src[irow*k + i], src[icol*k + i]);
         }

      indxr[col] = irow;
      indxc[col] = icol;
      pivot_row = &src[icol*k];
      byte c = pivot_row[icol];

      if(c == 0)
         throw std::invalid_argument("singlar matrix");

      if(c != 1)
         { /* otherwhise this is a NOP */
         /*
         * this is done often, but optimizing is not so
         * fruitful, at least in the obvious ways (unrolling)
         */
         c = GF_INVERSE[c];
         pivot_row[icol] = 1;
         for(int i = 0; i < k; i++)
            pivot_row[i] = gf_mul(c, pivot_row[i]);
         }

      /*
      * from all rows, remove multiples of the selected row
      * to zero the relevant entry (in fact, the entry is not zero
      * because we know it must be zero).
      * (Here, if we know that the pivot_row is the identity,
      * we can optimize the addmul).
      */
      id_row[icol] = 1;
      if(memcmp(pivot_row, &id_row[0], k) != 0)
         {
         byte* p = src;

         for(int i = 0; i < k; ++i)
            {
            if(i != icol)
               {
               c = p[icol];
               p[icol] = 0;
               addmul(p, pivot_row, c, k);
               }
            p += k;
            }
         }
      id_row[icol] = 0;
      } /* done all columns */

   for(int col = k-1; col >= 0; col--)
      {
      if(indxr[col] < 0 || indxr[col] >= k)
         fprintf(stderr, "AARGH, indxr[col] %d\n", indxr[col]);
      else if(indxc[col] < 0 || indxc[col] >= k)
         fprintf(stderr, "AARGH, indxc[col] %d\n", indxc[col]);
      else if(indxr[col] != indxc[col])
         {
         for(int row = 0; row < k; row++)
            std::swap(src[row*k + indxr[col]], src[row*k + indxc[col]]);
         }
      }
   }

/*
* fast code for inverting a vandermonde matrix.
* XXX NOTE: It assumes that the matrix is not singular and _IS_ a
* vandermonde matrix. Only uses the second column of the matrix,
* containing the p_i's.
*
* Algorithm borrowed from "Numerical recipes in C" -- sec.2.8, but
* largely revised for my purposes.
* p = coefficients of the matrix (p_i)
* q = values of the polynomial (known)
*/
void invert_vdm(byte *src, int k)
   {
   if(k == 1) /* degenerate case, matrix must be p^0 = 1 */
      return;

   /*
   * c holds the coefficient of P(x) = Prod (x - p_i), i=0..k-1
   * b holds the coefficient for the matrix inversion
   */
   std::vector<byte> c(k), b(k), p(k);

   for(int j = 1, i = 0; i < k; i++, j += k)
      {
      c[i] = 0;
      p[i] = src[j];    /* p[i] */
      }

   /*
   * construct coeffs. recursively. We know c[k] = 1 (implicit)
   * and start P_0 = x - p_0, then at each stage multiply by
   * x - p_i generating P_i = x P_{i-1} - p_i P_{i-1}
   * After k steps we are done.
   */
   c[k-1] = p[0]; /* really -p(0), but x = -x in GF(2^m) */
   for(int i = 1; i < k; i++)
      {
      byte p_i = p[i]; /* see above comment */
      for(int j = k-1  - (i - 1); j < k-1; j++)
         c[j] ^= gf_mul(p_i, c[j+1]);
      c[k-1] ^= p_i;
      }

   for(int row = 0; row < k; row++)
      {
      // synthetic division etc.
      byte xx = p[row];
      byte t = 1;
      b[k-1] = 1; /* this is in fact c[k] */
      for(int i = k-2; i >= 0; i--)
         {
         b[i] = c[i+1] ^ gf_mul(xx, b[i+1]);
         t = gf_mul(xx, t) ^ b[i];
         }
      for(int col = 0; col < k; col++)
         src[col*k + row] = gf_mul(GF_INVERSE[t], b[col]);
      }
   }

/*
* shuffle move src packets in their position
*/
void shuffle(byte *pkt[], size_t index[], size_t k)
   {
   for(size_t i = 0; i < k;)
      {
      if(index[i] >= k || index[i] == i)
         i++;
      else
         {
         /*
         * put pkt in the right position (first check for conflicts).
         */
         size_t c = index[i];

         if(index[c] == c)
            throw std::logic_error("decode shuffle failed");

         std::swap(index[i], index[c]);
         std::swap(pkt[i], pkt[c]);
         }
      }
   }

/*
* build_decode_matrix constructs the encoding matrix given the
* indexes. The matrix must be already allocated as
* a vector of k*k elements, in row-major order
*/
std::vector<byte>
build_decode_matrix(size_t k, size_t n,
                    const byte* enc_matrix, const size_t index[])
   {
   std::vector<byte> matrix(k * k);

   byte* p = &matrix[0];

   for(size_t i = 0; i != k; ++i)
      {
      if(index[i] >= n)
         throw std::logic_error("bad index in build_decode_matrix");

      if(1 && index[i] < k)
         p[i] = 1;
      else
         std::memcpy(p, &(enc_matrix[index[i]*k]), k);

      p += k;
      }

   invert_mat(&matrix[0], k);
   return matrix;
   }

}

/*
* This section contains the proper FEC encoding/decoding routines.
* The encoding matrix is computed starting with a Vandermonde matrix,
* and then transforming it into a systematic matrix.
*/

/*
* create a new encoder, returning a descriptor. This contains k,n and
* the encoding matrix.
*/
fec_code::fec_code(size_t k_arg, size_t n_arg) :
   k(k_arg), n(n_arg), enc_matrix(n * k)
   {
   init_fec();

   if(k > 256 || n > 256)
      throw std::invalid_argument("fec_code: k and n must be < 256");

   if(k > n)
      throw std::invalid_argument("fec_code: k must be <= n");

   /*
   * the upper matrix is I so do not bother with a slow multiply
   */
   for(size_t i = 0; i != k; ++i)
      enc_matrix[i*(k+1)] = 1;

   /*
   * Fill the matrix with powers of field elements, starting from 0.
   * The first row is special, cannot be computed with exp. table.
   */
   std::vector<byte> tmp_m(n * k);
   tmp_m[0] = 1;
   // rest of row 0 is 0s

   for(size_t i = k; i != tmp_m.size(); ++i)
      tmp_m[i] = GF_EXP[((i / k) * (i % k)) % 255];

   /*
   * quick code to build systematic matrix: invert the top
   * k*k vandermonde matrix, multiply right the bottom n-k rows
   * by the inverse, and construct the identity matrix at the top.
   */
   invert_vdm(&tmp_m[0], k); /* much faster than invert_mat */

   /*
   * computes C = AB where A is n*k, B is k*m, C is n*m
   */
   for(size_t row = k*k; row != n*k; row += k)
      {
      for(size_t col = 0; col < k; col++)
         {
         const byte* pa = &tmp_m[row];
         const byte* pb = &tmp_m[col];
         byte acc = 0;
         for(size_t i = 0; i < k; i++, pa++, pb += k)
            acc ^= gf_mul(*pa, *pb);
         enc_matrix[row + col] = acc;
         }
      }
   }

/*
* fec_encode accepts as input pointers to k data packets of size sz,
* and produces as output a packet pointed to by fec, computed
* with index "index".
*/
void fec_code::encode(
   const byte input[], size_t size,
   std::tr1::function<void (size_t, size_t, const byte[], size_t)> output)
   const
   {
   if(size % k != 0)
      throw std::invalid_argument("encode: input must be multiple of k bytes");

   size_t block_size = size / k;

   for(size_t i = 0; i != k; ++i)
      output(i, n, input + i*block_size, block_size);

   for(size_t i = k; i != n; ++i)
      {
      std::vector<byte> fec_buf(block_size);

      for(size_t j = 0; j != k; ++j)
         addmul(&fec_buf[0], input + j*block_size,
                enc_matrix[i*k+j], block_size);

      output(i, n, &fec_buf[0], fec_buf.size());
      }
   }

/*
* fec_decode receives as input a vector of packets, the indexes of
* packets, and produces the correct vector as output.
*
* Input:
*   code: pointer to code descriptor
*   pkt:  pointers to received packets. They are modified
*         to store the output packets (in place)
*   index: pointer to packet indexes (modified)
*   sz:    size of each packet
*/
template<typename K, typename V>
inline V search_map(const std::map<K, V>& mapping,
                    const K& key,
                    const V& null_result = V())
   {
   typename std::map<K, V>::const_iterator i = mapping.find(key);
   if(i == mapping.end())
      return null_result;
   return i->second;
   }

std::vector<size_t>
choose_indexes(const std::map<size_t, const byte*>& shares, size_t k)
   {
   if(shares.size() < k)
      throw std::invalid_argument("not enough shares");

   std::map<size_t, const byte*>::const_iterator b = shares.begin();
   std::map<size_t, const byte*>::const_reverse_iterator e = shares.rbegin();

   std::vector<size_t> indexes;

   for(size_t i = 0; i != k; ++i)
      {
      if(b->first == i)
         {
         indexes.push_back(b->first);
         ++b;
         }
      else
         {
         indexes.push_back(e->first);
         ++e;
         }
      }

   return indexes;
   }

void fec_code::decode(
   const std::map<size_t, const byte*>& shares, size_t share_size,
   std::tr1::function<void (size_t, size_t, const byte[], size_t)> output) const
   {
   if(shares.size() < k)
      throw std::logic_error("Could not decode, less than k surviving shares");

   std::vector<size_t> indexes = choose_indexes(shares, k);

   std::vector<byte> m_dec = build_decode_matrix(k, n, &enc_matrix[0], &indexes[0]);

   /*
   Todo:
    If shares.size() < k:
          signal decoding error for missing shares < k
          emit existing shares < k
        (ie, partial recovery if possible)
    Assert share_size % k == 0
    Check that all share #s < n
   */

   for(size_t i = 0; i != indexes.size(); ++i)
      {
      size_t share_id = indexes[i];

      /*
      This is a systematic code (encoding matrix includes k*k identity
      matrix), so shares less than k are copies of the input data,
      can output directly
      */
      if(share_id < k)
         output(share_id, k, search_map(shares, share_id), share_size);
      else
         {
         std::vector<byte> buf(share_size);
         for(size_t col = 0; col < k; col++)
            {
            addmul(&buf[0], search_map(shares, indexes[col]), m_dec[i*k + col], share_size);
            }
         output(i, k, &buf[0], share_size);
         }
      }
   }

void fec_code::decode(byte* pkt[], size_t index[], size_t sz) const
   {
   shuffle(pkt, index, k);

   std::vector<byte> m_dec = build_decode_matrix(k, n, &enc_matrix[0], index);

   /*
   * do the actual decoding
   */
   std::vector<byte*> new_pkt(k);

   for(size_t row = 0; row < k; row++)
      {
      if(index[row] >= k)
         {
         new_pkt[row] = new byte[sz];
         std::memset(new_pkt[row], 0, sz);
         for(size_t col = 0; col < k; col++)
            addmul(new_pkt[row], pkt[col], m_dec[row*k + col], sz);
         }
      }

   /*
   * move pkts to their final destination
   */
   for(size_t row = 0; row < k; row++)
      {
      if(index[row] >= k)
         {
         std::memcpy(pkt[row], new_pkt[row], sz);
         delete[] new_pkt[row];
         }
      }
   }
