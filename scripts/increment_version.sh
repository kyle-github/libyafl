#!/bin/bash
set -e

VERSION_FILE="VERSION"

if [ ! -f "$VERSION_FILE" ]; then
    echo "Error: $VERSION_FILE not found."
    exit 1
fi

CURRENT_VERSION=$(cat "$VERSION_FILE")

# Split into BASE (Boost version) and RELEASE (N)
BASE_VERSION=${CURRENT_VERSION%-*}
RELEASE_NUM=${CURRENT_VERSION##*-}

# Increment N
NEW_RELEASE_NUM=$((RELEASE_NUM + 1))
NEW_VERSION="${BASE_VERSION}-${NEW_RELEASE_NUM}"

echo "$NEW_VERSION" > "$VERSION_FILE"

echo "Version bumped: $CURRENT_VERSION -> $NEW_VERSION"
echo "::set-output name=version::$NEW_VERSION"