# FindFFMPEG — AirGlass variant.
#
# Upstream AirPlay-Windows expects vcpkg's FFMPEG module (which builds FFmpeg
# from source, 20-45 min on Windows). AirGlass links a prebuilt shared FFmpeg
# instead (BtbN win64-gpl-shared): same ABI, hardware decoders included
# (d3d11va, dxva2, cuda/nvdec, qsv), and the build stays under a minute.
#
# Provides the same variables upstream consumes:
#   FFMPEG_FOUND, FFMPEG_LIBRARIES (target names), FFMPEG_INCLUDE_DIRS
#
# Usage:
#   cmake -DAIRGLASS_FFMPEG_DIR=C:/project/airglass/vendor/ffmpeg ...

set(_ffmpeg_root "${AIRGLASS_FFMPEG_DIR}")
if(NOT _ffmpeg_root)
    set(_ffmpeg_root "${CMAKE_CURRENT_LIST_DIR}/../vendor/ffmpeg")
endif()

find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h
    HINTS "${_ffmpeg_root}/include"
    NO_DEFAULT_PATH)
find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h)

if(NOT FFMPEG_INCLUDE_DIR)
    set(FFMPEG_FOUND FALSE)
    set(FFMPEG_INCLUDE_DIRS "")
    set(FFMPEG_LIBRARIES "")
    return()
endif()

set(_ffmpeg_libdir "${_ffmpeg_root}/lib")
set(_ffmpeg_bindir "${_ffmpeg_root}/bin")

# Each component becomes an imported SHARED target so the DLLs are copied
# next to the executable by $<TARGET_RUNTIME_DLLS:...>.
foreach(_comp avcodec avformat avutil swscale swresample)
    find_library(FFMPEG_${_comp}_LIBRARY
        NAMES ${_comp}
        HINTS "${_ffmpeg_libdir}"
        NO_DEFAULT_PATH)
    if(NOT FFMPEG_${_comp}_LIBRARY)
        find_library(FFMPEG_${_comp}_LIBRARY NAMES ${_comp})
    endif()
    if(FFMPEG_${_comp}_LIBRARY AND NOT TARGET FFmpeg::${_comp})
        add_library(FFmpeg::${_comp} SHARED IMPORTED)
        set_target_properties(FFmpeg::${_comp} PROPERTIES
            IMPORTED_IMPLIB "${FFMPEG_${_comp}_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIR}")
        # Resolve the DLL from the import library name (avcodec.lib -> avcodec-*.dll).
        file(GLOB _dll "${_ffmpeg_bindir}/${_comp}-*.dll")
        if(_dll)
            list(GET _dll 0 _dll0)
            set_target_properties(FFmpeg::${_comp} PROPERTIES IMPORTED_LOCATION "${_dll0}")
        endif()
    endif()
endforeach()

set(FFMPEG_INCLUDE_DIRS "${FFMPEG_INCLUDE_DIR}")
set(FFMPEG_LIBRARIES
    FFmpeg::avcodec FFmpeg::avformat FFmpeg::avutil FFmpeg::swscale FFmpeg::swresample)

set(FFMPEG_FOUND TRUE)
message(STATUS "FFmpeg (prebuilt): ${FFMPEG_INCLUDE_DIR} from ${_ffmpeg_root}")
