// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_FILTERED_FOLDERS_COMBO_MODEL_H_
#define CHROME_BROWSER_UI_BOOKMARKS_FILTERED_FOLDERS_COMBO_MODEL_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/bookmarks/recently_used_folders_combo_model.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/combobox_model_observer.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

// Enhanced bookmark metadata for richer bookmark management.
//
// This structure stores additional information about bookmark folders beyond
// what's available in the core BookmarkNode. All metadata is volatile and
// stored in memory only - it's not persisted to disk in this implementation.
//
// Thread safety: This struct is not thread-safe. Access must be synchronized
// by the owning FilteredFoldersComboModel.
struct BookmarkMetadata {
  // User-defined tags for categorization and filtering.
  std::vector<std::u16string> tags;

  // Optional description providing context about the folder's purpose.
  std::u16string description;

  // Number of times this folder has been accessed/selected.
  // Used for "frequently used" recommendations.
  int access_count = 0;

  // Timestamp of the most recent access.
  // Used for "recently used" recommendations.
  base::Time last_accessed;

  // Timestamp when this metadata entry was created.
  base::Time created;

  BookmarkMetadata() = default;
  BookmarkMetadata(const BookmarkMetadata&) = default;
  BookmarkMetadata& operator=(const BookmarkMetadata&) = default;
  BookmarkMetadata(BookmarkMetadata&&) noexcept = default;
  BookmarkMetadata& operator=(BookmarkMetadata&&) noexcept = default;
  ~BookmarkMetadata() = default;
};

// A wrapper around RecentlyUsedFoldersComboModel that adds advanced filtering,
// tagging, and smart folder capabilities based on search text and metadata.
//
// This model provides:
// - Real-time search filtering with intelligent ranking (11-tier scoring)
// - Tag-based organization and filtering
// - Smart folders based on usage patterns
// - Enhanced metadata (descriptions, access counts, timestamps)
// - Multi-criteria search (name, path, tags, descriptions)
// - Unicode and i18n support for all languages
// - Accessibility support (keyboard navigation, screen readers)
//
// Usage example:
//   auto model = std::make_unique<FilteredFoldersComboModel>(
//       bookmark_model, current_node);
//
//   // Add tags for organization
//   model->AddTagToFolder(work_folder, u"important");
//   model->AddTagToFolder(work_folder, u"urgent");
//
//   // Set description
//   model->SetFolderDescription(work_folder, u"Client work and projects");
//
//   // Search across all metadata
//   model->SetSearchFilter(u"client");  // Matches name, path, tags, desc
//
//   // Get smart recommendations
//   auto frequent = model->GetFrequentlyUsedFolders(5);
//
// Thread safety: Not thread-safe. Must be accessed from UI thread only.
//
// Performance: Optimized for collections of 1000+ folders with O(n log n)
// filtering and O(1) metadata lookups using flat_map.
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
  // Supports searching by name, path, tags, and descriptions.
  void SetSearchFilter(std::u16string_view search_text);

  // Tag management methods
  // Adds a tag to the specified folder. Does nothing if node is null or tag
  // is empty. Duplicate tags are automatically ignored.
  void AddTagToFolder(const bookmarks::BookmarkNode* node,
                      std::u16string_view tag);

  // Removes a tag from the specified folder. Does nothing if node is null
  // or the tag doesn't exist.
  void RemoveTagFromFolder(const bookmarks::BookmarkNode* node,
                           std::u16string_view tag);

  // Returns all tags associated with the specified folder.
  // Returns an empty vector if node is null or has no tags.
  [[nodiscard]] std::vector<std::u16string> GetTagsForFolder(
      const bookmarks::BookmarkNode* node) const;

  // Returns all unique tags across all folders.
  // Useful for tag autocomplete and suggestion UIs.
  [[nodiscard]] std::vector<std::u16string> GetAllTags() const;

  // Description management
  // Sets a description for the specified folder. Does nothing if node is null.
  // Passing an empty string clears the description.
  void SetFolderDescription(const bookmarks::BookmarkNode* node,
                            std::u16string_view description);

  // Returns the description for the specified folder.
  // Returns an empty string if node is null or has no description.
  [[nodiscard]] std::u16string GetFolderDescription(
      const bookmarks::BookmarkNode* node) const;

  // Metadata management
  // Returns metadata for the specified folder, or nullptr if node is null
  // or has no metadata. The returned pointer is valid until the next
  // modification to this model or the underlying bookmark model.
  [[nodiscard]] const BookmarkMetadata* GetMetadata(
      const bookmarks::BookmarkNode* node) const;

  // Records an access to the specified folder, incrementing its access count
  // and updating its last accessed timestamp. Used for smart recommendations.
  void RecordFolderAccess(const bookmarks::BookmarkNode* node);

  // Smart folder detection - identifies frequently used folders
  // Returns up to max_count folders sorted by access count (descending).
  // Only includes folders with at least one access.
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  GetFrequentlyUsedFolders(size_t max_count = 5) const;

  // Returns up to max_count folders sorted by last access time (most recent
  // first). Only includes folders that have been accessed at least once.
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  GetRecentlyUsedFolders(size_t max_count = 5) const;

  // Filter by tags
  // Filters the folder list to only show folders with any of the specified
  // tags. Notifies observers of the change.
  void SetTagFilter(const std::vector<std::u16string>& tags);

  // Clears all tag filters, showing all folders again.
  // Notifies observers of the change.
  void ClearTagFilter();

  // Returns the underlying model's index for the given filtered index.
  // Only valid if the filtered index refers to an underlying item.
  // DCHECK fails in debug builds if filtered_index is out of range or
  // doesn't refer to an underlying item.
  [[nodiscard]] size_t GetUnderlyingIndex(size_t filtered_index) const;

  // Overridden from ui::ComboboxModel:
  [[nodiscard]] size_t GetItemCount() const override;
  [[nodiscard]] std::u16string GetItemAt(size_t index) const override;
  [[nodiscard]] std::u16string GetDropDownSecondaryTextAt(
      size_t index) const override;
  [[nodiscard]] bool IsItemSeparatorAt(size_t index) const override;
  [[nodiscard]] bool IsItemTitleAt(size_t index) const override;
  [[nodiscard]] std::optional<size_t> GetDefaultIndex() const override;
  [[nodiscard]] std::optional<ui::ColorId> GetDropdownForegroundColorIdAt(
      size_t index) const override;
  [[nodiscard]] ui::ComboboxModel::ItemCheckmarkConfig GetCheckmarkConfig()
      const override;

  // Get the currently selected index in the filtered view.
  // Returns nullopt if no item is selected.
  [[nodiscard]] std::optional<size_t> GetSelectedIndex() const {
    return selected_index_;
  }

  // Get enhanced display text for items (with full path for nested folders).
  // Returns the full hierarchical path for nested folders, or just the
  // folder name for top-level folders.
  [[nodiscard]] std::u16string GetEnhancedItemAt(size_t index) const;

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
  bool MatchesFilter(std::u16string_view text) const;
  bool MatchesFilterWithContext(size_t underlying_index) const;
  bool FuzzyMatchesFilter(std::u16string_view text) const;
  bool MatchesTagFilter(const bookmarks::BookmarkNode* node) const;
  std::u16string GetFullPath(const bookmarks::BookmarkNode* node) const;
  int GetMatchScore(size_t underlying_index) const;
  bool HasSuggestionsBoundary() const {
    return suggestions_end_index_.has_value();
  }
  size_t SuggestionsEnd() const { return suggestions_end_index_.value_or(0); }

  // Helper to get or create metadata for a node
  BookmarkMetadata& GetOrCreateMetadata(const bookmarks::BookmarkNode* node);

  std::unique_ptr<RecentlyUsedFoldersComboModel> underlying_model_;
  std::u16string search_filter_;
  // Lowercased version of the current filter for cheap comparisons
  std::u16string lower_filter_;

  // Active tag filters
  std::vector<std::u16string> tag_filters_;

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
  mutable std::unordered_map<int64_t, std::u16string>
      node_id_to_full_path_cache_;

  std::optional<size_t> suggestions_end_index_;

  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;

  // Metadata storage: maps node ID to enhanced bookmark metadata
  // Using flat_map for better cache locality and performance
  base::flat_map<int64_t, BookmarkMetadata> node_metadata_;

  // Weak pointer factory for async operations
  base::WeakPtrFactory<FilteredFoldersComboModel> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_FILTERED_FOLDERS_COMBO_MODEL_H_
