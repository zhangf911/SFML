# This CMake script will try to download and build all external dependencies.
# 
# Additional required tools:
# 
# Git, SVN, NASM

cmake_minimum_required(VERSION 2.8)
include(ExternalProject)

# Tool dependencies
# find_package(Git REQUIRED)
# find_package(Subversion REQUIRED)
# find_program(NASM_PATH nasm)

# setup base properties
set_property(DIRECTORY PROPERTY EP_BASE "${CMAKE_CURRENT_BINARY_DIR}")

# libfreetype
ExternalProject_Add(freetype
                    GIT_REPOSITORY http://git.sv.nongnu.org/r/freetype/freetype2.git
                    CMAKE_CACHE_ARGS
                        "-DCMAKE_BUILD_TYPE:STRING=Release"
                        "-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/Install/freetype"
                    )

# libjpeg(-turbo)
ExternalProject_Add(jpeg
                    SVN_REPOSITORY svn://svn.code.sf.net/p/libjpeg-turbo/code/trunk
                    CMAKE_CACHE_ARGS
                        "-DCMAKE_BUILD_TYPE:STRING=Release"
                        "-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/Install/jpeg"
                    )

# libopenal-soft
ExternalProject_Add(openal
                    GIT_REPOSITORY git://repo.or.cz/openal-soft.git
                    CMAKE_CACHE_ARGS
                        "-DCMAKE_BUILD_TYPE:STRING=Release"
                        "-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/Install/openal"
                    )
