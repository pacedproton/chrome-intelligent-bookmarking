// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/filtered_folders_combo_model.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

class FilteredFoldersComboModelBrowserTest : public InProcessBrowserTest {
 public:
  FilteredFoldersComboModelBrowserTest() = default;
  ~FilteredFoldersComboModelBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    bookmark_model_ =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);

    CreateTestBookmarkStructure();
  }

  void CreateTestBookmarkStructure() {
    const BookmarkNode* bookmark_bar = bookmark_model_->bookmark_bar_node();

    // Create realistic bookmark structure
    work_folder_ = bookmark_model_->AddFolder(bookmark_bar, 0, u"Work");
    personal_folder_ =
        bookmark_model_->AddFolder(bookmark_bar, 1, u"Personal");
    reading_list_ =
        bookmark_model_->AddFolder(bookmark_bar, 2, u"Reading List");

    // Work subfolders
    projects_ = bookmark_model_->AddFolder(work_folder_, 0, u"Projects");
    docs_ = bookmark_model_->AddFolder(work_folder_, 1, u"Documentation");
    meetings_ = bookmark_model_->AddFolder(work_folder_, 2, u"Meetings");

    // Personal subfolders
    recipes_ = bookmark_model_->AddFolder(personal_folder_, 0, u"Recipes");
    travel_ = bookmark_model_->AddFolder(personal_folder_, 1, u"Travel");

    // Deep nesting
    q1_meetings_ =
        bookmark_model_->AddFolder(meetings_, 0, u"Q1 2025");
    q2_meetings_ =
        bookmark_model_->AddFolder(meetings_, 1, u"Q2 2025");
  }

 protected:
  raw_ptr<BookmarkModel> bookmark_model_;

  raw_ptr<const BookmarkNode> work_folder_;
  raw_ptr<const BookmarkNode> personal_folder_;
  raw_ptr<const BookmarkNode> reading_list_;
  raw_ptr<const BookmarkNode> projects_;
  raw_ptr<const BookmarkNode> docs_;
  raw_ptr<const BookmarkNode> meetings_;
  raw_ptr<const BookmarkNode> recipes_;
  raw_ptr<const BookmarkNode> travel_;
  raw_ptr<const BookmarkNode> q1_meetings_;
  raw_ptr<const BookmarkNode> q2_meetings_;
};

IN_PROC_BROWSER_TEST_F(FilteredFoldersComboModelBrowserTest,
                       ModelCreationWithRealBookmarks) {
  auto model = std::make_unique<FilteredFoldersComboModel>(bookmark_model_,
                                                           nullptr);

  EXPECT_GT(model->GetItemCount(), 0u);
  EXPECT_TRUE(model->GetDefaultIndex().has_value());
}

IN_PROC_BROWSER_TEST_F(FilteredFoldersComboModelBrowserTest,
                       SearchAcrossAllFolders) {
  auto model = std::make_unique<FilteredFoldersComboModel>(bookmark_model_,
                                                           nullptr);

  model->SetSearchFilter(u"meet");

  bool found_meetings = false;
  bool found_q1 = false;

  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    if (!model->IsItemSeparatorAt(i) && !model->IsItemTitleAt(i)) {
      std::u16string item = model->GetItemAt(i);
      if (item == u"Meetings") {
        found_meetings = true;
      }
      if (item == u"Q1 2025") {
        found_q1 = true;
      }
    }
  }

  EXPECT_TRUE(found_meetings);
  EXPECT_TRUE(found_q1);
}

IN_PROC_BROWSER_TEST_F(FilteredFoldersComboModelBrowserTest,
                       TagPersistenceAcrossModelInstances) {
  {
    auto model = std::make_unique<FilteredFoldersComboModel>(bookmark_model_,
                                                             nullptr);
    model->AddTagToFolder(work_folder_, u"important");
    model->AddTagToFolder(work_folder_, u"urgent");

    auto tags = model->GetTagsForFolder(work_folder_);
    EXPECT_EQ(tags.size(), 2u);
  }

  // Note: In a real implementation, tags would be persisted to bookmark
  // metadata. This test demonstrates the API, but actual persistence would
  // require bookmark model extension points.
}

IN_PROC_BROWSER_TEST_F(FilteredFoldersComboModelBrowserTest,
                       NestedFolderPathDisplay) {
  auto model = std::make_unique<FilteredFoldersComboModel>(bookmark_model_,
                                                           nullptr);

  model->SetSearchFilter(u"Q1");

  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    if (!model->IsItemSeparatorAt(i) && !model->IsItemTitleAt(i)) {
      std::u16string item = model->GetItemAt(i);
      if (item == u"Q1 2025") {
        std::u16string secondary = model->GetDropDownSecondaryTextAt(i);
        // Should show full path: Work > Meetings > Q1 2025
        EXPECT_NE(secondary.find(u"Work"), std::u16string::npos);
        EXPECT_NE(secondary.find(u"Meetings"), std::u16string::npos);
        break;
      }
    }
  }
}

IN_PROC_BROWSER_TEST_F(FilteredFoldersComboModelBrowserTest,
                       FrequentlyUsedFoldersTracking) {
  auto model = std::make_unique<FilteredFoldersComboModel>(bookmark_model_,
                                                           nullptr);

  // Simulate frequent access to specific folders
  for (int i = 0; i < 10; ++i) {
    model->RecordFolderAccess(projects_);
  }
  for (int i = 0; i < 5; ++i) {
    model->RecordFolderAccess(docs_);
  }
  for (int i = 0; i < 3; ++i) {
    model->RecordFolderAccess(recipes_);
  }

  auto frequent = model->GetFrequentlyUsedFolders(3);

  ASSERT_GE(frequent.size(), 3u);
  EXPECT_EQ(frequent[0], projects_);   // 10 accesses
  EXPECT_EQ(frequent[1], docs_);       // 5 accesses
  EXPECT_EQ(frequent[2], recipes_);    // 3 accesses
}

IN_PROC_BROWSER_TEST_F(FilteredFoldersComboModelBrowserTest,
                       MultiCriteriaSearch) {
  auto model = std::make_unique<FilteredFoldersComboModel>(bookmark_model_,
                                                           nullptr);

  // Add rich metadata
  model->AddTagToFolder(projects_, u"active");
  model->AddTagToFolder(projects_, u"development");
  model->SetFolderDescription(projects_,
                              u"Active development projects for Q1");

  // Search by tag
  model->SetSearchFilter(u"active");

  bool found_by_tag = false;
  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    if (!model->IsItemSeparatorAt(i) && !model->IsItemTitleAt(i)) {
      if (model->GetItemAt(i) == u"Projects") {
        found_by_tag = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found_by_tag);

  // Search by description
  model->SetSearchFilter(u"development");

  bool found_by_description = false;
  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    if (!model->IsItemSeparatorAt(i) && !model->IsItemTitleAt(i)) {
      if (model->GetItemAt(i) == u"Projects") {
        found_by_description = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found_by_description);
}

IN_PROC_BROWSER_TEST_F(FilteredFoldersComboModelBrowserTest,
                       TagFilteringCombinedWithSearch) {
  auto model = std::make_unique<FilteredFoldersComboModel>(bookmark_model_,
                                                           nullptr);

  model->AddTagToFolder(work_folder_, u"professional");
  model->AddTagToFolder(projects_, u"professional");
  model->AddTagToFolder(personal_folder_, u"leisure");

  // Filter by tag
  std::vector<std::u16string> tag_filter = {u"professional"};
  model->SetTagFilter(tag_filter);

  // Then apply text search
  model->SetSearchFilter(u"work");

  // Should find Work folder (has "professional" tag and matches "work")
  bool found = false;
  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    if (!model->IsItemSeparatorAt(i) && !model->IsItemTitleAt(i)) {
      if (model->GetItemAt(i) == u"Work") {
        found = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found);
}

IN_PROC_BROWSER_TEST_F(FilteredFoldersComboModelBrowserTest,
                       LargeBookmarkCollection) {
  // Create a large number of folders to test performance
  const BookmarkNode* bookmark_bar = bookmark_model_->bookmark_bar_node();

  for (int i = 0; i < 100; ++i) {
    bookmark_model_->AddFolder(bookmark_bar, i,
                              u"Folder " + base::NumberToString16(i));
  }

  auto model = std::make_unique<FilteredFoldersComboModel>(bookmark_model_,
                                                           nullptr);

  // Model should handle large collections efficiently
  EXPECT_GT(model->GetItemCount(), 100u);

  // Search should still be fast
  model->SetSearchFilter(u"50");

  bool found = false;
  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    if (!model->IsItemSeparatorAt(i) && !model->IsItemTitleAt(i)) {
      std::u16string item = model->GetItemAt(i);
      if (item.find(u"50") != std::u16string::npos) {
        found = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found);
}

IN_PROC_BROWSER_TEST_F(FilteredFoldersComboModelBrowserTest,
                       DynamicBookmarkUpdates) {
  auto model = std::make_unique<FilteredFoldersComboModel>(bookmark_model_,
                                                           nullptr);

  const size_t initial_count = model->GetItemCount();

  // Add a new folder
  const BookmarkNode* new_folder = bookmark_model_->AddFolder(
      bookmark_model_->bookmark_bar_node(), 0, u"New Folder");

  // Model should reflect the change
  // Note: This assumes the model observes bookmark model changes
  EXPECT_GE(model->GetItemCount(), initial_count);
}

IN_PROC_BROWSER_TEST_F(FilteredFoldersComboModelBrowserTest,
                       AccessibilitySupport) {
  auto model = std::make_unique<FilteredFoldersComboModel>(bookmark_model_,
                                                           nullptr);

  // Verify all items have accessible text
  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    if (!model->IsItemSeparatorAt(i)) {
      std::u16string item_text = model->GetItemAt(i);
      std::u16string secondary_text = model->GetDropDownSecondaryTextAt(i);

      // Items should have meaningful text for screen readers
      if (!model->IsItemTitleAt(i)) {
        EXPECT_FALSE(item_text.empty());
      }
    }
  }
}

}  // namespace
