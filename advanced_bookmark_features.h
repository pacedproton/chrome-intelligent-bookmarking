// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_ADVANCED_BOOKMARK_FEATURES_H_
#define CHROME_BROWSER_UI_BOOKMARKS_ADVANCED_BOOKMARK_FEATURES_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "url/gurl.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

// ===== Smart Folders / Saved Searches =====

// Represents a smart folder that dynamically updates based on criteria.
//
// Smart folders are virtual folders that contain bookmarks matching specific
// filter criteria. They automatically update when bookmarks change.
//
// Example:
//   SmartFolder tech_articles;
//   tech_articles.name = u"Tech Articles";
//   tech_articles.criteria.tags = {u"tech", u"programming"};
//   tech_articles.criteria.min_rating = 4;
//   tech_articles.auto_update = true;
struct SmartFolder {
  int64_t id = 0;
  std::u16string name;
  std::u16string description;
  BookmarkFilter criteria;
  bool auto_update = true;
  base::Time created;
  base::Time last_updated;

  // Cached results (updated on-demand or automatically)
  std::vector<const bookmarks::BookmarkNode*> cached_results;

  SmartFolder() = default;
  SmartFolder(const SmartFolder&) = default;
  SmartFolder& operator=(const SmartFolder&) = default;
  SmartFolder(SmartFolder&&) noexcept = default;
  SmartFolder& operator=(SmartFolder&&) noexcept = default;
  ~SmartFolder() = default;
};

// ===== Collections =====

// A collection is a named group of bookmarks with optional auto-add rules.
//
// Collections are similar to playlists - you can manually add bookmarks,
// and optionally set up rules to automatically include matching bookmarks.
//
// Example:
//   Collection work_docs;
//   work_docs.name = u"Work Documents";
//   work_docs.auto_rule = BookmarkFilter{.tags = {u"work", u"important"}};
struct Collection {
  int64_t id = 0;
  std::u16string name;
  std::u16string description;

  // Manually added bookmarks (by ID)
  base::flat_set<int64_t> manual_bookmark_ids;

  // Optional auto-add rule
  std::optional<BookmarkFilter> auto_rule;

  base::Time created;
  base::Time last_modified;

  // Color/icon for visual identification (0-11 for predefined colors)
  int color_id = 0;

  Collection() = default;
  Collection(const Collection&) = default;
  Collection& operator=(const Collection&) = default;
  Collection(Collection&&) noexcept = default;
  Collection& operator=(Collection&&) noexcept = default;
  ~Collection() = default;
};

// ===== Bookmark Health =====

// Health analysis results for the bookmark collection.
//
// Provides insights into bookmark quality and recommendations for improvement.
struct BookmarkHealth {
  // Overall health score (0-100)
  // 100 = perfect health, 0 = needs attention
  int health_score = 0;

  // Counts for various issues
  int total_bookmarks = 0;
  int broken_links = 0;
  int duplicates = 0;
  int untagged = 0;
  int never_visited = 0;
  int archived = 0;

  // Positive metrics
  int high_value = 0;  // High-rated + frequently used
  int well_organized = 0;  // Has tags and description

  // Last analysis time
  base::Time analyzed_at;

  // Actionable recommendations
  std::vector<std::u16string> recommendations;
};

// ===== Link Validation =====

// Status of a bookmark URL validation check.
enum class LinkStatus {
  kUnknown,      // Not yet checked
  kValid,        // URL is accessible (200-299)
  kBroken,       // URL is broken (404, 500, etc.)
  kRedirect,     // URL redirects (301, 302)
  kTimeout,      // Request timed out
  kSSLError,     // SSL/TLS error
  kDNSError,     // DNS resolution failed
};

// Result of a link validation check.
struct LinkValidationResult {
  const bookmarks::BookmarkNode* bookmark = nullptr;
  LinkStatus status = LinkStatus::kUnknown;
  int http_code = 0;
  std::string redirect_url;
  base::Time checked_at;
  std::string error_message;
};

// ===== Related Bookmarks =====

// Relationship type between bookmarks.
enum class RelationType {
  kSameDomain,     // Same website/domain
  kSimilarTags,    // Share common tags
  kSameFolder,     // In the same folder
  kSimilarTitle,   // Similar titles
  kFrequentlyUsedTogether,  // Accessed in same time period
};

// A related bookmark suggestion.
struct RelatedBookmark {
  const bookmarks::BookmarkNode* bookmark = nullptr;
  RelationType relation_type;
  float similarity_score = 0.0f;  // 0.0-1.0
  std::vector<std::u16string> shared_tags;
};

// ===== Advanced Bookmark Features Manager =====

// Main class managing advanced bookmark features including smart folders,
// collections, health analysis, link validation, and related bookmarks.
//
// Usage:
//   auto features = std::make_unique<AdvancedBookmarkFeatures>(
//       bookmark_model, bookmark_manager);
//
//   // Create smart folder
//   BookmarkFilter filter;
//   filter.tags = {u"tech"};
//   filter.min_rating = 4;
//   int64_t id = features->CreateSmartFolder(u"Top Tech", filter);
//   auto results = features->GetSmartFolderContents(id);
//
//   // Check bookmark health
//   auto health = features->AnalyzeHealth();
//   LOG(INFO) << "Health score: " << health.health_score;
//
//   // Validate links
//   features->ValidateAllLinks(base::BindOnce(&OnValidationComplete));
//
// Thread safety: Not thread-safe. Must be accessed from UI thread only.
class AdvancedBookmarkFeatures {
 public:
  AdvancedBookmarkFeatures(bookmarks::BookmarkModel* model,
                          BookmarkManager* manager);

  AdvancedBookmarkFeatures(const AdvancedBookmarkFeatures&) = delete;
  AdvancedBookmarkFeatures& operator=(const AdvancedBookmarkFeatures&) = delete;

  ~AdvancedBookmarkFeatures();

  // ===== Smart Folders =====

  // Creates a new smart folder with the given criteria.
  // Returns the smart folder ID.
  [[nodiscard]] int64_t CreateSmartFolder(std::u16string_view name,
                                          const BookmarkFilter& criteria);

  // Updates a smart folder's criteria.
  void UpdateSmartFolder(int64_t id, const BookmarkFilter& criteria);

  // Deletes a smart folder.
  void DeleteSmartFolder(int64_t id);

  // Gets all smart folders.
  [[nodiscard]] std::vector<SmartFolder> GetAllSmartFolders() const;

  // Gets contents of a smart folder (refreshes if needed).
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  GetSmartFolderContents(int64_t id);

  // Manually refreshes a smart folder's contents.
  void RefreshSmartFolder(int64_t id);

  // Refreshes all smart folders that have auto-update enabled.
  void RefreshAllSmartFolders();

  // ===== Collections =====

  // Creates a new collection.
  // Returns the collection ID.
  [[nodiscard]] int64_t CreateCollection(std::u16string_view name);

  // Deletes a collection.
  void DeleteCollection(int64_t id);

  // Adds a bookmark to a collection manually.
  void AddToCollection(int64_t collection_id,
                      const bookmarks::BookmarkNode* bookmark);

  // Removes a bookmark from a collection.
  void RemoveFromCollection(int64_t collection_id,
                           const bookmarks::BookmarkNode* bookmark);

  // Sets an auto-add rule for a collection.
  void SetCollectionAutoRule(int64_t collection_id,
                            const BookmarkFilter& rule);

  // Clears the auto-add rule for a collection.
  void ClearCollectionAutoRule(int64_t collection_id);

  // Gets all collections.
  [[nodiscard]] std::vector<Collection> GetAllCollections() const;

  // Gets bookmarks in a collection (manual + auto-matched).
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  GetCollectionContents(int64_t id);

  // Refreshes auto-added bookmarks in a collection.
  void RefreshCollection(int64_t id);

  // ===== Health Analysis =====

  // Analyzes bookmark health and returns comprehensive report.
  [[nodiscard]] BookmarkHealth AnalyzeHealth();

  // Gets the last health analysis (cached).
  [[nodiscard]] std::optional<BookmarkHealth> GetLastHealthAnalysis() const;

  // Finds bookmarks that appear to be broken (cached from last validation).
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  FindBrokenLinks() const;

  // Finds bookmarks that are untagged.
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  FindUntaggedBookmarks() const;

  // Finds bookmarks never visited.
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  FindNeverVisitedBookmarks() const;

  // Gets health recommendations as user-friendly strings.
  [[nodiscard]] std::vector<std::u16string> GetHealthRecommendations();

  // ===== Link Validation =====

  using ValidationCallback =
      base::OnceCallback<void(std::vector<LinkValidationResult>)>;

  // Validates all bookmark links asynchronously.
  // Calls callback with results when done.
  void ValidateAllLinks(ValidationCallback callback);

  // Validates a single bookmark's link.
  void ValidateLink(const bookmarks::BookmarkNode* bookmark,
                   base::OnceCallback<void(LinkValidationResult)> callback);

  // Gets cached validation results.
  [[nodiscard]] std::vector<LinkValidationResult>
  GetCachedValidationResults() const;

  // Clears validation cache.
  void ClearValidationCache();

  // ===== Related Bookmarks =====

  // Finds bookmarks related to the given bookmark.
  // Returns up to max_results bookmarks, sorted by relevance.
  [[nodiscard]] std::vector<RelatedBookmark> FindRelatedBookmarks(
      const bookmarks::BookmarkNode* bookmark,
      size_t max_results = 10);

  // Finds bookmarks from the same domain.
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  FindSameDomainBookmarks(const bookmarks::BookmarkNode* bookmark);

  // Finds bookmarks with similar tags.
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  FindSimilarTaggedBookmarks(const bookmarks::BookmarkNode* bookmark,
                            size_t min_shared_tags = 2);

  // ===== HTML Import/Export =====

  // Exports bookmarks to HTML (Netscape bookmark format).
  // This is the standard format supported by all browsers.
  [[nodiscard]] std::string ExportToHTML() const;

  // Exports specific bookmarks to HTML.
  [[nodiscard]] std::string ExportBookmarksToHTML(
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks) const;

  // Imports bookmarks from HTML (Netscape bookmark format).
  // Returns number of bookmarks imported.
  size_t ImportFromHTML(std::string_view html_data);

  // ===== Quick Access =====

  // Gets most frequently used bookmarks.
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  GetMostUsedBookmarks(size_t max_count = 10);

  // Gets high-value bookmarks (high-rated + frequently used).
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  GetHighValueBookmarks(size_t max_count = 10);

  // Gets bookmarks needing attention (never visited, low-rated, old).
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  GetBookmarksNeedingAttention(size_t max_count = 10);

 private:
  // Helper: Get all bookmarks recursively
  void CollectAllBookmarks(
      const bookmarks::BookmarkNode* node,
      std::vector<const bookmarks::BookmarkNode*>& bookmarks) const;

  // Helper: Calculate health score
  int CalculateHealthScore(const BookmarkHealth& health) const;

  // Helper: Extract domain from URL
  std::string GetDomain(const GURL& url) const;

  // Helper: Calculate tag similarity between two bookmarks
  float CalculateTagSimilarity(
      const bookmarks::BookmarkNode* a,
      const bookmarks::BookmarkNode* b) const;

  // Helper: Validate single link (internal)
  LinkValidationResult ValidateLinkSync(
      const bookmarks::BookmarkNode* bookmark);

  // Helper: Parse HTML bookmark file
  void ParseHTMLBookmarks(std::string_view html,
                         std::vector<const bookmarks::BookmarkNode*>& results);

  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<BookmarkManager> bookmark_manager_;

  // Smart folders storage (ID -> SmartFolder)
  base::flat_map<int64_t, SmartFolder> smart_folders_;
  int64_t next_smart_folder_id_ = 1;

  // Collections storage (ID -> Collection)
  base::flat_map<int64_t, Collection> collections_;
  int64_t next_collection_id_ = 1;

  // Cached health analysis
  std::optional<BookmarkHealth> cached_health_;

  // Link validation cache (bookmark ID -> result)
  base::flat_map<int64_t, LinkValidationResult> validation_cache_;

  base::WeakPtrFactory<AdvancedBookmarkFeatures> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_ADVANCED_BOOKMARK_FEATURES_H_
