Rosenblum's code: http://pages.cs.wisc.edu/~nater/esorics-supp/
This directory contains programs for binary code feature extraction as
described in the paper. 

### Prerequisites & Building:

1. Install cmake (`brew install cmake`)

This software requires a recent beta of the Dyninst library with the ParseAPI
component, which can be downloaded at the following URL:

http://www.paradyn.org/html/parse0.9-features.html

Please follow the build instructions for building and installing Dyninst,
including obtaining additional dependencies (libelf and libdwarf in particular)
as necessary.

Once Dyninst is installed, run the configure script and specify the
installation directories of Dyninst, libelf and libdwarf, as well as the
appropriate Dyninst platform string (see the Dyninst documentation)

For example, if the Dyninst root directory (the one containing the include/ and
${PLATFORM}/lib subdirectories) is /tmp/dyninst and libelf and libdwarf are
installed in /usr/local/lib on a 64-bit Linux system, use the following
configuration command:

    ./configure --with-dyninst-root=/tmp/dyninst \
                --with-dyinst-platform=x86_64-unknown-linux2.4 \
                --with-libelf=/usr/local/lib \
                --with-libdwarf=/usr/local/lib



### Usage:

Usage instructions for each feature extraction utility can be obtained with the
--help option. There are two different modes of output: the `ngrams` program
lists all ngrams in the binary as a comma-separated list with duplicates; all
other programs list features as a comma-separated list of tuples feature:count. The representations for the different feature templates are disjoint. Briefly:

    n-grams: <xxxxxx>, depending on the length of n-gram requested
    idioms: I*, where * is a value encoding the instruction patterns 
            (see the libfeat implementation for details)
    graphlets: compact representations of the form a/b/c/d_a/b/c/d_a/b/c/d
        that encode the node in/out/self edge types (a,b,c) and possibly the
        node color (d) for the three nodes forming a graphlet.
    supergraphlets: same as graphlets, but prefixed by SG_
    calldfa: same as graphlets, but prefixed by CD_
    libcalls: the actual names of external library functions

The output format for these programs is not meant to be interpretable, but
rather to form input for learning algorithms that recognize stylistic
features.
