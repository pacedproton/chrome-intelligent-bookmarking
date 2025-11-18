// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// BookmarkManager implementation.
//
// This file implements a comprehensive bookmark management system with:
// - Recently added bookmarks tracking
// - Rich metadata (tags, descriptions, ratings, favorites)
// - Advanced search and filtering
// - Duplicate detection
// - Batch operations
// - Export/import functionality
//
// Performance characteristics:
// - Metadata lookups: O(1) using flat_map
// - Search: O(n) where n = number of bookmarks
// - Sorting: O(n log n) using stable_sort
// - Duplicate detection: O(n) using flat_map for URL grouping
//
// Thread safety: Not thread-safe. Must be used on UI thread only.

#include "chrome/browser/ui/bookmarks/bookmark_manager.h"

#include <algorithm>
#include <ranges>
#include <utility>

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"

BookmarkManager::BookmarkManager(bookmarks::BookmarkModel* model)
    : bookmark_model_(model) {
  DCHECK(model) << "BookmarkModel must not be null";
}

BookmarkManager::~BookmarkManager() = default;

// ===== Recently Added Tracking =====

void BookmarkManager::OnBookmarkAdded(
    const bookmarks::BookmarkNode* bookmark) {
  if (!bookmark || !bookmark->is_url()) {
    return;  // Only track URL bookmarks, not folders
  }

  const int64_t node_id = bookmark->id();

  // Add to front of recently added list
  recently_added_.insert(recently_added_.begin(), node_id);

  // Limit the size to prevent unbounded growth
  if (recently_added_.size() > kMaxRecentBookmarks) {
    recently_added_.resize(kMaxRecentBookmarks);
  }

  // Initialize metadata with current timestamp
  auto& metadata = GetOrCreateMetadata(bookmark);
  metadata.date_added = base::Time::Now();
}

std::vector<const bookmarks::BookmarkNode*>
BookmarkManager::GetRecentlyAddedBookmarks(size_t max_count) const {
  std::vector<const bookmarks::BookmarkNode*> result;
  result.reserve(std::min(max_count, recently_added_.size()));

  for (const int64_t node_id : recently_added_) {
    if (result.size() >= max_count) {
      break;
    }

    const bookmarks::BookmarkNode* node =
        bookmarks::GetBookmarkNodeByID(bookmark_model_, node_id);

    // Node might have been deleted
    if (node && node->is_url()) {
      // Check if archived should be excluded
      const auto it = bookmark_metadata_.find(node_id);
      if (it == bookmark_metadata_.end() || !it->second.is_archived) {
        result.push_back(node);
      }
    }
  }

  return result;
}

std::vector<const bookmarks::BookmarkNode*>
BookmarkManager::GetBookmarksAddedSince(base::Time since) const {
  std::vector<const bookmarks::BookmarkNode*> result;

  for (const auto& [node_id, metadata] : bookmark_metadata_) {
    if (metadata.date_added >= since) {
      const bookmarks::BookmarkNode* node =
          bookmarks::GetBookmarkNodeByID(bookmark_model_, node_id);
      if (node && node->is_url() && !metadata.is_archived) {
        result.push_back(node);
      }
    }
  }

  // Sort by date added (most recent first)
  std::ranges::sort(result, [this](const auto* a, const auto* b) {
    const auto* meta_a = GetBookmarkMetadata(a);
    const auto* meta_b = GetBookmarkMetadata(b);
    if (!meta_a || !meta_b) {
      return false;
    }
    return meta_a->date_added > meta_b->date_added;
  });

  return result;
}

// ===== Metadata Management =====

void BookmarkManager::AddTagToBookmark(
    const bookmarks::BookmarkNode* bookmark,
    std::u16string_view tag) {
  if (!bookmark || !bookmark->is_url() || tag.empty()) {
    DCHECK(bookmark) << "Cannot add tag to null bookmark";
    DCHECK(bookmark->is_url()) << "Tags can only be added to URL bookmarks";
    DCHECK(!tag.empty()) << "Cannot add empty tag";
    return;
  }

  auto& metadata = GetOrCreateMetadata(bookmark);
  const std::u16string tag_str(tag);

  // Avoid duplicates
  if (std::ranges::find(metadata.tags, tag_str) == metadata.tags.end()) {
    metadata.tags.push_back(tag_str);
    metadata.last_modified = base::Time::Now();
  }
}

void BookmarkManager::RemoveTagFromBookmark(
    const bookmarks::BookmarkNode* bookmark,
    std::u16string_view tag) {
  if (!bookmark || !bookmark->is_url()) {
    return;
  }

  const int64_t node_id = bookmark->id();
  auto it = bookmark_metadata_.find(node_id);
  if (it != bookmark_metadata_.end()) {
    auto& tags = it->second.tags;
    std::erase_if(tags, [tag](const std::u16string& t) { return t == tag; });
    it->second.last_modified = base::Time::Now();
  }
}

std::vector<std::u16string> BookmarkManager::GetTagsForBookmark(
    const bookmarks::BookmarkNode* bookmark) const {
  if (!bookmark || !bookmark->is_url()) {
    return {};
  }

  const int64_t node_id = bookmark->id();
  const auto it = bookmark_metadata_.find(node_id);
  if (it != bookmark_metadata_.end()) {
    return it->second.tags;
  }
  return {};
}

std::vector<std::u16string> BookmarkManager::GetAllBookmarkTags() const {
  base::flat_set<std::u16string> unique_tags;
  for (const auto& [node_id, metadata] : bookmark_metadata_) {
    for (const auto& tag : metadata.tags) {
      unique_tags.insert(tag);
    }
  }
  return std::vector<std::u16string>(unique_tags.begin(), unique_tags.end());
}

void BookmarkManager::SetBookmarkDescription(
    const bookmarks::BookmarkNode* bookmark,
    std::u16string_view description) {
  if (!bookmark || !bookmark->is_url()) {
    return;
  }

  auto& metadata = GetOrCreateMetadata(bookmark);
  metadata.description = std::u16string(description);
  metadata.last_modified = base::Time::Now();
}

std::u16string BookmarkManager::GetBookmarkDescription(
    const bookmarks::BookmarkNode* bookmark) const {
  if (!bookmark || !bookmark->is_url()) {
    return std::u16string();
  }

  const int64_t node_id = bookmark->id();
  const auto it = bookmark_metadata_.find(node_id);
  if (it != bookmark_metadata_.end()) {
    return it->second.description;
  }
  return std::u16string();
}

void BookmarkManager::SetBookmarkRating(
    const bookmarks::BookmarkNode* bookmark,
    int rating) {
  if (!bookmark || !bookmark->is_url()) {
    return;
  }

  // Clamp rating to 0-5 range
  rating = std::clamp(rating, 0, 5);

  auto& metadata = GetOrCreateMetadata(bookmark);
  metadata.rating = rating;
  metadata.last_modified = base::Time::Now();
}

int BookmarkManager::GetBookmarkRating(
    const bookmarks::BookmarkNode* bookmark) const {
  if (!bookmark || !bookmark->is_url()) {
    return 0;
  }

  const int64_t node_id = bookmark->id();
  const auto it = bookmark_metadata_.find(node_id);
  if (it != bookmark_metadata_.end()) {
    return it->second.rating;
  }
  return 0;
}

void BookmarkManager::SetBookmarkFavorite(
    const bookmarks::BookmarkNode* bookmark,
    bool is_favorite) {
  if (!bookmark || !bookmark->is_url()) {
    return;
  }

  auto& metadata = GetOrCreateMetadata(bookmark);
  metadata.is_favorite = is_favorite;
  metadata.last_modified = base::Time::Now();
}

bool BookmarkManager::IsBookmarkFavorite(
    const bookmarks::BookmarkNode* bookmark) const {
  if (!bookmark || !bookmark->is_url()) {
    return false;
  }

  const int64_t node_id = bookmark->id();
  const auto it = bookmark_metadata_.find(node_id);
  if (it != bookmark_metadata_.end()) {
    return it->second.is_favorite;
  }
  return false;
}

std::vector<const bookmarks::BookmarkNode*>
BookmarkManager::GetFavoriteBookmarks() const {
  std::vector<const bookmarks::BookmarkNode*> result;

  for (const auto& [node_id, metadata] : bookmark_metadata_) {
    if (metadata.is_favorite && !metadata.is_archived) {
      const bookmarks::BookmarkNode* node =
          bookmarks::GetBookmarkNodeByID(bookmark_model_, node_id);
      if (node && node->is_url()) {
        result.push_back(node);
      }
    }
  }

  return result;
}

void BookmarkManager::SetBookmarkArchived(
    const bookmarks::BookmarkNode* bookmark,
    bool is_archived) {
  if (!bookmark || !bookmark->is_url()) {
    return;
  }

  auto& metadata = GetOrCreateMetadata(bookmark);
  metadata.is_archived = is_archived;
  metadata.last_modified = base::Time::Now();
}

bool BookmarkManager::IsBookmarkArchived(
    const bookmarks::BookmarkNode* bookmark) const {
  if (!bookmark || !bookmark->is_url()) {
    return false;
  }

  const int64_t node_id = bookmark->id();
  const auto it = bookmark_metadata_.find(node_id);
  if (it != bookmark_metadata_.end()) {
    return it->second.is_archived;
  }
  return false;
}

void BookmarkManager::RecordBookmarkAccess(
    const bookmarks::BookmarkNode* bookmark) {
  if (!bookmark || !bookmark->is_url()) {
    return;
  }

  auto& metadata = GetOrCreateMetadata(bookmark);
  metadata.access_count++;
  metadata.last_accessed = base::Time::Now();
}

const EnhancedBookmarkMetadata* BookmarkManager::GetBookmarkMetadata(
    const bookmarks::BookmarkNode* bookmark) const {
  if (!bookmark || !bookmark->is_url()) {
    return nullptr;
  }

  const int64_t node_id = bookmark->id();
  const auto it = bookmark_metadata_.find(node_id);
  if (it != bookmark_metadata_.end()) {
    return &it->second;
  }
  return nullptr;
}

// ===== Search and Filtering =====

std::vector<const bookmarks::BookmarkNode*> BookmarkManager::SearchBookmarks(
    const BookmarkFilter& filter) const {
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;
  CollectAllBookmarks(bookmark_model_->bookmark_bar_node(), all_bookmarks,
                      !filter.exclude_archived);
  CollectAllBookmarks(bookmark_model_->other_node(), all_bookmarks,
                      !filter.exclude_archived);
  CollectAllBookmarks(bookmark_model_->mobile_node(), all_bookmarks,
                      !filter.exclude_archived);

  std::vector<const bookmarks::BookmarkNode*> result;
  for (const auto* bookmark : all_bookmarks) {
    if (MatchesFilter(bookmark, filter)) {
      result.push_back(bookmark);
    }
  }

  return result;
}

std::vector<const bookmarks::BookmarkNode*>
BookmarkManager::GetSortedBookmarks(BookmarkSortOrder order,
                                    bool include_archived) const {
  std::vector<const bookmarks::BookmarkNode*> bookmarks;
  CollectAllBookmarks(bookmark_model_->bookmark_bar_node(), bookmarks,
                      include_archived);
  CollectAllBookmarks(bookmark_model_->other_node(), bookmarks,
                      include_archived);
  CollectAllBookmarks(bookmark_model_->mobile_node(), bookmarks,
                      include_archived);

  // Sort based on order
  switch (order) {
    case BookmarkSortOrder::kDateAddedNewest:
      std::ranges::sort(bookmarks, [this](const auto* a, const auto* b) {
        const auto* meta_a = GetBookmarkMetadata(a);
        const auto* meta_b = GetBookmarkMetadata(b);
        if (!meta_a && !meta_b) return false;
        if (!meta_a) return false;
        if (!meta_b) return true;
        return meta_a->date_added > meta_b->date_added;
      });
      break;

    case BookmarkSortOrder::kDateAddedOldest:
      std::ranges::sort(bookmarks, [this](const auto* a, const auto* b) {
        const auto* meta_a = GetBookmarkMetadata(a);
        const auto* meta_b = GetBookmarkMetadata(b);
        if (!meta_a && !meta_b) return false;
        if (!meta_a) return true;
        if (!meta_b) return false;
        return meta_a->date_added < meta_b->date_added;
      });
      break;

    case BookmarkSortOrder::kAlphabetical:
      std::ranges::sort(bookmarks, [](const auto* a, const auto* b) {
        return base::ToLowerASCII(a->GetTitle()) <
               base::ToLowerASCII(b->GetTitle());
      });
      break;

    case BookmarkSortOrder::kReverseAlphabetical:
      std::ranges::sort(bookmarks, [](const auto* a, const auto* b) {
        return base::ToLowerASCII(a->GetTitle()) >
               base::ToLowerASCII(b->GetTitle());
      });
      break;

    case BookmarkSortOrder::kMostVisited:
      std::ranges::sort(bookmarks, [this](const auto* a, const auto* b) {
        const auto* meta_a = GetBookmarkMetadata(a);
        const auto* meta_b = GetBookmarkMetadata(b);
        if (!meta_a && !meta_b) return false;
        if (!meta_a) return false;
        if (!meta_b) return true;
        return meta_a->access_count > meta_b->access_count;
      });
      break;

    case BookmarkSortOrder::kRating:
      std::ranges::sort(bookmarks, [this](const auto* a, const auto* b) {
        const auto* meta_a = GetBookmarkMetadata(a);
        const auto* meta_b = GetBookmarkMetadata(b);
        if (!meta_a && !meta_b) return false;
        if (!meta_a) return false;
        if (!meta_b) return true;
        return meta_a->rating > meta_b->rating;
      });
      break;

    case BookmarkSortOrder::kLastModified:
      std::ranges::sort(bookmarks, [this](const auto* a, const auto* b) {
        const auto* meta_a = GetBookmarkMetadata(a);
        const auto* meta_b = GetBookmarkMetadata(b);
        if (!meta_a && !meta_b) return false;
        if (!meta_a) return false;
        if (!meta_b) return true;
        return meta_a->last_modified > meta_b->last_modified;
      });
      break;
  }

  return bookmarks;
}

// ===== Duplicate Detection =====

std::vector<std::vector<const bookmarks::BookmarkNode*>>
BookmarkManager::FindDuplicateBookmarks() const {
  // Group bookmarks by URL
  base::flat_map<std::u16string, std::vector<const bookmarks::BookmarkNode*>>
      url_groups;

  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;
  CollectAllBookmarks(bookmark_model_->bookmark_bar_node(), all_bookmarks,
                      true);
  CollectAllBookmarks(bookmark_model_->other_node(), all_bookmarks, true);
  CollectAllBookmarks(bookmark_model_->mobile_node(), all_bookmarks, true);

  for (const auto* bookmark : all_bookmarks) {
    const std::u16string url = base::UTF8ToUTF16(bookmark->url().spec());
    url_groups[url].push_back(bookmark);
  }

  // Extract groups with duplicates (size > 1)
  std::vector<std::vector<const bookmarks::BookmarkNode*>> duplicates;
  for (const auto& [url, bookmarks] : url_groups) {
    if (bookmarks.size() > 1) {
      duplicates.push_back(bookmarks);
    }
  }

  return duplicates;
}

std::vector<const bookmarks::BookmarkNode*>
BookmarkManager::FindBookmarksByURL(std::u16string_view url) const {
  std::vector<const bookmarks::BookmarkNode*> result;
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;

  CollectAllBookmarks(bookmark_model_->bookmark_bar_node(), all_bookmarks,
                      true);
  CollectAllBookmarks(bookmark_model_->other_node(), all_bookmarks, true);
  CollectAllBookmarks(bookmark_model_->mobile_node(), all_bookmarks, true);

  for (const auto* bookmark : all_bookmarks) {
    const std::u16string bookmark_url =
        base::UTF8ToUTF16(bookmark->url().spec());
    if (bookmark_url == url) {
      result.push_back(bookmark);
    }
  }

  return result;
}

// ===== Batch Operations =====

void BookmarkManager::BatchAddTag(
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks,
    std::u16string_view tag) {
  for (const auto* bookmark : bookmarks) {
    AddTagToBookmark(bookmark, tag);
  }
}

void BookmarkManager::BatchRemoveTag(
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks,
    std::u16string_view tag) {
  for (const auto* bookmark : bookmarks) {
    RemoveTagFromBookmark(bookmark, tag);
  }
}

void BookmarkManager::BatchArchive(
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks) {
  for (const auto* bookmark : bookmarks) {
    SetBookmarkArchived(bookmark, true);
  }
}

void BookmarkManager::BatchUnarchive(
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks) {
  for (const auto* bookmark : bookmarks) {
    SetBookmarkArchived(bookmark, false);
  }
}

// ===== Export/Import =====

std::string BookmarkManager::ExportToJSON() const {
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;
  CollectAllBookmarks(bookmark_model_->bookmark_bar_node(), all_bookmarks,
                      true);
  CollectAllBookmarks(bookmark_model_->other_node(), all_bookmarks, true);
  CollectAllBookmarks(bookmark_model_->mobile_node(), all_bookmarks, true);

  return ExportBookmarksToJSON(all_bookmarks);
}

std::string BookmarkManager::ExportBookmarksToJSON(
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks) const {
  base::Value::List bookmark_list;

  for (const auto* bookmark : bookmarks) {
    base::Value::Dict bookmark_dict;
    bookmark_dict.Set("title", bookmark->GetTitle());
    bookmark_dict.Set("url", bookmark->url().spec());

    const auto* metadata = GetBookmarkMetadata(bookmark);
    if (metadata) {
      // Export tags
      base::Value::List tags;
      for (const auto& tag : metadata->tags) {
        tags.Append(tag);
      }
      bookmark_dict.Set("tags", std::move(tags));

      // Export other metadata
      bookmark_dict.Set("description", metadata->description);
      bookmark_dict.Set("rating", metadata->rating);
      bookmark_dict.Set("is_favorite", metadata->is_favorite);
      bookmark_dict.Set("is_archived", metadata->is_archived);
      bookmark_dict.Set("access_count", metadata->access_count);

      if (!metadata->date_added.is_null()) {
        bookmark_dict.Set("date_added",
                         metadata->date_added.InMillisecondsSinceUnixEpoch());
      }
    }

    bookmark_list.Append(std::move(bookmark_dict));
  }

  base::Value::Dict root;
  root.Set("bookmarks", std::move(bookmark_list));
  root.Set("version", 1);
  root.Set("exported_at",
           base::Time::Now().InMillisecondsSinceUnixEpoch());

  std::string json;
  base::JSONWriter::WriteWithOptions(
      root, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

size_t BookmarkManager::ImportFromJSON(std::string_view json_data) {
  // TODO: Implement JSON import
  // This would parse the JSON and create bookmarks with metadata
  return 0;
}

// ===== Statistics =====

size_t BookmarkManager::GetTotalBookmarkCount() const {
  std::vector<const bookmarks::BookmarkNode*> bookmarks;
  CollectAllBookmarks(bookmark_model_->bookmark_bar_node(), bookmarks, true);
  CollectAllBookmarks(bookmark_model_->other_node(), bookmarks, true);
  CollectAllBookmarks(bookmark_model_->mobile_node(), bookmarks, true);
  return bookmarks.size();
}

size_t BookmarkManager::GetFavoriteCount() const {
  size_t count = 0;
  for (const auto& [node_id, metadata] : bookmark_metadata_) {
    if (metadata.is_favorite && !metadata.is_archived) {
      ++count;
    }
  }
  return count;
}

size_t BookmarkManager::GetArchivedCount() const {
  size_t count = 0;
  for (const auto& [node_id, metadata] : bookmark_metadata_) {
    if (metadata.is_archived) {
      ++count;
    }
  }
  return count;
}

size_t BookmarkManager::GetUnreadCount() const {
  size_t count = 0;
  for (const auto& [node_id, metadata] : bookmark_metadata_) {
    if (metadata.access_count == 0 && !metadata.is_archived) {
      ++count;
    }
  }
  return count;
}

std::vector<std::pair<std::u16string, size_t>> BookmarkManager::GetTopTags(
    size_t max_count) const {
  // Count tag usage
  base::flat_map<std::u16string, size_t> tag_counts;
  for (const auto& [node_id, metadata] : bookmark_metadata_) {
    for (const auto& tag : metadata.tags) {
      tag_counts[tag]++;
    }
  }

  // Convert to vector and sort by count
  std::vector<std::pair<std::u16string, size_t>> top_tags(tag_counts.begin(),
                                                           tag_counts.end());
  std::ranges::sort(top_tags,
                   [](const auto& a, const auto& b) { return a.second > b.second; });

  // Limit to max_count
  if (top_tags.size() > max_count) {
    top_tags.resize(max_count);
  }

  return top_tags;
}

// ===== Cleanup =====

void BookmarkManager::CleanupDeletedBookmarks() {
  std::vector<int64_t> to_remove;

  for (const auto& [node_id, metadata] : bookmark_metadata_) {
    const bookmarks::BookmarkNode* node =
        bookmarks::GetBookmarkNodeByID(bookmark_model_, node_id);
    if (!node) {
      to_remove.push_back(node_id);
    }
  }

  for (const int64_t node_id : to_remove) {
    bookmark_metadata_.erase(node_id);
  }

  // Clean up recently_added_ list
  std::erase_if(recently_added_, [this](int64_t node_id) {
    return !bookmarks::GetBookmarkNodeByID(bookmark_model_, node_id);
  });
}

void BookmarkManager::ClearAllMetadata() {
  bookmark_metadata_.clear();
  recently_added_.clear();
}

// ===== Private Helpers =====

EnhancedBookmarkMetadata& BookmarkManager::GetOrCreateMetadata(
    const bookmarks::BookmarkNode* bookmark) {
  DCHECK(bookmark) << "Cannot get metadata for null bookmark";
  DCHECK(bookmark->is_url()) << "Metadata only for URL bookmarks";

  const int64_t node_id = bookmark->id();
  auto it = bookmark_metadata_.find(node_id);

  if (it == bookmark_metadata_.end()) {
    EnhancedBookmarkMetadata metadata;
    metadata.date_added = base::Time::Now();
    it = bookmark_metadata_.emplace(node_id, std::move(metadata)).first;
  }

  return it->second;
}

void BookmarkManager::CollectAllBookmarks(
    const bookmarks::BookmarkNode* node,
    std::vector<const bookmarks::BookmarkNode*>& bookmarks,
    bool include_archived) const {
  if (!node) {
    return;
  }

  if (node->is_url()) {
    if (include_archived || !IsBookmarkArchived(node)) {
      bookmarks.push_back(node);
    }
  }

  // Recurse into folders
  for (size_t i = 0; i < node->children().size(); ++i) {
    CollectAllBookmarks(node->children()[i].get(), bookmarks,
                       include_archived);
  }
}

bool BookmarkManager::MatchesFilter(const bookmarks::BookmarkNode* bookmark,
                                   const BookmarkFilter& filter) const {
  const auto* metadata = GetBookmarkMetadata(bookmark);

  // Archive filter
  if (filter.archived_only && (!metadata || !metadata->is_archived)) {
    return false;
  }
  if (filter.exclude_archived && metadata && metadata->is_archived) {
    return false;
  }

  // Favorites filter
  if (filter.favorites_only && (!metadata || !metadata->is_favorite)) {
    return false;
  }

  // Rating filter
  if (metadata) {
    if (filter.min_rating.has_value() &&
        metadata->rating < filter.min_rating.value()) {
      return false;
    }
    if (filter.max_rating.has_value() &&
        metadata->rating > filter.max_rating.value()) {
      return false;
    }

    // Date filter
    if (filter.date_from.has_value() &&
        metadata->date_added < filter.date_from.value()) {
      return false;
    }
    if (filter.date_to.has_value() &&
        metadata->date_added > filter.date_to.value()) {
      return false;
    }
  }

  // Tag filter
  if (!filter.tags.empty() && metadata) {
    bool has_matching_tag = false;
    for (const auto& filter_tag : filter.tags) {
      if (std::ranges::find(metadata->tags, filter_tag) !=
          metadata->tags.end()) {
        has_matching_tag = true;
        break;
      }
    }
    if (!has_matching_tag) {
      return false;
    }
  }

  // Text search
  if (!filter.search_query.empty()) {
    const std::u16string lower_query =
        base::ToLowerASCII(filter.search_query);
    const std::u16string lower_title =
        base::ToLowerASCII(bookmark->GetTitle());
    const std::u16string lower_url =
        base::ToLowerASCII(base::UTF8ToUTF16(bookmark->url().spec()));

    bool matches = lower_title.find(lower_query) != std::u16string::npos ||
                   lower_url.find(lower_query) != std::u16string::npos;

    // Also search in description and tags
    if (!matches && metadata) {
      const std::u16string lower_desc =
          base::ToLowerASCII(metadata->description);
      matches = lower_desc.find(lower_query) != std::u16string::npos;

      if (!matches) {
        for (const auto& tag : metadata->tags) {
          if (base::ToLowerASCII(tag).find(lower_query) !=
              std::u16string::npos) {
            matches = true;
            break;
          }
        }
      }
    }

    if (!matches) {
      return false;
    }
  }

  return true;
}
