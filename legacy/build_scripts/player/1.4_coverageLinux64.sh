#!/bin/bash
# based on the work of https://github.com/kelvins/CodeCoverageExampleQt

SCRIPTDIR=$(dirname "$0")
source $SCRIPTDIR/envLinux64.sh

# we need to step out from shadow build dir otherwise coverage failed
cd ..

# SRC_DIR is the directory containing the .gcno files (%{buildDir} in Qt Creator)
SRC_DIR="$SHADOW_BUILD_DIR/$TEST_DIR/garlic-lib/"

# COV_DIR is the directory where the coverage results will be stored
#COV_DIR="$SHADOW_BUILD_DIR/coverage"
COV_DIR="coverage"

# Path where the HTML files should be saved
HTML_RESULTS="${COV_DIR}""/html"

# Create the html folder if it does not exists
mkdir -p ${HTML_RESULTS}

# Generate our initial info
lcov --no-recursion -d "${SRC_DIR}" -c -o "${COV_DIR}/coverage.info"
 
# Remove some paths/files which we don't want to calculate the code coverage (e.g. third party libraries) and generate a new coverage file filtered (feel free to edit it when necessary)
lcov -r "${COV_DIR}/coverage.info" "*c++/8*" "*c++/7*" "*c++/6*" "*Qt*.framework*" "*.h" "*/tests/*" "*Xcode.app*" "*.moc" "*moc_*.cpp" "*/test/*" "*/build*/*" -o "${COV_DIR}/coverage-filtered.info"
 
# Generate the HTML files
genhtml -o "${HTML_RESULTS}" "${COV_DIR}//coverage-filtered.info"
 
# Reset our counts
lcov -d "${COV_DIR}" -z

# set xml needed for jenkins cobertura 
gcovr -r $GARLIC_DIR/src/garlic-lib -e '../../../garlic-player/src/ext' -e '../../../Qt'  --xml -o $COV_DIR/cobertura.xml $SRC_DIR
#gcovr -r $GARLIC_DIR/src/garlic-lib -e '/(.*).h/' -e '../../../garlic-player/src/ext' -e '../../../Qt'  --html -o $COV_DIR/cobertura.html $SRC_DIR

exit 0
