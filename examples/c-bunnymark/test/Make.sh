#!/bin/bash

# define an abort function to call on error
abort_build()
{
    echo
    echo BUILD FAILED
    exit 1
}

# create bin folders if non existing, since the
# development tools will not create it themselves
mkdir -p bin

echo
echo Pack the ROM
echo --------------------------
packrom "CBunnymark.xml" -o "bin/CBunnymark.v32" || abort_build

echo
echo BUILD SUCCESSFUL
