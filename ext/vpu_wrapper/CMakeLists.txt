project(vpu_wrapper)

add_library(vpu_wrapper STATIC hantro_vc8000e_enc.c hantro_dec.c codec_dlopen.c utils.c vpu_wrapper_hantro_VCencoder.c vpu_wrapper_hantro.c)
target_link_libraries(vpu_wrapper dl m)
target_include_directories(vpu_wrapper PUBLIC ${CMAKE_CURRENT_LIST_DIR})