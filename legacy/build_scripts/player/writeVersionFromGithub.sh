#!/bin/bash

echo 
echo ========== detect version information from github
echo
# check if called from jenkins
if [ -z "$BUILD_NUMBER" ]; then
	export GIT_DIR=$GARLIC_DIR
else
	export GIT_DIR=$WORKSPACE/
fi
COMMIT_NUMBER=`git --git-dir="$GIT_DIR.git" rev-list --all --count`
VERSION_NAME=`git --git-dir="$GIT_DIR.git" describe --tags $(git --git-dir="$GIT_DIR.git" rev-list --tags --max-count=1)`

export GARLIC_VERSION=${VERSION_NAME%%-*}.$COMMIT_NUMBER

echo 
echo ========== write header file and version information to Androidmanifest
echo 

echo "#define version_from_git \"$GARLIC_VERSION\"" > $GIT_DIR/src/garlic-lib/version.h

xmlstarlet edit --inplace --update "/manifest/@android:versionName" --value $GARLIC_VERSION $GIT_DIR/src/player-c2qml/android/AndroidManifest.xml
xmlstarlet edit --inplace --update "/manifest/@android:versionCode" --value $COMMIT_NUMBER $GIT_DIR/src/player-c2qml/android/AndroidManifest.xml
