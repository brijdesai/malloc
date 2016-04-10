# malloc
Dynamic Memory Allocator using explicit free list with approx. 89% performance index.

When compiling the code for the first time (or while switching between 32bit and 64 bit) in your computer, delete all of the .O files in the directory, then type make command. 
You can use -m32 option of the gcc command in makefile to get 32 bit assembly in 64 bit machine. You require libc6-dev-i386 package to use -m32 option of gcc command.

Finally enter ./mdriver to see performance index which is average of all of the trace files.

The Trace-driven Driver Program

The driver program mdriver.c tests your mm.c package for correctness, space utilization, and throughput. The driver program is controlled by a set of trace files that are included in lab folder (config.h indicates their location). Each trace file contains a sequence of allocate, reallocate, and free directions that instruct the driver to call your mm_malloc, mm_realloc, and mm_free routines in some sequence. The driver and the trace files are the same ones we will use when we grade your handin mm.c file. The driver mdriver.c accepts the following command line arguments:

• –t tracedir: Look for the default trace files in directory tracedir instead
of the default directory defined in config.h.
• –f tracefile: Use one particular tracefile for testing instead of the
default set of tracefiles.
• –h: Print a summary of the command line arguments.
• –l: Run and measure libc malloc in addition to your malloc package.
• –v: Verbose output. Print a performance breakdown for each tracefile in a
compact table.
• –V: More verbose output. Prints additional diagnostic information as each trace
file is processed. Useful during debugging for determining which trace file is
causing your malloc package to fail.
