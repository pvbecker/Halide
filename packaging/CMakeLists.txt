include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

##
# Custom locations
##

set(HALIDE_INSTALL_CMAKEDIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/Halide"
    CACHE STRING "Path to Halide cmake files")

##
# Main library exports
##

# TODO(#4053): add autoschedulers when refactored

# Sends Debug -> Debug, Release/MinSizeRel/RelWithDebInfo -> Release
set(CONFIG_DIR $<IF:$<CONFIG:Debug>,Debug,Release>)

install(TARGETS Halide Halide_Generator Halide_RunGenMain
        EXPORT Halide_Targets

        RUNTIME
        DESTINATION ${CMAKE_INSTALL_BINDIR}/${CONFIG_DIR}
        COMPONENT Halide_Runtime

        LIBRARY
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/${CONFIG_DIR}
        COMPONENT Halide_Runtime
        NAMELINK_COMPONENT Halide_Development

        ARCHIVE
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/${CONFIG_DIR}
        COMPONENT Halide_Development

        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(TARGETS Halide_LLVM
        EXPORT Halide_Targets)

install(TARGETS Halide_Tools Halide_ImageIO Halide_LanguageOptions
        EXPORT Halide_Interfaces
        INCLUDES DESTINATION ${CMAKE_INSTALL_DATADIR}/tools)

install(TARGETS Halide_Runtime
        EXPORT Halide_Interfaces
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Captures both the runtime and Halide.h
install(DIRECTORY ${Halide_BINARY_DIR}/include/ TYPE INCLUDE FILES_MATCHING PATTERN "include/*.h")

##
# READMEs and other top-level documentation
##

install(FILES
        ${Halide_SOURCE_DIR}/README_cmake.md
        ${Halide_SOURCE_DIR}/README.md
        ${Halide_SOURCE_DIR}/README_rungen.md
        ${Halide_SOURCE_DIR}/README_webassembly.md
        TYPE DATA)

##
# Tools
##

install(DIRECTORY ${Halide_SOURCE_DIR}/tools
        TYPE DATA
        FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.cpp"
        PATTERN "*.m"
        PATTERN "binary2cpp.cpp" EXCLUDE
        PATTERN "build_halide_h.cpp" EXCLUDE
        PATTERN "find_inverse.cpp" EXCLUDE)

##
# Tutorial
##

install(DIRECTORY ${Halide_SOURCE_DIR}/tutorial
        TYPE DATA
        FILES_MATCHING
        PATTERN "*.cpp"
        PATTERN "*.h"
        PATTERN "lesson_*.sh"
        PATTERN "*.gif"
        PATTERN "*.jpg"
        PATTERN "*.mp4"
        PATTERN "*.png")

##
# CMake scripts
##

if (BUILD_SHARED_LIBS)
    set(LIB_TYPE shared)
else ()
    set(LIB_TYPE static)
endif ()

# If Halide explicitly links against shared LLVM or if it is a static library
# and we are not bundling our static dependencies, then end-users must have
# the relevant system libraries installed.
if (Halide_SHARED_LLVM OR (NOT BUILD_SHARED_LIBS AND NOT Halide_BUNDLE_LLVM))
    set(depFile "${CMAKE_CURRENT_BINARY_DIR}/Halide-Deps-${LIB_TYPE}.cmake")
    file(WRITE "${depFile}" "find_dependency(LLVM CONFIG)\n")

    if (TARGET_WEBASSEMBLY)
        file(APPEND "${depFile}" "find_dependency(LLD CONFIG HINTS \"\${LLVM_DIR}/../lld\")\n")
    endif ()

    install(FILES "${depFile}" DESTINATION ${HALIDE_INSTALL_CMAKEDIR})
endif ()

install(EXPORT Halide_Targets
        DESTINATION ${HALIDE_INSTALL_CMAKEDIR}
        NAMESPACE Halide::${LIB_TYPE}::
        FILE Halide-Targets-${LIB_TYPE}.cmake
        COMPONENT Halide_Development)

install(EXPORT Halide_Interfaces
        DESTINATION ${HALIDE_INSTALL_CMAKEDIR}
        NAMESPACE Halide::
        FILE Halide-Interfaces.cmake
        COMPONENT Halide_Development)

write_basic_package_version_file(
        HalideConfigVersion.cmake
        COMPATIBILITY SameMajorVersion)

install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/HalideConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/HalideConfigVersion.cmake
        ${Halide_SOURCE_DIR}/cmake/HalideGeneratorHelpers.cmake
        DESTINATION ${HALIDE_INSTALL_CMAKEDIR})

##
# Documentation
##

# See /doc/CMakeLists.txt. This way, special install logic isn't needed.

##
# Actual packaging
##

set(CPACK_PACKAGE_VENDOR "Halide")
set(CPACK_RESOURCE_FILE_LICENSE "${Halide_SOURCE_DIR}/LICENSE.txt")
if (WIN32)
    set(CPACK_GENERATOR "ZIP")
else ()
    set(CPACK_GENERATOR "TGZ")
endif ()

include(CPack)
