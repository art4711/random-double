/*
 * Copyright (c) 2015 Artur Grabowski <art@blahonga.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <assert.h>
#include <strings.h>

/*
 * I need to generate floating point doubles in the range [0,1) that
 * are uniformly distributed. The distribution isn't allowed to be
 * denser in any part of the range.
 *
 * The Stackoverflow questions I can find about the topic have
 * obviously flawed answers. `((double)rand()/(double)RAND_MAX)` will
 * neither give us a random number, nor will the distribution be
 * paricularly good. This is not even under discussion here. If you
 * think this is a good way to generate uniformly distributed random
 * doubles, either read on or just go away.
 */

/*
 * I have a good random source. That's not part of the problem.
 * Assume that my source of random bits is perfect and is a function
 * that returns a number in the range [0,2^X) and looks like this (for
 * X <= 64):
 */
static uint64_t
rX(uint64_t X)
{
	uint64_t res;
	assert(X > 0 && X < 65);
	arc4random_buf(&res, sizeof(res));
	if (X == 64)
		return res;
	return res & ((1ULL << X) - 1);
}
/*
 * If your operating system does not provide `arc4random_buf` get
 * a better operating system or substitute this function for your
 * favourite randomness source.
 */

/*
 * Let's also just limit ourselves IEEE 754 binary64 numbers known as
 * `double` in most C implementations. This code should be relatively
 * easily generalizable to any doubles that have a mantissa with less
 * than 64 bits or if you have more than that if you have an integer
 * type that is as big as your dobule. This is a proof of concept, not
 * a generic solution that will work everywhere every time.
 */

/*
 * We know that we have a 52 bit mantissa and that makes the numbers
 * in the range [2^52,2^53) have integer precision . So a uniformly
 * distributed random double in that range is trivial to generate
 * without resorting to bit fiddling:
 */
double
r52to53(void)
{
	return ldexp(0x1p52 + rX(52), 0);
}

/*
 * We also don't lose anything if we cut this down to a [1,2) range:
 */
double
r1to2(void)
{
	return ldexp(0x1p52 + rX(52), 0) / 0x1p52;
}

/*
 * Another variant where we trust the compiler a bit less:
 */
double
r1to2bis(void)
{
	return ldexp(0x1p52 + rX(52), -52);
}

/*
 * The previous two functions can be shown to work correctly with a
 * little bit of bit fiddling (and standard violating) by only
 * randomizing the bits in the mantissa while keeping the exponent the
 * same:
 */
double
r1to2check(void)
{
	union {
		uint64_t u;
		double d;
	} a, b, c;
	uint64_t r = rX(52);

	/*
	 * This assumes we're little-endian. Or actually i387 endian
	 * because endianness for floating point isn't defined
	 * anywhere.
	 */
	a.d = 0x1p0;
	a.u |= r;

	b.d = ldexp(0x1p52 + r, 0) / 0x1p52;
	c.d = ldexp(0x1p52 + r, -52);
	assert(a.d == b.d && a.u == b.u && a.d == c.d && b.u == c.u);
	assert(a.d >= 1.0 && a.d < 2.0);
	return a.d;
}

/*
 * It doesn't take much imagination to map this to any [2^X,2^(X+1))
 * range:
 */
double
r2range(int X)
{
	assert(X >= -1023 && X <= 1025);
	return ldexp(r1to2bis(),X);
}

/*
 * Everything up to here is obvious and trivial. But it doesn't really
 * get us closer to a [0,1) range. I was just warming up my
 * understanding of IEEE floating point as a preparation for the main
 * event.
 *
 * Intuitively I can't trust `r1to2() - 1.0`.
 *
 * To get us a uniformly distributed number in the [0,1) range, we
 * have 1/2 chance to get a number in the [0.5,1.0) range, 1/4
 * [0.25,0.5), etc.
 *
 * There are 2^52 possible numbers in the [0.5,1.0) range and the
 * range has a 2^-1 probablity to be picked, this means that the
 * probability of any number in that range is 2^-53. Since we don't
 * want any number to be more likely to be picked, it means that the
 * probability for 0 should be 2^-53.
 *
 * This gives us a very simple way to pick our exponent. Generate a 52
 * bit number, find the lowest set bit (1 indexed) and zero minus that
 * bit is our exponent. If no bits are set, we special case and return
 * 0.0 (this has a 2^-53 probability). The probability of each bit
 * being set is 1/2, this means that bit 1 has 1/2 probability of
 * being picked, bit 2 1/4, etc. The ffsll function is just perfect
 * for this.  Unfortunately POSIX only has ffs, but everyone (I care
 * about) implements ffsll.
 *
 * The only question that remains is the mantissa. On one hand it
 * shouldn't hurt to set the mantissa to completely random bits. We
 * would get the worst entropy for 0 and [0.5,1.0), every other range
 * would be less predictable. On the other hand, there's something
 * elegant about having all the pickable numbers to be at an equal
 * distance from each other. Intuitively, this should be as simple as
 * not setting the lower bits of the mantissa (or should they be all
 * 1?). XXX - this needs to be verified.
 */
double
r0to1(void)
{
	int e;
	uint64_t m;

	e = ffsll(rX(52));
	if (e == 0)
		return 0.0;
	m = rX(52 - e + 1) << (e - 1);
	return ldexp(0x1p52 + m, -52 - e);
}

/*
 * But wait a minute. Why are we using (up to) 104 random bits when we
 * only provide 53 bits of entropy. 
 *
 * This can be improved.
 *
 * Generate a 53 bit number, the number of the first set bit is our
 * exponent, clear that bit and shift one bit (since it's known, while
 * the next bits aren't), use the rest of the generated number as the
 * mantissa.
 */
double
r0to1b(void)
{
	uint64_t r = rX(53);
	int e = ffsll(r);
	uint64_t m;
	if (e > 52 || e == 0)
		return 0.0;
	m = (r ^ (1 << (e - 1))) >> 1;
	return ldexp(0x1p52 + m, -52 - e);
}

/*
 * Tests that our assumptions hold.
 */
static void
A(double r, double min, double max)
{
	if (r < min) {
		printf("A: %f < %f\n", r, min);
		abort();
	}
	if (r > max) {
		printf("A: %f > %f\n", r, max);
		abort();
	}
}

struct r1_test {
	uint64_t efreq[52];
	uint64_t m_bits_set[52];
	int mmset;
	double min[52], max[52];
};
struct r1_test r1a/* = { .min = 2.0, .max = -1.0 }*/;
struct r1_test r1b/* = { .min = 2.0, .max = -1.0 }*/;

static void
B(double r, struct r1_test *rt)
{
	union {
		uint64_t u;
		double d;
	} foo;

	foo.d = r;

	uint64_t E, m;
	int e;

	E = (foo.u >> 52LL) & ((1LL << 11) - 1);
	e = E - 1023;
	if (e > -1) {
		printf("B(e): %d > -1\n", e);
		abort();
	}
	if (e <= -52) {
		printf("B(e): %d <= -52\n", e);
		abort();
	}
	int o = -e -1;
	assert(o >= 0 && o < 52);
	rt->efreq[o]++;
	m = foo.u & ((1LL << 52) - 1);
	rt->m_bits_set[o] |= m;
	uint64_t expected_bits = ((1LL << 52) - 1);
	expected_bits ^= (1LL << (-e - 1)) - 1;
	if (m & ~expected_bits) {
		printf("B(m): unexpected bits set: 0x%llx, expected 0x%llx, diff 0x%llx\n",
		    m, expected_bits, m & ~expected_bits);
		abort();
	}
	if (!rt->mmset) {
		int i;
		for (i = 0; i < 52; i++) {
			rt->min[i] = 2.0;
			rt->max[i] = -1.0;
		}
		rt->mmset = 1;
	}
	if (r < rt->min[o])
		rt->min[o] = r;
	if (r > rt->max[o])
		rt->max[o] = r;

	A(rt->min[o], ldexp(0x1p0, e), ldexp(0x1p0, e + 1));
	A(rt->max[o], ldexp(0x1p0, e), ldexp(0x1p0, e + 1));
}

uint64_t rX_bits_set;

static void
check_simple(void)
{
	rX_bits_set |= rX(52);

	r1to2check();
	A(r1to2(), 1.0, 2.0);
	A(r1to2bis(), 1.0, 2.0);
	A(r2range(7), 127.0, 256.0);
	A(r2range(-1), 0.5, 1.0);
}

static void
check_1to2(double (*fn)(void), struct r1_test *rt)
{
	double x = fn();
	A(x, 0.0, 1.0);
	B(x, rt);
}

int
main(int argc, char **argv)
{
	uint64_t i;
	const uint64_t numruns = 1LL << 25;

	for (i = 0; i < numruns / 10; i++) {
		check_simple();
	}

	/* should work most of the time. */
	assert(rX_bits_set == 0xfffffffffffffLLU);

	for (i = 0; i < numruns; i++) {
		check_1to2(r0to1, &r1a);
	}

	for (i = 0; i < numruns; i++) {
		check_1to2(r0to1b, &r1b);
	}

	for (i = 1; i < 52; i++) {
		uint64_t expected_bits = ((1LL << 52) - 1);
		expected_bits ^= (1LL << i) - 1;
		/*
		 * Check that the bits we expect to be used in the
		 * mantissa are actually used and not more.
		 *
		 * We only check if that particular exponent has been
		 * used at least 25 times. Why 25? Because that shut
		 * up the false positives for the numbers with too
		 * small sample size most of the time.
		 */
		if (r1a.m_bits_set[i] != expected_bits && r1a.efreq[i] > 25) {
			printf("bits1[%llu]: 0x%llx, expected 0x%llx, diff: 0x%llx\n",
			    i, r1a.m_bits_set[i], expected_bits,
			    r1a.m_bits_set[i] ^ expected_bits);
		}
		if (r1b.m_bits_set[i] != expected_bits && r1b.efreq[i] > 25) {
			printf("bits2[%llu]: 0x%llx, expected 0x%llx, diff: 0x%llx\n",
			    i, r1b.m_bits_set[i], expected_bits,
			    r1b.m_bits_set[i] ^ expected_bits);
		}
	}

	for (i = 0; i < 52; i++) {
		if (r1a.efreq[i] == 0)
			continue;
		printf("freq1[%lld]: %llu, expected: %llu, deviation %f, range %f - %f\n",
		    -i, r1a.efreq[i], numruns / (1LLU << i),
		    (double)((double)r1a.efreq[i] / ((double)numruns / (double)(1LLU << i))),
		    r1a.min[i], r1a.max[i]);
	}

	for (i = 0; i < 52; i++) {
		if (r1b.efreq[i] == 0)
			continue;
		printf("freq2[%lld]: %llu, expected: %llu, deviation %f, range %f - %f\n",
		    -i, r1b.efreq[i], numruns / (1LLU << i),
		    (double)((double)r1b.efreq[i] / ((double)numruns / (double)(1LLU << i))),
		    r1b.min[i], r1b.max[i]);
	}
	return 0;
}
