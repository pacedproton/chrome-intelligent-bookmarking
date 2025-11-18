// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/advanced_bookmark_features.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class AdvancedBookmarkFeaturesTest : public testing::Test {
 protected:
  void SetUp() override {
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_.get());

    manager_ = std::make_unique<BookmarkManager>(bookmark_model_.get());
    features_ = std::make_unique<AdvancedBookmarkFeatures>(
        bookmark_model_.get(), manager_.get());

    // Create test folder structure
    const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

    tech_folder_ = bookmark_model_->AddFolder(bar, 0, u"Tech");
    work_folder_ = bookmark_model_->AddFolder(bar, 1, u"Work");

    // Add test bookmarks
    google_ = bookmark_model_->AddURL(tech_folder_, 0, u"Google",
                                      GURL("https://google.com"));
    github_ = bookmark_model_->AddURL(tech_folder_, 1, u"GitHub",
                                      GURL("https://github.com"));
    stackoverflow_ = bookmark_model_->AddURL(
        tech_folder_, 2, u"Stack Overflow",
        GURL("https://stackoverflow.com"));

    work_docs_ = bookmark_model_->AddURL(work_folder_, 0, u"Work Docs",
                                         GURL("https://docs.company.com"));

    // Setup metadata
    manager_->OnBookmarkAdded(google_);
    manager_->OnBookmarkAdded(github_);
    manager_->OnBookmarkAdded(stackoverflow_);
    manager_->OnBookmarkAdded(work_docs_);

    // Add tags
    manager_->AddTagToBookmark(google_, u"search");
    manager_->AddTagToBookmark(google_, u"tech");
    manager_->AddTagToBookmark(github_, u"code");
    manager_->AddTagToBookmark(github_, u"tech");
    manager_->AddTagToBookmark(stackoverflow_, u"code");
    manager_->AddTagToBookmark(stackoverflow_, u"tech");
    manager_->AddTagToBookmark(work_docs_, u"work");

    // Add ratings
    manager_->SetBookmarkRating(google_, 5);
    manager_->SetBookmarkRating(github_, 5);
    manager_->SetBookmarkRating(stackoverflow_, 4);
    manager_->SetBookmarkRating(work_docs_, 3);

    // Simulate access
    manager_->RecordBookmarkAccess(google_);
    manager_->RecordBookmarkAccess(google_);
    manager_->RecordBookmarkAccess(google_);
    manager_->RecordBookmarkAccess(github_);
  }

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<BookmarkManager> manager_;
  std::unique_ptr<AdvancedBookmarkFeatures> features_;

  raw_ptr<const bookmarks::BookmarkNode> tech_folder_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> work_folder_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> google_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> github_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> stackoverflow_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> work_docs_ = nullptr;
};

// ===== Smart Folders Tests =====

TEST_F(AdvancedBookmarkFeaturesTest, CreateSmartFolder) {
  BookmarkFilter filter;
  filter.tags = {u"tech"};

  int64_t id = features_->CreateSmartFolder(u"Tech Bookmarks", filter);
  EXPECT_GT(id, 0);

  auto folders = features_->GetAllSmartFolders();
  EXPECT_EQ(folders.size(), 1u);
  EXPECT_EQ(folders[0].name, u"Tech Bookmarks");
  EXPECT_EQ(folders[0].id, id);
}

TEST_F(AdvancedBookmarkFeaturesTest, SmartFolderContents) {
  BookmarkFilter filter;
  filter.tags = {u"tech"};

  int64_t id = features_->CreateSmartFolder(u"Tech", filter);
  auto contents = features_->GetSmartFolderContents(id);

  // Should contain Google, GitHub, and Stack Overflow
  EXPECT_EQ(contents.size(), 3u);
}

TEST_F(AdvancedBookmarkFeaturesTest, SmartFolderHighRated) {
  BookmarkFilter filter;
  filter.min_rating = 5;

  int64_t id = features_->CreateSmartFolder(u"Top Rated", filter);
  auto contents = features_->GetSmartFolderContents(id);

  // Should contain Google and GitHub (both rated 5)
  EXPECT_EQ(contents.size(), 2u);
}

TEST_F(AdvancedBookmarkFeaturesTest, UpdateSmartFolder) {
  BookmarkFilter filter1;
  filter1.tags = {u"tech"};

  int64_t id = features_->CreateSmartFolder(u"Tech", filter1);
  auto contents1 = features_->GetSmartFolderContents(id);
  EXPECT_EQ(contents1.size(), 3u);

  // Update to filter by rating instead
  BookmarkFilter filter2;
  filter2.min_rating = 5;

  features_->UpdateSmartFolder(id, filter2);
  auto contents2 = features_->GetSmartFolderContents(id);
  EXPECT_EQ(contents2.size(), 2u);
}

TEST_F(AdvancedBookmarkFeaturesTest, DeleteSmartFolder) {
  BookmarkFilter filter;
  filter.tags = {u"tech"};

  int64_t id = features_->CreateSmartFolder(u"Tech", filter);
  EXPECT_EQ(features_->GetAllSmartFolders().size(), 1u);

  features_->DeleteSmartFolder(id);
  EXPECT_EQ(features_->GetAllSmartFolders().size(), 0u);
}

TEST_F(AdvancedBookmarkFeaturesTest, RefreshSmartFolder) {
  BookmarkFilter filter;
  filter.tags = {u"tech"};

  int64_t id = features_->CreateSmartFolder(u"Tech", filter);
  auto contents1 = features_->GetSmartFolderContents(id);
  EXPECT_EQ(contents1.size(), 3u);

  // Add a new tech bookmark
  auto* new_bookmark = bookmark_model_->AddURL(
      tech_folder_, 3, u"Mozilla", GURL("https://mozilla.org"));
  manager_->OnBookmarkAdded(new_bookmark);
  manager_->AddTagToBookmark(new_bookmark, u"tech");

  // Refresh should pick up the new bookmark
  features_->RefreshSmartFolder(id);
  auto contents2 = features_->GetSmartFolderContents(id);
  EXPECT_EQ(contents2.size(), 4u);
}

// ===== Collections Tests =====

TEST_F(AdvancedBookmarkFeaturesTest, CreateCollection) {
  int64_t id = features_->CreateCollection(u"My Collection");
  EXPECT_GT(id, 0);

  auto collections = features_->GetAllCollections();
  EXPECT_EQ(collections.size(), 1u);
  EXPECT_EQ(collections[0].name, u"My Collection");
}

TEST_F(AdvancedBookmarkFeaturesTest, AddToCollection) {
  int64_t id = features_->CreateCollection(u"Favorites");

  features_->AddToCollection(id, google_);
  features_->AddToCollection(id, github_);

  auto contents = features_->GetCollectionContents(id);
  EXPECT_EQ(contents.size(), 2u);
}

TEST_F(AdvancedBookmarkFeaturesTest, RemoveFromCollection) {
  int64_t id = features_->CreateCollection(u"Favorites");

  features_->AddToCollection(id, google_);
  features_->AddToCollection(id, github_);
  EXPECT_EQ(features_->GetCollectionContents(id).size(), 2u);

  features_->RemoveFromCollection(id, google_);
  EXPECT_EQ(features_->GetCollectionContents(id).size(), 1u);
}

TEST_F(AdvancedBookmarkFeaturesTest, CollectionAutoRule) {
  int64_t id = features_->CreateCollection(u"Tech Collection");

  BookmarkFilter rule;
  rule.tags = {u"tech"};

  features_->SetCollectionAutoRule(id, rule);

  auto contents = features_->GetCollectionContents(id);
  // Should auto-include all tech bookmarks
  EXPECT_EQ(contents.size(), 3u);
}

TEST_F(AdvancedBookmarkFeaturesTest, CollectionManualAndAuto) {
  int64_t id = features_->CreateCollection(u"Mixed");

  // Add manual bookmark
  features_->AddToCollection(id, work_docs_);

  // Add auto rule for tech
  BookmarkFilter rule;
  rule.tags = {u"tech"};
  features_->SetCollectionAutoRule(id, rule);

  auto contents = features_->GetCollectionContents(id);
  // Should have 4: work_docs (manual) + 3 tech (auto)
  EXPECT_EQ(contents.size(), 4u);
}

TEST_F(AdvancedBookmarkFeaturesTest, DeleteCollection) {
  int64_t id = features_->CreateCollection(u"Test");
  EXPECT_EQ(features_->GetAllCollections().size(), 1u);

  features_->DeleteCollection(id);
  EXPECT_EQ(features_->GetAllCollections().size(), 0u);
}

// ===== Health Analysis Tests =====

TEST_F(AdvancedBookmarkFeaturesTest, AnalyzeHealth) {
  auto health = features_->AnalyzeHealth();

  EXPECT_EQ(health.total_bookmarks, 4);
  EXPECT_EQ(health.untagged, 0);  // All have tags
  EXPECT_GE(health.health_score, 0);
  EXPECT_LE(health.health_score, 100);
}

TEST_F(AdvancedBookmarkFeaturesTest, FindUntaggedBookmarks) {
  // Add an untagged bookmark
  auto* untagged = bookmark_model_->AddURL(
      work_folder_, 1, u"Untagged", GURL("https://example.com"));
  manager_->OnBookmarkAdded(untagged);

  auto untagged_bookmarks = features_->FindUntaggedBookmarks();
  EXPECT_EQ(untagged_bookmarks.size(), 1u);
  EXPECT_EQ(untagged_bookmarks[0], untagged);
}

TEST_F(AdvancedBookmarkFeaturesTest, FindNeverVisitedBookmarks) {
  auto never_visited = features_->FindNeverVisitedBookmarks();

  // stackoverflow and work_docs were never accessed
  EXPECT_EQ(never_visited.size(), 2u);
}

TEST_F(AdvancedBookmarkFeaturesTest, HealthScore) {
  auto health = features_->AnalyzeHealth();

  // Should have high health score (all tagged, some visited, no broken links)
  EXPECT_GE(health.health_score, 70);
}

TEST_F(AdvancedBookmarkFeaturesTest, HealthRecommendations) {
  // Add some issues
  auto* untagged = bookmark_model_->AddURL(
      work_folder_, 1, u"Untagged", GURL("https://example.com"));
  manager_->OnBookmarkAdded(untagged);

  auto health = features_->AnalyzeHealth();
  EXPECT_FALSE(health.recommendations.empty());
}

// ===== Link Validation Tests =====

TEST_F(AdvancedBookmarkFeaturesTest, ValidateSingleLink) {
  bool callback_called = false;
  LinkValidationResult result;

  features_->ValidateLink(
      google_,
      base::BindOnce(
          [](bool* called, LinkValidationResult* out,
             LinkValidationResult res) {
            *called = true;
            *out = res;
          },
          &callback_called, &result));

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(result.bookmark, google_);
  EXPECT_NE(result.status, LinkStatus::kUnknown);
}

TEST_F(AdvancedBookmarkFeaturesTest, ValidateAllLinks) {
  bool callback_called = false;
  std::vector<LinkValidationResult> results;

  features_->ValidateAllLinks(base::BindOnce(
      [](bool* called, std::vector<LinkValidationResult>* out,
         std::vector<LinkValidationResult> res) {
        *called = true;
        *out = std::move(res);
      },
      &callback_called, &results));

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(results.size(), 4u);
}

TEST_F(AdvancedBookmarkFeaturesTest, ValidationCaching) {
  // First validation
  features_->ValidateAllLinks(base::BindOnce(
      [](std::vector<LinkValidationResult>) {}));

  auto cached1 = features_->GetCachedValidationResults();
  EXPECT_EQ(cached1.size(), 4u);

  // Clear cache
  features_->ClearValidationCache();

  auto cached2 = features_->GetCachedValidationResults();
  EXPECT_EQ(cached2.size(), 0u);
}

// ===== Related Bookmarks Tests =====

TEST_F(AdvancedBookmarkFeaturesTest, FindRelatedBookmarks) {
  auto related = features_->FindRelatedBookmarks(github_, 10);

  // Should find stackoverflow (both have 'code' tag) and google (both have 'tech' tag)
  EXPECT_GE(related.size(), 1u);
}

TEST_F(AdvancedBookmarkFeaturesTest, FindSameDomainBookmarks) {
  // Add another google.com bookmark
  auto* google_maps = bookmark_model_->AddURL(
      tech_folder_, 3, u"Google Maps",
      GURL("https://maps.google.com"));
  manager_->OnBookmarkAdded(google_maps);

  auto same_domain = features_->FindSameDomainBookmarks(google_);

  // Should find google_maps (same domain)
  EXPECT_EQ(same_domain.size(), 1u);
  EXPECT_EQ(same_domain[0], google_maps);
}

TEST_F(AdvancedBookmarkFeaturesTest, FindSimilarTaggedBookmarks) {
  auto similar = features_->FindSimilarTaggedBookmarks(github_, 1);

  // Should find stackoverflow (shares 'code' tag) and google (shares 'tech' tag)
  EXPECT_GE(similar.size(), 2u);
}

TEST_F(AdvancedBookmarkFeaturesTest, RelatedBookmarksScoring) {
  auto related = features_->FindRelatedBookmarks(github_, 10);

  if (!related.empty()) {
    // First result should have highest similarity
    for (size_t i = 1; i < related.size(); ++i) {
      EXPECT_GE(related[i-1].similarity_score, related[i].similarity_score);
    }
  }
}

// ===== HTML Import/Export Tests =====

TEST_F(AdvancedBookmarkFeaturesTest, ExportToHTML) {
  std::string html = features_->ExportToHTML();

  EXPECT_FALSE(html.empty());
  EXPECT_NE(html.find("<!DOCTYPE NETSCAPE-Bookmark-file-1>"),
            std::string::npos);
  EXPECT_NE(html.find("Google"), std::string::npos);
  EXPECT_NE(html.find("https://google.com"), std::string::npos);
}

TEST_F(AdvancedBookmarkFeaturesTest, ExportSpecificBookmarks) {
  std::vector<const bookmarks::BookmarkNode*> bookmarks = {google_, github_};
  std::string html = features_->ExportBookmarksToHTML(bookmarks);

  EXPECT_FALSE(html.empty());
  EXPECT_NE(html.find("Google"), std::string::npos);
  EXPECT_NE(html.find("GitHub"), std::string::npos);
  EXPECT_EQ(html.find("Stack Overflow"), std::string::npos);
}

TEST_F(AdvancedBookmarkFeaturesTest, ExportPreservesMetadata) {
  manager_->SetBookmarkDescription(google_, u"Search engine");

  std::vector<const bookmarks::BookmarkNode*> bookmarks = {google_};
  std::string html = features_->ExportBookmarksToHTML(bookmarks);

  // Should contain tags and rating
  EXPECT_NE(html.find("TAGS"), std::string::npos);
  EXPECT_NE(html.find("RATING"), std::string::npos);
}

TEST_F(AdvancedBookmarkFeaturesTest, ImportFromHTML) {
  std::string html = R"(
<!DOCTYPE NETSCAPE-Bookmark-file-1>
<TITLE>Bookmarks</TITLE>
<H1>Bookmarks</H1>
<DL><p>
    <DT><A HREF="https://example.com">Example Site</A>
    <DT><A HREF="https://test.com">Test Site</A>
</DL><p>
)";

  size_t imported = features_->ImportFromHTML(html);
  EXPECT_EQ(imported, 2u);
}

// ===== Quick Access Tests =====

TEST_F(AdvancedBookmarkFeaturesTest, GetMostUsedBookmarks) {
  auto most_used = features_->GetMostUsedBookmarks(10);

  EXPECT_FALSE(most_used.empty());
  // Google should be first (accessed 3 times)
  EXPECT_EQ(most_used[0], google_);
}

TEST_F(AdvancedBookmarkFeaturesTest, GetHighValueBookmarks) {
  auto high_value = features_->GetHighValueBookmarks(10);

  EXPECT_FALSE(high_value.empty());
  // Should include google and github (both rated 5)
  bool has_google = false;
  bool has_github = false;

  for (const auto* bookmark : high_value) {
    if (bookmark == google_) has_google = true;
    if (bookmark == github_) has_github = true;
  }

  EXPECT_TRUE(has_google);
  EXPECT_TRUE(has_github);
}

TEST_F(AdvancedBookmarkFeaturesTest, GetBookmarksNeedingAttention) {
  // Add untagged, never-visited bookmark
  auto* needs_attention = bookmark_model_->AddURL(
      work_folder_, 1, u"Old Bookmark",
      GURL("https://old.example.com"));
  manager_->OnBookmarkAdded(needs_attention);

  auto attention = features_->GetBookmarksNeedingAttention(10);

  EXPECT_FALSE(attention.empty());
}

TEST_F(AdvancedBookmarkFeaturesTest, MostUsedLimit) {
  auto most_used = features_->GetMostUsedBookmarks(2);

  // Should limit to 2 results
  EXPECT_LE(most_used.size(), 2u);
}

TEST_F(AdvancedBookmarkFeaturesTest, HighValueScoring) {
  auto high_value = features_->GetHighValueBookmarks(10);

  // Google should rank high (rating 5 + accessed 3 times)
  if (!high_value.empty()) {
    EXPECT_EQ(high_value[0], google_);
  }
}

// ===== Edge Cases =====

TEST_F(AdvancedBookmarkFeaturesTest, EmptySmartFolder) {
  BookmarkFilter filter;
  filter.tags = {u"nonexistent"};

  int64_t id = features_->CreateSmartFolder(u"Empty", filter);
  auto contents = features_->GetSmartFolderContents(id);

  EXPECT_TRUE(contents.empty());
}

TEST_F(AdvancedBookmarkFeaturesTest, EmptyCollection) {
  int64_t id = features_->CreateCollection(u"Empty");
  auto contents = features_->GetCollectionContents(id);

  EXPECT_TRUE(contents.empty());
}

TEST_F(AdvancedBookmarkFeaturesTest, InvalidSmartFolderId) {
  auto contents = features_->GetSmartFolderContents(999999);
  EXPECT_TRUE(contents.empty());
}

TEST_F(AdvancedBookmarkFeaturesTest, InvalidCollectionId) {
  auto contents = features_->GetCollectionContents(999999);
  EXPECT_TRUE(contents.empty());
}

TEST_F(AdvancedBookmarkFeaturesTest, NullBookmarkValidation) {
  bool callback_called = false;

  features_->ValidateLink(
      nullptr,
      base::BindOnce(
          [](bool* called, LinkValidationResult) {
            *called = true;
          },
          &callback_called));

  EXPECT_TRUE(callback_called);
}

TEST_F(AdvancedBookmarkFeaturesTest, RelatedBookmarksForNull) {
  auto related = features_->FindRelatedBookmarks(nullptr, 10);
  EXPECT_TRUE(related.empty());
}

TEST_F(AdvancedBookmarkFeaturesTest, HealthWithNoBookmarks) {
  // Create fresh model with no bookmarks
  auto empty_model = bookmarks::TestBookmarkClient::CreateModel();
  bookmarks::test::WaitForBookmarkModelToLoad(empty_model.get());

  auto empty_manager = std::make_unique<BookmarkManager>(empty_model.get());
  auto empty_features = std::make_unique<AdvancedBookmarkFeatures>(
      empty_model.get(), empty_manager.get());

  auto health = empty_features->AnalyzeHealth();

  EXPECT_EQ(health.total_bookmarks, 0);
  EXPECT_EQ(health.health_score, 100);  // Perfect health with no bookmarks
}

}  // namespace
