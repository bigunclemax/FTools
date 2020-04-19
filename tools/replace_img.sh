#!/bin/sh
set -x
FW_PATH="./"
CONTENT="example.eif"
ZIP_IDX="0"
ORIG_NAME="orig_example.eif"

usage()
{
    echo "if this was a real script you would see something useful here"
    echo ""
    echo "./simple_args_parsing.sh"
    echo "\t-h --help"
    echo "\t--fw-path=$FW_PATH"
    echo "\t--content=$CONTENT"
    echo "\t--zip-idx=$ZIP_IDX"
    echo "\t--orig-name=$ORIG_NAME"
    echo ""
}

while [ "$1" != "" ]; do
    PARAM=`echo $1 | awk -F= '{print $1}'`
    VALUE=`echo $1 | awk -F= '{print $2}'`
    case $PARAM in
        -h | --help)
            usage
            exit
            ;;
        --fw-path)
            FW_PATH=$VALUE
            ;;
        --content)
            CONTENT=$VALUE
            ;;
        --zip-idx)
          ZIP_IDX=$VALUE
          ;;
        --orig-name)
          ORIG_NAME=$VALUE
          ;;
        *)
            echo "ERROR: unknown parameter \"$PARAM\""
            usage
            exit 1
            ;;
    esac
    shift
done



#serach config
images_conifg=$(realpath $FW_PATH/images/*_config.json)
if ! [ -f "$images_conifg" ]; then
  echo "ERROR: can't find images config"
  exit 1
fi

#new image
if ! [ -f "$CONTENT" ]; then
  echo "ERROR: can't get content file"
  exit 1
fi

replace_by_idx() {
  set -x
  local zip_str="zip/$ZIP_IDX.zip"
  local zip_str_num=$(grep -n "${zip_str}" "$images_conifg" | cut -f1 -d:)
  local zip_sz_str_num=$((${zip_str_num}+2))
  local zip_path=$FW_PATH/images/zip/$ZIP_IDX.zip

  ziprepacker -i "$zip_path" -c "$CONTENT"
  local new_zip_sz=$(stat -c%s "${zip_path}repacked.zip")
  sed -i -E "${zip_sz_str_num}s/([[:digit:]]+)/${new_zip_sz}/" "$images_conifg"
  sed -i -E "${zip_str_num}s#${zip_str}#${zip_str}repacked.zip#" "$images_conifg"

}

replace_by_name() {
  echo 1
}

#
if [ "$ZIP_IDX" ]; then
  replace_by_idx
elif [ "$ORIG_NAME" ]; then
  replace_by_name
fi