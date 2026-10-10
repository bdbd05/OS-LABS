#!/bin/bash
ls -l "$1" > directory-info.last
while true; do
    sleep "$3"
    ls -l "$1" > directory-info.new
    if ! cmp -s directory-info.last directory-info.new; then
        echo "Error! File mismatch"
        #logic to be done
        cp directory-info.new directory-info.last
    fi
done
