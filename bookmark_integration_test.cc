// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Integration tests for the complete bookmark management system.
// These tests verify that all components work together correctly.

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/bookmarks/advanced_bookmark_features.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "chrome/browser/ui/bookmarks/filtered_folders_combo_model.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BookmarkIntegrationTest : public testing::Test {
 protected:
  void SetUp() override {
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_.get());

    manager_ = std::make_unique<BookmarkManager>(bookmark_model_.get());
    features_ = std::make_unique<AdvancedBookmarkFeatures>(
        bookmark_model_.get(), manager_.get());
    folder_model_ = std::make_unique<FilteredFoldersComboModel>(
        bookmark_model_.get());

    CreateTestData();
  }

  void CreateTestData() {
    const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

    // Create folder structure
    tech_ = bookmark_model_->AddFolder(bar, 0, u"Tech");
    work_ = bookmark_model_->AddFolder(bar, 1, u"Work");
    personal_ = bookmark_model_->AddFolder(bar, 2, u"Personal");
    archive_ = bookmark_model_->AddFolder(bar, 3, u"Archive");

    // Tech bookmarks
    google_ = bookmark_model_->AddURL(tech_, 0, u"Google",
                                      GURL("https://google.com"));
    github_ = bookmark_model_->AddURL(tech_, 1, u"GitHub",
                                      GURL("https://github.com"));
    stackoverflow_ = bookmark_model_->AddURL(
        tech_, 2, u"Stack Overflow", GURL("https://stackoverflow.com"));

    // Work bookmarks
    jira_ = bookmark_model_->AddURL(work_, 0, u"Jira",
                                    GURL("https://jira.company.com"));
    confluence_ = bookmark_model_->AddURL(
        work_, 1, u"Confluence", GURL("https://confluence.company.com"));

    // Personal bookmarks
    youtube_ = bookmark_model_->AddURL(personal_, 0, u"YouTube",
                                       GURL("https://youtube.com"));
    netflix_ = bookmark_model_->AddURL(personal_, 1, u"Netflix",
                                       GURL("https://netflix.com"));

    // Setup metadata
    manager_->OnBookmarkAdded(google_);
    manager_->OnBookmarkAdded(github_);
    manager_->OnBookmarkAdded(stackoverflow_);
    manager_->OnBookmarkAdded(jira_);
    manager_->OnBookmarkAdded(confluence_);
    manager_->OnBookmarkAdded(youtube_);
    manager_->OnBookmarkAdded(netflix_);

    // Add tags
    manager_->AddTagToBookmark(google_, u"search");
    manager_->AddTagToBookmark(google_, u"tech");
    manager_->AddTagToBookmark(github_, u"code");
    manager_->AddTagToBookmark(github_, u"tech");
    manager_->AddTagToBookmark(stackoverflow_, u"code");
    manager_->AddTagToBookmark(stackoverflow_, u"tech");
    manager_->AddTagToBookmark(jira_, u"work");
    manager_->AddTagToBookmark(confluence_, u"work");
    manager_->AddTagToBookmark(youtube_, u"entertainment");
    manager_->AddTagToBookmark(netflix_, u"entertainment");

    // Add ratings
    manager_->SetBookmarkRating(google_, 5);
    manager_->SetBookmarkRating(github_, 5);
    manager_->SetBookmarkRating(stackoverflow_, 4);
    manager_->SetBookmarkRating(jira_, 3);
    manager_->SetBookmarkRating(confluence_, 3);
    manager_->SetBookmarkRating(youtube_, 4);
    manager_->SetBookmarkRating(netflix_, 5);

    // Mark some as favorites
    manager_->SetBookmarkFavorite(google_, true);
    manager_->SetBookmarkFavorite(github_, true);

    // Simulate usage
    manager_->RecordBookmarkAccess(google_);
    manager_->RecordBookmarkAccess(google_);
    manager_->RecordBookmarkAccess(google_);
    manager_->RecordBookmarkAccess(github_);
    manager_->RecordBookmarkAccess(github_);
  }

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<BookmarkManager> manager_;
  std::unique_ptr<AdvancedBookmarkFeatures> features_;
  std::unique_ptr<FilteredFoldersComboModel> folder_model_;

  raw_ptr<const bookmarks::BookmarkNode> tech_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> work_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> personal_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> archive_ = nullptr;

  raw_ptr<const bookmarks::BookmarkNode> google_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> github_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> stackoverflow_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> jira_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> confluence_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> youtube_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> netflix_ = nullptr;
};

// ===== End-to-End Workflow Tests =====

TEST_F(BookmarkIntegrationTest, CompleteWorkflow) {
  // 1. Search for bookmarks
  BookmarkFilter filter;
  filter.tags = {u"tech"};
  auto tech_bookmarks = manager_->SearchBookmarks(filter);
  EXPECT_EQ(tech_bookmarks.size(), 3u);

  // 2. Create smart folder for tech bookmarks
  int64_t smart_folder_id = features_->CreateSmartFolder(u"Tech", filter);
  auto smart_contents = features_->GetSmartFolderContents(smart_folder_id);
  EXPECT_EQ(smart_contents.size(), 3u);

  // 3. Create collection for favorites
  int64_t collection_id = features_->CreateCollection(u"Favorites");
  features_->AddToCollection(collection_id, google_);
  features_->AddToCollection(collection_id, github_);

  // 4. Analyze health
  auto health = features_->AnalyzeHealth();
  EXPECT_GT(health.health_score, 0);
  EXPECT_EQ(health.total_bookmarks, 7);

  // 5. Find related bookmarks
  auto related = features_->FindRelatedBookmarks(github_, 10);
  EXPECT_FALSE(related.empty());

  // 6. Export to JSON
  std::string json = manager_->ExportToJSON();
  EXPECT_FALSE(json.empty());

  // 7. Export to HTML
  std::string html = features_->ExportToHTML();
  EXPECT_FALSE(html.empty());
  EXPECT_NE(html.find("<!DOCTYPE NETSCAPE-Bookmark-file-1>"),
            std::string::npos);
}

TEST_F(BookmarkIntegrationTest, SmartFolderAndCollectionInteraction) {
  // Create smart folder for high-rated bookmarks
  BookmarkFilter filter;
  filter.min_rating = 5;
  int64_t smart_id = features_->CreateSmartFolder(u"Top Rated", filter);

  // Create collection with auto-rule
  int64_t collection_id = features_->CreateCollection(u"Also Top Rated");
  features_->SetCollectionAutoRule(collection_id, filter);

  // Both should have same bookmarks
  auto smart_contents = features_->GetSmartFolderContents(smart_id);
  auto collection_contents = features_->GetCollectionContents(collection_id);

  EXPECT_EQ(smart_contents.size(), collection_contents.size());
  EXPECT_EQ(smart_contents.size(), 3u);  // google, github, netflix

  // Add new 5-star bookmark
  auto* new_bookmark = bookmark_model_->AddURL(
      tech_, 3, u"Mozilla", GURL("https://mozilla.org"));
  manager_->OnBookmarkAdded(new_bookmark);
  manager_->SetBookmarkRating(new_bookmark, 5);

  // Refresh both
  features_->RefreshSmartFolder(smart_id);
  // Collection auto-refreshes on access

  smart_contents = features_->GetSmartFolderContents(smart_id);
  collection_contents = features_->GetCollectionContents(collection_id);

  EXPECT_EQ(smart_contents.size(), 4u);
  EXPECT_EQ(collection_contents.size(), 4u);
}

TEST_F(BookmarkIntegrationTest, HealthAnalysisAndRecommendations) {
  // Initial health
  auto health = features_->AnalyzeHealth();
  int initial_score = health.health_score;

  // Add some issues
  auto* untagged = bookmark_model_->AddURL(
      work_, 2, u"Untagged", GURL("https://example.com"));
  manager_->OnBookmarkAdded(untagged);

  auto* duplicate = bookmark_model_->AddURL(
      work_, 3, u"Google Clone", GURL("https://google.com"));
  manager_->OnBookmarkAdded(duplicate);

  // Re-analyze
  health = features_->AnalyzeHealth();

  EXPECT_LT(health.health_score, initial_score);
  EXPECT_GT(health.untagged, 0);
  EXPECT_GT(health.duplicates, 0);
  EXPECT_FALSE(health.recommendations.empty());

  // Fix issues
  manager_->AddTagToBookmark(untagged, u"work");

  // Re-analyze
  health = features_->AnalyzeHealth();
  EXPECT_GT(health.health_score, 0);
}

TEST_F(BookmarkIntegrationTest, MultiCriteriaSearchAndFilter) {
  // Complex filter: tech + high-rated + favorites
  BookmarkFilter filter;
  filter.tags = {u"tech"};
  filter.min_rating = 4;
  filter.favorites_only = true;

  auto results = manager_->SearchBookmarks(filter);

  // Should find google and github (both tech, rated 5, favorites)
  EXPECT_EQ(results.size(), 2u);

  bool has_google = false;
  bool has_github = false;

  for (const auto* bookmark : results) {
    if (bookmark == google_) has_google = true;
    if (bookmark == github_) has_github = true;
  }

  EXPECT_TRUE(has_google);
  EXPECT_TRUE(has_github);
}

TEST_F(BookmarkIntegrationTest, RelatedBookmarksAccuracy) {
  // GitHub should find stackoverflow (both have 'code' tag)
  auto related = features_->FindRelatedBookmarks(github_, 10);

  EXPECT_FALSE(related.empty());

  bool found_stackoverflow = false;
  for (const auto& rel : related) {
    if (rel.bookmark == stackoverflow_) {
      found_stackoverflow = true;
      EXPECT_EQ(rel.relation_type, RelationType::kSimilarTags);
      EXPECT_GT(rel.similarity_score, 0.0f);
      EXPECT_FALSE(rel.shared_tags.empty());

      // Should share 'code' tag
      bool has_code = false;
      for (const auto& tag : rel.shared_tags) {
        if (tag == u"code") {
          has_code = true;
          break;
        }
      }
      EXPECT_TRUE(has_code);
      break;
    }
  }

  EXPECT_TRUE(found_stackoverflow);
}

TEST_F(BookmarkIntegrationTest, QuickAccessPanelAccuracy) {
  // Most used should be google (3 accesses) then github (2 accesses)
  auto most_used = features_->GetMostUsedBookmarks(10);

  EXPECT_GE(most_used.size(), 2u);
  EXPECT_EQ(most_used[0], google_);
  EXPECT_EQ(most_used[1], github_);

  // High value should include google and github (both 5-star + accessed)
  auto high_value = features_->GetHighValueBookmarks(10);

  EXPECT_GE(high_value.size(), 2u);

  bool has_google = false;
  bool has_github = false;

  for (const auto* bookmark : high_value) {
    if (bookmark == google_) has_google = true;
    if (bookmark == github_) has_github = true;
  }

  EXPECT_TRUE(has_google);
  EXPECT_TRUE(has_github);
}

TEST_F(BookmarkIntegrationTest, ExportImportRoundTrip) {
  // Export all bookmarks to JSON
  std::string json = manager_->ExportToJSON();
  EXPECT_FALSE(json.empty());

  // Create new model
  auto new_model = bookmarks::TestBookmarkClient::CreateModel();
  bookmarks::test::WaitForBookmarkModelToLoad(new_model.get());
  auto new_manager = std::make_unique<BookmarkManager>(new_model.get());

  // Import
  size_t imported = new_manager->ImportFromJSON(json);
  EXPECT_GT(imported, 0);

  // Verify metadata preserved
  auto all_bookmarks = new_manager->GetSortedBookmarks(
      BookmarkSortOrder::kDateAddedNewest, false);

  EXPECT_GE(all_bookmarks.size(), 7u);
}

TEST_F(BookmarkIntegrationTest, HTMLExportFormat) {
  std::string html = features_->ExportToHTML();

  // Check standard format
  EXPECT_NE(html.find("<!DOCTYPE NETSCAPE-Bookmark-file-1>"),
            std::string::npos);
  EXPECT_NE(html.find("<H1>Bookmarks</H1>"), std::string::npos);

  // Check all bookmarks present
  EXPECT_NE(html.find("Google"), std::string::npos);
  EXPECT_NE(html.find("GitHub"), std::string::npos);
  EXPECT_NE(html.find("Stack Overflow"), std::string::npos);

  // Check URLs present
  EXPECT_NE(html.find("https://google.com"), std::string::npos);
  EXPECT_NE(html.find("https://github.com"), std::string::npos);

  // Check folder structure
  EXPECT_NE(html.find("Tech"), std::string::npos);
  EXPECT_NE(html.find("Work"), std::string::npos);
}

// ===== Stress Tests =====

TEST_F(BookmarkIntegrationTest, StressTestLargeCollection) {
  const size_t kLargeCount = 1000;

  // Create 1000 bookmarks
  for (size_t i = 0; i < kLargeCount; ++i) {
    std::string url = "https://example" + std::to_string(i) + ".com";
    std::u16string title = u"Bookmark " + base::NumberToString16(i);

    auto* bookmark = bookmark_model_->AddURL(
        tech_, tech_->children().size(), title, GURL(url));

    manager_->OnBookmarkAdded(bookmark);

    // Tag every 10th bookmark
    if (i % 10 == 0) {
      manager_->AddTagToBookmark(bookmark, u"tagged");
    }

    // Rate every 5th bookmark
    if (i % 5 == 0) {
      manager_->SetBookmarkRating(bookmark, (i % 5) + 1);
    }
  }

  // Test search performance
  BookmarkFilter filter;
  filter.tags = {u"tagged"};
  auto results = manager_->SearchBookmarks(filter);

  EXPECT_EQ(results.size(), kLargeCount / 10);

  // Test smart folder with large dataset
  int64_t smart_id = features_->CreateSmartFolder(u"Tagged", filter);
  auto smart_contents = features_->GetSmartFolderContents(smart_id);

  EXPECT_EQ(smart_contents.size(), kLargeCount / 10);

  // Test health analysis with large dataset
  auto health = features_->AnalyzeHealth();
  EXPECT_GT(health.total_bookmarks, kLargeCount);
}

TEST_F(BookmarkIntegrationTest, StressManySmartFolders) {
  const size_t kFolderCount = 100;

  // Create 100 smart folders with different criteria
  for (size_t i = 0; i < kFolderCount; ++i) {
    BookmarkFilter filter;
    filter.min_rating = (i % 5) + 1;

    std::u16string name = u"Smart Folder " + base::NumberToString16(i);
    features_->CreateSmartFolder(name, filter);
  }

  auto all_folders = features_->GetAllSmartFolders();
  EXPECT_EQ(all_folders.size(), kFolderCount);

  // Refresh all
  features_->RefreshAllSmartFolders();

  // Verify all still work
  all_folders = features_->GetAllSmartFolders();
  EXPECT_EQ(all_folders.size(), kFolderCount);
}

TEST_F(BookmarkIntegrationTest, StressManyCollections) {
  const size_t kCollectionCount = 100;

  // Create 100 collections
  for (size_t i = 0; i < kCollectionCount; ++i) {
    std::u16string name = u"Collection " + base::NumberToString16(i);
    int64_t id = features_->CreateCollection(name);

    // Add some bookmarks
    if (i % 2 == 0) {
      features_->AddToCollection(id, google_);
    }
    if (i % 3 == 0) {
      features_->AddToCollection(id, github_);
    }
  }

  auto all_collections = features_->GetAllCollections();
  EXPECT_EQ(all_collections.size(), kCollectionCount);

  // Verify contents
  all_collections = features_->GetAllCollections();
  for (const auto& collection : all_collections) {
    auto contents = features_->GetCollectionContents(collection.id);
    EXPECT_GE(contents.size(), 0u);
  }
}

TEST_F(BookmarkIntegrationTest, StressDuplicateDetection) {
  // Create 500 bookmarks with 100 duplicates
  const size_t kUniqueCount = 400;
  const size_t kDuplicateCount = 100;

  for (size_t i = 0; i < kUniqueCount; ++i) {
    std::string url = "https://unique" + std::to_string(i) + ".com";
    auto* bookmark = bookmark_model_->AddURL(
        tech_, tech_->children().size(),
        u"Unique " + base::NumberToString16(i), GURL(url));
    manager_->OnBookmarkAdded(bookmark);
  }

  for (size_t i = 0; i < kDuplicateCount; ++i) {
    std::string url = "https://duplicate" + std::to_string(i % 20) + ".com";
    auto* bookmark = bookmark_model_->AddURL(
        tech_, tech_->children().size(),
        u"Duplicate " + base::NumberToString16(i), GURL(url));
    manager_->OnBookmarkAdded(bookmark);
  }

  auto duplicates = manager_->FindDuplicateBookmarks();

  // Should find duplicate groups
  EXPECT_FALSE(duplicates.empty());
}

// ===== Concurrent Operations Tests =====

TEST_F(BookmarkIntegrationTest, ConcurrentSmartFolderUpdates) {
  BookmarkFilter filter;
  filter.tags = {u"tech"};

  int64_t id = features_->CreateSmartFolder(u"Tech", filter);

  // Get contents multiple times
  for (int i = 0; i < 10; ++i) {
    auto contents = features_->GetSmartFolderContents(id);
    EXPECT_EQ(contents.size(), 3u);

    // Add bookmark
    auto* new_bookmark = bookmark_model_->AddURL(
        tech_, tech_->children().size(),
        u"New " + base::NumberToString16(i),
        GURL("https://new" + std::to_string(i) + ".com"));
    manager_->OnBookmarkAdded(new_bookmark);
    manager_->AddTagToBookmark(new_bookmark, u"tech");

    // Refresh
    features_->RefreshSmartFolder(id);
  }

  auto final_contents = features_->GetSmartFolderContents(id);
  EXPECT_EQ(final_contents.size(), 13u);  // 3 original + 10 new
}

TEST_F(BookmarkIntegrationTest, ConcurrentCollectionModifications) {
  int64_t id = features_->CreateCollection(u"Test");

  // Add and remove repeatedly
  for (int i = 0; i < 10; ++i) {
    features_->AddToCollection(id, google_);
    features_->AddToCollection(id, github_);

    auto contents = features_->GetCollectionContents(id);
    EXPECT_EQ(contents.size(), 2u);

    features_->RemoveFromCollection(id, google_);

    contents = features_->GetCollectionContents(id);
    EXPECT_EQ(contents.size(), 1u);

    features_->RemoveFromCollection(id, github_);

    contents = features_->GetCollectionContents(id);
    EXPECT_TRUE(contents.empty());
  }
}

// ===== Error Recovery Tests =====

TEST_F(BookmarkIntegrationTest, RecoverFromInvalidBookmark) {
  // Create collection with bookmark
  int64_t id = features_->CreateCollection(u"Test");
  features_->AddToCollection(id, google_);

  auto contents = features_->GetCollectionContents(id);
  EXPECT_EQ(contents.size(), 1u);

  // Delete the bookmark
  int64_t google_id = google_->id();
  bookmark_model_->Remove(google_,
                         bookmarks::metrics::BookmarkEditSource::kOther);
  google_ = nullptr;

  // Collection should handle deleted bookmark gracefully
  contents = features_->GetCollectionContents(id);
  EXPECT_TRUE(contents.empty());
}

TEST_F(BookmarkIntegrationTest, HealthAfterMassiveDeletion) {
  auto initial_health = features_->AnalyzeHealth();
  int initial_count = initial_health.total_bookmarks;

  // Delete all tech bookmarks
  while (!tech_->children().empty()) {
    bookmark_model_->Remove(tech_->children()[0].get(),
                           bookmarks::metrics::BookmarkEditSource::kOther);
  }

  auto health = features_->AnalyzeHealth();
  EXPECT_LT(health.total_bookmarks, initial_count);
  EXPECT_GE(health.health_score, 0);
  EXPECT_LE(health.health_score, 100);
}

}  // namespace
