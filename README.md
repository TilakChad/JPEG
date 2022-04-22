# JPEG
Decoder for jpeg lossy signal compression <br>

Program to decode .jpg/.jpeg files and convert them into uncompressed bitmap (.bmp) image. 

No support for decoding of : 
- Progressive DCT mode (yet) 
- Lossless mode 
- Arithmetic Encoding 
- Oops, haven't considered single channel jpg images (normal map, specular map mostly use that)

## Build Instructions 

With cmake installed, <br>
`cmake CMakeLists.txt` <br>
`make`<br>
or <br>
`gcc ./Decoder/src/jpeg.c ./Decoder/src/QHTable.c ./Decoder/src/bitstream.c -Og ./utility/bmp.c -lm -o jpeg_decoder` 
<br>-DDEBUG flag should be passed to gcc to generate debug output 

## Usage
`./jpeg_decoder img.jpg`<br>
Output will be saved as `jpeg_output.bmp`

## Sample DCT compressed output
### Original Image
<p align="left">
  <img src="./test/star2.jpg">

### DCT Compressed Image
<p align="left">
  <img src="./test/dct_output.bmp">
  
### Decoded Image 
<p align="left"> 
  <img src="./test/reconstructed.bmp">
  
## References 
https://en.wikipedia.org/wiki/JPEG <br>
https://www.w3.org/Graphics/JPEG/itu-t81.pdf <br>
https://en.wikipedia.org/wiki/Discrete_cosine_transform<br>
https://www.ietf.org/rfc/rfc1951.txt [Deflate]<br>
https://www.youtube.com/watch?v=CPT4FSkFUgs
