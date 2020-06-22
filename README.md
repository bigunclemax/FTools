# FTools

This toolset useful for unpacking\repacking firmware part of the ford ipc which contain pictures resources.

##### The tools in this package:
- imgunpkr can be used to unpack\pack images and fonts from vbf.
- vbfeditor can be used to unpack\pack vbf files.
- imgparcer can be used to extract .zip archives with images and .ttf fonts from image section.
- eifconverter can turn a .eif image into a .bmp and back.

Linux scripts from tools dir:
- extract_images.sh
- pack.sh
- replace_img.sh

#### How to
##### imgunpkr
###### Extract images and fonts
`imgunpkr -u -o ./dest_dir original_images.vbf`
Images will be extracted to:
- `./dest_dir/eif` - EIF images resources represent in internal IPC format.   
- `./dest_dir/bmp` - Converted EIFs to bmp.  
    
Fonts will be extracted to:
- `./dest_dir/ttf`  
    
And will be created `./dest_dir/export_list.csv` which contain info about extracted resources.

###### Modify image
You could modify bmp images with usage [eif converter](#eifconverter)

###### Pack images and fonts
`imgunpkr -p -c export_list.csv -o ./dest_dir original_images.vbf`  
As a result `./dest_dir.patched.vbf` will be created.

##### eifconverter
###### Convert bmp images to eif
`./eifconverter -u ./_logo.eif -o ./out.bmp`

###### Convert eif images to bmp  
`./eifconverter -p -d 16 ./edited.bmp -o ./out.eif`

EIF images has differ color depth and could be:
- SUPERCOLOR - TrueColor(24bit) `-d 32`
- MULTICOLOR - 256 color with palette `-d 16`
- MONOCOLOR - 8bit color `-d 8`
