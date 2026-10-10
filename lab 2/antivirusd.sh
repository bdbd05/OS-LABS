#!/bin/bash
ls -l "$1" > directory-info.last
while true; do
    sleep "$3"
    ls -l "$1" > directory-info.new
    if ! cmp -s directory-info.last directory-info.new; then
        echo "Error! A change was detected, scanning directory..."
        for file in "$1"/*; do
        filter=$(basename "$file")
        if [[ "$fiter" == *.exe || "$filter" == *.bat || "$filter" == *.vbs || "$filter" == *.scr || "$filter" == *.ps1 ]] || grep -iqE "virus|trojan|malware|worm|ransomware" "$file"; then
            echo "$filter is malicious and it is DELETED"
            cp "$file" "$2/$filter"
            rm -f "$file"
        fi
        done
        ls -l "$1" > directory-info.last
    fi
done
