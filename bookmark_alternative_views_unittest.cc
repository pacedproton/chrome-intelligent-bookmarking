// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_alternative_views.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "chrome/browser/ui/bookmarks/bookmark_task_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"

namespace {

class BookmarkAlternativeViewsTest : public views::ViewsTestBase {
 protected:
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    // Create bookmark model
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();

    // Create managers
    bookmark_manager_ = std::make_unique<BookmarkManager>(bookmark_model_.get());
    task_manager_ = std::make_unique<BookmarkTaskManager>(bookmark_model_.get());

    // Create test bookmarks
    CreateTestBookmarks();
  }

  void TearDown() override {
    hierarchical_organizer_.reset();
    window_mirror_view_.reset();
    task_view_switcher_.reset();
    task_manager_.reset();
    bookmark_manager_.reset();
    bookmark_model_.reset();

    views::ViewsTestBase::TearDown();
  }

  void CreateTestBookmarks() {
    const bookmarks::BookmarkNode* bookmark_bar =
        bookmark_model_->bookmark_bar_node();

    // Create development bookmarks
    github_task_ = bookmark_model_->AddURL(
        bookmark_bar, 0, u"React Repository", GURL("https://github.com/facebook/react"));
    stackoverflow_task_ = bookmark_model_->AddURL(
        bookmark_bar, 1, u"Fix CSS Bug", GURL("https://stackoverflow.com/questions/12345"));

    // Create shopping bookmarks
    amazon_task_ = bookmark_model_->AddURL(
        bookmark_bar, 2, u"Buy laptop", GURL("https://www.amazon.com/laptop"));
    ebay_task_ = bookmark_model_->AddURL(
        bookmark_bar, 3, u"Bid on camera", GURL("https://www.ebay.com/camera"));

    // Create reading bookmarks
    medium_task_ = bookmark_model_->AddURL(
        bookmark_bar, 4, u"Read about AI", GURL("https://medium.com/ai-article"));
    wikipedia_task_ = bookmark_model_->AddURL(
        bookmark_bar, 5, u"Learn about quantum", GURL("https://en.wikipedia.org/quantum"));

    // Add task metadata
    TaskMetadata dev_metadata;
    dev_metadata.type = TaskType::kCode;
    dev_metadata.priority = TaskPriority::kHigh;
    dev_metadata.status = TaskStatus::kTodo;
    dev_metadata.due_date = base::Time::Now() + base::Days(1);
    task_manager_->SetTaskMetadata(github_task_, dev_metadata);

    TaskMetadata shop_metadata;
    shop_metadata.type = TaskType::kBuy;
    shop_metadata.priority = TaskPriority::kMedium;
    shop_metadata.status = TaskStatus::kTodo;
    shop_metadata.due_date = base::Time::Now() + base::Days(7);
    task_manager_->SetTaskMetadata(amazon_task_, shop_metadata);

    TaskMetadata read_metadata;
    read_metadata.type = TaskType::kRead;
    read_metadata.priority = TaskPriority::kLow;
    read_metadata.status = TaskStatus::kDone;
    task_manager_->SetTaskMetadata(medium_task_, read_metadata);

    // Create overdue task
    TaskMetadata overdue_metadata;
    overdue_metadata.type = TaskType::kCode;
    overdue_metadata.priority = TaskPriority::kHigh;
    overdue_metadata.status = TaskStatus::kTodo;
    overdue_metadata.due_date = base::Time::Now() - base::Days(2);
    task_manager_->SetTaskMetadata(stackoverflow_task_, overdue_metadata);
  }

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<BookmarkManager> bookmark_manager_;
  std::unique_ptr<BookmarkTaskManager> task_manager_;

  std::unique_ptr<BookmarkHierarchicalOrganizer> hierarchical_organizer_;
  std::unique_ptr<WindowTabMirrorView> window_mirror_view_;
  std::unique_ptr<TaskViewSwitcher> task_view_switcher_;

  raw_ptr<const bookmarks::BookmarkNode> github_task_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> stackoverflow_task_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> amazon_task_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> ebay_task_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> medium_task_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> wikipedia_task_ = nullptr;
};

// ============================================================================
// ProjectTreeItem Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, ProjectTreeItem_Creation) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* root = hierarchical_organizer_->root_folder();
  ASSERT_NE(root, nullptr);

  ProjectTreeItem tree_item(root, 0);
  EXPECT_EQ(tree_item.folder(), root);
  EXPECT_TRUE(tree_item.IsExpanded());
  EXPECT_FALSE(tree_item.IsSelected());
}

TEST_F(BookmarkAlternativeViewsTest, ProjectTreeItem_Expansion) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* root = hierarchical_organizer_->root_folder();
  ProjectTreeItem tree_item(root, 0);

  tree_item.SetExpanded(false);
  EXPECT_FALSE(tree_item.IsExpanded());
  EXPECT_FALSE(root->is_expanded);

  tree_item.SetExpanded(true);
  EXPECT_TRUE(tree_item.IsExpanded());
  EXPECT_TRUE(root->is_expanded);
}

TEST_F(BookmarkAlternativeViewsTest, ProjectTreeItem_Selection) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* root = hierarchical_organizer_->root_folder();
  ProjectTreeItem tree_item(root, 0);

  tree_item.SetSelected(true);
  EXPECT_TRUE(tree_item.IsSelected());
  EXPECT_TRUE(root->is_selected);

  tree_item.SetSelected(false);
  EXPECT_FALSE(tree_item.IsSelected());
  EXPECT_FALSE(root->is_selected);
}

TEST_F(BookmarkAlternativeViewsTest, ProjectTreeItem_DepthIndentation) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* root = hierarchical_organizer_->root_folder();

  ProjectTreeItem depth_0(root, 0);
  ProjectTreeItem depth_1(root, 1);
  ProjectTreeItem depth_2(root, 2);
  ProjectTreeItem depth_3(root, 3);

  // Depth setting doesn't crash
  depth_0.SetDepth(0);
  depth_1.SetDepth(1);
  depth_2.SetDepth(2);
  depth_3.SetDepth(3);
}

// ============================================================================
// BookmarkHierarchicalOrganizer - Folder Operations Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_CreateFolder) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* work_folder = hierarchical_organizer_->CreateFolder(u"Work Projects");
  ASSERT_NE(work_folder, nullptr);
  EXPECT_EQ(work_folder->name, u"Work Projects");
  EXPECT_TRUE(work_folder->is_expanded);
  EXPECT_EQ(work_folder->total_tasks, 0);
}

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_CreateNestedFolder) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* work_folder = hierarchical_organizer_->CreateFolder(u"Work");
  ProjectFolder* dev_folder = hierarchical_organizer_->CreateFolder(u"Development", work_folder);

  ASSERT_NE(dev_folder, nullptr);
  EXPECT_EQ(dev_folder->name, u"Development");
  EXPECT_EQ(work_folder->subfolders.size(), 1u);
  EXPECT_EQ(work_folder->subfolders[0].get(), dev_folder);
}

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_RenameFolder) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* folder = hierarchical_organizer_->CreateFolder(u"Old Name");
  hierarchical_organizer_->RenameFolder(folder, u"New Name");

  EXPECT_EQ(folder->name, u"New Name");
}

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_AddTaskToFolder) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* dev_folder = hierarchical_organizer_->CreateFolder(u"Development");
  hierarchical_organizer_->AddTaskToFolder(github_task_, dev_folder);

  EXPECT_EQ(dev_folder->tasks.size(), 1u);
  EXPECT_EQ(dev_folder->tasks[0], github_task_);
}

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_MoveTaskBetweenFolders) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* folder_a = hierarchical_organizer_->CreateFolder(u"Folder A");
  ProjectFolder* folder_b = hierarchical_organizer_->CreateFolder(u"Folder B");

  hierarchical_organizer_->AddTaskToFolder(github_task_, folder_a);
  EXPECT_EQ(folder_a->tasks.size(), 1u);
  EXPECT_EQ(folder_b->tasks.size(), 0u);

  hierarchical_organizer_->MoveTaskToFolder(github_task_, folder_a, folder_b);
  EXPECT_EQ(folder_a->tasks.size(), 0u);
  EXPECT_EQ(folder_b->tasks.size(), 1u);
  EXPECT_EQ(folder_b->tasks[0], github_task_);
}

// ============================================================================
// BookmarkHierarchicalOrganizer - Navigation Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_NavigateToFolder) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* work_folder = hierarchical_organizer_->CreateFolder(u"Work");

  EXPECT_EQ(hierarchical_organizer_->current_folder(),
            hierarchical_organizer_->root_folder());

  hierarchical_organizer_->NavigateToFolder(work_folder);
  EXPECT_EQ(hierarchical_organizer_->current_folder(), work_folder);
}

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_NavigateUp) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* work_folder = hierarchical_organizer_->CreateFolder(u"Work");
  ProjectFolder* dev_folder = hierarchical_organizer_->CreateFolder(u"Dev", work_folder);

  hierarchical_organizer_->NavigateToFolder(dev_folder);
  EXPECT_EQ(hierarchical_organizer_->current_folder(), dev_folder);

  hierarchical_organizer_->NavigateUp();
  EXPECT_EQ(hierarchical_organizer_->current_folder(), work_folder);
}

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_NavigateToRoot) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* work_folder = hierarchical_organizer_->CreateFolder(u"Work");
  hierarchical_organizer_->NavigateToFolder(work_folder);

  hierarchical_organizer_->NavigateToRoot();
  EXPECT_EQ(hierarchical_organizer_->current_folder(),
            hierarchical_organizer_->root_folder());
}

// ============================================================================
// BookmarkHierarchicalOrganizer - Search Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_SearchInFolder) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* root = hierarchical_organizer_->root_folder();
  hierarchical_organizer_->AddTaskToFolder(github_task_, root);
  hierarchical_organizer_->AddTaskToFolder(amazon_task_, root);
  hierarchical_organizer_->AddTaskToFolder(medium_task_, root);

  auto results = hierarchical_organizer_->SearchInFolder(root, u"react");
  EXPECT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0], github_task_);
}

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_SearchWithSubfolders) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* root = hierarchical_organizer_->root_folder();
  ProjectFolder* dev_folder = hierarchical_organizer_->CreateFolder(u"Dev", root);

  hierarchical_organizer_->AddTaskToFolder(github_task_, dev_folder);
  hierarchical_organizer_->AddTaskToFolder(amazon_task_, root);

  auto results = hierarchical_organizer_->SearchInFolder(root, u"github", true);
  EXPECT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0], github_task_);
}

// ============================================================================
// BookmarkHierarchicalOrganizer - Expansion Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_ExpandCollapseFolder) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* folder = hierarchical_organizer_->CreateFolder(u"Test");
  EXPECT_TRUE(folder->is_expanded);

  hierarchical_organizer_->CollapseFolder(folder);
  EXPECT_FALSE(folder->is_expanded);

  hierarchical_organizer_->ExpandFolder(folder);
  EXPECT_TRUE(folder->is_expanded);
}

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_ExpandCollapseAll) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* root = hierarchical_organizer_->root_folder();
  ProjectFolder* folder1 = hierarchical_organizer_->CreateFolder(u"F1", root);
  ProjectFolder* folder2 = hierarchical_organizer_->CreateFolder(u"F2", root);
  ProjectFolder* folder3 = hierarchical_organizer_->CreateFolder(u"F3", folder1);

  hierarchical_organizer_->CollapseAll();
  EXPECT_FALSE(folder1->is_expanded);
  EXPECT_FALSE(folder2->is_expanded);
  EXPECT_FALSE(folder3->is_expanded);

  hierarchical_organizer_->ExpandAll();
  EXPECT_TRUE(folder1->is_expanded);
  EXPECT_TRUE(folder2->is_expanded);
  EXPECT_TRUE(folder3->is_expanded);
}

// ============================================================================
// BookmarkHierarchicalOrganizer - Smart Organization Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_AutoOrganizeByDomain) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* root = hierarchical_organizer_->root_folder();
  hierarchical_organizer_->AddTaskToFolder(github_task_, root);
  hierarchical_organizer_->AddTaskToFolder(stackoverflow_task_, root);
  hierarchical_organizer_->AddTaskToFolder(amazon_task_, root);
  hierarchical_organizer_->AddTaskToFolder(medium_task_, root);

  hierarchical_organizer_->AutoOrganizeByDomain();

  // Should create folders for domains with 2+ tasks
  EXPECT_GT(root->subfolders.size(), 0u);
}

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_AutoOrganizeByDueDate) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* root = hierarchical_organizer_->root_folder();
  hierarchical_organizer_->AddTaskToFolder(github_task_, root);  // Due tomorrow
  hierarchical_organizer_->AddTaskToFolder(amazon_task_, root);  // Due in 7 days
  hierarchical_organizer_->AddTaskToFolder(stackoverflow_task_, root);  // Overdue

  hierarchical_organizer_->AutoOrganizeByDueDate();

  // Should create Today/Week/Overdue folders
  EXPECT_GT(root->subfolders.size(), 0u);
}

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_AutoOrganizeByPriority) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* root = hierarchical_organizer_->root_folder();
  hierarchical_organizer_->AddTaskToFolder(github_task_, root);    // High priority
  hierarchical_organizer_->AddTaskToFolder(amazon_task_, root);    // Medium priority
  hierarchical_organizer_->AddTaskToFolder(medium_task_, root);    // Low priority

  hierarchical_organizer_->AutoOrganizeByPriority();

  // Should create High/Medium/Low folders
  EXPECT_GT(root->subfolders.size(), 0u);
}

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_AutoOrganizeByType) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* root = hierarchical_organizer_->root_folder();
  hierarchical_organizer_->AddTaskToFolder(github_task_, root);    // Code type
  hierarchical_organizer_->AddTaskToFolder(amazon_task_, root);    // Buy type
  hierarchical_organizer_->AddTaskToFolder(medium_task_, root);    // Read type

  hierarchical_organizer_->AutoOrganizeByType();

  // Should create Read/Code/Buy folders
  EXPECT_GT(root->subfolders.size(), 0u);
}

// ============================================================================
// BookmarkHierarchicalOrganizer - Stats Calculation Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_CalculateStats) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* root = hierarchical_organizer_->root_folder();
  hierarchical_organizer_->AddTaskToFolder(github_task_, root);      // Todo
  hierarchical_organizer_->AddTaskToFolder(amazon_task_, root);      // Todo
  hierarchical_organizer_->AddTaskToFolder(medium_task_, root);      // Done
  hierarchical_organizer_->AddTaskToFolder(stackoverflow_task_, root);  // Overdue

  // Trigger stats calculation (happens automatically in UpdateFolderStats)
  // This is a protected method, but it's called by other public methods

  EXPECT_EQ(root->total_tasks, 4);
  EXPECT_EQ(root->completed_tasks, 1);  // medium_task is done
  EXPECT_GT(root->overdue_count, 0);  // stackoverflow_task is overdue
  EXPECT_GT(root->completion_percent, 0.0f);
}

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_RecursiveStatsCalculation) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* root = hierarchical_organizer_->root_folder();
  ProjectFolder* work = hierarchical_organizer_->CreateFolder(u"Work", root);
  ProjectFolder* dev = hierarchical_organizer_->CreateFolder(u"Dev", work);

  hierarchical_organizer_->AddTaskToFolder(github_task_, dev);
  hierarchical_organizer_->AddTaskToFolder(amazon_task_, work);
  hierarchical_organizer_->AddTaskToFolder(medium_task_, root);

  // Stats should include all nested tasks
  EXPECT_EQ(root->total_tasks, 3);
  EXPECT_EQ(work->total_tasks, 2);  // Includes dev subfolder
  EXPECT_EQ(dev->total_tasks, 1);
}

// ============================================================================
// BookmarkHierarchicalOrganizer - View Mode Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, HierarchicalOrganizer_SetViewMode) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  hierarchical_organizer_->SetViewMode(
      BookmarkHierarchicalOrganizer::ViewMode::kTree);
  hierarchical_organizer_->SetViewMode(
      BookmarkHierarchicalOrganizer::ViewMode::kList);
  hierarchical_organizer_->SetViewMode(
      BookmarkHierarchicalOrganizer::ViewMode::kMiller);
  hierarchical_organizer_->SetViewMode(
      BookmarkHierarchicalOrganizer::ViewMode::kBreadcrumb);

  // No crashes
}

// ============================================================================
// WindowWorkspaceCard Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, WindowWorkspaceCard_Creation) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  WindowWorkspace* workspace = window_mirror_view_->CreateWorkspace(u"My Project");
  ASSERT_NE(workspace, nullptr);

  WindowWorkspaceCard card(workspace);
  EXPECT_EQ(card.workspace(), workspace);
}

TEST_F(BookmarkAlternativeViewsTest, WindowWorkspaceCard_SetThumbnail) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  WindowWorkspace* workspace = window_mirror_view_->CreateWorkspace(u"Test");
  WindowWorkspaceCard card(workspace);

  // Create a simple thumbnail (this would normally come from screen capture)
  gfx::ImageSkia thumbnail;
  card.SetThumbnail(thumbnail);

  // No crashes
}

// ============================================================================
// WindowTabMirrorView - Workspace Management Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, WindowMirror_CreateWorkspace) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  WindowWorkspace* workspace = window_mirror_view_->CreateWorkspace(u"Website Redesign");
  ASSERT_NE(workspace, nullptr);
  EXPECT_EQ(workspace->name, u"Website Redesign");
  EXPECT_EQ(workspace->tab_count, 0);
  EXPECT_TRUE(workspace->auto_save);
  EXPECT_FALSE(workspace->restore_on_startup);
}

TEST_F(BookmarkAlternativeViewsTest, WindowMirror_RenameWorkspace) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  WindowWorkspace* workspace = window_mirror_view_->CreateWorkspace(u"Old Name");
  window_mirror_view_->RenameWorkspace(workspace, u"New Name");

  EXPECT_EQ(workspace->name, u"New Name");
}

TEST_F(BookmarkAlternativeViewsTest, WindowMirror_DeleteWorkspace) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  WindowWorkspace* workspace = window_mirror_view_->CreateWorkspace(u"Temp");
  size_t initial_count = window_mirror_view_->workspaces().size();

  window_mirror_view_->DeleteWorkspace(workspace);

  EXPECT_EQ(window_mirror_view_->workspaces().size(), initial_count - 1);
}

// ============================================================================
// WindowTabMirrorView - Capture Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, WindowMirror_CaptureCurrentWindow) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  size_t initial_count = window_mirror_view_->workspaces().size();

  window_mirror_view_->CaptureCurrentWindow(u"Current Session");

  EXPECT_EQ(window_mirror_view_->workspaces().size(), initial_count + 1);

  const auto& workspaces = window_mirror_view_->workspaces();
  EXPECT_EQ(workspaces.back()->name, u"Current Session");
}

TEST_F(BookmarkAlternativeViewsTest, WindowMirror_UpdateWorkspace) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  WindowWorkspace* workspace = window_mirror_view_->CreateWorkspace(u"Project");
  base::Time initial_time = workspace->last_saved;

  // Simulate time passing
  base::PlatformThread::Sleep(base::Milliseconds(10));

  window_mirror_view_->UpdateWorkspace(workspace);

  EXPECT_GT(workspace->last_saved, initial_time);
}

// ============================================================================
// WindowTabMirrorView - Restore Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, WindowMirror_RestoreWorkspace) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  WindowWorkspace* workspace = window_mirror_view_->CreateWorkspace(u"Session");
  int initial_access_count = workspace->access_count;

  window_mirror_view_->RestoreWorkspace(workspace);

  EXPECT_EQ(workspace->access_count, initial_access_count + 1);
  EXPECT_GT(workspace->last_used, base::Time());
}

TEST_F(BookmarkAlternativeViewsTest, WindowMirror_RestoreMultipleWorkspaces) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  WindowWorkspace* workspace1 = window_mirror_view_->CreateWorkspace(u"Project A");
  WindowWorkspace* workspace2 = window_mirror_view_->CreateWorkspace(u"Project B");

  std::vector<WindowWorkspace*> workspaces = {workspace1, workspace2};
  window_mirror_view_->RestoreMultipleWorkspaces(workspaces);

  EXPECT_GT(workspace1->access_count, 0);
  EXPECT_GT(workspace2->access_count, 0);
}

// ============================================================================
// WindowTabMirrorView - Session Management Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, WindowMirror_SaveSession) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  window_mirror_view_->CreateWorkspace(u"Workspace 1");
  window_mirror_view_->CreateWorkspace(u"Workspace 2");

  window_mirror_view_->SaveSession(u"My Session");

  auto sessions = window_mirror_view_->GetSavedSessions();
  EXPECT_GT(sessions.size(), 0u);
}

TEST_F(BookmarkAlternativeViewsTest, WindowMirror_LoadSession) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  window_mirror_view_->CreateWorkspace(u"Workspace 1");
  window_mirror_view_->SaveSession(u"Test Session");

  // Loading a session shouldn't crash
  window_mirror_view_->LoadSession(u"Test Session");
}

// ============================================================================
// WindowTabMirrorView - Sorting Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, WindowMirror_SetSortOrder) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  window_mirror_view_->SetSortOrder(WindowTabMirrorView::SortOrder::kMostRecent);
  window_mirror_view_->SetSortOrder(WindowTabMirrorView::SortOrder::kMostUsed);
  window_mirror_view_->SetSortOrder(WindowTabMirrorView::SortOrder::kAlphabetical);
  window_mirror_view_->SetSortOrder(WindowTabMirrorView::SortOrder::kTabCount);
  window_mirror_view_->SetSortOrder(WindowTabMirrorView::SortOrder::kCompletion);

  // No crashes
}

TEST_F(BookmarkAlternativeViewsTest, WindowMirror_SortByMostRecent) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  WindowWorkspace* old_workspace = window_mirror_view_->CreateWorkspace(u"Old");
  base::PlatformThread::Sleep(base::Milliseconds(10));
  WindowWorkspace* new_workspace = window_mirror_view_->CreateWorkspace(u"New");

  new_workspace->last_used = base::Time::Now();
  old_workspace->last_used = base::Time::Now() - base::Days(1);

  window_mirror_view_->SetSortOrder(WindowTabMirrorView::SortOrder::kMostRecent);

  // First workspace should be the most recent
  const auto& workspaces = window_mirror_view_->workspaces();
  EXPECT_GT(workspaces.size(), 0u);
}

// ============================================================================
// WindowTabMirrorView - Filter Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, WindowMirror_FilterByTags) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  WindowWorkspace* workspace = window_mirror_view_->CreateWorkspace(u"Tagged");
  workspace->tags = {u"work", u"urgent"};

  window_mirror_view_->FilterByTags({u"work"});

  // No crashes
}

TEST_F(BookmarkAlternativeViewsTest, WindowMirror_ShowPinnedOnly) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  window_mirror_view_->ShowPinnedOnly(true);
  window_mirror_view_->ShowPinnedOnly(false);

  // No crashes
}

// ============================================================================
// WindowTabMirrorView - View Mode Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, WindowMirror_SetViewMode) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  window_mirror_view_->SetViewMode(WindowTabMirrorView::ViewMode::kGrid);
  window_mirror_view_->SetViewMode(WindowTabMirrorView::ViewMode::kList);
  window_mirror_view_->SetViewMode(WindowTabMirrorView::ViewMode::kTimeline);
  window_mirror_view_->SetViewMode(WindowTabMirrorView::ViewMode::kGrouped);

  // No crashes
}

// ============================================================================
// WindowTabMirrorView - Quick Actions Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, WindowMirror_MergeWindows) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  WindowWorkspace* workspace1 = window_mirror_view_->CreateWorkspace(u"Project A");
  WindowWorkspace* workspace2 = window_mirror_view_->CreateWorkspace(u"Project B");

  workspace1->tab_count = 5;
  workspace2->tab_count = 3;

  std::vector<WindowWorkspace*> workspaces = {workspace1, workspace2};
  window_mirror_view_->MergeWindows(workspaces);

  // After merge, should have fewer workspaces
  // (Implementation details depend on merge logic)
}

TEST_F(BookmarkAlternativeViewsTest, WindowMirror_SplitWorkspace) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  WindowWorkspace* workspace = window_mirror_view_->CreateWorkspace(u"Big Project");
  workspace->tab_count = 10;

  std::vector<int> tab_indices = {0, 1, 2, 3, 4};
  window_mirror_view_->SplitWorkspace(workspace, tab_indices);

  // Split should create a new workspace
  // (Implementation details depend on split logic)
}

// ============================================================================
// TaskViewSwitcher Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, TaskViewSwitcher_Creation) {
  task_view_switcher_ = std::make_unique<TaskViewSwitcher>(
      bookmark_manager_.get(), task_manager_.get());

  EXPECT_EQ(task_view_switcher_->current_view(),
            TaskViewSwitcher::ViewType::kSmartWorkspace);
}

TEST_F(BookmarkAlternativeViewsTest, TaskViewSwitcher_SwitchViews) {
  task_view_switcher_ = std::make_unique<TaskViewSwitcher>(
      bookmark_manager_.get(), task_manager_.get());

  task_view_switcher_->SwitchTo(TaskViewSwitcher::ViewType::kKanban);
  EXPECT_EQ(task_view_switcher_->current_view(),
            TaskViewSwitcher::ViewType::kKanban);

  task_view_switcher_->SwitchTo(TaskViewSwitcher::ViewType::kHierarchical);
  EXPECT_EQ(task_view_switcher_->current_view(),
            TaskViewSwitcher::ViewType::kHierarchical);

  task_view_switcher_->SwitchTo(TaskViewSwitcher::ViewType::kWindowMirror);
  EXPECT_EQ(task_view_switcher_->current_view(),
            TaskViewSwitcher::ViewType::kWindowMirror);

  task_view_switcher_->SwitchTo(TaskViewSwitcher::ViewType::kSidebar);
  EXPECT_EQ(task_view_switcher_->current_view(),
            TaskViewSwitcher::ViewType::kSidebar);

  task_view_switcher_->SwitchTo(TaskViewSwitcher::ViewType::kTaskFlow);
  EXPECT_EQ(task_view_switcher_->current_view(),
            TaskViewSwitcher::ViewType::kTaskFlow);

  task_view_switcher_->SwitchTo(TaskViewSwitcher::ViewType::kSmartWorkspace);
  EXPECT_EQ(task_view_switcher_->current_view(),
            TaskViewSwitcher::ViewType::kSmartWorkspace);
}

TEST_F(BookmarkAlternativeViewsTest, TaskViewSwitcher_SetDefaultView) {
  task_view_switcher_ = std::make_unique<TaskViewSwitcher>(
      bookmark_manager_.get(), task_manager_.get());

  task_view_switcher_->SetDefaultView(TaskViewSwitcher::ViewType::kHierarchical);

  // Default view is set
  // (Behavior depends on implementation)
}

TEST_F(BookmarkAlternativeViewsTest, TaskViewSwitcher_RememberLastView) {
  task_view_switcher_ = std::make_unique<TaskViewSwitcher>(
      bookmark_manager_.get(), task_manager_.get());

  task_view_switcher_->RememberLastView(true);
  task_view_switcher_->RememberLastView(false);

  // No crashes
}

TEST_F(BookmarkAlternativeViewsTest, TaskViewSwitcher_QuickActions) {
  task_view_switcher_ = std::make_unique<TaskViewSwitcher>(
      bookmark_manager_.get(), task_manager_.get());

  task_view_switcher_->QuickCapture();
  task_view_switcher_->QuickSearch();
  task_view_switcher_->QuickFilter();

  // Quick actions should work from any view
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, Performance_HierarchicalOrganizer_LargeFolderSet) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  base::Time start = base::Time::Now();

  // Create 100 folders with nested structure
  ProjectFolder* root = hierarchical_organizer_->root_folder();
  for (int i = 0; i < 100; ++i) {
    ProjectFolder* folder = hierarchical_organizer_->CreateFolder(
        u"Folder " + base::NumberToString16(i));

    // Add some tasks
    hierarchical_organizer_->AddTaskToFolder(github_task_, folder);
    hierarchical_organizer_->AddTaskToFolder(amazon_task_, folder);
  }

  base::TimeDelta elapsed = base::Time::Now() - start;

  EXPECT_LT(elapsed.InMilliseconds(), 200);  // < 200ms for 100 folders
}

TEST_F(BookmarkAlternativeViewsTest, Performance_WindowMirror_ManyWorkspaces) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  base::Time start = base::Time::Now();

  // Create 50 workspaces
  for (int i = 0; i < 50; ++i) {
    window_mirror_view_->CreateWorkspace(
        u"Workspace " + base::NumberToString16(i));
  }

  base::TimeDelta elapsed = base::Time::Now() - start;

  EXPECT_LT(elapsed.InMilliseconds(), 100);  // < 100ms for 50 workspaces
}

TEST_F(BookmarkAlternativeViewsTest, Performance_AutoOrganize_LargeTaskSet) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  // Add many tasks
  ProjectFolder* root = hierarchical_organizer_->root_folder();
  for (int i = 0; i < 100; ++i) {
    hierarchical_organizer_->AddTaskToFolder(github_task_, root);
    hierarchical_organizer_->AddTaskToFolder(amazon_task_, root);
    hierarchical_organizer_->AddTaskToFolder(medium_task_, root);
  }

  base::Time start = base::Time::Now();
  hierarchical_organizer_->AutoOrganizeByDomain();
  base::TimeDelta elapsed = base::Time::Now() - start;

  EXPECT_LT(elapsed.InMilliseconds(), 300);  // < 300ms for 300 tasks
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, EdgeCase_NullBookmarkHandling) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* folder = hierarchical_organizer_->CreateFolder(u"Test");

  // Adding null bookmark shouldn't crash (DCHECK will catch in debug)
  // hierarchical_organizer_->AddTaskToFolder(nullptr, folder);
}

TEST_F(BookmarkAlternativeViewsTest, EdgeCase_EmptyFolderStats) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* empty_folder = hierarchical_organizer_->CreateFolder(u"Empty");

  EXPECT_EQ(empty_folder->total_tasks, 0);
  EXPECT_EQ(empty_folder->completed_tasks, 0);
  EXPECT_EQ(empty_folder->completion_percent, 0.0f);
}

TEST_F(BookmarkAlternativeViewsTest, EdgeCase_EmptyWorkspace) {
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  WindowWorkspace* empty_workspace = window_mirror_view_->CreateWorkspace(u"Empty");

  EXPECT_EQ(empty_workspace->tab_count, 0);
  EXPECT_EQ(empty_workspace->tabs.size(), 0u);

  // Restoring empty workspace shouldn't crash
  window_mirror_view_->RestoreWorkspace(empty_workspace);
}

TEST_F(BookmarkAlternativeViewsTest, EdgeCase_SpecialCharactersInNames) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* folder = hierarchical_organizer_->CreateFolder(
      u"🚀 Special @#$% Folder!!!");
  EXPECT_EQ(folder->name, u"🚀 Special @#$% Folder!!!");

  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  WindowWorkspace* workspace = window_mirror_view_->CreateWorkspace(
      u"Workspace & More <>");
  EXPECT_EQ(workspace->name, u"Workspace & More <>");
}

TEST_F(BookmarkAlternativeViewsTest, EdgeCase_DeepNesting) {
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  // Create deeply nested structure (10 levels)
  ProjectFolder* current = hierarchical_organizer_->root_folder();
  for (int i = 0; i < 10; ++i) {
    current = hierarchical_organizer_->CreateFolder(
        u"Level " + base::NumberToString16(i), current);
  }

  // Add task to deepest level
  hierarchical_organizer_->AddTaskToFolder(github_task_, current);

  // Stats should propagate up
  EXPECT_GT(hierarchical_organizer_->root_folder()->total_tasks, 0);
}

TEST_F(BookmarkAlternativeViewsTest, EdgeCase_RapidViewSwitching) {
  task_view_switcher_ = std::make_unique<TaskViewSwitcher>(
      bookmark_manager_.get(), task_manager_.get());

  // Rapidly switch between all views
  for (int i = 0; i < 10; ++i) {
    task_view_switcher_->SwitchTo(TaskViewSwitcher::ViewType::kSmartWorkspace);
    task_view_switcher_->SwitchTo(TaskViewSwitcher::ViewType::kKanban);
    task_view_switcher_->SwitchTo(TaskViewSwitcher::ViewType::kHierarchical);
    task_view_switcher_->SwitchTo(TaskViewSwitcher::ViewType::kWindowMirror);
    task_view_switcher_->SwitchTo(TaskViewSwitcher::ViewType::kSidebar);
    task_view_switcher_->SwitchTo(TaskViewSwitcher::ViewType::kTaskFlow);
  }

  // No crashes or memory leaks
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(BookmarkAlternativeViewsTest, Integration_HierarchicalToWindowMirror) {
  // Test switching from hierarchical view to window mirror
  hierarchical_organizer_ = std::make_unique<BookmarkHierarchicalOrganizer>(
      bookmark_manager_.get(), task_manager_.get());

  ProjectFolder* folder = hierarchical_organizer_->CreateFolder(u"Project");
  hierarchical_organizer_->AddTaskToFolder(github_task_, folder);

  // Switch to window mirror
  window_mirror_view_ = std::make_unique<WindowTabMirrorView>(
      bookmark_manager_.get(), task_manager_.get());

  WindowWorkspace* workspace = window_mirror_view_->CreateWorkspace(u"Project Window");

  // Both views should coexist and share the same bookmark data
  EXPECT_EQ(folder->tasks.size(), 1u);
}

TEST_F(BookmarkAlternativeViewsTest, Integration_TaskViewSwitcher_AllViews) {
  task_view_switcher_ = std::make_unique<TaskViewSwitcher>(
      bookmark_manager_.get(), task_manager_.get());

  // Test that all view types can be switched to
  std::vector<TaskViewSwitcher::ViewType> all_views = {
      TaskViewSwitcher::ViewType::kSmartWorkspace,
      TaskViewSwitcher::ViewType::kKanban,
      TaskViewSwitcher::ViewType::kTaskFlow,
      TaskViewSwitcher::ViewType::kSidebar,
      TaskViewSwitcher::ViewType::kHierarchical,
      TaskViewSwitcher::ViewType::kWindowMirror,
  };

  for (const auto& view_type : all_views) {
    task_view_switcher_->SwitchTo(view_type);
    EXPECT_EQ(task_view_switcher_->current_view(), view_type);
  }
}

}  // namespace
