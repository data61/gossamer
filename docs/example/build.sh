#!/bin/bash 

INPUT1=reads1.fq.gz
INPUT2=reads2.fq.gz
K=21
INSERT_SIZE=100
EXPECTED_COVERAGE=35

# Check to see if goss or gossple.sh is on the current PATH. If not,
# assume we're in the example directory so use the parent directory.

if (command -v goss || command -v gossple.sh) 2>&-; then
    echo "Using installed goss/gossple.sh"
else
    PATH="..:$PATH"
    if (command -v goss || command -v gossple.sh) 2>&-; then
        echo "Using goss/gossple.sh from in the parent directory"
    else
        echo "Can't find goss and/or gossple.sh"
        exit 1
    fi
fi

gossple.sh -v -k $K -c $EXPECTED_COVERAGE -p $INSERT_SIZE $INPUT1 $INPUT2

