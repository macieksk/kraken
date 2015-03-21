# Installation

Clone the repository from [github](https://github.com/macieksk/seed-kraken) together with **Jellyfish** submodule (`--recursive`).

    git clone --recursive git@github.com:macieksk/seed-kraken.git
    cd seed-kraken    
    
The current branch should be `seedmod128b`.

    git branch


## Building seed-Kraken

Build **seed-Kraken** and install it into a directory in your `$PATH`, for example `~/bin`. 
This step requires C++11 enabled GCC with support for `auto`, so GCC >4.4 is required.

    install_kraken.sh ~/bin
    
## Building Jellyfish

    cd Jellyfish/build/
    autoreconf -i ../
    ../configure
    make -j 4
    
Now, link the seed-modified `jellyfish` binary to a directory in in your `$PATH`. 
Watch out, as this may conflict with another version of **Jellyfish** you have already installed.

    cd bin
    ln -s "$(realpath jellyfish)" ~/bin/

Note: this a modified version of **Jellyfish** v1.11_seedmod made to work with *spaced seeds*.
**seed-Kraken** calls `jellyfish` from within its scripts.
Make sure that the correct version of jellyfish is in your path:
    
    $ jellyfish --version
    jellyfish 1.1.11_seedmod

## Setting Paths

Optionally you can setup `PATH` every time before running **seed-Kraken**:

    export PATH="/directory/to/jellyfish/bin/:$PATH"

