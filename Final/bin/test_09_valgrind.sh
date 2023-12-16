#!/bin/bash

SCRATCH=$(mktemp -d)
trap "rm -fr $SCRATCH" INT QUIT TERM EXIT

# Test: data/image.200

test-input() {
    cat <<EOF
debug
format
debug
mount
create
copyin README.md 0
create
copyin bin/test_cat.sh 1
create
stat 0
stat 1
stat 2
cat 0
cat 1
cat 2
debug
copyout 0 $SCRATCH/1.copy
copyout 1 $SCRATCH/2.copy
copyout 2 $SCRATCH/2.copy
remove 0
remove 1
remove 2
EOF

}

# cp data/image.200 $SCRATCH/image.200
# echo -n "Testing valgrind on $SCRATCH/image.200 ... "
# errors=$(test-input | valgrind --leak-check=full ./bin/sfssh $SCRATCH/image.200 200 |& awk '/ERROR SUMMARY/ { print $4 }')
# errors=$(test-input | valgrind --leak-check=full ./bin/sfssh $SCRATCH/image.200 200 |& awk '/ERROR SUMMARY/ { print $4 }')
# # errors=245    # test of errors
# if [ $errors = 0 ]; then
#     echo ">>>>> SUCCESS !"
# else
#     echo "Failed, has: $errors errors"
#     echo ""
# fi

cp data/image.200 $SCRATCH/image.200
echo -n "Testing valgrind on $SCRATCH/image.200 ... "

# Running the test-input function and capturing Valgrind errors
errors=$(test-input | valgrind --leak-check=full ./bin/sfssh $SCRATCH/image.200 200 2>&1 | awk '/ERROR SUMMARY/ { print $4 }')

# Check if errors variable is set and numeric
if [[ -z "$errors" ]] || ! [[ "$errors" =~ ^[0-9]+$ ]]; then
    echo "Error: Unable to read Valgrind output or no numeric error count found."
    exit 1
fi

# Comparing the number of errors
if [ "$errors" -eq 0 ]; then
    echo ">>>>> SUCCESS !"
else
    echo "Failed, has: $errors errors"
fi
