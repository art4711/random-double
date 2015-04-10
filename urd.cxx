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

#include <random>
#include <iostream>

#include <stdio.h>
#include <assert.h>

/*
 * Hey, I heard that C++11 has magic to get a uniform distribution of
 * floating point numbers, let's see how it performs.
 */

int
main(int argc, char **argv)
{
	std::default_random_engine gen;
	double from, step, to;
	int range = 3;

	switch (argc) {
	case 1:
		from = 1.0;
		break;
	default:
	case 3:
		range = atoi(argv[2]);
	case 2:
		from = atof(argv[1]);
		break;
	}

	/*
	 * Calculate the nearest representable number after from.
	 * add 1.0 to deal with from being 0 (shouldn't cause overflow
	 * because at that range 1.0 means nothing).
	 */
	step = nextafter(from, (from + 1.0) * 2.0) - from;
	/*
	 * uniform_real_distribution should return [from,to), but they
	 * all return [from,to], so that's why 1 is subtracted from range.
	 */
	to = from + step * (range - 1);

	printf("%a %a %a %d\n", from, to, step, range);

	std::uniform_real_distribution<double> dis(from, to);
	int bucket[range];
	int i;

	for (i = 0; i < range; i++) {
		bucket[i] = 0;
	}

	for (i = 0; i < 100 * range; i++) {
		double r = dis(gen);
		unsigned int b = (int)((r - from)/step);
		if (b >= range) {
			printf("foo: %d %f\n", b, r);
		}
		assert(b < range);
		bucket[b]++;
	}

	for (i = 0; i < range; i++) {
		printf("%d\n", bucket[i]);
	}
	return 0;
}
