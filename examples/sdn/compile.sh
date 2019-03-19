#!/bin/bash
trap 'killallp' INT

killallp() {
    trap '' INT TERM     # ignore INT and TERM while shutting down
    echo -n "> Finishing... "     # added double quotes
    kill -TERM 0         # fixed order, send TERM not INT
    wait
    echo "DONE"
}

echo "*** COMPILING... ***"
echo "> Compile controller"
cd controller
make clean && make "$@"
echo "> Compile node"
cd ../node
make clean && make "$@"

killallp
wait < <(jobs -p)
echo "*** FINISHED! ***"
