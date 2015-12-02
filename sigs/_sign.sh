#! /bin/bash

SIGS_DIR=$( cd "$( dirname "$0" )" && pwd )

X86_BIN_DIR=$SIGS_DIR/../bin/Release/Win32
X64_BIN_DIR=$SIGS_DIR/../bin/Release/x64

cd $X86_BIN_DIR

X86_BIN_FILES=($(ls))
for f in ${X86_BIN_FILES[*]}; do  
  gpg -sab --local-user jeog.dev $f;
done

mv *.asc $SIGS_DIR

cd $X64_BIN_DIR

X64_BIN_FILES=($(ls))
for f in ${X64_BIN_FILES[*]}; do  
  gpg -sab --local-user jeog.dev $f;
done

mv *.asc $SIGS_DIR

cd $SIGS_DIR


