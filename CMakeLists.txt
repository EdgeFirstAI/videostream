cmake_minimum_required(VERSION 3.14)

# Parse version from videostream.h (single source of truth)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/videostream.h" HEADER_CONTENT)
string(REGEX MATCH "#define VSL_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+(-[a-zA-Z0-9]+)?)\"" _ "${HEADER_CONTENT}")
set(PROJECT_VERSION_FULL ${CMAKE_MATCH_1})

# Extract numeric version for CMake project() command (major.minor.patch only)
string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)" PROJECT_VERSION "${PROJECT_VERSION_FULL}")

# Project declaration with version
project(VideoStream 
    VERSION ${PROJECT_VERSION}
    DESCRIPTION "VideoStream - Zero-copy IPC frame sharing library"
    LANGUAGES C
    HOMEPAGE_URL "https://github.com/EdgeFirstAI/videostream"
)

# Set SPDX license variables for CMake 4.2+ compatibility
# CMake 4.2 adds SPDX_LICENSE to project() but it's experimental and gated
# We set these variables manually so they're available if needed by export/install
set(PROJECT_SPDX_LICENSE "Apache-2.0")
set(VideoStream_SPDX_LICENSE "Apache-2.0")

# Project metadata
set(COMPANY "Au-Zone Technologies")
set(EMAIL "support@au-zone.com")
set(COPYRIGHT "Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.")
set(SOVERSION "1")

if (WIN32 OR APPLE)
    set(PACKAGE_NAME "VideoStream")
else()
    set(PACKAGE_NAME "videostream")
endif()

message(STATUS "VideoStream version: ${PROJECT_VERSION_FULL}")

# Build options
option(ENABLE_OPENMP "Enable OpenMP threading acceleration" ON)
option(ENABLE_GSTREAMER "Enable building the VideoStream GStreamer plugin" ON)
option(INSTALL_MANUAL "Install User Manual" OFF)
option(ENABLE_COVER "Enable code coverage for the library" OFF)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(SONAR_ANALYSIS "Disable compiler flags incompatible with SonarCloud" OFF)

if (UNIX AND NOT APPLE)
    option(ENABLE_G2D "Enable G2D for frame conversion acceleration" ON)
    option(ENABLE_DMABUF "Enable Linux dmabuf support" ON)
    option(ENABLE_VPU "Enable VPU for frame encoding acceleration" ON)
    option(ENABLE_V4L2_CODEC "Enable V4L2 mem2mem codec backend" ON)
    option(ENABLE_HANTRO_CODEC "Enable Hantro libcodec.so codec backend" ON)
endif()

# OpenMP support
if (ENABLE_OPENMP)
    find_package(OpenMP)
    if (OpenMP_C_FOUND)
        message(STATUS "OpenMP found: ${OpenMP_C_FLAGS}")
    endif()
endif()

# Standard CMake modules
include(CTest)
include(GenerateExportHeader)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CMakeToolsHelpers OPTIONAL)

# Configure install directories
set(PYTHON_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/python3/dist-packages)
set(SAMPLES_INSTALL_DIR ${CMAKE_INSTALL_DATADIR}/videostream/samples)

# C standard and compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# RPATH configuration for relocatable binaries
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

if (UNIX AND NOT APPLE)
    set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib")
endif()

# Normalize architecture naming for package filenames
# CMake 3.14+ provides CMAKE_SYSTEM_PROCESSOR which we normalize to common conventions
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" TARGET_ARCH)

# Normalize ARM64 variants to aarch64 (standard Linux convention)
if(TARGET_ARCH MATCHES "^(arm64|armv8)")
    set(TARGET_ARCH "aarch64")
# Normalize AMD64 variants to x86_64
elseif(TARGET_ARCH MATCHES "^(amd64)")
    set(TARGET_ARCH "x86_64")
endif()

message(STATUS "Target architecture: ${TARGET_ARCH} (detected: ${CMAKE_SYSTEM_PROCESSOR})")

# Configure pkg-config file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/videostream.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/videostream.pc
    @ONLY)

# Platform-specific definitions will be set on targets

# Compiler warnings and flags
if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    # MSVC compiler flags
    add_compile_options(
        /W4
        /we4013  # Error on implicit function declaration
        /we6246  # Error on local declaration hiding outer scope
        /wd4204  # Disable warning: non-constant aggregate initializer
        /wd4152  # Disable warning: nonstandard extension, function/data pointer conversion
    )
    
    if (ENABLE_OPENMP)
        add_compile_options(/openmp)
    endif()
    
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU")
    # GCC compiler flags
    set(MAX_STACK 4096 CACHE STRING "Maximum stack usage allowed by the library")
    
    add_compile_options(
        -Wall
        -Wextra
        -Werror=implicit-function-declaration
        -Werror=shadow
        -Wno-multichar
    )
    
    # Flags that may confuse SonarCloud analyzer - skip when SONAR_ANALYSIS=ON
    if(NOT SONAR_ANALYSIS)
        add_compile_options(
            -fstack-usage
            -Wstack-usage=${MAX_STACK}
            -fdump-rtl-expand
        )
    endif()
    
    add_compile_options("$<$<CONFIG:RelWithDebInfo>:-fno-inline;-fno-omit-frame-pointer;-g3>")
    add_link_options("$<$<CONFIG:Release>:-s>")
    
    if (CMAKE_SYSTEM_NAME STREQUAL "Generic")
        add_link_options(-Wl,-Map)
    endif()
    
    if (ENABLE_COVER)
        message(STATUS "Enabling code coverage reporting")
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
        add_link_options(--coverage)
    endif()
    
    if (ENABLE_OPENMP AND OpenMP_C_FOUND)
        add_compile_options(${OpenMP_C_FLAGS})
        add_link_options(${OpenMP_C_FLAGS})
    endif()
    
else()
    # Other compilers (Clang, etc.)
    add_compile_options(-Wall -Wextra)
    
    if (ENABLE_OPENMP AND OpenMP_C_FOUND)
        add_compile_options(${OpenMP_C_FLAGS})
        add_link_options(${OpenMP_C_FLAGS})
    endif()
endif()

# Dependencies
find_package(Threads REQUIRED)

# External dependencies
add_subdirectory(ext/stb)

# Codec backend support
# Always include the unified encoder/decoder dispatch and backend detection
set(CODEC_SRCS
    lib/codec_backend.h
    lib/codec_backend.c
    lib/decoder.c
    lib/encoder.c
)

# V4L2 mem2mem codec backend (optional)
if(ENABLE_V4L2_CODEC)
    list(APPEND CODEC_SRCS
        lib/decoder_v4l2.h
        lib/decoder_v4l2.c
        lib/encoder_v4l2.h
        lib/encoder_v4l2.c
    )
endif()

# Hantro/libcodec.so codec backend (optional, requires VPU wrapper)
if(ENABLE_HANTRO_CODEC AND ENABLE_VPU)
    add_subdirectory(ext/vpu_wrapper)
    list(APPEND CODEC_SRCS
        lib/decoder_hantro.c
        lib/decoder_hantro.h
        lib/decoder_hantro_dmabuf.c
        lib/encoder_hantro.c
        lib/encoder_hantro.h
        lib/encoder_hantro_dmabuf.c
    )
    set(ENCODER_LIBS vpu_wrapper)
elseif(ENABLE_VPU AND NOT ENABLE_HANTRO_CODEC)
    # Legacy: VPU enabled but Hantro codec disabled - still build wrapper for encoder
    add_subdirectory(ext/vpu_wrapper)
    list(APPEND CODEC_SRCS
        lib/encoder_hantro.c
        lib/encoder_hantro.h
        lib/encoder_hantro_dmabuf.c
    )
    set(ENCODER_LIBS vpu_wrapper)
endif()

# Main VideoStream library
add_library(videostream
    include/videostream.h
    lib/common.h
    lib/common.c
    lib/frame.h
    lib/frame.c
    lib/videostream.c
    lib/client.c
    lib/host.c
    lib/g2d.c
    lib/v4l2.c
    lib/v4l2.h
    ${CODEC_SRCS}
)

target_include_directories(videostream
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ext/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/ext/stb/include
)

target_link_libraries(videostream 
    PUBLIC 
        ${CMAKE_DL_LIBS} 
        Threads::Threads
    PRIVATE 
        ${ENCODER_LIBS}
)

# Link rt library on Linux (POSIX realtime extensions)
if(UNIX AND NOT APPLE)
    target_link_libraries(videostream PUBLIC rt)
endif()

# Set compile definitions
if (BUILD_SHARED_LIBS)
    target_compile_definitions(videostream PRIVATE VSL_API_EXPORT)
else()
    target_compile_definitions(videostream PUBLIC VSL_API_STATIC)
endif()

if(ENABLE_G2D)
    target_compile_definitions(videostream PRIVATE ENABLE_G2D)
endif()

if(ENABLE_DMABUF)
    target_compile_definitions(videostream PRIVATE ENABLE_DMABUF)
endif()

if(ENABLE_V4L2_CODEC)
    target_compile_definitions(videostream PRIVATE ENABLE_V4L2_CODEC)
endif()

if(ENABLE_HANTRO_CODEC AND ENABLE_VPU)
    target_compile_definitions(videostream PRIVATE ENABLE_HANTRO_CODEC)
endif()

# Set library version
set_target_properties(videostream PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${SOVERSION}
)

# Install public header
install(FILES include/videostream.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT development
)

# Install pkg-config file
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/videostream.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    COMPONENT development
)

# CMake package configuration location
if (APPLE)
    set(ConfigPackageLocation Frameworks/VideoStream.framework/Resources/CMake/VideoStream)
else()
    set(ConfigPackageLocation ${CMAKE_INSTALL_LIBDIR}/cmake/VideoStream)
endif()

# Generate and install CMake package config for find_package() support
if (BUILD_SHARED_LIBS)
    install(EXPORT VideoStreamConfig
        FILE VideoStreamConfig.cmake
        NAMESPACE VideoStream::
        COMPONENT development
        DESTINATION ${ConfigPackageLocation}
    )
    
    # Generate and install package version file
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/VideoStreamConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/VideoStreamConfigVersion.cmake"
        DESTINATION ${ConfigPackageLocation}
        COMPONENT development
    )
endif()

# Install license and documentation
install(FILES LICENSE NOTICE
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    COMPONENT development
)

# Install user manual if available and requested
if(INSTALL_MANUAL)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/doc/VideoStream.pdf")
        install(FILES doc/VideoStream.pdf
            DESTINATION ${CMAKE_INSTALL_DOCDIR}
            COMPONENT documentation
        )
    else()
        message(WARNING "INSTALL_MANUAL=ON but doc/VideoStream.pdf not found. Run 'make -C doc latexpdf' first.")
    endif()
    
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/doc/html")
        install(DIRECTORY doc/html
            DESTINATION ${CMAKE_INSTALL_DOCDIR}
            COMPONENT documentation
        )
    else()
        message(WARNING "INSTALL_MANUAL=ON but doc/html not found. Run 'make -C doc html' first.")
    endif()
endif()

# GStreamer plugins and tools
if(ENABLE_GSTREAMER)
    find_package(PkgConfig QUIET)
    if (NOT PKG_CONFIG_FOUND)
        message(WARNING "pkg-config not found, GStreamer plugin will not be built")
    endif()
endif()

if (ENABLE_GSTREAMER AND PKG_CONFIG_FOUND)
    if (WIN32)
        set(GSTREAMER_ROOT "C:/Program Files/GStreamer/1.0/x86_64" CACHE PATH "GStreamer Installation Path")
        set(ENV{PKG_CONFIG_PATH} "${GSTREAMER_ROOT}/lib/pkgconfig")
    endif()

    pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET
        gstreamer-1.0>=1.4
        gstreamer-video-1.0
        gstreamer-app-1.0
        gstreamer-allocators-1.0
        glib-2.0
    )

    # GStreamer plugin library
    add_library(gstvideostream SHARED
        gst/plugin.c
        gst/vslsink.h 
        gst/vslsink.c
        gst/vslsrc.h 
        gst/vslsrc.c
    )
    target_link_libraries(gstvideostream 
        PRIVATE 
            videostream
            PkgConfig::GSTREAMER
    )

    # Frame lock demo tool
    add_executable(vsl-framelock src/framelock_demo.c)
    target_link_libraries(vsl-framelock 
        PRIVATE 
            stb 
            videostream
            PkgConfig::GSTREAMER
    )

    # Install GStreamer plugin
    install(TARGETS gstvideostream
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/gstreamer-1.0
                COMPONENT gstreamer
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT gstreamer
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                COMPONENT gstreamer
        FRAMEWORK DESTINATION Frameworks
                COMPONENT gstreamer
    )
endif()

# Install videostream library with proper components
install(TARGETS videostream
    EXPORT VideoStreamConfig
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT runtime
            NAMELINK_COMPONENT development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT development
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT runtime
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FRAMEWORK DESTINATION Frameworks
)

# Add subdirectories
add_subdirectory(src)

# Install Python package
install(DIRECTORY videostream
    COMPONENT python
    DESTINATION ${PYTHON_INSTALL_DIR}
)

# Rust CLI binary (videostream)
# Note: The Rust CLI is built by cargo during the build process (see debian/rules and build.yml)
# This install command picks up the pre-built binary for packaging
if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/usr/bin/videostream")
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/usr/bin/videostream
        COMPONENT runtime
        DESTINATION bin
    )
endif()

# CPack configuration for relocatable archives
string(TOLOWER ${CMAKE_SYSTEM_NAME} SYSTEM_NAME)

set(CPACK_GENERATOR ZIP)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT ${EMAIL})
set(CPACK_PACKAGE_VENDOR ${COMPANY})
set(CPACK_PACKAGE_FILE_NAME ${PACKAGE_NAME}-${SYSTEM_NAME}-${TARGET_ARCH})

# For relocatable archives, users can:
# 1. Build: cmake -DCMAKE_INSTALL_PREFIX=/opt/videostream ..
# 2. Package: cpack
# 3. Extract anywhere and use with LD_LIBRARY_PATH or RPATH
# For system install: cmake -DCMAKE_INSTALL_PREFIX=/usr ..

include(CPack)
