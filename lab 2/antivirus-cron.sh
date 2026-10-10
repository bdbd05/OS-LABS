#!/bin/bash
sleep 23

for file in "$1"/*; do
    [ -f "$file" ] || continue
    name=$(basename "$file")
    
    if [ -f "$3" ] && grep -qiFx -- "$name" "$3"; then
        continue
    fi
    
    if [[ "$name" == *.exe || "$name" == *.bat || "$name" == *.vbs || "$name" == *.scr || "$name" == *.ps1 ]] || grep -iqE "virus|trojan|malware|worm|ransomware" "$file"; then
        echo "$name is malicious and it is DELETED"
        cp "$file" "$2/$name" && rm -f "$file"
    fi
done