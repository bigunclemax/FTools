#!/usr/bin/env bash

set -euo pipefail

function extract_eifs() {
  local zip_dir=$1
  local eifs_dir=$1/eifs
  local bmp_dir=$1/bmp

  mkdir -p $eifs_dir
  mkdir -p $bmp_dir

  for z in $zip_dir/*.zip ; do
    if [ -f $z ]; then
      unzip $z -d $eifs_dir
    fi
  done

  for eif in $eifs_dir/*.eif ; do
    if [ -f $eif ]; then
      local eif_name=$(basename $eif)
      eifviewer -u $eif -o $bmp_dir/${eif_name}.bmp
    fi
  done
}

function unpack_images() {
  local img_file=$1
  local img_dir=$(dirname $1)/images
  mkdir -p $img_dir

  imgparcer -u ${img_file} -o $img_dir/

  extract_eifs $img_dir/
}

function unpack_vbf() {
  local vbf=$1
  local vbf_dir=${vbf}_unpacked
  mkdir -p ./${vbf_dir}
  vbfeditor -u ${vbf} -o ${vbf_dir}/

  unpack_images $(echo ${vbf_dir}/${vbf}_section_1400000_*)
}

if [ "$#" -ne 1 ]; then
  echo "Specify images vbf file"
  exit 0
fi

unpack_vbf $1
