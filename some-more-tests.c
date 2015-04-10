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
 * I need to convince myself of a few things.
 */

/*
 * Why I think that generating a number in [1,2) and then mapping it
 * to [0,1) by subtracting one doesn't actually work.
 */
void
test_minus_one(void)
{
	double less_than_two;
	double less_than_one;
	int i;


	/*
	 * Here, we step through the first 10 numbers smaller than 2,
	 * subtract 1.0 from them and compare that to the first 10
	 * numbers smaller than 1. If mapping from [1,2) to [0,1)
	 * worked by subtracting 1.0 there should be no diff.
	 */
	less_than_two = nextafter(2.0, 0.0);
	less_than_one = nextafter(1.0, 0.0);
	for (i = 0; i < 10; i++) {
		double diff = (less_than_two - 1.0) - less_than_one;
		printf("%e %e %e %e\n", less_than_two, less_than_two - 1.0, less_than_one, diff);
		less_than_two = nextafter(less_than_two, 0.0);
		less_than_one = nextafter(less_than_one, 0.0);
	}
	/*
	 * The same thing, but this time here is how I think the
	 * result will actually end up being. We'll end up not being
	 * able to pick every other less_than_one number.
	 */
	less_than_two = nextafter(2.0, 0.0);
	less_than_one = nextafter(1.0, 0.0);
	for (i = 0; i < 10; i++) {
		less_than_one = nextafter(less_than_one, 0.0);
		double diff = (less_than_two - 1.0) - less_than_one;
		printf("%e %e %e %e\n", less_than_two, less_than_two - 1.0, less_than_one, diff);
		less_than_two = nextafter(less_than_two, 0.0);
		less_than_one = nextafter(less_than_one, 0.0);
	}	
}

int
main(int argc, char **argv)
{
	test_minus_one();
}
