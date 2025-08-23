# 🔧 Integration Guide

## Quick Start (30 seconds)

```bash
# 1. Clone this repo
git clone https://github.com/yourusername/smart-bookmark-folder-search.git
cd smart-bookmark-folder-search

# 2. Run the integration script
./scripts/apply-to-chromium.sh /path/to/your/chromium/src

# 3. Build and test
cd /path/to/your/chromium/src
autoninja -C out/Default chrome
```

## Manual Integration

### Step 1: Copy Files
```bash
cp src/filtered_folders_combo_model.* \
   /path/to/chromium/src/chrome/browser/ui/bookmarks/
```

### Step 2: Update BUILD.gn
Add to `chrome/browser/ui/bookmarks/BUILD.gn`:
```gn
sources += [
  "filtered_folders_combo_model.cc",
  "filtered_folders_combo_model.h",
]
```

### Step 3: Integration Points
The `FilteredFoldersComboModel` wraps the existing `RecentlyUsedFoldersComboModel`. Update your bookmark dialog to use:

```cpp
#include "chrome/browser/ui/bookmarks/filtered_folders_combo_model.h"

// Replace RecentlyUsedFoldersComboModel with FilteredFoldersComboModel
auto model = std::make_unique<FilteredFoldersComboModel>(
    bookmark_model, node_being_edited);

// Add search functionality
model->SetSearchFilter(user_search_text);
```

## Testing

```bash
# Run bookmark-related tests
out/Default/unit_tests --gtest_filter="*BookmarkFolder*"
out/Default/browser_tests --gtest_filter="*Bookmark*Dialog*"
```

## Rollback

Simply remove the files and revert BUILD.gn changes - completely non-destructive integration.