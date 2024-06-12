# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-src"
  "/Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-build"
  "/Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/sciplot_content-populate-prefix"
  "/Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/sciplot_content-populate-prefix/tmp"
  "/Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/sciplot_content-populate-prefix/src/sciplot_content-populate-stamp"
  "/Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/sciplot_content-populate-prefix/src"
  "/Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/sciplot_content-populate-prefix/src/sciplot_content-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/sciplot_content-populate-prefix/src/sciplot_content-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
