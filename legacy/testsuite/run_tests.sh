#!/bin/bash

GARLIC_LIB_BUILD_DIR_NAME="build-garlic-lib-Qt_5_15_14_System-Debug"

###########################################################################################################
#
# Build and run testsuite
#
###########################################################################################################

mkdir -p build-tests			# create the test dir if not exists
rm -rf build-tests/*			# delete all entries in test dir
cd build-tests					# enter test dir

# create the tests and run
cmake ..
make
ctest -V

###########################################################################################################
#
# Coverage
#
###########################################################################################################


SCRIPT_DIR=$(dirname $0)
LIB_BUILD_PATH="$SCRIPT_DIR/../../src/${GARLIC_LIB_BUILD_DIR_NAME}" # the directory containing the .gcno files (%{buildDir} in Qt Creator)
COVERAGE_PATH="$SCRIPT_DIR/./coverage"				# stores the coverage results
HTML_RESULTS="${COVERAGE_PATH}""/html"   				# stores the HTML files


mkdir -p ${HTML_RESULTS}						# Create the coverage and html folder if they does not exists

# Generate our initial info
lcov --directory "${LIB_BUILD_PATH}" --capture --output-file "${COVERAGE_PATH}/coverage.info" --rc lcov_branch_coverage=1

# Remove paths/files not required for code coverage (e.g. 3rd party libs)
lcov --ignore-errors unused,unused \
 --remove "${COVERAGE_PATH}/coverage.info" "*Qt*.framework*" "*.h" "*/tests/*" "*Xcode.app*" "*.moc" "*moc_*.cpp" "*/test/*" "*/build*/*" "/usr/include/*" "ext/*" \
 --output-file "${COVERAGE_PATH}/coverage-filtered.info"

genhtml -o "${HTML_RESULTS}" "${COVERAGE_PATH}/coverage-filtered.info"	# Generate the HTML files
lcov --directory "${COVERAGE_PATH}" --zerocounters    					# Reset our counts
firefox "${HTML_RESULTS}/index.html" 								# Open the index.html


exit 0

