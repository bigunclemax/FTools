# FTools

This toolset useful for unpacking\repacking firmware part of the ford ipc which contain pictures resources.

##### The tools in this package:
- **imgunpkr** can be used to unpack\pack images and fonts from vbf.
- **vbfeditor** can be used to unpack\pack vbf files.
- **imgparcer** can be used to extract .zip archives with images and .ttf fonts from image section.
- **eifconverter** can turn a .eif image into a .bmp and back.
- **textparser** can be used to extract IPC fw text lines to CSV format and pack them back.  

[README russian version](README_ru.md)

---

## imgunpkr
##### Extract images and fonts
`imgunpkr -u -o ./dest_dir original.vbf`  

Images will be extracted to:
- `./dest_dir/eif` - EIF images resources represent in internal IPC format.   
- `./dest_dir/bmp` - Converted EIFs to bmp.  
    
Fonts will be extracted to:
- `./dest_dir/ttf`  
    
Info files:  
- `./dest_dir/export_list.csv` which contain info about extracted resources (basically you don't need to edit this file).  
- `./dest_dir/header_lines.csv` which describe properties of the UI elements (size, color, position)  

[More about header_lines.csv](https://github.com/AuRoN89/FTools/blob/master/Doc/HEADER%20Usage.txt)

And will be created `./dest_dir/custom` folder.  

##### Modify resources
To change images or fonts, place the modified .bmp or .ttf to a `./dest_dir/custom` folder.  
**Files must have the same names as the extracted resources.**

##### Pack images and fonts
`imgunpkr -p -c export_list.csv -v original.vbf -o ./dest_dir`
  
As a result `./dest_dir/patched.vbf` will be created.

---

## eifconverter
##### Convert eif images to bmp  
`./eifconverter -u ./logo.eif -o ./out.bmp`

##### Convert bmp images to eif  
`./eifconverter -p -d 16 ./edited.bmp -o ./out.eif`

EIF images has differ color depth and could be:
- SUPERCOLOR - TrueColor(24bit) `-d 32`
- MULTICOLOR - 256 color with palette `-d 16`
- MONOCOLOR - 8bit color `-d 8`

---

## vbfeditor
##### Unpack vbf (extract binary sections)
`vbfeditor -u path_to.vbf -o dir/to/extract`

##### Pack vbf  
`vbfeditor -p vbf_conifg -o out/dir`

---

## textparser (alfa)  
##### Extract text resources to .csv files  
`textparser -u path/to/vbf_text_section.bin -o dir/to/extract`

##### Modify resources  
After extracting you will get two files `ui_alerts.csv` and `ui_texts.csv`  
Modify strings of `line_content` as you wish  

##### Pack text section  
`textparser -p path/to/vbf_text_section.bin -o path/to/patched_vbf_text_section.bin`  
> modified .csv files must be in the same directory as the original vbf_text_section.bin  