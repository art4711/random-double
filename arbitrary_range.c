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

/* Below copyright is from openbsd:src/lib/libc/crypt/arc4random_uniform.c: */
/*
 * Copyright (c) 2008, Damien Miller <djm@openbsd.org>
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
 * Time to think about how to expand this to an arbitrary range.
 */

/*
 * Following two functions are copied from rd.c, see that file
 * for explanation.
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

static double
r0to1(void)
{
	uint64_t r = rX(53);
	int e = ffsll(r);
	uint64_t m;
	if (e > 52 || e == 0)
		return 0.0;
	/* Shift out the bit we don't want set. */
	m = (r >> e) << (e - 1);
	return ldexp(0x1p52 + m, -52 - e);
}


/*
 * We want our function to look something like this:
 *
 * double
 * random_double(double from, double to)
 * {
 * 	double ret = some magic.
 *      assert(ret >= from && ret < to);
 *      return ret;
 * }
 */

/*
 * Let's just start with a completely naive implementation that seems to be industry standard (see urd.cxx).
 */

static double
rd_naive(double from, double to)
{
	return (to - from) * r0to1() + from;
}

/*
 * And a test showing why this doesn't work.
 */

static void
test_rd_naive(void)
{
	int buckets = 3;
	int attempts = 1000000;
	double from = 0x1p52, to = from + buckets - 1;
	int bucket[buckets];
	int i;

	memset(bucket, 0, sizeof(bucket));

	printf("f: %f, t: %f\n", from, to);

	for (i = 0; i < attempts; i++) {
		double r = rd_naive(from, to);
		unsigned int b = (int)(r - from);
		assert(r >= from && r <= to);
		assert(b < buckets);
		bucket[b]++;
	}
	int minbucket = attempts, maxbucket = 0;
	for (i = 0; i < buckets; i++) {
		if (bucket[i] < minbucket)
			minbucket = bucket[i];
		if (bucket[i] > maxbucket)
			maxbucket = bucket[i];
		printf("%d\n", bucket[i]);
	}
	double diff = (double)maxbucket/(double)minbucket - 1.0;
	if (diff > 0.001) {
		printf("rd_naive: very large diff (should be close to 0): %f\n", diff);
	}
}

/*
 * So what's going on here? Why doesn't math work?
 *
 * We have to understand that we're not actually dealing with numbers
 * here.  We're mapping from one set to another. We have a set of
 * elements that our random number generator is generating and we're
 * mapping to a different set of elements that are all the numbers
 * [from, to) _that_a_double_can_represent_. This is the key point,
 * there are many numbers [from,to), in fact there are infinitely many
 * numbers, but the numbers are irrelevant, get numbers out of your
 * head.  Actually, let's get numbers out of our head and rephrase
 * this:
 *
 * We have a stream of pigeons and we want to put those pigeons into
 * pigeonholes. A uniform distribution of pigeons in pigeonholes can
 * only be achieved if the number of unique pigeons is a multiple of
 * pigeonholes. In fact to make our life simple, let's just say that
 * we want the function mapping pigeons to pigeonholes to be
 * bijective.  It's not strictly necessary, it's enough that the
 * function is surjective and <my knowledge of set theory ends here,
 * so fuck it, just make it bijective>.
 *
 * Since we can count the number the possible pigeonholes [from, to)
 * that a double can represent all we need is a function that
 * generates a random pigeon with a number [0, count). So we need a
 * function that returns random number [0,x). This is of course a
 * solved problem, but I'll reimplement it here because I want 64 bit
 * numbers and the standard function I have in mind `arc4random_uniform`
 * only deals with 32 bit numbers.
 *
 * Below is a copy from openbsd:src/lib/libc/crypt/arc4random_uniform.c
 * adapted to 64 bit numbers.
 */

/*
 * Calculate a uniformly distributed random number less than upper_bound
 * avoiding "modulo bias".
 *
 * Uniformity is achieved by generating new random numbers until the one
 * returned is outside the range [0, 2**64 % upper_bound).  This
 * guarantees the selected random number will be inside
 * [2**64 % upper_bound, 2**64) which maps back to [0, upper_bound)
 * after reduction modulo upper_bound.
 */

static uint64_t
r_uniform(uint64_t upper_bound)
{
	uint64_t r, min;

	if (upper_bound < 2)
		return 0;

	/* 2**64 % x == (2**64 - x) % x */
	min = -upper_bound % upper_bound;
	/*
	 * This could theoretically loop forever but each retry has
	 * p > 0.5 (worst case, usually far better) of selecting a
	 * number inside the range we need, so it should rarely need
	 * to re-roll.
	 */
	for (;;) {
		r = rX(64);
		if (r >= min)
			break;
	}

	return r % upper_bound;
}

/*
 * Now we just need to count our pigeonholes.
 *
 * Let's limit ourselves to positive numbers for now.  I want to
 * argue, just like in rd.c that the only way we can keep our sanity
 * is if the distance between the possible numbers we can generate,
 * errrm. I mean if the distance between the pigeonholes... I'm
 * streching this analogy too far. Let's try again.
 *
 * The only way to keep our sanity is if the distance between the
 * possible numbers we can generate is the distance between the
 * numbers in the highest possible range. So in the [0,1) range we
 * know that the biggest distance between numbers is in the [0.5,1)
 * range because that floating point number has the highest exponent.
 *
 * The C standard gives us a tool for this exact purpose: nextafter.
 */
static uint64_t
numbers_between(double from, double to)
{
	assert(from >= 0 && to > 0 && from < to);	/* positive numbers for now. */
	double nxt = nextafter(to, from);		/* next representable number from "to" in the direction of "from" */
	double step = to - nxt;				/* step between the numbers. */
	double count = (to - from) / step;		/* Leap of faith, I actually don't know if this will always be correct. */
	uint64_t r = count;

	assert((double)r == count);			/* the integer conversion should have been accurate. */

	return r;
}

/*
 * Let's just verify that I'm not full of shit:
 */
static void
test_ranges(void)
{
	uint64_t r;
	r = numbers_between(0x1p52, 0x1p52 + 3);
	assert(r == 3);
	r = numbers_between(0x1p55, 0x1p55 + 25);
	assert(r == 3);

	/* 
	 * This test below actually gives us separate verification that my
	 * stumbling math about the numbers in [0,1) range was correct.
	 */
	r = numbers_between(0, 1);
	assert(r == (1LL << 53));
}

/*
 * So the function that works for positive numbers should be
 * relatively trivial:
 */
static double
rd_positive(double from, double to)
{
	assert(from >= 0 && to > 0 && from < to);	/* positive numbers for now. */
	double nxt = nextafter(to, from);		/* next representable number from "to" in the direction of "from" */
	double step = to - nxt;				/* step between the numbers. */
	double count = (to - from) / step;		/* Leap of faith, I actually don't know if this will always be correct. */

	assert(count <= (1LL << 53));
	return from + (double)(r_uniform((uint64_t)count + 1))* step;
}

/*
 * And a test that things at least appear to make sense. 
 */
static void
test_rd_positive_n(int buckets)
{
	int attempts = 10000000;
	double from = 0x1p52, to = from + buckets - 1;
	int bucket[buckets];
	int i;

	memset(bucket, 0, sizeof(bucket));

	printf("f: %f, t: %f\n", from, to);

	for (i = 0; i < attempts; i++) {
		double r = rd_positive(from, to);
		unsigned int b = (int)(r - from);
		if (r < from || r > to)
			printf("BAD: %f\n", r);
		assert(r >= from && r <= to);
		assert(b < buckets);
		bucket[b]++;
	}
	int minbucket = attempts, maxbucket = 0;
	for (i = 0; i < buckets; i++) {
		if (bucket[i] < minbucket)
			minbucket = bucket[i];
		if (bucket[i] > maxbucket)
			maxbucket = bucket[i];
		printf("%d, ", bucket[i]);
	}
	printf("\n");
	double diff = (double)maxbucket/(double)minbucket - 1.0;
	if (diff > 0.05) {
		printf("rd_positive: very large diff (should be close to 0): %f (%d %d)\n", diff, maxbucket, minbucket);
	}
}

static void
test_rd_positive(void)
{
	test_rd_positive_n(2);
	test_rd_positive_n(3);
	test_rd_positive_n(4);
	test_rd_positive_n(17);
	test_rd_positive_n(42);
}

/*
 * And the last test, from rd.c, check that rd_positive(0,1) is equivalent to r0to1b.
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
	int mmset;
	uint64_t efreq[52];
	uint64_t m_bits_set[52];
	double min[52], max[52];
};

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
static void
check_1to2(double (*fn)(void), struct r1_test *rt)
{
	double x = fn();
	A(x, 0.0, 1.0);
	B(x, rt);
}

static double
rd_positive0to1(void)
{
	return rd_positive(0.0, 1.0);
}

static void
test_rd_positive0to1(void)
{
	struct r1_test r1b = { 0 };
	uint64_t numruns = 1LL << 25;
	uint64_t i;

	for (i = 0; i < numruns; i++) {
		check_1to2(rd_positive0to1, &r1b);
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
		if (r1b.m_bits_set[i] != expected_bits && r1b.efreq[i] > 25) {
			printf("bits2[%llu]: 0x%llx, expected 0x%llx, diff: 0x%llx\n",
			    i, r1b.m_bits_set[i], expected_bits,
			    r1b.m_bits_set[i] ^ expected_bits);
		}
	}

	for (i = 0; i < 52; i++) {
		if (r1b.efreq[i] == 0)
			continue;
		uint64_t expected = numruns / (1LLU << (i + 1));
		printf("freq2[%lld]: %llu, expected: %llu, deviation %.2f, range %f - %f\n",
		    -i, r1b.efreq[i], expected, (double)r1b.efreq[i] / (double)expected,
		    r1b.min[i], r1b.max[i]);
	}
}

/*
 * And this was a separate verification that my guesses about which
 * bits need to be set in r0to1 in rd.c were correct, because a
 * completely different method to generate doubles in [0,1) gave the
 * same results.
 */

int
main(int argc, char **argv)
{
	test_rd_naive();
	test_ranges();
	test_rd_positive();
	test_rd_positive0to1();
	return 0;
}
