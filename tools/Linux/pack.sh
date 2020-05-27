#!/usr/bin/env bash
set -x
set -euo pipefail

export PATH=$PATH:../VbfEditor/bin:../EifViewer/bin:../ImgSectionParser/bin

function pack_images() {
  local cfg_file=$(realpath "$1"/*_config.json)
  local img_dir=$(dirname "$1")/images
  imgparcer -p "$cfg_file" -o "$1"/../_packed.bin
}

function pack_vbf() {
  local vbf_conifg=$(realpath "$1"/*_config.json)
  pack_images "$1"/images/
  #  local img_sec_name=$(sed  -n '9s/.*file.*"\(.*\)".*/_packed.bin/p' "${vbf_conifg}")
  sed -i "9s/:.*\"/: \"_packed.bin\"/" "${vbf_conifg}"
  vbfeditor -p "$vbf_conifg" -o "$1"/../packed.vbf
}

if [ "$#" -ne 1 ]; then
  echo "Specify images vbf file"
  exit 0
fi

pack_vbf $1
