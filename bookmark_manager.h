// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MANAGER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

// Enhanced metadata for individual bookmarks (URLs).
//
// This structure stores rich information about bookmarks beyond what's
// available in the core BookmarkNode. Metadata is volatile and stored
// in memory only in this implementation.
//
// Thread safety: Not thread-safe. Must be synchronized by owner.
struct EnhancedBookmarkMetadata {
  // User-defined tags for categorization.
  std::vector<std::u16string> tags;

  // Optional description/notes about the bookmark.
  std::u16string description;

  // Number of times this bookmark has been accessed.
  int access_count = 0;

  // Last time this bookmark was accessed.
  base::Time last_accessed;

  // When this bookmark was added.
  base::Time date_added;

  // When metadata was last modified.
  base::Time last_modified;

  // Custom rating (0-5 stars).
  int rating = 0;

  // Whether this bookmark is marked as favorite.
  bool is_favorite = false;

  // Whether this bookmark should be archived (hidden from main view).
  bool is_archived = false;

  // Screenshot thumbnail data (optional).
  std::string thumbnail_data;

  EnhancedBookmarkMetadata() = default;
  EnhancedBookmarkMetadata(const EnhancedBookmarkMetadata&) = default;
  EnhancedBookmarkMetadata& operator=(const EnhancedBookmarkMetadata&) =
      default;
  EnhancedBookmarkMetadata(EnhancedBookmarkMetadata&&) noexcept = default;
  EnhancedBookmarkMetadata& operator=(EnhancedBookmarkMetadata&&) noexcept =
      default;
  ~EnhancedBookmarkMetadata() = default;
};

// Sorting options for bookmark lists.
enum class BookmarkSortOrder {
  kDateAddedNewest,    // Most recently added first
  kDateAddedOldest,    // Oldest first
  kAlphabetical,       // A-Z by title
  kReverseAlphabetical,// Z-A by title
  kMostVisited,        // Highest access count first
  kRating,             // Highest rated first
  kLastModified,       // Most recently modified first
};

// Filter criteria for bookmark searches.
struct BookmarkFilter {
  // Text search across title, URL, description, tags.
  std::u16string search_query;

  // Filter by specific tags (any match).
  std::vector<std::u16string> tags;

  // Filter by rating range (inclusive).
  std::optional<int> min_rating;
  std::optional<int> max_rating;

  // Filter by date range.
  std::optional<base::Time> date_from;
  std::optional<base::Time> date_to;

  // Show only favorites.
  bool favorites_only = false;

  // Show only archived.
  bool archived_only = false;

  // Exclude archived from results.
  bool exclude_archived = true;
};

// Comprehensive bookmark manager with advanced features.
//
// This class provides:
// - Individual bookmark metadata (tags, descriptions, ratings)
// - Recently added bookmarks tracking
// - Advanced sorting and filtering
// - Duplicate detection
// - Export/import functionality
// - Batch operations
// - Usage analytics
//
// Usage example:
//   auto manager = std::make_unique<BookmarkManager>(bookmark_model);
//
//   // Track new bookmark
//   manager->OnBookmarkAdded(new_bookmark);
//
//   // Get recently added
//   auto recent = manager->GetRecentlyAddedBookmarks(10);
//
//   // Add tags and description
//   manager->AddTagToBookmark(bookmark, u"important");
//   manager->SetBookmarkDescription(bookmark, u"Useful article");
//
//   // Search with filters
//   BookmarkFilter filter;
//   filter.search_query = u"chromium";
//   filter.favorites_only = true;
//   auto results = manager->SearchBookmarks(filter);
//
//   // Export bookmarks
//   std::string json = manager->ExportToJSON();
//
// Thread safety: Not thread-safe. Must be accessed from UI thread only.
class BookmarkManager {
 public:
  explicit BookmarkManager(bookmarks::BookmarkModel* model);

  BookmarkManager(const BookmarkManager&) = delete;
  BookmarkManager& operator=(const BookmarkManager&) = delete;

  ~BookmarkManager();

  // ===== Recently Added Tracking =====

  // Notifies the manager that a bookmark was added.
  // Should be called whenever a new bookmark is created.
  void OnBookmarkAdded(const bookmarks::BookmarkNode* bookmark);

  // Returns the N most recently added bookmarks.
  // Only includes URL nodes, not folders.
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  GetRecentlyAddedBookmarks(size_t max_count = 20) const;

  // Returns bookmarks added within the specified time period.
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  GetBookmarksAddedSince(base::Time since) const;

  // ===== Metadata Management =====

  // Tag operations for bookmarks.
  void AddTagToBookmark(const bookmarks::BookmarkNode* bookmark,
                        std::u16string_view tag);
  void RemoveTagFromBookmark(const bookmarks::BookmarkNode* bookmark,
                             std::u16string_view tag);
  [[nodiscard]] std::vector<std::u16string> GetTagsForBookmark(
      const bookmarks::BookmarkNode* bookmark) const;
  [[nodiscard]] std::vector<std::u16string> GetAllBookmarkTags() const;

  // Description management.
  void SetBookmarkDescription(const bookmarks::BookmarkNode* bookmark,
                              std::u16string_view description);
  [[nodiscard]] std::u16string GetBookmarkDescription(
      const bookmarks::BookmarkNode* bookmark) const;

  // Rating management (0-5 stars).
  void SetBookmarkRating(const bookmarks::BookmarkNode* bookmark, int rating);
  [[nodiscard]] int GetBookmarkRating(
      const bookmarks::BookmarkNode* bookmark) const;

  // Favorite management.
  void SetBookmarkFavorite(const bookmarks::BookmarkNode* bookmark,
                           bool is_favorite);
  [[nodiscard]] bool IsBookmarkFavorite(
      const bookmarks::BookmarkNode* bookmark) const;
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  GetFavoriteBookmarks() const;

  // Archive management.
  void SetBookmarkArchived(const bookmarks::BookmarkNode* bookmark,
                           bool is_archived);
  [[nodiscard]] bool IsBookmarkArchived(
      const bookmarks::BookmarkNode* bookmark) const;

  // Access tracking.
  void RecordBookmarkAccess(const bookmarks::BookmarkNode* bookmark);
  [[nodiscard]] const EnhancedBookmarkMetadata* GetBookmarkMetadata(
      const bookmarks::BookmarkNode* bookmark) const;

  // ===== Search and Filtering =====

  // Advanced search with multiple criteria.
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*> SearchBookmarks(
      const BookmarkFilter& filter) const;

  // Get bookmarks sorted by specified order.
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  GetSortedBookmarks(BookmarkSortOrder order,
                     bool include_archived = false) const;

  // ===== Duplicate Detection =====

  // Find bookmarks with identical URLs.
  [[nodiscard]] std::vector<std::vector<const bookmarks::BookmarkNode*>>
  FindDuplicateBookmarks() const;

  // Check if a URL already exists in bookmarks.
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  FindBookmarksByURL(std::u16string_view url) const;

  // ===== Batch Operations =====

  // Add tag to multiple bookmarks.
  void BatchAddTag(const std::vector<const bookmarks::BookmarkNode*>& bookmarks,
                   std::u16string_view tag);

  // Remove tag from multiple bookmarks.
  void BatchRemoveTag(
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks,
      std::u16string_view tag);

  // Archive multiple bookmarks.
  void BatchArchive(
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks);

  // Unarchive multiple bookmarks.
  void BatchUnarchive(
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks);

  // ===== Export/Import =====

  // Export bookmarks and metadata to JSON format.
  [[nodiscard]] std::string ExportToJSON() const;

  // Export specific bookmarks to JSON.
  [[nodiscard]] std::string ExportBookmarksToJSON(
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks) const;

  // Import bookmarks from JSON (returns number of imported bookmarks).
  size_t ImportFromJSON(std::string_view json_data);

  // ===== Statistics =====

  // Get total number of bookmarks (excluding folders).
  [[nodiscard]] size_t GetTotalBookmarkCount() const;

  // Get number of bookmarks by category.
  [[nodiscard]] size_t GetFavoriteCount() const;
  [[nodiscard]] size_t GetArchivedCount() const;
  [[nodiscard]] size_t GetUnreadCount() const;  // access_count == 0

  // Get most used tags.
  [[nodiscard]] std::vector<std::pair<std::u16string, size_t>>
  GetTopTags(size_t max_count = 10) const;

  // ===== Cleanup =====

  // Remove metadata for deleted bookmarks.
  void CleanupDeletedBookmarks();

  // Clear all metadata (useful for reset).
  void ClearAllMetadata();

 private:
  // Helper to get or create metadata for a bookmark.
  EnhancedBookmarkMetadata& GetOrCreateMetadata(
      const bookmarks::BookmarkNode* bookmark);

  // Helper to collect all bookmarks recursively.
  void CollectAllBookmarks(
      const bookmarks::BookmarkNode* node,
      std::vector<const bookmarks::BookmarkNode*>& bookmarks,
      bool include_archived) const;

  // Helper to check if bookmark matches filter.
  bool MatchesFilter(const bookmarks::BookmarkNode* bookmark,
                     const BookmarkFilter& filter) const;

  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;

  // Metadata storage: maps node ID to enhanced bookmark metadata.
  // Using flat_map for better cache locality.
  base::flat_map<int64_t, EnhancedBookmarkMetadata> bookmark_metadata_;

  // Recently added bookmarks (node IDs in chronological order).
  // Most recent at the front.
  std::vector<int64_t> recently_added_;

  // Maximum number of recent bookmarks to track.
  static constexpr size_t kMaxRecentBookmarks = 1000;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MANAGER_H_
