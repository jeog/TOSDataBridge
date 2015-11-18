#! /bin/bash

echo "verify.sh assumes: 
  1) you have a working gpg in your path
  2) you've imported jeog.dev.key 
  3) you've checked the hashes of all the relevant files(see notes_checksums.txt)
"

SIGS_DIR=$( cd "$( dirname "$0" )" && pwd )

cd $SIGS_DIR

X86_SIG_FILES=($(ls | egrep '.+x86\..+asc'))
X64_SIG_FILES=($(ls | egrep '.+x64\..+asc'))

for f in ${X86_SIG_FILES[*]}; do
  echo "::: $f :::"
  gpg --verify $f "../bin/Release/Win32/${f%.asc}" ;
done

for f in ${X64_SIG_FILES[*]}; do
  echo "::: $f :::"
  gpg --verify $f "../bin/Release/x64/${f%.asc}" ;
done
