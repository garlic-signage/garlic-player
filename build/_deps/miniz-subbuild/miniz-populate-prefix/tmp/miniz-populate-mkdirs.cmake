# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/niko/Source/github/garlic-player/build/_deps/miniz-src")
  file(MAKE_DIRECTORY "/home/niko/Source/github/garlic-player/build/_deps/miniz-src")
endif()
file(MAKE_DIRECTORY
  "/home/niko/Source/github/garlic-player/build/_deps/miniz-build"
  "/home/niko/Source/github/garlic-player/build/_deps/miniz-subbuild/miniz-populate-prefix"
  "/home/niko/Source/github/garlic-player/build/_deps/miniz-subbuild/miniz-populate-prefix/tmp"
  "/home/niko/Source/github/garlic-player/build/_deps/miniz-subbuild/miniz-populate-prefix/src/miniz-populate-stamp"
  "/home/niko/Source/github/garlic-player/build/_deps/miniz-subbuild/miniz-populate-prefix/src"
  "/home/niko/Source/github/garlic-player/build/_deps/miniz-subbuild/miniz-populate-prefix/src/miniz-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/niko/Source/github/garlic-player/build/_deps/miniz-subbuild/miniz-populate-prefix/src/miniz-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/niko/Source/github/garlic-player/build/_deps/miniz-subbuild/miniz-populate-prefix/src/miniz-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
