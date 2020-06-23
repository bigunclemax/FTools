#!/usr/bin/env bash
export PATH=$PATH:../../../bin

pref="../../../bin/eif/"

function check() {
  L=$1
  D=$2
  while read eif; do
    eifconverter -U "${pref}${eif}.eif"
    eifconverter -P "${pref}${eif}.bmp" -o "${pref}_${eif}.eif" -d "$D" -s "${pref}${eif}.pal"
    diff "${pref}${eif}.eif" "${pref}_${eif}.eif"
  done < "$L"
}

#test 8bpp
check 8bppList.txt 8
echo "Test 8bpp finished"

#test 32bpp
check 32bppList.txt 32
echo "Test 32bpp finished"

#test 16bpp with predefined palette
check 16bppList.txt 16
echo "Test 16bpp w palette finished"

#test 16bpp with custom
#TODO:

#test 16bpp in bulk mode
#TODO: