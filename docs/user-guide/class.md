# Classification

Classification is run in the same way as in *Kraken*:

    kraken --db seed-kraken-db --preload --threads 20 input.fa > output.out 2> output.log

To classify **seed-Kraken** will use the *spaced seed* stored in the 
file `seed-kraken-db/spaced_seed_sequence`.

<!--
    #!/bin/bash

    KRAKEN_DB_NAME="$1"
    set -u  # Protect against uninitialized vars.
    set -e  # Stop on error
    set -o pipefail  # Stop on failures in non-final pipeline commands

    THREADS=20
    #PRELOADF=""
    PRELOADF="--preload"

    run_kraken ()
    {
    FA_DIR="$1"
    FFILE="$2"
    RDIR="$3"
    time kraken $PRELOADF --db $KRAKEN_DB_NAME --threads $THREADS "$FA_DIR/$FFILE" > "$RDIR/${FFILE}.out" 2> "$RDIR/${FFILE}.log"
    }

    ACCDIR="accuracy_data"
    RESDIR="results-$KRAKEN_DB_NAME"

    mkdir -p $RESDIR

    run_kraken $ACCDIR HiSeq_accuracy.fa $RESDIR
    run_kraken $ACCDIR MiSeq_accuracy.fa $RESDIR
    run_kraken $ACCDIR simBA5_accuracy.fa $RESDIR
-->

# Output format

Output format differs slightly from the original **Kraken**. An example line in the output file:

    C       A_hydrophila_HiSeq.18518        1288394 101     0:62|644:2 1288394:3 644:2 1288394:7 644:48
    
Notice `|` separating classification outputs for *sense/anti-sense* strand.
Refer to [README](http://github.com/macieksk/seed-kraken/) of the original **Kraken** for precise description of the output.