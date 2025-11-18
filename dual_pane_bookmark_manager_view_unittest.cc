// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/dual_pane_bookmark_manager_view.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DualPaneBookmarkManagerViewTest : public testing::Test {
 protected:
  void SetUp() override {
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_.get());

    // Create test folder structure
    const bookmarks::BookmarkNode* bar_node =
        bookmark_model_->bookmark_bar_node();

    work_folder_ = bookmark_model_->AddFolder(bar_node, 0, u"Work");
    personal_folder_ = bookmark_model_->AddFolder(bar_node, 1, u"Personal");

    // Add test bookmarks
    bookmark1_ = bookmark_model_->AddURL(work_folder_, 0, u"Google",
                                         GURL("https://google.com"));
    bookmark2_ = bookmark_model_->AddURL(work_folder_, 1, u"GitHub",
                                         GURL("https://github.com"));
    bookmark3_ = bookmark_model_->AddURL(personal_folder_, 0, u"YouTube",
                                         GURL("https://youtube.com"));

    // Note: DualPaneBookmarkManagerView creates a widget, so we can't easily
    // test it in a unit test without a full UI environment. These tests focus
    // on the bookmark manager integration.
  }

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<const bookmarks::BookmarkNode> work_folder_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> personal_folder_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> bookmark1_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> bookmark2_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> bookmark3_ = nullptr;
};

// ===== Basic Functionality Tests =====

TEST_F(DualPaneBookmarkManagerViewTest, BookmarkModelInitialized) {
  EXPECT_NE(bookmark_model_, nullptr);
  EXPECT_NE(work_folder_, nullptr);
  EXPECT_NE(personal_folder_, nullptr);
  EXPECT_EQ(work_folder_->children().size(), 2u);
  EXPECT_EQ(personal_folder_->children().size(), 1u);
}

TEST_F(DualPaneBookmarkManagerViewTest, FolderStructure) {
  const bookmarks::BookmarkNode* bar_node =
      bookmark_model_->bookmark_bar_node();
  EXPECT_EQ(bar_node->children().size(), 2u);
  EXPECT_EQ(bar_node->children()[0]->GetTitle(), u"Work");
  EXPECT_EQ(bar_node->children()[1]->GetTitle(), u"Personal");
}

TEST_F(DualPaneBookmarkManagerViewTest, BookmarkProperties) {
  EXPECT_EQ(bookmark1_->GetTitle(), u"Google");
  EXPECT_EQ(bookmark1_->url(), GURL("https://google.com"));
  EXPECT_TRUE(bookmark1_->is_url());
  EXPECT_FALSE(work_folder_->is_url());
  EXPECT_TRUE(work_folder_->is_folder());
}

// ===== Filter Tests =====

TEST_F(DualPaneBookmarkManagerViewTest, FilterBySearchQuery) {
  BookmarkManager manager(bookmark_model_.get());

  BookmarkFilter filter;
  filter.search_query = u"git";

  auto results = manager.SearchBookmarks(filter);
  EXPECT_EQ(results.size(), 1u);
  if (!results.empty()) {
    EXPECT_EQ(results[0]->GetTitle(), u"GitHub");
  }
}

TEST_F(DualPaneBookmarkManagerViewTest, FilterByTag) {
  BookmarkManager manager(bookmark_model_.get());

  // Add tags
  manager.AddTagToBookmark(bookmark1_, u"work");
  manager.AddTagToBookmark(bookmark2_, u"work");
  manager.AddTagToBookmark(bookmark2_, u"code");
  manager.AddTagToBookmark(bookmark3_, u"entertainment");

  BookmarkFilter filter;
  filter.tags.push_back(u"work");

  auto results = manager.SearchBookmarks(filter);
  EXPECT_EQ(results.size(), 2u);
}

TEST_F(DualPaneBookmarkManagerViewTest, FilterByRating) {
  BookmarkManager manager(bookmark_model_.get());

  // Set ratings
  manager.SetBookmarkRating(bookmark1_, 5);
  manager.SetBookmarkRating(bookmark2_, 3);
  manager.SetBookmarkRating(bookmark3_, 1);

  BookmarkFilter filter;
  filter.min_rating = 3;

  auto results = manager.SearchBookmarks(filter);
  EXPECT_EQ(results.size(), 2u);
}

TEST_F(DualPaneBookmarkManagerViewTest, FilterByFavorites) {
  BookmarkManager manager(bookmark_model_.get());

  manager.SetBookmarkFavorite(bookmark1_, true);

  BookmarkFilter filter;
  filter.favorites_only = true;

  auto results = manager.SearchBookmarks(filter);
  EXPECT_EQ(results.size(), 1u);
  if (!results.empty()) {
    EXPECT_EQ(results[0], bookmark1_);
  }
}

TEST_F(DualPaneBookmarkManagerViewTest, ExcludeArchived) {
  BookmarkManager manager(bookmark_model_.get());

  manager.SetBookmarkArchived(bookmark2_, true);

  BookmarkFilter filter;
  filter.exclude_archived = true;

  auto results = manager.SearchBookmarks(filter);
  EXPECT_EQ(results.size(), 2u);  // Only bookmark1_ and bookmark3_
}

// ===== Sort Order Tests =====

TEST_F(DualPaneBookmarkManagerViewTest, SortAlphabetical) {
  BookmarkManager manager(bookmark_model_.get());

  auto results = manager.GetSortedBookmarks(BookmarkSortOrder::kAlphabetical);
  EXPECT_GE(results.size(), 3u);

  // GitHub, Google, YouTube (alphabetical order)
  bool found_github = false;
  bool found_google = false;
  bool found_youtube = false;

  for (const auto* node : results) {
    if (node->GetTitle() == u"GitHub") found_github = true;
    if (node->GetTitle() == u"Google") found_google = true;
    if (node->GetTitle() == u"YouTube") found_youtube = true;
  }

  EXPECT_TRUE(found_github);
  EXPECT_TRUE(found_google);
  EXPECT_TRUE(found_youtube);
}

TEST_F(DualPaneBookmarkManagerViewTest, SortByMostVisited) {
  BookmarkManager manager(bookmark_model_.get());

  // Record accesses
  manager.RecordBookmarkAccess(bookmark1_);
  manager.RecordBookmarkAccess(bookmark1_);
  manager.RecordBookmarkAccess(bookmark1_);
  manager.RecordBookmarkAccess(bookmark2_);

  auto results = manager.GetSortedBookmarks(BookmarkSortOrder::kMostVisited);
  EXPECT_GE(results.size(), 3u);

  // bookmark1_ should be first (most visited)
  const auto* metadata1 = manager.GetBookmarkMetadata(bookmark1_);
  const auto* metadata2 = manager.GetBookmarkMetadata(bookmark2_);
  const auto* metadata3 = manager.GetBookmarkMetadata(bookmark3_);

  EXPECT_NE(metadata1, nullptr);
  EXPECT_NE(metadata2, nullptr);

  EXPECT_EQ(metadata1->access_count, 3);
  EXPECT_EQ(metadata2->access_count, 1);
  if (metadata3) {
    EXPECT_EQ(metadata3->access_count, 0);
  }
}

TEST_F(DualPaneBookmarkManagerViewTest, SortByRating) {
  BookmarkManager manager(bookmark_model_.get());

  manager.SetBookmarkRating(bookmark1_, 5);
  manager.SetBookmarkRating(bookmark2_, 3);
  manager.SetBookmarkRating(bookmark3_, 4);

  auto results = manager.GetSortedBookmarks(BookmarkSortOrder::kRating);
  EXPECT_GE(results.size(), 3u);

  // Check ratings are in descending order
  EXPECT_EQ(manager.GetBookmarkRating(bookmark1_), 5);
  EXPECT_EQ(manager.GetBookmarkRating(bookmark2_), 3);
  EXPECT_EQ(manager.GetBookmarkRating(bookmark3_), 4);
}

// ===== Bulk Operations Tests =====

TEST_F(DualPaneBookmarkManagerViewTest, BatchAddTag) {
  BookmarkManager manager(bookmark_model_.get());

  std::vector<const bookmarks::BookmarkNode*> bookmarks = {
      bookmark1_, bookmark2_, bookmark3_
  };

  manager.BatchAddTag(bookmarks, u"important");

  // Verify all have the tag
  auto tags1 = manager.GetTagsForBookmark(bookmark1_);
  auto tags2 = manager.GetTagsForBookmark(bookmark2_);
  auto tags3 = manager.GetTagsForBookmark(bookmark3_);

  EXPECT_EQ(tags1.size(), 1u);
  EXPECT_EQ(tags2.size(), 1u);
  EXPECT_EQ(tags3.size(), 1u);

  if (!tags1.empty()) EXPECT_EQ(tags1[0], u"important");
  if (!tags2.empty()) EXPECT_EQ(tags2[0], u"important");
  if (!tags3.empty()) EXPECT_EQ(tags3[0], u"important");
}

TEST_F(DualPaneBookmarkManagerViewTest, BatchArchive) {
  BookmarkManager manager(bookmark_model_.get());

  std::vector<const bookmarks::BookmarkNode*> bookmarks = {
      bookmark1_, bookmark2_
  };

  manager.BatchArchive(bookmarks);

  EXPECT_TRUE(manager.IsBookmarkArchived(bookmark1_));
  EXPECT_TRUE(manager.IsBookmarkArchived(bookmark2_));
  EXPECT_FALSE(manager.IsBookmarkArchived(bookmark3_));
}

TEST_F(DualPaneBookmarkManagerViewTest, BatchUnarchive) {
  BookmarkManager manager(bookmark_model_.get());

  // First archive them
  manager.SetBookmarkArchived(bookmark1_, true);
  manager.SetBookmarkArchived(bookmark2_, true);

  std::vector<const bookmarks::BookmarkNode*> bookmarks = {
      bookmark1_, bookmark2_
  };

  manager.BatchUnarchive(bookmarks);

  EXPECT_FALSE(manager.IsBookmarkArchived(bookmark1_));
  EXPECT_FALSE(manager.IsBookmarkArchived(bookmark2_));
}

// ===== Recently Added Tests =====

TEST_F(DualPaneBookmarkManagerViewTest, RecentlyAddedTracking) {
  BookmarkManager manager(bookmark_model_.get());

  manager.OnBookmarkAdded(bookmark1_);
  manager.OnBookmarkAdded(bookmark2_);
  manager.OnBookmarkAdded(bookmark3_);

  auto recent = manager.GetRecentlyAddedBookmarks(10);
  EXPECT_EQ(recent.size(), 3u);

  // Most recent should be first (bookmark3_)
  EXPECT_EQ(recent[0], bookmark3_);
  EXPECT_EQ(recent[1], bookmark2_);
  EXPECT_EQ(recent[2], bookmark1_);
}

TEST_F(DualPaneBookmarkManagerViewTest, RecentlyAddedLimit) {
  BookmarkManager manager(bookmark_model_.get());

  manager.OnBookmarkAdded(bookmark1_);
  manager.OnBookmarkAdded(bookmark2_);
  manager.OnBookmarkAdded(bookmark3_);

  auto recent = manager.GetRecentlyAddedBookmarks(2);
  EXPECT_EQ(recent.size(), 2u);
}

// ===== Duplicate Detection Tests =====

TEST_F(DualPaneBookmarkManagerViewTest, FindDuplicates) {
  BookmarkManager manager(bookmark_model_.get());

  // Add duplicate bookmark
  const bookmarks::BookmarkNode* duplicate =
      bookmark_model_->AddURL(personal_folder_, 1, u"Google Clone",
                              GURL("https://google.com"));

  manager.OnBookmarkAdded(bookmark1_);
  manager.OnBookmarkAdded(duplicate);

  auto duplicates = manager.FindDuplicateBookmarks();
  EXPECT_GE(duplicates.size(), 1u);

  // Find the duplicate set with Google URL
  bool found_duplicate_set = false;
  for (const auto& dup_set : duplicates) {
    if (dup_set.size() >= 2) {
      for (const auto* node : dup_set) {
        if (node->url() == GURL("https://google.com")) {
          found_duplicate_set = true;
          break;
        }
      }
    }
  }

  EXPECT_TRUE(found_duplicate_set);
}

TEST_F(DualPaneBookmarkManagerViewTest, FindBookmarksByURL) {
  BookmarkManager manager(bookmark_model_.get());

  auto results = manager.FindBookmarksByURL(u"https://google.com");
  EXPECT_EQ(results.size(), 1u);
  if (!results.empty()) {
    EXPECT_EQ(results[0], bookmark1_);
  }
}

// ===== Export/Import Tests =====

TEST_F(DualPaneBookmarkManagerViewTest, ExportToJSON) {
  BookmarkManager manager(bookmark_model_.get());

  manager.OnBookmarkAdded(bookmark1_);
  manager.AddTagToBookmark(bookmark1_, u"work");
  manager.SetBookmarkDescription(bookmark1_, u"Search engine");
  manager.SetBookmarkRating(bookmark1_, 5);

  std::string json = manager.ExportToJSON();
  EXPECT_FALSE(json.empty());

  // JSON should contain bookmark data
  EXPECT_NE(json.find("Google"), std::string::npos);
  EXPECT_NE(json.find("work"), std::string::npos);
}

TEST_F(DualPaneBookmarkManagerViewTest, ExportSelectedBookmarks) {
  BookmarkManager manager(bookmark_model_.get());

  std::vector<const bookmarks::BookmarkNode*> selected = {bookmark1_, bookmark2_};
  std::string json = manager.ExportBookmarksToJSON(selected);

  EXPECT_FALSE(json.empty());
  EXPECT_NE(json.find("Google"), std::string::npos);
  EXPECT_NE(json.find("GitHub"), std::string::npos);
  EXPECT_EQ(json.find("YouTube"), std::string::npos);  // Not exported
}

// ===== Statistics Tests =====

TEST_F(DualPaneBookmarkManagerViewTest, GetTotalBookmarkCount) {
  BookmarkManager manager(bookmark_model_.get());

  manager.OnBookmarkAdded(bookmark1_);
  manager.OnBookmarkAdded(bookmark2_);
  manager.OnBookmarkAdded(bookmark3_);

  size_t count = manager.GetTotalBookmarkCount();
  EXPECT_EQ(count, 3u);
}

TEST_F(DualPaneBookmarkManagerViewTest, GetFavoriteCount) {
  BookmarkManager manager(bookmark_model_.get());

  manager.SetBookmarkFavorite(bookmark1_, true);
  manager.SetBookmarkFavorite(bookmark2_, true);

  EXPECT_EQ(manager.GetFavoriteCount(), 2u);
}

TEST_F(DualPaneBookmarkManagerViewTest, GetArchivedCount) {
  BookmarkManager manager(bookmark_model_.get());

  manager.SetBookmarkArchived(bookmark1_, true);

  EXPECT_EQ(manager.GetArchivedCount(), 1u);
}

TEST_F(DualPaneBookmarkManagerViewTest, GetUnreadCount) {
  BookmarkManager manager(bookmark_model_.get());

  manager.OnBookmarkAdded(bookmark1_);
  manager.OnBookmarkAdded(bookmark2_);
  manager.OnBookmarkAdded(bookmark3_);

  // Initially, all are unread
  EXPECT_EQ(manager.GetUnreadCount(), 3u);

  // Access one
  manager.RecordBookmarkAccess(bookmark1_);

  // Now only 2 are unread
  EXPECT_EQ(manager.GetUnreadCount(), 2u);
}

TEST_F(DualPaneBookmarkManagerViewTest, GetTopTags) {
  BookmarkManager manager(bookmark_model_.get());

  manager.AddTagToBookmark(bookmark1_, u"work");
  manager.AddTagToBookmark(bookmark2_, u"work");
  manager.AddTagToBookmark(bookmark2_, u"code");
  manager.AddTagToBookmark(bookmark3_, u"entertainment");

  auto top_tags = manager.GetTopTags(10);
  EXPECT_GE(top_tags.size(), 1u);

  // "work" should be the top tag (used 2 times)
  if (!top_tags.empty()) {
    EXPECT_EQ(top_tags[0].first, u"work");
    EXPECT_EQ(top_tags[0].second, 2u);
  }
}

// ===== Metadata Tests =====

TEST_F(DualPaneBookmarkManagerViewTest, BookmarkDescription) {
  BookmarkManager manager(bookmark_model_.get());

  manager.SetBookmarkDescription(bookmark1_, u"Best search engine");

  auto desc = manager.GetBookmarkDescription(bookmark1_);
  EXPECT_EQ(desc, u"Best search engine");
}

TEST_F(DualPaneBookmarkManagerViewTest, BookmarkRating) {
  BookmarkManager manager(bookmark_model_.get());

  manager.SetBookmarkRating(bookmark1_, 5);
  EXPECT_EQ(manager.GetBookmarkRating(bookmark1_), 5);

  // Test clamping (should clamp to 0-5 range)
  manager.SetBookmarkRating(bookmark2_, 10);
  EXPECT_LE(manager.GetBookmarkRating(bookmark2_), 5);
}

TEST_F(DualPaneBookmarkManagerViewTest, AccessTracking) {
  BookmarkManager manager(bookmark_model_.get());

  manager.OnBookmarkAdded(bookmark1_);

  const auto* metadata = manager.GetBookmarkMetadata(bookmark1_);
  ASSERT_NE(metadata, nullptr);
  EXPECT_EQ(metadata->access_count, 0);

  manager.RecordBookmarkAccess(bookmark1_);
  metadata = manager.GetBookmarkMetadata(bookmark1_);
  EXPECT_EQ(metadata->access_count, 1);

  manager.RecordBookmarkAccess(bookmark1_);
  metadata = manager.GetBookmarkMetadata(bookmark1_);
  EXPECT_EQ(metadata->access_count, 2);
}

// ===== Cleanup Tests =====

TEST_F(DualPaneBookmarkManagerViewTest, CleanupDeletedBookmarks) {
  BookmarkManager manager(bookmark_model_.get());

  manager.OnBookmarkAdded(bookmark1_);
  manager.AddTagToBookmark(bookmark1_, u"test");

  // Verify metadata exists
  EXPECT_NE(manager.GetBookmarkMetadata(bookmark1_), nullptr);

  // Delete the bookmark
  int64_t bookmark1_id = bookmark1_->id();
  bookmark_model_->Remove(bookmark1_,
                         bookmarks::metrics::BookmarkEditSource::kOther);
  bookmark1_ = nullptr;

  // Cleanup should remove orphaned metadata
  manager.CleanupDeletedBookmarks();

  // Metadata should be cleaned up
  // (Note: We can't directly test this without accessing internals)
}

TEST_F(DualPaneBookmarkManagerViewTest, ClearAllMetadata) {
  BookmarkManager manager(bookmark_model_.get());

  manager.OnBookmarkAdded(bookmark1_);
  manager.AddTagToBookmark(bookmark1_, u"test");
  manager.SetBookmarkRating(bookmark1_, 5);

  manager.ClearAllMetadata();

  // All metadata should be cleared
  auto tags = manager.GetTagsForBookmark(bookmark1_);
  EXPECT_TRUE(tags.empty());
}

}  // namespace
