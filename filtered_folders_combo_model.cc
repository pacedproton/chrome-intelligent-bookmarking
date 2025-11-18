// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// FilteredFoldersComboModel implementation.
//
// This file implements an enhanced bookmark folder selection model with:
// - Multi-criteria search (name, path, tags, descriptions)
// - Intelligent ranking with 11-tier scoring system
// - Tag-based organization and filtering
// - Smart folder recommendations (frequently/recently used)
// - Rich metadata (descriptions, access tracking, timestamps)
//
// Performance characteristics:
// - Search filtering: O(n) where n = number of folders
// - Sorting: O(n log n) using stable_sort
// - Memory: O(n) for metadata storage using flat_map for cache locality
//
// Thread safety: Not thread-safe. Must be used on UI thread only.

#include "chrome/browser/ui/bookmarks/filtered_folders_combo_model.h"

#include <algorithm>
#include <cctype>
#include <ranges>

#include "base/check.h"
#include "base/check_op.h"
#include "base/i18n/string_search.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"  // nogncheck (kept if needed by build deps)
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "ui/base/l10n/l10n_util.h"  // nogncheck

FilteredFoldersComboModel::FilteredFoldersComboModel(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node)
    : underlying_model_(
          std::make_unique<RecentlyUsedFoldersComboModel>(model, node)),
      bookmark_model_(model) {
  DCHECK(model) << "BookmarkModel must not be null";
  underlying_model_->AddObserver(static_cast<ui::ComboboxModelObserver*>(this));
  UpdateFilteredIndices();
  selected_index_ = GetDefaultIndex();
}

FilteredFoldersComboModel::~FilteredFoldersComboModel() {
  if (underlying_model_) {
    underlying_model_->RemoveObserver(
        static_cast<ui::ComboboxModelObserver*>(this));
  }
}

void FilteredFoldersComboModel::SetSearchFilter(
    std::u16string_view search_text) {
  if (search_filter_ != search_text) {
    search_filter_ = std::u16string(search_text);
    lower_filter_ = base::ToLowerASCII(search_filter_);

    // Store the currently selected item text before filtering
    std::optional<std::u16string> selected_text;
    if (selected_index_.has_value() &&
        selected_index_.value() < filtered_entries_.size()) {
      const Entry& entry = filtered_entries_[selected_index_.value()];
      if (entry.kind == EntryKind::kUnderlying &&
          entry.underlying_index < underlying_model_->GetItemCount() &&
          !underlying_model_->IsItemSeparatorAt(entry.underlying_index) &&
          !underlying_model_->IsItemTitleAt(entry.underlying_index)) {
        selected_text = underlying_model_->GetItemAt(entry.underlying_index);
      }
    }

    // Clear selected index before updating to avoid bad access
    selected_index_ = std::nullopt;

    UpdateFilteredIndices();

    // Try to restore the selection to the same item if it's still visible
    if (selected_text.has_value()) {
      selected_index_ = std::nullopt;  // Reset first
      for (size_t i = 0; i < filtered_entries_.size(); ++i) {
        const Entry& entry = filtered_entries_[i];
        if (entry.kind != EntryKind::kUnderlying) {
          continue;
        }
        if (!underlying_model_->IsItemSeparatorAt(entry.underlying_index) &&
            !underlying_model_->IsItemTitleAt(entry.underlying_index) &&
            underlying_model_->GetItemAt(entry.underlying_index) ==
                selected_text.value()) {
          selected_index_ = i;
          break;
        }
      }
    }

    // If we couldn't find the selected item, use the default index
    if (!selected_index_.has_value() ||
        (selected_index_.has_value() &&
         selected_index_.value() >= filtered_entries_.size())) {
      selected_index_ = GetDefaultIndex();
    }

    for (ui::ComboboxModelObserver& observer : observers()) {
      observer.OnComboboxModelChanged(this);
    }
  }
}

size_t FilteredFoldersComboModel::GetUnderlyingIndex(
    size_t filtered_index) const {
  CHECK_LT(filtered_index, filtered_entries_.size())
      << "Filtered index out of range: " << filtered_index
      << " >= " << filtered_entries_.size();
  DCHECK_EQ(filtered_entries_[filtered_index].kind, EntryKind::kUnderlying)
      << "Filtered index does not refer to an underlying item";
  return filtered_entries_[filtered_index].underlying_index;
}

size_t FilteredFoldersComboModel::GetItemCount() const {
  if (!underlying_model_) {
    return 0u;
  }
  return filtered_entries_.size();
}

std::u16string FilteredFoldersComboModel::GetItemAt(size_t index) const {
  if (!underlying_model_) {
    return std::u16string();
  }
  const Entry& entry = filtered_entries_[index];
  if (entry.kind != EntryKind::kUnderlying) {
    return std::u16string();
  }
  const size_t underlying_index = GetUnderlyingIndex(index);
  return underlying_model_->GetItemAt(underlying_index);
}

std::u16string FilteredFoldersComboModel::GetDropDownSecondaryTextAt(
    size_t index) const {
  if (!underlying_model_) {
    return std::u16string();
  }
  // Only show secondary text for real folder items.
  if (IsItemSeparatorAt(index) || IsItemTitleAt(index)) {
    return std::u16string();
  }
  const size_t underlying_index = GetUnderlyingIndex(index);
  const bookmarks::BookmarkNode* node =
      underlying_model_->GetNodeAt(underlying_index);
  if (!node) {
    return std::u16string();
  }
  // If the folder has a parent (nested), show its path as secondary text.
  const std::u16string full_path = GetFullPath(node);
  if (full_path.empty() ||
      full_path == underlying_model_->GetItemAt(underlying_index)) {
    return std::u16string();
  }
  return full_path;
}

bool FilteredFoldersComboModel::IsItemSeparatorAt(size_t index) const {
  if (!underlying_model_) {
    return false;
  }
  if (suggestions_end_index_.has_value() && index == *suggestions_end_index_) {
    return true;
  }
  const Entry& entry = filtered_entries_[index];
  if (entry.kind == EntryKind::kSeparator) {
    return true;
  }
  if (entry.kind != EntryKind::kUnderlying) {
    return false;
  }
  return underlying_model_->IsItemSeparatorAt(entry.underlying_index);
}

bool FilteredFoldersComboModel::IsItemTitleAt(size_t index) const {
  if (!underlying_model_) {
    return false;
  }
  const Entry& entry = filtered_entries_[index];
  if (entry.kind != EntryKind::kUnderlying) {
    return false;
  }
  return underlying_model_->IsItemTitleAt(entry.underlying_index);
}

std::optional<size_t> FilteredFoldersComboModel::GetDefaultIndex() const {
  if (!underlying_model_) {
    return std::nullopt;
  }
  // First try to find the underlying model's default index in our filtered list
  auto default_index = underlying_model_->GetDefaultIndex();
  if (default_index.has_value()) {
    for (size_t i = 0; i < filtered_entries_.size(); ++i) {
      const Entry& e = filtered_entries_[i];
      if (e.kind == EntryKind::kUnderlying &&
          e.underlying_index == *default_index) {
        return i;
      }
    }
  }

  // If the default is not in the filtered list, return the first
  // non-separator/non-title item
  for (size_t i = 0; i < filtered_entries_.size(); ++i) {
    if (!IsItemSeparatorAt(i) && !IsItemTitleAt(i)) {
      return i;
    }
  }

  // If no valid items, return nullopt
  return std::nullopt;
}

std::optional<ui::ColorId>
FilteredFoldersComboModel::GetDropdownForegroundColorIdAt(size_t index) const {
  if (!underlying_model_) {
    return std::nullopt;
  }
  const Entry& entry = filtered_entries_[index];
  if (entry.kind != EntryKind::kUnderlying) {
    return std::nullopt;
  }
  return underlying_model_->GetDropdownForegroundColorIdAt(
      entry.underlying_index);
}

ui::ComboboxModel::ItemCheckmarkConfig
FilteredFoldersComboModel::GetCheckmarkConfig() const {
  if (!underlying_model_) {
    return {};
  }
  return underlying_model_->GetCheckmarkConfig();
}

void FilteredFoldersComboModel::OnComboboxModelChanged(
    ui::ComboboxModel* model) {
  // Underlying model changed (e.g., nodes added/removed). Preserve the
  // current search filter and recompute the filtered view.
  selected_index_ = std::nullopt;
  node_id_to_full_path_cache_.clear();
  UpdateFilteredIndices();
  selected_index_ = GetDefaultIndex();
  for (ui::ComboboxModelObserver& observer : observers()) {
    observer.OnComboboxModelChanged(this);
  }
}

void FilteredFoldersComboModel::OnComboboxModelDestroying(
    ui::ComboboxModel* model) {
  // Underlying model is being destroyed; clear pointer to avoid use-after-free
  underlying_model_.reset();
}

void FilteredFoldersComboModel::MaybeChangeParent(
    const bookmarks::BookmarkNode* node,
    size_t selected_index) {
  if (!underlying_model_) {
    return;
  }
  if (selected_index >= filtered_entries_.size() ||
      filtered_entries_[selected_index].kind != EntryKind::kUnderlying) {
    return;
  }
  const bookmarks::BookmarkNode* target_node =
      underlying_model_->GetNodeAt(GetUnderlyingIndex(selected_index));
  if (target_node) {
    RecordFolderAccess(target_node);
  }
  underlying_model_->MaybeChangeParent(node,
                                       GetUnderlyingIndex(selected_index));
}

// Tag management implementation
void FilteredFoldersComboModel::AddTagToFolder(
    const bookmarks::BookmarkNode* node,
    std::u16string_view tag) {
  if (!node || tag.empty()) {
    DCHECK(node) << "Cannot add tag to null node";
    DCHECK(!tag.empty()) << "Cannot add empty tag";
    return;
  }

  DCHECK(node->is_folder()) << "Tags can only be added to folder nodes";

  auto& metadata = GetOrCreateMetadata(node);
  std::u16string tag_str(tag);

  // Check if tag already exists to avoid duplicates
  if (std::ranges::find(metadata.tags, tag_str) == metadata.tags.end()) {
    metadata.tags.push_back(tag_str);
  }
}

void FilteredFoldersComboModel::RemoveTagFromFolder(
    const bookmarks::BookmarkNode* node,
    std::u16string_view tag) {
  if (!node) {
    return;
  }

  const int64_t node_id = node->id();
  auto it = node_metadata_.find(node_id);
  if (it != node_metadata_.end()) {
    auto& tags = it->second.tags;
    std::erase_if(tags,
                  [tag](const std::u16string& t) { return t == tag; });
  }
}

std::vector<std::u16string> FilteredFoldersComboModel::GetTagsForFolder(
    const bookmarks::BookmarkNode* node) const {
  if (!node) {
    return {};
  }

  const int64_t node_id = node->id();
  auto it = node_metadata_.find(node_id);
  if (it != node_metadata_.end()) {
    return it->second.tags;
  }
  return {};
}

std::vector<std::u16string> FilteredFoldersComboModel::GetAllTags() const {
  std::unordered_set<std::u16string> unique_tags;
  for (const auto& [node_id, metadata] : node_metadata_) {
    for (const auto& tag : metadata.tags) {
      unique_tags.insert(tag);
    }
  }
  return std::vector<std::u16string>(unique_tags.begin(), unique_tags.end());
}

// Description management
void FilteredFoldersComboModel::SetFolderDescription(
    const bookmarks::BookmarkNode* node,
    std::u16string_view description) {
  if (!node) {
    return;
  }

  auto& metadata = GetOrCreateMetadata(node);
  metadata.description = std::u16string(description);
}

std::u16string FilteredFoldersComboModel::GetFolderDescription(
    const bookmarks::BookmarkNode* node) const {
  if (!node) {
    return std::u16string();
  }

  const int64_t node_id = node->id();
  auto it = node_metadata_.find(node_id);
  if (it != node_metadata_.end()) {
    return it->second.description;
  }
  return std::u16string();
}

// Metadata management
const BookmarkMetadata* FilteredFoldersComboModel::GetMetadata(
    const bookmarks::BookmarkNode* node) const {
  if (!node) {
    return nullptr;
  }

  const int64_t node_id = node->id();
  auto it = node_metadata_.find(node_id);
  if (it != node_metadata_.end()) {
    return &it->second;
  }
  return nullptr;
}

void FilteredFoldersComboModel::RecordFolderAccess(
    const bookmarks::BookmarkNode* node) {
  if (!node) {
    return;
  }

  auto& metadata = GetOrCreateMetadata(node);
  metadata.access_count++;
  metadata.last_accessed = base::Time::Now();
}

// Smart folder detection
std::vector<const bookmarks::BookmarkNode*>
FilteredFoldersComboModel::GetFrequentlyUsedFolders(size_t max_count) const {
  std::vector<std::pair<const bookmarks::BookmarkNode*, int>> folder_counts;

  for (const auto& [node_id, metadata] : node_metadata_) {
    if (metadata.access_count > 0 && bookmark_model_) {
      const bookmarks::BookmarkNode* node =
          bookmarks::GetBookmarkNodeByID(bookmark_model_, node_id);
      if (node && node->is_folder()) {
        folder_counts.emplace_back(node, metadata.access_count);
      }
    }
  }

  // Sort by access count (descending)
  std::ranges::sort(folder_counts, [](const auto& a, const auto& b) {
    return a.second > b.second;
  });

  std::vector<const bookmarks::BookmarkNode*> result;
  const size_t count = std::min(max_count, folder_counts.size());
  for (size_t i = 0; i < count; ++i) {
    result.push_back(folder_counts[i].first);
  }

  return result;
}

std::vector<const bookmarks::BookmarkNode*>
FilteredFoldersComboModel::GetRecentlyUsedFolders(size_t max_count) const {
  std::vector<std::pair<const bookmarks::BookmarkNode*, base::Time>>
      folder_times;

  for (const auto& [node_id, metadata] : node_metadata_) {
    if (!metadata.last_accessed.is_null() && bookmark_model_) {
      const bookmarks::BookmarkNode* node =
          bookmarks::GetBookmarkNodeByID(bookmark_model_, node_id);
      if (node && node->is_folder()) {
        folder_times.emplace_back(node, metadata.last_accessed);
      }
    }
  }

  // Sort by last accessed time (most recent first)
  std::ranges::sort(folder_times, [](const auto& a, const auto& b) {
    return a.second > b.second;
  });

  std::vector<const bookmarks::BookmarkNode*> result;
  const size_t count = std::min(max_count, folder_times.size());
  for (size_t i = 0; i < count; ++i) {
    result.push_back(folder_times[i].first);
  }

  return result;
}

// Tag filtering
void FilteredFoldersComboModel::SetTagFilter(
    const std::vector<std::u16string>& tags) {
  tag_filters_ = tags;
  UpdateFilteredIndices();
  for (ui::ComboboxModelObserver& observer : observers()) {
    observer.OnComboboxModelChanged(this);
  }
}

void FilteredFoldersComboModel::ClearTagFilter() {
  tag_filters_.clear();
  UpdateFilteredIndices();
  for (ui::ComboboxModelObserver& observer : observers()) {
    observer.OnComboboxModelChanged(this);
  }
}

void FilteredFoldersComboModel::UpdateFilteredIndices() {
  // Rebuilds the filtered entries list based on current search filter and
  // tag filters. This method implements the core filtering and ranking logic.
  //
  // Algorithm overview:
  // 1. Fast path: If no filters are active, passthrough all items
  // 2. Score all items using GetMatchScore()
  // 3. Sort by score (highest first) while maintaining stable order
  // 4. Separate into "suggestions" (top N) and "other matches"
  // 5. Rebuild filtered_entries_ with suggestions, separator, other matches
  //
  // Performance: O(n log n) where n = number of folders
  // - O(n) for scoring each item
  // - O(n log n) for stable sort
  // - O(n) for rebuilding filtered_entries_
  //
  // Memory: O(n) temporary vector for matched indices

  filtered_entries_.clear();
  if (!underlying_model_) {
    return;
  }

  const size_t item_count = underlying_model_->GetItemCount();

  // Reset suggestions boundary marker
  suggestions_end_index_.reset();

  // Fast path: with an empty filter and no tag filters, preserve the underlying
  // ordering including titles and any separators so headers render correctly.
  if (search_filter_.empty() && tag_filters_.empty()) {
    for (size_t i = 0; i < item_count; ++i) {
      filtered_entries_.push_back({EntryKind::kUnderlying, i});
    }
    return;
  }
  // Gather matched underlying indices excluding titles, separators, and the
  // trailing "Choose another folder" action. We will construct the filtered
  // list deterministically from these.
  std::vector<size_t> matched_indices;
  size_t choose_another_index = std::string::npos;
  for (size_t i = 0; i < item_count; ++i) {
    if (underlying_model_->IsItemSeparatorAt(i)) {
      continue;
    }
    if (i == item_count - 1) {
      // Sentinel action is always last in the underlying model.
      choose_another_index = i;
      continue;
    }
    if (underlying_model_->IsItemTitleAt(i)) {
      continue;
    }

    // Apply tag filter if active
    if (!tag_filters_.empty()) {
      const bookmarks::BookmarkNode* node = underlying_model_->GetNodeAt(i);
      if (!node || !MatchesTagFilter(node)) {
        continue;
      }
    }

    const int score = GetMatchScore(i);
    if (search_filter_.empty() || score > 0) {
      matched_indices.push_back(i);
    }
  }

  // Stable-score-sort when filtering so top matches surface first; compute
  // all scores once to avoid re-scoring within comparator.
  if (!search_filter_.empty()) {
    std::vector<int> scores(matched_indices.size());
    for (size_t i = 0; i < matched_indices.size(); ++i) {
      scores[i] = GetMatchScore(matched_indices[i]);
    }
    std::stable_sort(matched_indices.begin(), matched_indices.end(),
                     [&](size_t a, size_t b) {
                       size_t ai = 0;
                       size_t bi = 0;
                       // Find positions of a and b in matched_indices.
                       for (size_t i = 0; i < matched_indices.size(); ++i) {
                         if (matched_indices[i] == a) {
                           ai = i;
                         }
                         if (matched_indices[i] == b) {
                           bi = i;
                         }
                       }
                       return scores[ai] > scores[bi];
                     });
  }

  // Build suggestions: first up to kMaxSuggestions entries, then a boundary.
  const size_t kMaxSuggestions = 3;
  if (!search_filter_.empty()) {
    if (!matched_indices.empty()) {
      const size_t num_suggestions =
          std::min(kMaxSuggestions, matched_indices.size());
      for (size_t si = 0; si < num_suggestions; ++si) {
        filtered_entries_.push_back(
            {EntryKind::kUnderlying, matched_indices[si]});
      }
    } else {
      // Fallback: if nothing matched MRU, surface the first few underlying
      // real folder items as suggestions for a consistent UI shape.
      size_t added = 0;
      for (size_t i = 0; i < item_count && added < kMaxSuggestions; ++i) {
        if (i == item_count - 1) {
          continue;  // skip sentinel
        }
        if (underlying_model_->IsItemTitleAt(i) ||
            underlying_model_->IsItemSeparatorAt(i)) {
          continue;
        }
        filtered_entries_.push_back({EntryKind::kUnderlying, i});
        ++added;
      }
    }
    // Mark a virtual boundary (no extra item).
    suggestions_end_index_ = filtered_entries_.size();
  }

  // Add the remaining matched items (excluding those already added as
  // suggestions).
  size_t start_rest = (!search_filter_.empty())
                          ? std::min(kMaxSuggestions, matched_indices.size())
                          : 0u;
  for (size_t ri = start_rest; ri < matched_indices.size(); ++ri) {
    filtered_entries_.push_back({EntryKind::kUnderlying, matched_indices[ri]});
  }

  // Always keep "Choose Another Folder" at the end if it exists
  if (choose_another_index != std::string::npos) {
    filtered_entries_.push_back({EntryKind::kUnderlying, choose_another_index});
  }

  // Ensure we always have at least one item (even if it's just a separator)
  if (filtered_entries_.empty() && item_count > 0) {
    filtered_entries_.push_back({EntryKind::kUnderlying, 0});
  }
}

bool FilteredFoldersComboModel::MatchesFilter(
    std::u16string_view text) const {
  if (search_filter_.empty()) {
    return true;
  }

  size_t match_index = 0;
  size_t match_length = 0;
  return base::i18n::StringSearchIgnoringCaseAndAccents(
      search_filter_, std::u16string(text), &match_index, &match_length);
}

bool FilteredFoldersComboModel::MatchesFilterWithContext(
    size_t underlying_index) const {
  if (search_filter_.empty()) {
    return true;
  }

  // Use the scoring system - anything with a score > 0 matches
  int score = GetMatchScore(underlying_index);
  return score > 0;
}

bool FilteredFoldersComboModel::FuzzyMatchesFilter(
    std::u16string_view text) const {
  if (search_filter_.empty()) {
    return true;
  }

  // Convert both strings to lowercase for case-insensitive fuzzy matching
  const std::u16string lower_text = base::ToLowerASCII(text);
  const std::u16string lower_filter = base::ToLowerASCII(search_filter_);

  // Check if all characters in the filter appear in order in the text
  size_t text_pos = 0;
  for (const char16_t filter_char : lower_filter) {
    // Skip spaces in the filter
    if (filter_char == u' ') {
      continue;
    }

    // Find the next occurrence of this character
    const size_t found = lower_text.find(filter_char, text_pos);
    if (found == std::u16string::npos) {
      return false;
    }
    text_pos = found + 1;
  }

  return true;
}

std::u16string FilteredFoldersComboModel::GetFullPath(
    const bookmarks::BookmarkNode* node) const {
  if (!node) {
    return u"";
  }

  std::vector<std::u16string> path_parts;
  const bookmarks::BookmarkNode* current = node;

  // Walk up the tree collecting folder names
  while (current && !current->is_root()) {
    if (current->is_folder()) {
      std::u16string name = current->GetTitle();
      if (!name.empty()) {
        path_parts.push_back(name);
      }
    }
    current = current->parent();
  }

  // Reverse to get top-down order
  std::reverse(path_parts.begin(), path_parts.end());

  // Join with " > " separator and cache by node id
  const int64_t node_id = node->id();
  std::u16string joined = base::JoinString(path_parts, u" > ");
  node_id_to_full_path_cache_.emplace(node_id, joined);
  return joined;
}

bool FilteredFoldersComboModel::MatchesTagFilter(
    const bookmarks::BookmarkNode* node) const {
  if (!node || tag_filters_.empty()) {
    return true;
  }

  const int64_t node_id = node->id();
  const auto it = node_metadata_.find(node_id);
  if (it == node_metadata_.end()) {
    return false;  // No metadata means no tags, doesn't match tag filter
  }

  const auto& node_tags = it->second.tags;
  // Check if node has any of the filtered tags
  return std::ranges::any_of(
      tag_filters_, [&node_tags](const std::u16string& filter_tag) {
        return std::ranges::find(node_tags, filter_tag) != node_tags.end();
      });
}

BookmarkMetadata& FilteredFoldersComboModel::GetOrCreateMetadata(
    const bookmarks::BookmarkNode* node) {
  DCHECK(node) << "Cannot get metadata for null node";

  const int64_t node_id = node->id();
  auto it = node_metadata_.find(node_id);

  // Create new metadata entry if it doesn't exist.
  // Uses flat_map for better cache locality than unordered_map.
  if (it == node_metadata_.end()) {
    BookmarkMetadata metadata;
    metadata.created = base::Time::Now();
    // emplace returns pair<iterator, bool>, we want the iterator
    it = node_metadata_.emplace(node_id, std::move(metadata)).first;
  }

  return it->second;
}

int FilteredFoldersComboModel::GetMatchScore(size_t underlying_index) const {
  // Fast path: no filter means no scoring needed
  if (search_filter_.empty()) {
    return 0;
  }

  // Null check for underlying model (defensive programming)
  if (!underlying_model_) {
    return -1;
  }

  const bookmarks::BookmarkNode* node =
      underlying_model_->GetNodeAt(underlying_index);
  if (!node) {
    return -1;
  }

  // Pre-compute lowercased strings for fast case-insensitive comparison.
  // This avoids repeated ToLowerASCII calls in the checks below.
  const std::u16string folder_name =
      underlying_model_->GetItemAt(underlying_index);
  const std::u16string full_path = GetFullPath(node);
  const std::u16string lower_name = base::ToLowerASCII(folder_name);
  const std::u16string lower_path = base::ToLowerASCII(full_path);

  // Enhanced multi-tier scoring system (0-100 points).
  // Higher scores = better matches, shown first in filtered results.
  //
  // Scoring tiers are ordered from highest to lowest priority:
  // 100 points - Exact match of folder name (most specific)
  //  95 points - Exact tag match (high relevance)
  //  90 points - Folder name starts with filter (prefix match)
  //  85 points - Description exact match
  //  80 points - Folder name contains filter as substring (partial match)
  //  75 points - Tag substring match
  //  70 points - Path contains filter as substring (context match)
  //  65 points - Description substring match
  //  60 points - Fuzzy match on folder name (typo tolerance)
  //  55 points - Fuzzy tag match
  //  50 points - Parent folder matches (hierarchical context)
  //   0 points - No match (filtered out)
  //
  // Performance note: Checks are ordered from fastest to slowest:
  // exact string comparison > prefix > substring > i18n search > fuzzy

  if (lower_name == lower_filter_) {
    return 100;
  }

  // Check tags for exact match
  const auto* metadata = GetMetadata(node);
  if (metadata) {
    for (const auto& tag : metadata->tags) {
      const std::u16string lower_tag = base::ToLowerASCII(tag);
      if (lower_tag == lower_filter_) {
        return 95;
      }
    }
  }

  if (lower_name.find(lower_filter_) == 0) {
    return 90;
  }

  // Check description for exact match
  if (metadata && !metadata->description.empty()) {
    const std::u16string lower_desc =
        base::ToLowerASCII(metadata->description);
    if (lower_desc == lower_filter_) {
      return 85;
    }
  }

  size_t match_index = 0;
  size_t match_length = 0;
  if (base::i18n::StringSearchIgnoringCaseAndAccents(
          search_filter_, folder_name, &match_index, &match_length)) {
    return 80;
  }

  // Check tags for substring match
  if (metadata) {
    for (const auto& tag : metadata->tags) {
      if (base::i18n::StringSearchIgnoringCaseAndAccents(
              search_filter_, tag, &match_index, &match_length)) {
        return 75;
      }
    }
  }

  if (lower_path.find(lower_filter_) != std::u16string::npos) {
    return 70;
  }

  // Check description for substring match
  if (metadata && !metadata->description.empty()) {
    if (base::i18n::StringSearchIgnoringCaseAndAccents(
            search_filter_, metadata->description, &match_index,
            &match_length)) {
      return 65;
    }
  }

  if (FuzzyMatchesFilter(folder_name)) {
    return 60;
  }

  // Check tags for fuzzy match
  if (metadata) {
    for (const auto& tag : metadata->tags) {
      if (FuzzyMatchesFilter(tag)) {
        return 55;
      }
    }
  }

  // Check parent match
  const bookmarks::BookmarkNode* parent = node->parent();
  if (parent && MatchesFilter(parent->GetTitle())) {
    return 50;
  }

  return 0;
}

std::u16string FilteredFoldersComboModel::GetEnhancedItemAt(
    size_t index) const {
  if (!underlying_model_) {
    return std::u16string();
  }
  size_t underlying_index = GetUnderlyingIndex(index);
  const bookmarks::BookmarkNode* node =
      underlying_model_->GetNodeAt(underlying_index);

  if (!node || IsItemSeparatorAt(index) || IsItemTitleAt(index)) {
    return GetItemAt(index);
  }

  // For nested folders, show the path
  std::u16string full_path = GetFullPath(node);
  if (full_path.find(u" > ") != std::u16string::npos) {
    return full_path;
  }

  return GetItemAt(index);
}
