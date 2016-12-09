#! /bin/bash

SIGS_DIR=$( cd "$( dirname "$0" )" && pwd )
KEY_FILE=jeog.dev.key
CHECK_PATH=$SIGS_DIR/notes_checksums.txt

X86_BIN_DIR=$SIGS_DIR/../bin/Release/Win32
X64_BIN_DIR=$SIGS_DIR/../bin/Release/x64


echo -e "* All the binaries are signed with the jeog.dev.key (jeog.dev@gmail.com) 
public key. It should be available on public key servers. \n" > $CHECK_PATH

echo -e "* Being in a seprate 'sigs' folder you'll have to point the verifier to the 
corresponding binary file for each signature, or use verify.sh on linux. \n" >> $CHECK_PATH

echo -e "SHA256 checksums:" >> $CHECK_PATH

echo -e "\n(THE SIGNATURE FILES)" >> $CHECK_PATH
sha256sum $(ls | egrep '\.sig$') >> $CHECK_PATH

echo -e "\n(THE ACTUAL BINARIES)" >> $CHECK_PATH
cd $X86_BIN_DIR
sha256sum $(ls) >> $CHECK_PATH
cd $X64_BIN_DIR
sha256sum $(ls) >> $CHECK_PATH

cd $SIGS_DIR






