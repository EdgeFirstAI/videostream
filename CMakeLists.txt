cmake_minimum_required(VERSION 3.10)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(COMPANY "Au-Zone Technologies")
set(EMAIL "support@au-zone.com")
set(COPYRIGHT "Copyright Ⓒ 2025 Au-Zone Technologies. All Rights Reserved.")
set(SOVERSION "1")

project(VideoStream C)

if (WIN32 OR APPLE)
    set(PACKAGE_NAME "VideoStream")
else()
    set(PACKAGE_NAME "videostream")
endif()

# Parse version from videostream.h (single source of truth)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/videostream.h" HEADER_CONTENT)
string(REGEX MATCH "#define VSL_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)\"" _ ${HEADER_CONTENT})
set(PROJECT_VERSION ${CMAKE_MATCH_1})

# Extract components
string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ ${PROJECT_VERSION})
set(VERSION_MAJOR ${CMAKE_MATCH_1})
set(VERSION_MINOR ${CMAKE_MATCH_2})
set(VERSION_PATCH ${CMAKE_MATCH_3})

message(STATUS "VideoStream version: ${PROJECT_VERSION}")

option(ENABLE_OPENMP "Enables OpenMP threading acceleration" ON)
option(ENABLE_GSTREAMER "Enable building the VideoStream GStreamer plugin" ON)
option(INSTALL_MANUAL "Install User Manual" OFF)
option(ENABLE_COVER "Enables code coverage for the library" OFF)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

if (UNIX AND NOT APPLE)
    option(ENABLE_G2D "Enables G2D for frame conversion acceleration" ON)
    option(ENABLE_DMABUF "Enable Linux dmabuf support" ON)
    option(ENABLE_VPU "Enables VPU for frame encoding acceleration" ON)
endif()

if (ENABLE_OPENMP)
    find_package(OpenMP)
    message("OpenMP CFLAGS ${OpenMP_C_FLAGS} LDFLAGS ${OpenMP_EXE_LINKER_FLAGS}")

    if (OpenMP_C_FLAGS)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    endif()

    if (OpenMP_EXE_LINKER_FLAGS)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    endif()
endif()

include(TargetArch)
include(CTest)
include(GenerateExportHeader)
include(GNUInstallDirs)
include(CMakeToolsHelpers OPTIONAL)

# Configure install directories
set(PYTHON_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/python3/dist-packages)
set(SAMPLES_INSTALL_DIR ${CMAKE_INSTALL_DATADIR}/videostream/samples)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INCLUDE_DIRECTORIES_BEFORE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_SKIP_BUILD_RPATH TRUE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

if (UNIX AND NOT APPLE)
    set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib")
endif()

target_architecture(TARGET_ARCH)

if (WIN32 OR APPLE)
    set(PROJECT_NAME "VideoStream")
else()
    set(PROJECT_NAME "videostream")
endif()

message("${PROJECT_NAME} ${PROJECT_VERSION}")
message("    -- libraries arch ${CMAKE_LIBRARY_ARCHITECTURE}")
message("    -- libraries path ${CMAKE_INSTALL_LIBDIR}")
message("    -- libraries path ${CMAKE_INSTALL_FULL_LIBDIR}")

# Generate version.h for legacy compatibility (use VSL_VERSION from header instead)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/version.h
     "#define VIDEOSTREAM_VERSION \"${PROJECT_VERSION}\"\n"
     "#define VIDEOSTREAM_VERSION_MAJOR ${VERSION_MAJOR}\n"
     "#define VIDEOSTREAM_VERSION_MINOR ${VERSION_MINOR}\n"
     "#define VIDEOSTREAM_VERSION_PATCH ${VERSION_PATCH}\n")

# Configure pkg-config file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/videostream.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/videostream.pc
    @ONLY)

if(ENABLE_G2D)
     add_definitions(-DENABLE_G2D)
endif(ENABLE_G2D)

if (ENABLE_DMABUF)
    add_definitions(-DENABLE_DMABUF)
endif(ENABLE_DMABUF)

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
    if(CMAKE_C_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    else()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
    endif()

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /we4013 /we6246")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4204 /wd4152")

    if (ENABLE_OPENMP)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /openmp")
    endif()
elseif(CMAKE_COMPILER_IS_GNUCC)
    # Maximum stack allowed before a warning is emitted, only supports GCC currently.
    set(MAX_STACK 4096 CACHE STRING "Maximum stack usage allowed by the library")

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-usage -Wstack-usage=${MAX_STACK}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdump-rtl-expand")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=implicit-function-declaration")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror=shadow")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-multichar")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -fno-inline -fno-omit-frame-pointer -g3")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")

    if (CMAKE_SYSTEM_NAME STREQUAL "Generic")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map")
    endif()

    if (ENABLE_COVER)
        message("Enabling code coverage reporting")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    endif()
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
endif()

find_package(Threads REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(include)
include_directories(ext/include)

include_directories(ext/stb/include)
add_subdirectory(ext/stb)

if(ENABLE_VPU)
    add_subdirectory(ext/vpu_wrapper)
    set(ENCODER_SRCS
        lib/decoder_hantro.c
        lib/decoder_hantro.h
        lib/encoder_hantro.c
        lib/encoder_hantro.h)
    set(ENCODER_LIBS vpu_wrapper)
endif(ENABLE_VPU)

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
            ${ENCODER_SRCS})
target_link_libraries(videostream PUBLIC ${CMAKE_DL_LIBS} Threads::Threads rt PRIVATE ${ENCODER_LIBS})

if (BUILD_SHARED_LIBS)
    set_property(TARGET videostream
                 APPEND_STRING PROPERTY
                 COMPILE_DEFINITIONS "VSL_API_EXPORT")
else()
    set_property(TARGET videostream
                 APPEND_STRING PROPERTY
                 COMPILE_DEFINITIONS "VSL_API_STATIC")
endif()

# Set up CMake package location
if (APPLE)
    set(ConfigPackageLocation Frameworks/VideoStream.framework/Resources/CMake/VideoStream)
else()
    set(ConfigPackageLocation ${CMAKE_INSTALL_LIBDIR}/cmake/VideoStream)
endif()

# Install public header
install(FILES include/videostream.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT development)

# Install pkg-config file
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/videostream.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
        COMPONENT development)

# Install CMake package config for find_package() support
if (BUILD_SHARED_LIBS)
    install(EXPORT VideoStreamConfig
            FILE VideoStreamConfig.cmake
            NAMESPACE VideoStream::
            COMPONENT development
            DESTINATION ${ConfigPackageLocation})
    
    # Generate and install package version file
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/VideoStreamConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)
    
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/VideoStreamConfigVersion.cmake"
            DESTINATION ${ConfigPackageLocation}
            COMPONENT development)
endif()

# Install license and documentation
install(FILES LICENSE NOTICE
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        COMPONENT development)

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
    if (WIN32)
        set(GSTREAMER_ROOT "C:/Program Files/GStreamer/1.0/x86_64" CACHE PATH "GStreamer Installation Path")
        set(ENV{PKG_CONFIG_PATH} "${GSTREAMER_ROOT}/lib/pkgconfig")
    endif()

    if (APPLE)
        # Link keg-only brew paths on MacOS
        set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/opt/libffi/lib/pkgconfig")
        link_directories("/usr/local/opt/gettext/lib")
    endif()

    pkg_check_modules(GSTREAMER REQUIRED
        gstreamer-1.0>=1.4
        gstreamer-video-1.0
        gstreamer-app-1.0
        gstreamer-allocators-1.0
        glib-2.0)
    link_directories(${GSTREAMER_LIBRARY_DIRS})

    add_library(gstvideostream SHARED
        gst/plugin.c
        gst/vslsink.h gst/vslsink.c
        gst/vslsrc.h gst/vslsrc.c)
    target_link_libraries(gstvideostream videostream ${GSTREAMER_LIBRARIES})
    target_include_directories(gstvideostream PRIVATE ${GSTREAMER_INCLUDE_DIRS})

    add_executable(vsl-framelock src/framelock_demo.c)
    target_link_libraries(vsl-framelock stb videostream ${GSTREAMER_LIBRARIES})
    target_include_directories(vsl-framelock PRIVATE ${GSTREAMER_INCLUDE_DIRS})

    install(TARGETS gstvideostream
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/gstreamer-1.0
                    COMPONENT gstreamer
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                    COMPONENT gstreamer
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                    COMPONENT gstreamer
            FRAMEWORK DESTINATION Frameworks
                    COMPONENT gstreamer)
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
        FRAMEWORK DESTINATION Frameworks)

add_subdirectory(src)

install(DIRECTORY deepview COMPONENT python DESTINATION ${PYTHON_INSTALL_DIR})

string(TOLOWER ${CMAKE_SYSTEM_NAME} SYSTEM_NAME)

# CPack configuration for relocatable archives
# When CMAKE_INSTALL_PREFIX is not set to system paths (e.g., /usr or /usr/local),
# CPack creates a relocatable archive that can be extracted anywhere
set(CPACK_GENERATOR ZIP)
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})
set(CPACK_PACKAGE_CONTACT ${EMAIL})
set(CPACK_PACKAGE_VENDOR ${COMPANY})
set(CPACK_PACKAGE_FILE_NAME ${PACKAGE_NAME}-${SYSTEM_NAME}-${TARGET_ARCH})

# For relocatable archives, users can:
# 1. Build: cmake -DCMAKE_INSTALL_PREFIX=/opt/videostream ..
# 2. Package: cpack
# 3. Extract anywhere and use with LD_LIBRARY_PATH or RPATH
# For system install: cmake -DCMAKE_INSTALL_PREFIX=/usr ..

include(CPack)
