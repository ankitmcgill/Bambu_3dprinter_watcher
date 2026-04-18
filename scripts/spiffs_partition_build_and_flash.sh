# Spiffs Parition Build & Flash
# 8/27/25

#!/bin/bash

# Get The Path Where This Shell Script Is
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"

# Source Color Definitions
source $SCRIPT_DIR/colors.sh

printf "${LIGHT_YELLOW}*** Spiffs Parition Build & Flash ***${ENDCOLOR}\n"
idf.py partition-table && idf.py partition-table-flash