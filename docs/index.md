# seed-Kraken

**seed-Kraken** is a modification of [**Kraken**](http://ccb.jhu.edu/software/kraken/) software, 
a taxonomic sequence classifier that assigns a taxonomic label to short DNA reads.
Original Kraken does this by examining the k-mers withing a read and querying 
a database with those k-mers. 

**seed-Kraken** works with *spaced seeds* instead of contiguous k-mers, 
and this turns out to give it some advantage in *sensitivity/precision* trade off [1].

A *spaced seed* is a local match triggering an alignment, encoded with a pattern of
matches interleaved with spaces – jokers (*“don’t care”* positions). 
Below is an example of a *spaced seed* of weight 28 (non-spaces) and span 40 (total length):

    SEED="######-##-#-#-##-###-#-##---###--#######"

You can find more about *spaced seeds* and why is it worth working with them in [section about theory of 'Spaced seeds'.](theory.md)

Information on the original **Kraken** software can be found at [http://ccb.jhu.edu/software/kraken/](http://ccb.jhu.edu/software/kraken/).

If you use **seed-Kraken**, please cite our paper:

1. *Spaced seeds improve metagenomic classification.*  Karel Brinda, Maciej Sykulski, Gregory Kucherov [http://arxiv.org/abs/1502.06256](http://arxiv.org/abs/1502.06256)
