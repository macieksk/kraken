# Building a database

Building a database in **seed-Kraken** requires providing a *spaced seed* 
as an argument: `--seed SEED`, 

A seed sequence is then stored in a file inside the database directory: `seed-kraken-db/spaced_seed_sequence`.
Once built the database will work only with this seed.

Download options: `--download-library`, `--download-taxonomy` do not need the seed argument.
You will find  a more detailed description of the building process in [README](http://github.com/macieksk/seed-kraken/) of the original **Kraken**. 

## Bash script

After downloading taxonomy, and downloading/collecting the library,
this example database building bash script can be run.

    #!/bin/bash

    set -u  # Protect against uninitialized vars.
    set -e  # Stop on error
    set -o pipefail  # Stop on failures in non-final pipeline commands

    KRAKEN_DB_NAME="$1

    SEED="######-##-#-#-##-###-#-##---###--#######"

    THREADS=20    
    MINIMIZER_LEN="12"
    JELLYFISH_HASH_SIZE="5000M"
    
    check_for_jellyfish.sh
    kraken-build --db $KRAKEN_DB_NAME --build --threads $THREADS -seed $SEED \
	--minimizer-len $MINIMIZER_LEN --jellyfish-hash-size $JELLYFISH_HASH_SIZE

