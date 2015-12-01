#! /bin/bash

echo "verify.sh assumes: 
  1) you have a working gpg in your path
  2) you've imported jeog.dev.key 
  3) you've checked the hashes of all the relevant files(see notes_checksums.txt)
"

SIGS_DIR=$( cd "$( dirname "$0" )" && pwd )

cd $SIGS_DIR

X86_SIG_FILES=($(ls | egrep '.*tos-databridge.+x86\..+asc'))
X64_SIG_FILES=($(ls | egrep '.*tos-databridge.+x64\..+asc'))

_good_or_bad(){
  if (( $1 )); then
    VBAD=$(($VBAD+1))
  else
    VGOOD=$(($VGOOD+1))
  fi  
}

VGOOD=0
VBAD=0
for f in ${X86_SIG_FILES[*]}; do
  echo "::: $f :::"
  gpg --verify $f "../bin/Release/Win32/${f%.asc}";
  echo ""
  _good_or_bad $?
done
X86_SUMMARY="x86 binaries: $VGOOD good, $VBAD bad"

VGOOD=0
VBAD=0
for f in ${X64_SIG_FILES[*]}; do
  echo "::: $f :::"
  gpg --verify $f "../bin/Release/x64/${f%.asc}";
  echo ""
  _good_or_bad $?
done
X64_SUMMARY="x64 binaries: $VGOOD good, $VBAD bad"


echo ""
echo "::: SUMMARY :::"
echo "$X86_SUMMARY"
echo "$X64_SUMMARY"
echo ":::::::::::::::"
echo ""


