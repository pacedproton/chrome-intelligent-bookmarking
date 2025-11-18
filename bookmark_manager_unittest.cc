// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

class BookmarkManagerTest : public testing::Test {
 public:
  BookmarkManagerTest() = default;
  ~BookmarkManagerTest() override = default;

  void SetUp() override {
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();

    const BookmarkNode* bookmark_bar = bookmark_model_->bookmark_bar_node();

    // Create test bookmarks
    bookmark1_ = bookmark_model_->AddURL(bookmark_bar, 0, u"Google",
                                         GURL("https://google.com"));
    bookmark2_ = bookmark_model_->AddURL(bookmark_bar, 1, u"GitHub",
                                         GURL("https://github.com"));
    bookmark3_ = bookmark_model_->AddURL(bookmark_bar, 2, u"Chromium",
                                         GURL("https://chromium.org"));

    manager_ = std::make_unique<BookmarkManager>(bookmark_model_.get());
  }

  void TearDown() override {
    manager_.reset();
    bookmark_model_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<BookmarkModel> bookmark_model_;
  std::unique_ptr<BookmarkManager> manager_;

  raw_ptr<const BookmarkNode> bookmark1_;
  raw_ptr<const BookmarkNode> bookmark2_;
  raw_ptr<const BookmarkNode> bookmark3_;
};

// ===== Recently Added Tests =====

TEST_F(BookmarkManagerTest, TrackRecentlyAdded) {
  manager_->OnBookmarkAdded(bookmark1_);
  manager_->OnBookmarkAdded(bookmark2_);

  auto recent = manager_->GetRecentlyAddedBookmarks(10);

  ASSERT_EQ(recent.size(), 2u);
  EXPECT_EQ(recent[0], bookmark2_);  // Most recent first
  EXPECT_EQ(recent[1], bookmark1_);
}

TEST_F(BookmarkManagerTest, RecentlyAddedLimit) {
  manager_->OnBookmarkAdded(bookmark1_);
  manager_->OnBookmarkAdded(bookmark2_);
  manager_->OnBookmarkAdded(bookmark3_);

  auto recent = manager_->GetRecentlyAddedBookmarks(2);

  EXPECT_EQ(recent.size(), 2u);
}

TEST_F(BookmarkManagerTest, GetBookmarksAddedSince) {
  manager_->OnBookmarkAdded(bookmark1_);
  task_environment_.FastForwardBy(base::Seconds(5));

  const base::Time checkpoint = base::Time::Now();
  task_environment_.FastForwardBy(base::Seconds(5));

  manager_->OnBookmarkAdded(bookmark2_);
  manager_->OnBookmarkAdded(bookmark3_);

  auto recent = manager_->GetBookmarksAddedSince(checkpoint);

  EXPECT_EQ(recent.size(), 2u);
}

// ===== Tag Management Tests =====

TEST_F(BookmarkManagerTest, AddTag) {
  manager_->AddTagToBookmark(bookmark1_, u"important");

  auto tags = manager_->GetTagsForBookmark(bookmark1_);
  ASSERT_EQ(tags.size(), 1u);
  EXPECT_EQ(tags[0], u"important");
}

TEST_F(BookmarkManagerTest, AddMultipleTags) {
  manager_->AddTagToBookmark(bookmark1_, u"work");
  manager_->AddTagToBookmark(bookmark1_, u"reference");
  manager_->AddTagToBookmark(bookmark1_, u"important");

  auto tags = manager_->GetTagsForBookmark(bookmark1_);
  EXPECT_EQ(tags.size(), 3u);
}

TEST_F(BookmarkManagerTest, AddDuplicateTag) {
  manager_->AddTagToBookmark(bookmark1_, u"work");
  manager_->AddTagToBookmark(bookmark1_, u"work");

  auto tags = manager_->GetTagsForBookmark(bookmark1_);
  EXPECT_EQ(tags.size(), 1u);
}

TEST_F(BookmarkManagerTest, RemoveTag) {
  manager_->AddTagToBookmark(bookmark1_, u"work");
  manager_->AddTagToBookmark(bookmark1_, u"important");

  manager_->RemoveTagFromBookmark(bookmark1_, u"work");

  auto tags = manager_->GetTagsForBookmark(bookmark1_);
  ASSERT_EQ(tags.size(), 1u);
  EXPECT_EQ(tags[0], u"important");
}

TEST_F(BookmarkManagerTest, GetAllTags) {
  manager_->AddTagToBookmark(bookmark1_, u"work");
  manager_->AddTagToBookmark(bookmark2_, u"personal");
  manager_->AddTagToBookmark(bookmark3_, u"work");  // Duplicate

  auto all_tags = manager_->GetAllBookmarkTags();
  EXPECT_EQ(all_tags.size(), 2u);  // Unique tags only
}

// ===== Description Tests =====

TEST_F(BookmarkManagerTest, SetDescription) {
  manager_->SetBookmarkDescription(bookmark1_, u"Search engine");

  std::u16string desc = manager_->GetBookmarkDescription(bookmark1_);
  EXPECT_EQ(desc, u"Search engine");
}

TEST_F(BookmarkManagerTest, UpdateDescription) {
  manager_->SetBookmarkDescription(bookmark1_, u"Old description");
  manager_->SetBookmarkDescription(bookmark1_, u"New description");

  std::u16string desc = manager_->GetBookmarkDescription(bookmark1_);
  EXPECT_EQ(desc, u"New description");
}

// ===== Rating Tests =====

TEST_F(BookmarkManagerTest, SetRating) {
  manager_->SetBookmarkRating(bookmark1_, 5);

  EXPECT_EQ(manager_->GetBookmarkRating(bookmark1_), 5);
}

TEST_F(BookmarkManagerTest, RatingClamping) {
  manager_->SetBookmarkRating(bookmark1_, 10);  // Too high
  EXPECT_EQ(manager_->GetBookmarkRating(bookmark1_), 5);

  manager_->SetBookmarkRating(bookmark1_, -5);  // Too low
  EXPECT_EQ(manager_->GetBookmarkRating(bookmark1_), 0);
}

// ===== Favorite Tests =====

TEST_F(BookmarkManagerTest, SetFavorite) {
  manager_->SetBookmarkFavorite(bookmark1_, true);

  EXPECT_TRUE(manager_->IsBookmarkFavorite(bookmark1_));
  EXPECT_FALSE(manager_->IsBookmarkFavorite(bookmark2_));
}

TEST_F(BookmarkManagerTest, GetFavorites) {
  manager_->SetBookmarkFavorite(bookmark1_, true);
  manager_->SetBookmarkFavorite(bookmark3_, true);

  auto favorites = manager_->GetFavoriteBookmarks();

  EXPECT_EQ(favorites.size(), 2u);
}

// ===== Archive Tests =====

TEST_F(BookmarkManagerTest, SetArchived) {
  manager_->SetBookmarkArchived(bookmark1_, true);

  EXPECT_TRUE(manager_->IsBookmarkArchived(bookmark1_));
}

TEST_F(BookmarkManagerTest, ArchivedExcludedFromRecent) {
  manager_->OnBookmarkAdded(bookmark1_);
  manager_->OnBookmarkAdded(bookmark2_);

  manager_->SetBookmarkArchived(bookmark1_, true);

  auto recent = manager_->GetRecentlyAddedBookmarks(10);

  EXPECT_EQ(recent.size(), 1u);
  EXPECT_EQ(recent[0], bookmark2_);
}

// ===== Access Tracking Tests =====

TEST_F(BookmarkManagerTest, RecordAccess) {
  manager_->RecordBookmarkAccess(bookmark1_);
  manager_->RecordBookmarkAccess(bookmark1_);

  const auto* metadata = manager_->GetBookmarkMetadata(bookmark1_);
  ASSERT_NE(metadata, nullptr);
  EXPECT_EQ(metadata->access_count, 2);
}

// ===== Search and Filter Tests =====

TEST_F(BookmarkManagerTest, SearchByQuery) {
  manager_->AddTagToBookmark(bookmark1_, u"search");
  manager_->SetBookmarkDescription(bookmark2_, u"Code hosting");

  BookmarkFilter filter;
  filter.search_query = u"git";

  auto results = manager_->SearchBookmarks(filter);

  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0], bookmark2_);
}

TEST_F(BookmarkManagerTest, FilterByTag) {
  manager_->AddTagToBookmark(bookmark1_, u"important");
  manager_->AddTagToBookmark(bookmark2_, u"personal");

  BookmarkFilter filter;
  filter.tags.push_back(u"important");

  auto results = manager_->SearchBookmarks(filter);

  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0], bookmark1_);
}

TEST_F(BookmarkManagerTest, FilterByRating) {
  manager_->SetBookmarkRating(bookmark1_, 5);
  manager_->SetBookmarkRating(bookmark2_, 3);
  manager_->SetBookmarkRating(bookmark3_, 1);

  BookmarkFilter filter;
  filter.min_rating = 3;

  auto results = manager_->SearchBookmarks(filter);

  EXPECT_EQ(results.size(), 2u);
}

TEST_F(BookmarkManagerTest, FilterFavoritesOnly) {
  manager_->SetBookmarkFavorite(bookmark1_, true);

  BookmarkFilter filter;
  filter.favorites_only = true;

  auto results = manager_->SearchBookmarks(filter);

  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0], bookmark1_);
}

// ===== Sorting Tests =====

TEST_F(BookmarkManagerTest, SortAlphabetical) {
  auto sorted =
      manager_->GetSortedBookmarks(BookmarkSortOrder::kAlphabetical);

  ASSERT_GE(sorted.size(), 3u);
  EXPECT_EQ(sorted[0], bookmark3_);  // Chromium
  EXPECT_EQ(sorted[1], bookmark2_);  // GitHub
  EXPECT_EQ(sorted[2], bookmark1_);  // Google
}

TEST_F(BookmarkManagerTest, SortByRating) {
  manager_->SetBookmarkRating(bookmark1_, 5);
  manager_->SetBookmarkRating(bookmark2_, 3);
  manager_->SetBookmarkRating(bookmark3_, 4);

  auto sorted = manager_->GetSortedBookmarks(BookmarkSortOrder::kRating);

  ASSERT_GE(sorted.size(), 3u);
  EXPECT_EQ(sorted[0], bookmark1_);  // Rating 5
  EXPECT_EQ(sorted[1], bookmark3_);  // Rating 4
  EXPECT_EQ(sorted[2], bookmark2_);  // Rating 3
}

TEST_F(BookmarkManagerTest, SortByMostVisited) {
  manager_->RecordBookmarkAccess(bookmark2_);
  manager_->RecordBookmarkAccess(bookmark2_);
  manager_->RecordBookmarkAccess(bookmark2_);
  manager_->RecordBookmarkAccess(bookmark1_);

  auto sorted = manager_->GetSortedBookmarks(BookmarkSortOrder::kMostVisited);

  ASSERT_GE(sorted.size(), 3u);
  EXPECT_EQ(sorted[0], bookmark2_);  // 3 accesses
  EXPECT_EQ(sorted[1], bookmark1_);  // 1 access
}

// ===== Duplicate Detection Tests =====

TEST_F(BookmarkManagerTest, FindDuplicates) {
  // Add another bookmark with same URL as bookmark1
  const BookmarkNode* duplicate = bookmark_model_->AddURL(
      bookmark_model_->bookmark_bar_node(), 3, u"Google Duplicate",
      GURL("https://google.com"));

  auto duplicates = manager_->FindDuplicateBookmarks();

  ASSERT_EQ(duplicates.size(), 1u);
  EXPECT_EQ(duplicates[0].size(), 2u);
}

TEST_F(BookmarkManagerTest, FindByURL) {
  auto results = manager_->FindBookmarksByURL(u"https://google.com/");

  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0], bookmark1_);
}

// ===== Batch Operations Tests =====

TEST_F(BookmarkManagerTest, BatchAddTag) {
  std::vector<const BookmarkNode*> bookmarks = {bookmark1_, bookmark2_};

  manager_->BatchAddTag(bookmarks, u"batch-tag");

  EXPECT_EQ(manager_->GetTagsForBookmark(bookmark1_).size(), 1u);
  EXPECT_EQ(manager_->GetTagsForBookmark(bookmark2_).size(), 1u);
}

TEST_F(BookmarkManagerTest, BatchArchive) {
  std::vector<const BookmarkNode*> bookmarks = {bookmark1_, bookmark2_};

  manager_->BatchArchive(bookmarks);

  EXPECT_TRUE(manager_->IsBookmarkArchived(bookmark1_));
  EXPECT_TRUE(manager_->IsBookmarkArchived(bookmark2_));
  EXPECT_FALSE(manager_->IsBookmarkArchived(bookmark3_));
}

// ===== Export Tests =====

TEST_F(BookmarkManagerTest, ExportToJSON) {
  manager_->AddTagToBookmark(bookmark1_, u"tag1");
  manager_->SetBookmarkDescription(bookmark1_, u"Description");
  manager_->SetBookmarkRating(bookmark1_, 5);

  std::string json = manager_->ExportToJSON();

  EXPECT_FALSE(json.empty());
  EXPECT_NE(json.find("Google"), std::string::npos);
  EXPECT_NE(json.find("tag1"), std::string::npos);
}

TEST_F(BookmarkManagerTest, ExportSpecificBookmarks) {
  std::vector<const BookmarkNode*> bookmarks = {bookmark1_};

  std::string json = manager_->ExportBookmarksToJSON(bookmarks);

  EXPECT_FALSE(json.empty());
  EXPECT_NE(json.find("Google"), std::string::npos);
}

// ===== Statistics Tests =====

TEST_F(BookmarkManagerTest, GetTotalCount) {
  size_t count = manager_->GetTotalBookmarkCount();
  EXPECT_GE(count, 3u);
}

TEST_F(BookmarkManagerTest, GetFavoriteCount) {
  manager_->SetBookmarkFavorite(bookmark1_, true);
  manager_->SetBookmarkFavorite(bookmark2_, true);

  EXPECT_EQ(manager_->GetFavoriteCount(), 2u);
}

TEST_F(BookmarkManagerTest, GetUnreadCount) {
  manager_->RecordBookmarkAccess(bookmark1_);

  // bookmark2 and bookmark3 haven't been accessed
  size_t unread = manager_->GetUnreadCount();
  EXPECT_GE(unread, 2u);
}

TEST_F(BookmarkManagerTest, GetTopTags) {
  manager_->AddTagToBookmark(bookmark1_, u"popular");
  manager_->AddTagToBookmark(bookmark2_, u"popular");
  manager_->AddTagToBookmark(bookmark3_, u"popular");
  manager_->AddTagToBookmark(bookmark1_, u"rare");

  auto top_tags = manager_->GetTopTags(5);

  ASSERT_GE(top_tags.size(), 2u);
  EXPECT_EQ(top_tags[0].first, u"popular");
  EXPECT_EQ(top_tags[0].second, 3u);
}

// ===== Cleanup Tests =====

TEST_F(BookmarkManagerTest, CleanupDeletedBookmarks) {
  manager_->AddTagToBookmark(bookmark1_, u"test");

  // Simulate bookmark deletion by removing it
  bookmark_model_->Remove(bookmark1_,
                         bookmarks::metrics::BookmarkEditSource::kOther);

  manager_->CleanupDeletedBookmarks();

  // Metadata should be cleaned up
  EXPECT_EQ(manager_->GetTagsForBookmark(bookmark1_).size(), 0u);
}

TEST_F(BookmarkManagerTest, ClearAllMetadata) {
  manager_->AddTagToBookmark(bookmark1_, u"test");
  manager_->SetBookmarkRating(bookmark2_, 5);

  manager_->ClearAllMetadata();

  EXPECT_EQ(manager_->GetTagsForBookmark(bookmark1_).size(), 0u);
  EXPECT_EQ(manager_->GetBookmarkRating(bookmark2_), 0);
}

// ===== Edge Cases =====

TEST_F(BookmarkManagerTest, NullBookmarkHandling) {
  manager_->AddTagToBookmark(nullptr, u"tag");
  EXPECT_EQ(manager_->GetTagsForBookmark(nullptr).size(), 0u);

  manager_->SetBookmarkDescription(nullptr, u"desc");
  EXPECT_TRUE(manager_->GetBookmarkDescription(nullptr).empty());

  manager_->RecordBookmarkAccess(nullptr);
  EXPECT_EQ(manager_->GetBookmarkMetadata(nullptr), nullptr);
}

TEST_F(BookmarkManagerTest, FolderNodeHandling) {
  const BookmarkNode* folder = bookmark_model_->AddFolder(
      bookmark_model_->bookmark_bar_node(), 0, u"Test Folder");

  // Should not track folders in recently added
  manager_->OnBookmarkAdded(folder);

  auto recent = manager_->GetRecentlyAddedBookmarks(10);
  EXPECT_EQ(std::ranges::find(recent, folder), recent.end());
}

}  // namespace
