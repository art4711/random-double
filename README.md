# Generating random doubles #

The first step was to figure out how to generate numbers in the range
[0.0,1.0). Those experiments and the reasoning behind them is in
comments and code in [rd.c](rd.c).

A few months later I looked at gcc and llvm standard c++ libraries to
see if their std::uniform_real_distribution actually solved this
problem correctly. They haven't. The result of that is in
[urd.cxx](urd.cxx). The bug report for llvm is
[here](https://llvm.org/bugs/show_bug.cgi?id=23168), I haven't made a
bug report for gcc.

This triggered me to actually figure out how to extend this to
arbitrary ranges. The first attempt is documented in comments and code
in [arbitrary_range.c](arbitrary_range.c), but it only deals with
positive numbers for now. The nice thing about it is that despite a
completely different algorithm the generated numbers from
arbitrary_range.c set the exact same bits as the numbers in rd.c which
means that either my early assumptions were correct or that my brain
farts are at least consistent.

## TODO ##

 - Tackle negative numbers. Naively it should just be like
   arbitrary_range, except that we find the biggest absolute value of
   (from,to), use that to find the step and count and do some sign
   flipping in the right place. But that's for another day.

## DISCLAIMER ##

Please notice that I'm not claiming that anything above makes sense or
that the tests are even close to being comprehensive. A programmer
who tests his own edge cases is like an author writing his own book
reviews.

To be useful this would require vastly more testing and independent
verificaiton from someone who actually understands set theory and ieee
754 floating point numbers. I can confidently say that I know neither,
but as the industry standard on stackoverflow and the c++ libraries
seems to indicate, neither do most other people.
