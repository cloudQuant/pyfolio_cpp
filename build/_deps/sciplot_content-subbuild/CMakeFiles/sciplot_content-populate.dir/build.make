# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.29

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /opt/homebrew/Cellar/cmake/3.29.5/bin/cmake

# The command to remove a file.
RM = /opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild

# Utility rule file for sciplot_content-populate.

# Include any custom commands dependencies for this target.
include CMakeFiles/sciplot_content-populate.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/sciplot_content-populate.dir/progress.make

CMakeFiles/sciplot_content-populate: CMakeFiles/sciplot_content-populate-complete

CMakeFiles/sciplot_content-populate-complete: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-install
CMakeFiles/sciplot_content-populate-complete: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-mkdir
CMakeFiles/sciplot_content-populate-complete: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-download
CMakeFiles/sciplot_content-populate-complete: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-update
CMakeFiles/sciplot_content-populate-complete: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-patch
CMakeFiles/sciplot_content-populate-complete: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-configure
CMakeFiles/sciplot_content-populate-complete: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-build
CMakeFiles/sciplot_content-populate-complete: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-install
CMakeFiles/sciplot_content-populate-complete: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-test
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Completed 'sciplot_content-populate'"
	/opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -E make_directory /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/CMakeFiles
	/opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -E touch /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/CMakeFiles/sciplot_content-populate-complete
	/opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -E touch /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-done

sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-update:
.PHONY : sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-update

sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-build: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-configure
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "No build step for 'sciplot_content-populate'"
	cd /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-build && /opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -E echo_append
	cd /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-build && /opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -E touch /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-build

sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-configure: sciplot_content-populate-prefix/tmp/sciplot_content-populate-cfgcmd.txt
sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-configure: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-patch
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "No configure step for 'sciplot_content-populate'"
	cd /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-build && /opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -E echo_append
	cd /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-build && /opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -E touch /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-configure

sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-download: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-gitinfo.txt
sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-download: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-mkdir
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Performing download step (git clone) for 'sciplot_content-populate'"
	cd /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps && /opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -P /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/sciplot_content-populate-prefix/tmp/sciplot_content-populate-gitclone.cmake
	cd /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps && /opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -E touch /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-download

sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-install: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-build
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "No install step for 'sciplot_content-populate'"
	cd /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-build && /opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -E echo_append
	cd /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-build && /opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -E touch /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-install

sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-mkdir:
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Creating directories for 'sciplot_content-populate'"
	/opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -Dcfgdir= -P /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/sciplot_content-populate-prefix/tmp/sciplot_content-populate-mkdirs.cmake
	/opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -E touch /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-mkdir

sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-patch: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-patch-info.txt
sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-patch: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-update
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "No patch step for 'sciplot_content-populate'"
	/opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -E echo_append
	/opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -E touch /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-patch

sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-update:
.PHONY : sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-update

sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-test: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-install
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/CMakeFiles --progress-num=$(CMAKE_PROGRESS_8) "No test step for 'sciplot_content-populate'"
	cd /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-build && /opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -E echo_append
	cd /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-build && /opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -E touch /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-test

sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-update: sciplot_content-populate-prefix/tmp/sciplot_content-populate-gitupdate.cmake
sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-update: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-update-info.txt
sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-update: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-download
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/CMakeFiles --progress-num=$(CMAKE_PROGRESS_9) "Performing update step for 'sciplot_content-populate'"
	cd /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-src && /opt/homebrew/Cellar/cmake/3.29.5/bin/cmake -Dcan_fetch=YES -P /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/sciplot_content-populate-prefix/tmp/sciplot_content-populate-gitupdate.cmake

sciplot_content-populate: CMakeFiles/sciplot_content-populate
sciplot_content-populate: CMakeFiles/sciplot_content-populate-complete
sciplot_content-populate: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-build
sciplot_content-populate: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-configure
sciplot_content-populate: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-download
sciplot_content-populate: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-install
sciplot_content-populate: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-mkdir
sciplot_content-populate: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-patch
sciplot_content-populate: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-test
sciplot_content-populate: sciplot_content-populate-prefix/src/sciplot_content-populate-stamp/sciplot_content-populate-update
sciplot_content-populate: CMakeFiles/sciplot_content-populate.dir/build.make
.PHONY : sciplot_content-populate

# Rule to build all files generated by this target.
CMakeFiles/sciplot_content-populate.dir/build: sciplot_content-populate
.PHONY : CMakeFiles/sciplot_content-populate.dir/build

CMakeFiles/sciplot_content-populate.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/sciplot_content-populate.dir/cmake_clean.cmake
.PHONY : CMakeFiles/sciplot_content-populate.dir/clean

CMakeFiles/sciplot_content-populate.dir/depend:
	cd /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild /Users/yunjinqi/Documents/pyfolio_cpp/build/_deps/sciplot_content-subbuild/CMakeFiles/sciplot_content-populate.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : CMakeFiles/sciplot_content-populate.dir/depend

