/* crypto/bn/bn_rand.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <stdio.h>
#include <time.h>
#include "cryptlib.h"
#include "bn_lcl.h"
#include <openssl/rand.h>

static int bnrand(int pseudorand, BIGNUM *rnd, int bits, int top, int bottom)
	{
	unsigned char *buf=NULL;
	int ret=0,bit,bytes,mask;
	time_t tim;

	if (bits == 0)
		{
		BN_zero(rnd);
		return 1;
		}

	bytes=(bits+7)/8;
	bit=(bits-1)%8;
	mask=0xff<<bit;

	buf=(unsigned char *)OPENSSL_malloc(bytes);
	if (buf == NULL)
		{
		BNerr(BN_F_BN_RAND,ERR_R_MALLOC_FAILURE);
		goto err;
		}

	/* make a random number and set the top and bottom bits */
	time(&tim);
	RAND_add(&tim,sizeof(tim),0);

	if (pseudorand)
		{
		if (RAND_pseudo_bytes(buf, bytes) == -1)
			goto err;
		}
	else
		{
		if (RAND_bytes(buf, bytes) <= 0)
			goto err;
		}

#if 1
	if (pseudorand == 2)
		{
		/* generate patterns that are more likely to trigger BN
		   library bugs */
		int i;
		unsigned char c;

		for (i = 0; i < bytes; i++)
			{
			RAND_pseudo_bytes(&c, 1);
			if (c >= 128 && i > 0)
				buf[i] = buf[i-1];
			else if (c < 42)
				buf[i] = 0;
			else if (c < 84)
				buf[i] = 255;
			}
		}
#endif

	if (top)
		{
		if (bit == 0)
			{
			buf[0]=1;
			buf[1]|=0x80;
			}
		else
			{
			buf[0]|=(3<<(bit-1));
			buf[0]&= ~(mask<<1);
			}
		}
	else
		{
		buf[0]|=(1<<bit);
		buf[0]&= ~(mask<<1);
		}
	if (bottom) /* set bottom bits to whatever odd is */
		buf[bytes-1]|=1;
	if (!BN_bin2bn(buf,bytes,rnd)) goto err;
	ret=1;
err:
	if (buf != NULL)
		{
		memset(buf,0,bytes);
		OPENSSL_free(buf);
		}
	return(ret);
	}

int     BN_rand(BIGNUM *rnd, int bits, int top, int bottom)
	{
	return bnrand(0, rnd, bits, top, bottom);
	}

int     BN_pseudo_rand(BIGNUM *rnd, int bits, int top, int bottom)
	{
	return bnrand(1, rnd, bits, top, bottom);
	}

#if 1
int     BN_bntest_rand(BIGNUM *rnd, int bits, int top, int bottom)
	{
	return bnrand(2, rnd, bits, top, bottom);
	}
#endif

/* random number r: min <= r < min+range */
int	BN_rand_range(BIGNUM *r, BIGNUM *min, BIGNUM *range)
	{
	int n;

	if (range->neg || BN_is_zero(range))
		{
		BNerr(BN_F_BN_RAND_RANGE, BN_R_INVALID_RANGE);
		return 0;
		}

	n = BN_num_bits(range); /* n > 0 */

	if (n == 1)
		{
		if (!BN_zero(r)) return 0;
		}
	else if (BN_is_bit_set(range, n - 2))
		{
		do
			{
			/* range = 11..._2, so each iteration succeeds with probability > .75 */
			if (!BN_rand(r, n, 0, 0)) return 0;
			}
		while (BN_cmp(r, range) >= 0);
		}
	else
		{
		/* range = 10..._2,
		 * so  3*range (= 11..._2)  is exactly one bit longer than  range */
		do
			{
			if (!BN_rand(r, n + 1, 0, 0)) return 0;
			/* If  r < 3*range,  use  r := r MOD range
			 * (which is either  r, r - range,  or  r - 2*range).
			 * Otherwise, iterate once more.
			 * Since  3*range = 11..._2, each iteration succeeds with
			 * probability > .75. */
			if (BN_cmp(r ,range) >= 0)
				{
				if (!BN_sub(r, r, range)) return 0;
				if (BN_cmp(r, range) >= 0)
					if (!BN_sub(r, r, range)) return 0;
				}
			}
		while (BN_cmp(r, range) >= 0);
		}

	if (min != NULL)
		{
		if (!BN_add(r, r, min)) return 0;
		}
	
	return 1;
	}
