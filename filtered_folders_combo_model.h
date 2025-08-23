// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_FILTERED_FOLDERS_COMBO_MODEL_H_
#define CHROME_BROWSER_UI_BOOKMARKS_FILTERED_FOLDERS_COMBO_MODEL_H_

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/bookmarks/recently_used_folders_combo_model.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/combobox_model_observer.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

// A wrapper around RecentlyUsedFoldersComboModel that adds filtering capability
// based on search text.
class FilteredFoldersComboModel : public ui::ComboboxModel,
                                  public ui::ComboboxModelObserver {
 public:
  FilteredFoldersComboModel(bookmarks::BookmarkModel* model,
                            const bookmarks::BookmarkNode* node);

  FilteredFoldersComboModel(const FilteredFoldersComboModel&) = delete;
  FilteredFoldersComboModel& operator=(const FilteredFoldersComboModel&) =
      delete;

  ~FilteredFoldersComboModel() override;

  // Sets the search filter. Empty string shows all folders.
  void SetSearchFilter(const std::u16string& search_text);

  // Returns the underlying model's index for the given filtered index.
  // Only valid if the filtered index refers to an underlying item.
  size_t GetUnderlyingIndex(size_t filtered_index) const;

  // Overridden from ui::ComboboxModel:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  std::u16string GetDropDownSecondaryTextAt(size_t index) const override;
  bool IsItemSeparatorAt(size_t index) const override;
  bool IsItemTitleAt(size_t index) const override;
  std::optional<size_t> GetDefaultIndex() const override;
  std::optional<ui::ColorId> GetDropdownForegroundColorIdAt(
      size_t index) const override;
  ui::ComboboxModel::ItemCheckmarkConfig GetCheckmarkConfig() const override;

  // Get the currently selected index
  std::optional<size_t> GetSelectedIndex() const { return selected_index_; }

  // Get enhanced display text for items (with path for nested folders)
  std::u16string GetEnhancedItemAt(size_t index) const;

  // Overridden from ui::ComboboxModelObserver:
  void OnComboboxModelChanged(ui::ComboboxModel* model) override;
  void OnComboboxModelDestroying(ui::ComboboxModel* model) override;

  // Proxy method to underlying model
  void MaybeChangeParent(const bookmarks::BookmarkNode* node,
                         size_t selected_index);

  // Test-only helpers: compute suggestions size by scanning from start until
  // the first separator.
  size_t GetSuggestionsEndForTesting() const {
    if (suggestions_end_index_.has_value()) {
      return *suggestions_end_index_;
    }
    // Fallback: no virtual boundary recorded. Infer as up to first separator,
    // or cap at 3 non-title entries from the start.
    size_t sep = 0;
    for (; sep < filtered_entries_.size(); ++sep) {
      if (filtered_entries_[sep].kind == EntryKind::kSeparator) {
        break;
      }
    }
    if (sep < filtered_entries_.size()) {
      return sep;
    }
    // Cap at 3 entries if no separator is present.
    size_t count = 0;
    for (size_t i = 0; i < filtered_entries_.size() && count < 3; ++i) {
      if (filtered_entries_[i].kind == EntryKind::kUnderlying) {
        ++count;
      }
    }
    return count;
  }

 private:
  void UpdateFilteredIndices();
  bool MatchesFilter(const std::u16string& text) const;
  bool MatchesFilterWithContext(size_t underlying_index) const;
  bool FuzzyMatchesFilter(const std::u16string& text) const;
  std::u16string GetFullPath(const bookmarks::BookmarkNode* node) const;
  int GetMatchScore(size_t underlying_index) const;
  bool HasSuggestionsBoundary() const {
    return suggestions_end_index_.has_value();
  }
  size_t SuggestionsEnd() const { return suggestions_end_index_.value_or(0); }

  std::unique_ptr<RecentlyUsedFoldersComboModel> underlying_model_;
  std::u16string search_filter_;
  // Lowercased version of the current filter for cheap comparisons
  std::u16string lower_filter_;

  enum class EntryKind { kUnderlying, kTitle, kSeparator };
  struct Entry {
    EntryKind kind;
    size_t underlying_index;  // Only valid for kUnderlying
  };

  // Filtered entries including pseudo items (title/separator) and underlying
  // items
  std::vector<Entry> filtered_entries_;

  // Track the currently selected index in the filtered view
  std::optional<size_t> selected_index_;

  // Cache for full folder paths by BookmarkNode id()
  mutable std::unordered_map<long long, std::u16string>
      node_id_to_full_path_cache_;

  std::optional<size_t> suggestions_end_index_;

  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_FILTERED_FOLDERS_COMBO_MODEL_H_
