You are now looking at the first release of the chess-playing software project
called ZCT (which stands for Zach's Chess Test). This piece of software was
written by Zach Wegner, starting on June 1, 2005.

STANDARD USE

This program is intended for use on POSIX operating systems as well as Windows.
It is a standard "console" application that does not make use of any graphical
user interface. It can be used stand-alone, or through an interface using either
the XBoard Engine Communication Protocol or the Universal Chess Interface. It is
recommended to be used in either "console" mode or through the XBoard protocol,
as UCI is not fully compatible with some of ZCT's basic design philosophies. UCI
support is intended only as a last-resort option, and to be compatible with as
many interfaces as possible.

ZCT is a "monolithic" chess program, a la Crafty, that has many features in
console mode and thus does not rely on a GUI for functions such as PGN and EPD
support, an opening book, or time controls.

ZCT also supports multiple processors, but must be compiled with the support in.
Try 'make "CFLAGS=-DSMP -DMAX_CPUS=n"', where n is the number of processors your
machine has.

CODE

ZCT is a rather simple program that has a small evaluation and a simple but
efficient search. It's primary claim to fame is the open source implementation
of DTS (Dynamic Tree Splitting), which, to my knowledge, has only been
successfully implemented by four teams before: Cray Blitz (Bob Hyatt et al.),
Diep (Vincent Diepeveen), Zappa (Anthony Cozzie), and MessChess (Mridul
Muralidharan).

The intent was to write clean, bug free code. The code is pure, modern C that I
hope is found to be well written and easy to understand. ZCT is also heavily
commented. If you don't understand something, drop me a line and I'll clean it
up/comment it more.

RELEASE

This software is open source, and released with a license (COPYING.txt in the
main directory). Please read this license before you use, modify, or look at
anything else in the distribution! These components should be included in the
distribution:
README.txt--This file...
COPYING.txt--The licensing information for using this software
CHANGES.txt--The version-by-version history of ZCT, dating back further than
you would like to know.
Source files--A bunch of .c and .h files, as well as a Makefile
zct.png--The standard logo for ZCT, 100x50 pixels.
book.zbk--A standard opening book in ZCT's native format
ZCT.ini--A standard configuration file
Additionally, the distribution may contain one or more executable files, named
'zct' or 'zct.exe'. 

ACKNOWLEDGEMENTS

This software has been enriched due to the contribution of many individuals.
Among them are:

Kenny Dail--hardware, beta testing, bug fixing, book work, evaluation advice
Fonzy Bluemers--Beta testing, bug fixing, tweaking, eval advice, testing,
godliness
Teemu Pudas--Bug fixes, help in releasing, more bugfixes
Swaminathan Natarajan--Tournament running, lots of eval advice
Wael Deeb--Opening Book
Jim Ablett--Binaries, bug fixing
Dann Corbit--Bug fixes
Tony Thomas, Harun Taner, Olivier Deville, Patrick Buchmann, probably
others--Tournament running
Claude Shannon, Alan Turing, Donald E. Knuth, Ronald W. Moore, Frans Morsch,
David Slate, and more--Standard chess and tree-search theory
Tom Kerrigan--TSCP helped me learn about how to write a chess program
Robert Hyatt--bitboards and SMP, Crafty was also responsible for much of my
development in computer chess, being the first complex engine that I saw the
source to
Tord Romstad--Philosophy, ideas (LMR/null-move trick)
Anthony Cozzie--Philosophy, idea for print function
Steffan Westcott--Kogge-Stone and subset algorithms
Vincent Diepeveen, Fabien Letouzy, H.G. Muller, Gerd Isenberg, and countless
other chess engine authors, beta testers, tournament managers, and fans--For
general philosohpy, ideas, encouragement, advancements, and making this little
waste of time we call computer chess so interesting...

Additionally, the following people have supplied working code that is used in
ZCT, usually in a modified form:

Teemu Pudas--Windows SMP compatibility
Vincent Diepeveen/Agner Fog--random number generator
Gerd Isenberg--last_square()
Matt Taylor--first_square()
Someone else somewhere--pop_count()
Robert Hyatt--input_available()
Bob/Someone writing for the Linux Kernel--asm LOCK(), bitscans, and
popcount on x86
