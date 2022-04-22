cmake_minimum_required(VERSION 3.10)

project(jpeg)
add_executable(jpeg_decoder ./Decoder/src/jpeg.c ./Decoder/src/QHTable.c ./Decoder/src/bitstream.c ./utility/bmp.c)
target_link_libraries(jpeg_decoder m)
