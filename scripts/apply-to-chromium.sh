#!/bin/bash
# Smart Bookmark Folder Search - One-click Chromium Integration
# Usage: ./apply-to-chromium.sh /path/to/chromium/src

set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 /path/to/chromium/src"
    exit 1
fi

CHROMIUM_SRC="$1"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Validate Chromium source directory
if [ ! -f "$CHROMIUM_SRC/chrome/browser/ui/bookmarks/BUILD.gn" ]; then
    echo "❌ Error: Invalid Chromium source directory"
    echo "   Expected: $CHROMIUM_SRC/chrome/browser/ui/bookmarks/BUILD.gn"
    exit 1
fi

echo "🔍 Smart Bookmark Folder Search Integration"
echo "   Source: $PROJECT_ROOT"
echo "   Target: $CHROMIUM_SRC"
echo ""

# Copy source files
echo "📁 Copying source files..."
cp "$PROJECT_ROOT/src/filtered_folders_combo_model.cc" \
   "$CHROMIUM_SRC/chrome/browser/ui/bookmarks/"
cp "$PROJECT_ROOT/src/filtered_folders_combo_model.h" \
   "$CHROMIUM_SRC/chrome/browser/ui/bookmarks/"

# Check if BUILD.gn needs updating
BUILD_FILE="$CHROMIUM_SRC/chrome/browser/ui/bookmarks/BUILD.gn"
if grep -q "filtered_folders_combo_model.cc" "$BUILD_FILE"; then
    echo "✅ BUILD.gn already configured"
else
    echo "🔧 Updating BUILD.gn..."
    # Create backup
    cp "$BUILD_FILE" "$BUILD_FILE.backup"
    
    # Add our files to the sources list
    sed -i.tmp '/sources = \[/a\
    "filtered_folders_combo_model.cc",\
    "filtered_folders_combo_model.h",' "$BUILD_FILE"
    
    rm "$BUILD_FILE.tmp"
    echo "✅ BUILD.gn updated (backup saved as BUILD.gn.backup)"
fi

echo ""
echo "🎉 Integration complete!"
echo ""
echo "Next steps:"
echo "1. cd $CHROMIUM_SRC"
echo "2. autoninja -C out/Default chrome"
echo "3. Test the bookmark dialog with search functionality"
echo ""
echo "To rollback:"
echo "- Remove the copied files"
echo "- Restore BUILD.gn.backup if needed"