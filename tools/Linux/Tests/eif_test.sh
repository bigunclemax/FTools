#!/usr/bin/env bash
export PATH=$PATH:../../../bin

pref="../../../bin"

function check() {
  L=$1
  D=$2
  while read eif; do
    eifconverter -U "${pref}/eif/${eif}.eif"
    eifconverter -P "${pref}/eif/${eif}.bmp" -o "${pref}/eif/_${eif}.eif" -d "$D" -s "${pref}/eif/${eif}.pal"
    diff "${pref}/eif/${eif}.eif" "${pref}/eif/_${eif}.eif"
  done < "$L"
}

function check_bulk() {
  test_dir="./bulk"
  mkdir -p "${test_dir}"
  
  while read line; do
    eif=$(echo ${line} | cut -f1 -d';')
    cp "${pref}/bmp/${eif}.bmp" "${test_dir}/"    
  done < "16bppBulkList.txt"

  eifconverter -B "${test_dir}" -o "${test_dir}"

  while read line; do

    eif=$(echo "${line}" | cut -f1 -d';')
    crc=$(echo "${line}" | cut -f2 -d';')

    crc_new=$(cksum "${test_dir}/${eif}.eif" | cut -f1 -d' ')
    if [ "$crc_new" != "$crc" ]; then
      echo "CRC missmatch ${eif}"
    fi
      
  done < "16bppBulkList.txt"

  rm -rf "${test_dir}"
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
check_bulk
echo "Test 16bpp bulk mode finished"