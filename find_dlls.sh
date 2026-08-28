#!/usr/bin/env bash
set -e

EXE="$1"
DLL_DIR="$2"

if [ -z "$EXE" ] || [ -z "$DLL_DIR" ]; then
    exit 0
fi

OBJDUMP="${OBJDUMP:-x86_64-w64-mingw32-objdump}"
if ! command -v "$OBJDUMP" &> /dev/null; then
    OBJDUMP="objdump"
fi

declare -A SEEN

find_deps() {
    local target="$1"
    [ -f "$target" ] || return 0
    
    local deps
    deps=$("$OBJDUMP" -p "$target" 2>/dev/null | grep -i "DLL Name:" | awk '{print $3}' || true)
    
    for dll in $deps; do
        if [ -z "${SEEN[$dll]}" ]; then
            SEEN[$dll]=1
            if [ -f "$DLL_DIR/$dll" ]; then
                echo "$DLL_DIR/$dll"
                find_deps "$DLL_DIR/$dll"
            elif [ -f "$DLL_DIR/../bin/$dll" ]; then
                echo "$DLL_DIR/../bin/$dll"
                find_deps "$DLL_DIR/../bin/$dll"
            fi
        fi
    done
}

find_deps "$EXE"
