# Compile
# 8/27/25

#!/bin/bash

if [ "$#" -eq 0 ]; then
    echo "Usage: $0 <string1> [string2 ... stringN]"
    echo "Strings:"
    echo "  build"
    echo "  flash"
    echo "  monitor"
    echo "  clean"
    echo "  fullclean"
    echo "  settarget"
    echo "  size"
    exit 1
fi

# Get The Path Where This Shell Script Is
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"

# Source Color Definitions
source $SCRIPT_DIR/colors.sh

# List Of Valid Arguments
VALID=("build" "flash" "monitor" "clean" "fullclean" "set-target" "size" "erase-flash")

for arg in "$@"; do
    found=false
    for v in "${VALID[@]}"; do
        if [ "$arg" = "$v" ]; then
            found=true
            break
        fi
    done

    if $found; then
        printf "${LIGHT_YELLOW}*** "$arg" ***${ENDCOLOR}\n"
        if [ "$arg" = "set-target" ]; then
            idf.py "$arg" esp32s3
        else
            idf.py "$arg"
        fi

        # Exit If Command Failed
        if [ $? -ne 0 ]; then
            printf "${LIGHT_RED}Command Failed. Exiting!${ENDCOLOR}\n"
            exit 1
        fi
    else
        printf "${LIGHT_RED}Invalid Argument Found "$arg". Exiting!${ENDCOLOR}\n"
        exit 1
    fi
done
exit 0