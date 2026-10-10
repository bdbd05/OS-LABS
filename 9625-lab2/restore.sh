#!/bin/bash

while true; do
    if [ -z "$(ls -A "$2")" ]; then
        echo "No malicious files to review."
        exit 0
    fi
    files=("$2"/*)
    
    echo "Malicious files detected in $2:"
    for i in "${!files[@]}"; do
        echo "$((i+1))) $(basename "${files[$i]}")"
    done
    
    echo "Pick a file by its number:"
    read choice
    
    index=$((choice-1))
    target_file="${files[$index]}"
    target_name=$(basename "$target_file")
    
    echo "What do you want to do with $target_name?"
    echo "1: Restore this file back into $1"
    echo "2: Permanently delete this file from $2"
    echo "3: Leave this file as-is and go back to the list"
    read option
    
    if [ "$option" == "1" ]; then
        mv "$target_file" "$1/$target_name"
        echo "Restored $target_name to $1."
        echo "$target_name" >> whitelist.txt
        echo "Added $target_name to whitelist."
    elif [ "$option" == "2" ]; then
        rm "$target_file"
        echo "$target_name permanently deleted."
    fi
done