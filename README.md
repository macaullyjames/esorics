Rosenblum's code: http://pages.cs.wisc.edu/~nater/esorics-supp/


### Setting up the environment
OS: Fedora 23 x64

Ram: 2GB or more (for compiling)

Post-setup:
```sh
# Setup the locale
printf "LANG=en_US.UTF-8\nLC_ALL=en_US.UTF-8\n" > /etc/locale.conf

# Update dnf
dnf update

# Install the required packages
dnf install -y gcc-c++ make git elfutils-libelf-devel.x86_64 libdwarf-devel dyninst-devel
```

Package versions as of this writing:
```
gcc-c++-5.3.1-2.fc23.x86_64
make-1:4.0-5.1.fc23.x86_64
git-2.5.0-4.fc23.x86_64
elfutils-libelf-devel-0.165-2.fc23.x86_64
libdwarf-devel-20150915-1.fc23.x86_64
dyninst-devel-9.0.3-1.fc23.x86_64
```
### Copying over this repo
You can do this any way you want, but if you're not planning on making any changes it's probably easiest to download the repo as a .zip file and copy it over using `scp`.

### Building and installing
```sh
make && make install
```
This will add the six binaries `ngrams`, `idioms`, `graphlets`, `supergraphlets`, `calldfa`, and `libcalls` to the `usr/local/bin` folder. You can run these programs from any folder :)

### Usage (from Rosenblum's original README)

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
