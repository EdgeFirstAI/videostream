add_executable(vsl-monitor monitor.c)
target_link_libraries(vsl-monitor stb videostream)

add_executable(vsl-test-shm test_shm.c)
target_link_libraries(vsl-test-shm videostream m)

add_executable(vsl-camhost camhost.c)
target_link_libraries(vsl-camhost videostream)
target_include_directories(vsl-camhost PRIVATE)

if(GSTREAMER_FOUND)
    add_executable(vsl-display display.c)
    target_link_libraries(vsl-display videostream ${GSTREAMER_LIBRARIES})
    target_include_directories(vsl-display PRIVATE ${GSTREAMER_INCLUDE_DIRS})

    add_executable(vsl-record record.c)
    target_link_libraries(vsl-record videostream ${GSTREAMER_LIBRARIES})
    target_include_directories(vsl-record PRIVATE ${GSTREAMER_INCLUDE_DIRS})
endif()

if(ENABLE_VPU)
    add_executable(vsl-test-encoder test_encoder.c)
    target_link_libraries(vsl-test-encoder videostream m)

    add_executable(vsl-test-decoder test_decoder.c)
    target_link_libraries(vsl-test-decoder videostream m)

    add_executable(vsl-test-encoder-4k-tiles test_encoder_4k_tiles.c)
    target_link_libraries(vsl-test-encoder-4k-tiles videostream m)

    add_executable(vsl-test-encoder-keyframes-4k-tiles test_encoder_keyframes_4k_tiles.c)
    target_link_libraries(vsl-test-encoder-keyframes-4k-tiles videostream m)
endif(ENABLE_VPU)

install(TARGETS vsl-camhost COMPONENT runtime RUNTIME DESTINATION bin)
install(FILES camhost.c COMPONENT samples DESTINATION ${SAMPLES_INSTALL_DIR})