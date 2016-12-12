#! /bin/bash

echo "verify.sh assumes: 
  1) you have a working gpg in your path
  2) you've imported jeog.dev.key 
"

sigs_dir=$( cd "$( dirname "$0" )" && pwd )

cd $sigs_dir

x86_sig_files=($(ls | egrep '.*tos-databridge.+x86\..+sig'))
x64_sig_files=($(ls | egrep '.*tos-databridge.+x64\..+sig'))

check_return=0

cmp_checksums()
{
    echo '   ' $1 'sha256 checksums'
    off=$(cat notes_checksums.txt | grep ' '$1'$' | cut -d' ' -f1) 
    calc=$(sha256sum $2 | cut -d' ' -f1)
    echo '      ' $off 
    echo '      ' $calc        
    [[ $off == $calc ]]
    check_return=$?
}

run_checks()
{
    files=("${!1}")
    for s in ${files[*]}; do
        echo "::: ${s%.sig} :::"

        # signature checksums
        cmp_checksums $s $s
        if (( $check_return )); then
            vbad_checksum_sig=$(($vbad_checksum_sig+1))
        else
            vgood_checksum_sig=$(($vgood_checksum_sig+1))
        fi  

        # binary checksums
        bin="${s%.sig}"
        binpath=$2$bin 
        cmp_checksums $bin $binpath 
        if (( $check_return )); then
            vbad_checksum_bin=$(($vbad_checksum_bin+1))
        else
            vgood_checksum_bin=$(($vgood_checksum_bin+1))
        fi  

        # verify
        gpg --verify $s $binpath;       
        if (( $? )); then
            vbad=$(($vbad+1))
        else
            vgood=$(($vgood+1))
        fi 
        echo ""
    done
}


vgood=0
vbad=0
vgood_checksum_sig=0
vbad_checksum_sig=0
vgood_checksum_bin=0
vbad_checksum_bin=0


run_checks x86_sig_files[@] "../bin/Release/Win32/"

x86_summary="x86 binaries: $vgood good, $vbad bad"
x86_summary_checksum_sig="x86 binaries: $vgood_checksum_sig good, $vbad_checksum_sig bad"
x86_summary_checksum_bin="x86 binaries: $vgood_checksum_bin good, $vbad_checksum_bin bad"

bad_total=$(($vbad + $vbad_checksum_sig + $vbad_checksum_bin))


vgood=0
vbad=0
vgood_checksum_sig=0
vbad_checksum_sig=0
vgood_checksum_bin=0
vbad_checksum_bin=0

run_checks x64_sig_files[@] "../bin/Release/x64/"

x64_summary="x64 binaries: $vgood good, $vbad bad"
x64_summary_checksum_sig="x64 binaries: $vgood_checksum_sig good, $vbad_checksum_sig bad"
x64_summary_checksum_bin="x64 binaries: $vgood_checksum_bin good, $vbad_checksum_bin bad"


bad_total=$(($bad_total + $vbad + $vbad_checksum_sig + $vbad_checksum_bin))

echo 
if (( $bad_total )); then    
    echo "*** $bad_total ERRORS ***"
else
    echo "*** SUCCESS ***"
fi
echo

echo 
echo "::: summary (sig checksums) :::"
echo "$x86_summary_checksum_sig"
echo "$x64_summary_checksum_sig"
echo 
echo "::: summary (bin checksums) :::"
echo "$x86_summary_checksum_bin"
echo "$x64_summary_checksum_bin"
echo 
echo "::: summary (signatures) :::"
echo "$x86_summary"
echo "$x64_summary"
echo


