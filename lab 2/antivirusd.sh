#!/bin/bash
scan_directory() {
    for file in "$1"/*; do
        [ -f "$file" ] || continue

        filter=$(basename "$file")
        if [[ "$filter" == *.exe || "$filter" == *.bat || "$filter" == *.vbs || "$filter" == *.scr || "$filter" == *.ps1 ]] || grep -iqE "virus|trojan|malware|worm|ransomware" "$file"; then
            echo "$filter is malicious and it is DELETED"
            cp "$file" "$2/$filter" && rm -f "$file"
        fi
    done
}

if [ ! -f directory-info.last ]; then
    scan_directory "$1" "$2"
    ls -l "$1" > directory-info.last
fi

while true; do
    sleep "$3"
    ls -l "$1" > directory-info.new
    if ! cmp -s directory-info.last directory-info.new; then
        scan_directory "$1" "$2"
        ls -l "$1" > directory-info.last
    fi
done