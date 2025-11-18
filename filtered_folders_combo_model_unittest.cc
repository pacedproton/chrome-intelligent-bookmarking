// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/filtered_folders_combo_model.h"

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

class FilteredFoldersComboModelTest : public testing::Test {
 public:
  FilteredFoldersComboModelTest() = default;
  ~FilteredFoldersComboModelTest() override = default;

  void SetUp() override {
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();

    // Create a test folder structure
    const BookmarkNode* bookmark_bar =
        bookmark_model_->bookmark_bar_node();

    // Root folders
    work_folder_ = bookmark_model_->AddFolder(
        bookmark_bar, 0, u"Work");
    personal_folder_ = bookmark_model_->AddFolder(
        bookmark_bar, 1, u"Personal");
    dev_folder_ = bookmark_model_->AddFolder(
        bookmark_bar, 2, u"Development");

    // Nested folders
    projects_folder_ = bookmark_model_->AddFolder(
        work_folder_, 0, u"Projects");
    meetings_folder_ = bookmark_model_->AddFolder(
        work_folder_, 1, u"Meetings");
    frontend_folder_ = bookmark_model_->AddFolder(
        dev_folder_, 0, u"Frontend");
    backend_folder_ = bookmark_model_->AddFolder(
        dev_folder_, 1, u"Backend");

    model_ = std::make_unique<FilteredFoldersComboModel>(
        bookmark_model_.get(), nullptr);
  }

  void TearDown() override {
    model_.reset();
    bookmark_model_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<BookmarkModel> bookmark_model_;
  std::unique_ptr<FilteredFoldersComboModel> model_;

  // Test folder nodes
  raw_ptr<const BookmarkNode> work_folder_;
  raw_ptr<const BookmarkNode> personal_folder_;
  raw_ptr<const BookmarkNode> dev_folder_;
  raw_ptr<const BookmarkNode> projects_folder_;
  raw_ptr<const BookmarkNode> meetings_folder_;
  raw_ptr<const BookmarkNode> frontend_folder_;
  raw_ptr<const BookmarkNode> backend_folder_;
};

// Basic functionality tests
TEST_F(FilteredFoldersComboModelTest, InitialState) {
  EXPECT_GT(model_->GetItemCount(), 0u);
  EXPECT_TRUE(model_->GetDefaultIndex().has_value());
}

TEST_F(FilteredFoldersComboModelTest, EmptyFilterShowsAllItems) {
  const size_t initial_count = model_->GetItemCount();
  model_->SetSearchFilter(u"");
  EXPECT_EQ(model_->GetItemCount(), initial_count);
}

// Search filter tests
TEST_F(FilteredFoldersComboModelTest, ExactMatchSearch) {
  model_->SetSearchFilter(u"Work");

  // Should find at least one item (the Work folder)
  EXPECT_GT(model_->GetItemCount(), 0u);

  // Verify "Work" folder is in results
  bool found_work = false;
  for (size_t i = 0; i < model_->GetItemCount(); ++i) {
    if (!model_->IsItemSeparatorAt(i) && !model_->IsItemTitleAt(i)) {
      std::u16string item_text = model_->GetItemAt(i);
      if (item_text == u"Work") {
        found_work = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found_work);
}

TEST_F(FilteredFoldersComboModelTest, PrefixMatchSearch) {
  model_->SetSearchFilter(u"Dev");

  bool found_development = false;
  for (size_t i = 0; i < model_->GetItemCount(); ++i) {
    if (!model_->IsItemSeparatorAt(i) && !model_->IsItemTitleAt(i)) {
      std::u16string item_text = model_->GetItemAt(i);
      if (item_text.find(u"Dev") == 0) {
        found_development = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found_development);
}

TEST_F(FilteredFoldersComboModelTest, SubstringSearch) {
  model_->SetSearchFilter(u"ject");

  // Should find "Projects" folder
  bool found_projects = false;
  for (size_t i = 0; i < model_->GetItemCount(); ++i) {
    if (!model_->IsItemSeparatorAt(i) && !model_->IsItemTitleAt(i)) {
      std::u16string item_text = model_->GetItemAt(i);
      if (item_text == u"Projects") {
        found_projects = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found_projects);
}

TEST_F(FilteredFoldersComboModelTest, FuzzySearch) {
  model_->SetSearchFilter(u"prj");

  // Fuzzy match should find "Projects"
  bool found_fuzzy_match = false;
  for (size_t i = 0; i < model_->GetItemCount(); ++i) {
    if (!model_->IsItemSeparatorAt(i) && !model_->IsItemTitleAt(i)) {
      std::u16string item_text = model_->GetItemAt(i);
      if (item_text.find(u"Project") != std::u16string::npos) {
        found_fuzzy_match = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found_fuzzy_match);
}

TEST_F(FilteredFoldersComboModelTest, CaseInsensitiveSearch) {
  model_->SetSearchFilter(u"WORK");

  bool found_work = false;
  for (size_t i = 0; i < model_->GetItemCount(); ++i) {
    if (!model_->IsItemSeparatorAt(i) && !model_->IsItemTitleAt(i)) {
      std::u16string item_text = model_->GetItemAt(i);
      if (item_text == u"Work") {
        found_work = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found_work);
}

// Tag management tests
TEST_F(FilteredFoldersComboModelTest, AddTag) {
  model_->AddTagToFolder(work_folder_, u"important");

  auto tags = model_->GetTagsForFolder(work_folder_);
  EXPECT_EQ(tags.size(), 1u);
  EXPECT_EQ(tags[0], u"important");
}

TEST_F(FilteredFoldersComboModelTest, AddMultipleTags) {
  model_->AddTagToFolder(work_folder_, u"important");
  model_->AddTagToFolder(work_folder_, u"urgent");
  model_->AddTagToFolder(work_folder_, u"project");

  auto tags = model_->GetTagsForFolder(work_folder_);
  EXPECT_EQ(tags.size(), 3u);
}

TEST_F(FilteredFoldersComboModelTest, AddDuplicateTagIgnored) {
  model_->AddTagToFolder(work_folder_, u"important");
  model_->AddTagToFolder(work_folder_, u"important");

  auto tags = model_->GetTagsForFolder(work_folder_);
  EXPECT_EQ(tags.size(), 1u);
}

TEST_F(FilteredFoldersComboModelTest, RemoveTag) {
  model_->AddTagToFolder(work_folder_, u"important");
  model_->AddTagToFolder(work_folder_, u"urgent");

  model_->RemoveTagFromFolder(work_folder_, u"important");

  auto tags = model_->GetTagsForFolder(work_folder_);
  EXPECT_EQ(tags.size(), 1u);
  EXPECT_EQ(tags[0], u"urgent");
}

TEST_F(FilteredFoldersComboModelTest, GetAllTags) {
  model_->AddTagToFolder(work_folder_, u"work");
  model_->AddTagToFolder(personal_folder_, u"personal");
  model_->AddTagToFolder(dev_folder_, u"dev");

  auto all_tags = model_->GetAllTags();
  EXPECT_EQ(all_tags.size(), 3u);
}

// Tag filtering tests
TEST_F(FilteredFoldersComboModelTest, FilterByTag) {
  model_->AddTagToFolder(work_folder_, u"important");
  model_->AddTagToFolder(dev_folder_, u"important");
  model_->AddTagToFolder(personal_folder_, u"personal");

  std::vector<std::u16string> tag_filter = {u"important"};
  model_->SetTagFilter(tag_filter);

  // Should filter to only folders with "important" tag
  EXPECT_GT(model_->GetItemCount(), 0u);
}

TEST_F(FilteredFoldersComboModelTest, ClearTagFilter) {
  model_->AddTagToFolder(work_folder_, u"important");

  std::vector<std::u16string> tag_filter = {u"important"};
  model_->SetTagFilter(tag_filter);
  const size_t filtered_count = model_->GetItemCount();

  model_->ClearTagFilter();
  EXPECT_GT(model_->GetItemCount(), filtered_count);
}

// Tag search integration tests
TEST_F(FilteredFoldersComboModelTest, SearchByTag) {
  model_->AddTagToFolder(work_folder_, u"urgent");
  model_->AddTagToFolder(dev_folder_, u"review");

  model_->SetSearchFilter(u"urgent");

  // Should find the Work folder through its tag
  bool found = false;
  for (size_t i = 0; i < model_->GetItemCount(); ++i) {
    if (!model_->IsItemSeparatorAt(i) && !model_->IsItemTitleAt(i)) {
      std::u16string item_text = model_->GetItemAt(i);
      if (item_text == u"Work") {
        found = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found);
}

// Description tests
TEST_F(FilteredFoldersComboModelTest, SetDescription) {
  model_->SetFolderDescription(work_folder_, u"Work-related bookmarks");

  std::u16string desc = model_->GetFolderDescription(work_folder_);
  EXPECT_EQ(desc, u"Work-related bookmarks");
}

TEST_F(FilteredFoldersComboModelTest, UpdateDescription) {
  model_->SetFolderDescription(work_folder_, u"Old description");
  model_->SetFolderDescription(work_folder_, u"New description");

  std::u16string desc = model_->GetFolderDescription(work_folder_);
  EXPECT_EQ(desc, u"New description");
}

TEST_F(FilteredFoldersComboModelTest, SearchByDescription) {
  model_->SetFolderDescription(work_folder_,
                               u"Contains all work-related projects");

  model_->SetSearchFilter(u"projects");

  // Should find Work folder through its description
  bool found = false;
  for (size_t i = 0; i < model_->GetItemCount(); ++i) {
    if (!model_->IsItemSeparatorAt(i) && !model_->IsItemTitleAt(i)) {
      std::u16string item_text = model_->GetItemAt(i);
      if (item_text == u"Work") {
        found = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found);
}

// Metadata tests
TEST_F(FilteredFoldersComboModelTest, RecordFolderAccess) {
  model_->RecordFolderAccess(work_folder_);

  const BookmarkMetadata* metadata = model_->GetMetadata(work_folder_);
  ASSERT_NE(metadata, nullptr);
  EXPECT_EQ(metadata->access_count, 1);
  EXPECT_FALSE(metadata->last_accessed.is_null());
}

TEST_F(FilteredFoldersComboModelTest, MultipleAccesses) {
  model_->RecordFolderAccess(work_folder_);
  model_->RecordFolderAccess(work_folder_);
  model_->RecordFolderAccess(work_folder_);

  const BookmarkMetadata* metadata = model_->GetMetadata(work_folder_);
  ASSERT_NE(metadata, nullptr);
  EXPECT_EQ(metadata->access_count, 3);
}

// Smart folder tests
TEST_F(FilteredFoldersComboModelTest, FrequentlyUsedFolders) {
  model_->RecordFolderAccess(work_folder_);
  model_->RecordFolderAccess(work_folder_);
  model_->RecordFolderAccess(work_folder_);

  model_->RecordFolderAccess(dev_folder_);
  model_->RecordFolderAccess(dev_folder_);

  model_->RecordFolderAccess(personal_folder_);

  auto frequent_folders = model_->GetFrequentlyUsedFolders(3);

  // Should be ordered by access count: Work (3), Dev (2), Personal (1)
  EXPECT_GE(frequent_folders.size(), 1u);
  if (!frequent_folders.empty()) {
    EXPECT_EQ(frequent_folders[0], work_folder_);
  }
}

TEST_F(FilteredFoldersComboModelTest, RecentlyUsedFolders) {
  model_->RecordFolderAccess(work_folder_);
  task_environment_.FastForwardBy(base::Seconds(1));

  model_->RecordFolderAccess(personal_folder_);
  task_environment_.FastForwardBy(base::Seconds(1));

  model_->RecordFolderAccess(dev_folder_);

  auto recent_folders = model_->GetRecentlyUsedFolders(3);

  // Should be ordered by recency: Dev, Personal, Work
  EXPECT_GE(recent_folders.size(), 1u);
  if (!recent_folders.empty()) {
    EXPECT_EQ(recent_folders[0], dev_folder_);
  }
}

// Null safety tests
TEST_F(FilteredFoldersComboModelTest, NullNodeHandling) {
  model_->AddTagToFolder(nullptr, u"tag");
  EXPECT_EQ(model_->GetTagsForFolder(nullptr).size(), 0u);

  model_->SetFolderDescription(nullptr, u"description");
  EXPECT_TRUE(model_->GetFolderDescription(nullptr).empty());

  model_->RecordFolderAccess(nullptr);
  EXPECT_EQ(model_->GetMetadata(nullptr), nullptr);
}

TEST_F(FilteredFoldersComboModelTest, EmptyTagHandling) {
  model_->AddTagToFolder(work_folder_, u"");
  EXPECT_EQ(model_->GetTagsForFolder(work_folder_).size(), 0u);
}

// Full path display tests
TEST_F(FilteredFoldersComboModelTest, NestedFolderPaths) {
  model_->SetSearchFilter(u"");

  // Check that nested folders show their full path
  for (size_t i = 0; i < model_->GetItemCount(); ++i) {
    if (!model_->IsItemSeparatorAt(i) && !model_->IsItemTitleAt(i)) {
      std::u16string secondary = model_->GetDropDownSecondaryTextAt(i);
      std::u16string item = model_->GetItemAt(i);

      // Nested folders should have secondary text with path
      if (item == u"Projects" || item == u"Meetings") {
        EXPECT_FALSE(secondary.empty());
        EXPECT_NE(secondary.find(u" > "), std::u16string::npos);
      }
    }
  }
}

// Performance and edge case tests
TEST_F(FilteredFoldersComboModelTest, LargeNumberOfTags) {
  for (int i = 0; i < 100; ++i) {
    model_->AddTagToFolder(work_folder_,
                          u"tag" + base::NumberToString16(i));
  }

  auto tags = model_->GetTagsForFolder(work_folder_);
  EXPECT_EQ(tags.size(), 100u);
}

TEST_F(FilteredFoldersComboModelTest, SpecialCharactersInSearch) {
  const BookmarkNode* special_folder = bookmark_model_->AddFolder(
      bookmark_model_->bookmark_bar_node(), 0, u"C++ & Rust");

  model_->SetSearchFilter(u"C++");

  // Should handle special characters correctly
  bool found = false;
  for (size_t i = 0; i < model_->GetItemCount(); ++i) {
    if (!model_->IsItemSeparatorAt(i) && !model_->IsItemTitleAt(i)) {
      std::u16string item_text = model_->GetItemAt(i);
      if (item_text.find(u"C++") != std::u16string::npos) {
        found = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(FilteredFoldersComboModelTest, UnicodeSearch) {
  const BookmarkNode* unicode_folder = bookmark_model_->AddFolder(
      bookmark_model_->bookmark_bar_node(), 0, u"日本語");

  model_->SetSearchFilter(u"日本");

  bool found = false;
  for (size_t i = 0; i < model_->GetItemCount(); ++i) {
    if (!model_->IsItemSeparatorAt(i) && !model_->IsItemTitleAt(i)) {
      std::u16string item_text = model_->GetItemAt(i);
      if (item_text.find(u"日本") != std::u16string::npos) {
        found = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found);
}

}  // namespace
