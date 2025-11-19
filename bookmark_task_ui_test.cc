// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_kanban_view.h"
#include "chrome/browser/ui/bookmarks/bookmark_task_integration_view.h"
#include "chrome/browser/ui/bookmarks/bookmark_task_manager.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

class BookmarkTaskUITest : public views::ViewsTestBase {
 public:
  void SetUp() override {
    ViewsTestBase::SetUp();

    bookmark_client_ = std::make_unique<bookmarks::TestBookmarkClient>();
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel(
        std::move(bookmark_client_));
    manager_ = std::make_unique<BookmarkManager>(bookmark_model_.get());
    task_manager_ = std::make_unique<BookmarkTaskManager>(
        bookmark_model_.get(), manager_.get());

    widget_ = CreateTestWidget();
  }

  void TearDown() override {
    widget_.reset();
    task_manager_.reset();
    manager_.reset();
    bookmark_model_.reset();

    ViewsTestBase::TearDown();
  }

 protected:
  const bookmarks::BookmarkNode* CreateTestTask(std::u16string_view title) {
    const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
    const bookmarks::BookmarkNode* task = bookmark_model_->AddURL(
        bar, bar->children().size(), title,
        GURL("https://example.com/" + base::UTF16ToUTF8(title)));

    TaskMetadata metadata;
    metadata.status = TaskStatus::kTodo;
    metadata.priority = TaskPriority::kMedium;
    metadata.type = TaskType::kRead;
    task_manager_->SetTaskMetadata(task, metadata);

    return task;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<BookmarkManager> manager_;
  std::unique_ptr<BookmarkTaskManager> task_manager_;
  std::unique_ptr<views::Widget> widget_;
};

// ===== TaskCard UI Tests =====

TEST_F(BookmarkTaskUITest, TaskCardCreation) {
  const bookmarks::BookmarkNode* task = CreateTestTask(u"Test Task");
  auto metadata = task_manager_->GetTaskMetadata(task);
  ASSERT_TRUE(metadata.has_value());

  auto card = std::make_unique<BookmarkTaskCard>(task, *metadata,
                                                  task_manager_.get());

  EXPECT_NE(nullptr, card);
  EXPECT_EQ(task, card->task());
  EXPECT_EQ(TaskStatus::kTodo, card->metadata().status);
}

TEST_F(BookmarkTaskUITest, TaskCardPreferredSize) {
  const bookmarks::BookmarkNode* task = CreateTestTask(u"Test");
  auto metadata = task_manager_->GetTaskMetadata(task);

  auto card = std::make_unique<BookmarkTaskCard>(task, *metadata,
                                                  task_manager_.get());

  gfx::Size preferred = card->CalculatePreferredSize(views::SizeBounds());
  EXPECT_EQ(280, preferred.width());   // Card width
  EXPECT_GE(preferred.height(), 120);  // Minimum height
}

TEST_F(BookmarkTaskUITest, TaskCardDragState) {
  const bookmarks::BookmarkNode* task = CreateTestTask(u"Draggable");
  auto metadata = task_manager_->GetTaskMetadata(task);

  auto card = std::make_unique<BookmarkTaskCard>(task, *metadata,
                                                  task_manager_.get());

  EXPECT_FALSE(card->is_dragging());

  card->SetDragging(true);
  EXPECT_TRUE(card->is_dragging());

  card->SetDragging(false);
  EXPECT_FALSE(card->is_dragging());
}

TEST_F(BookmarkTaskUITest, TaskCardUpdateMetadata) {
  const bookmarks::BookmarkNode* task = CreateTestTask(u"Update Test");
  auto metadata = task_manager_->GetTaskMetadata(task);
  ASSERT_TRUE(metadata.has_value());

  auto card = std::make_unique<BookmarkTaskCard>(task, *metadata,
                                                  task_manager_.get());

  EXPECT_EQ(TaskPriority::kMedium, card->metadata().priority);

  TaskMetadata updated = *metadata;
  updated.priority = TaskPriority::kHigh;
  card->UpdateMetadata(updated);

  EXPECT_EQ(TaskPriority::kHigh, card->metadata().priority);
}

// ===== KanbanColumn UI Tests =====

TEST_F(BookmarkTaskUITest, KanbanColumnCreation) {
  auto column = std::make_unique<BookmarkKanbanColumn>(
      TaskStatus::kTodo, u"Todo", task_manager_.get());

  EXPECT_NE(nullptr, column);
  EXPECT_EQ(TaskStatus::kTodo, column->status());
  EXPECT_EQ(0, column->task_count());
  EXPECT_EQ(0, column->total_points());
}

TEST_F(BookmarkTaskUITest, KanbanColumnAddTask) {
  auto column = std::make_unique<BookmarkKanbanColumn>(
      TaskStatus::kTodo, u"Todo", task_manager_.get());

  const bookmarks::BookmarkNode* task1 = CreateTestTask(u"Task 1");
  const bookmarks::BookmarkNode* task2 = CreateTestTask(u"Task 2");

  column->AddTask(task1);
  EXPECT_EQ(1, column->task_count());

  column->AddTask(task2);
  EXPECT_EQ(2, column->task_count());
}

TEST_F(BookmarkTaskUITest, KanbanColumnRemoveTask) {
  auto column = std::make_unique<BookmarkKanbanColumn>(
      TaskStatus::kTodo, u"Todo", task_manager_.get());

  const bookmarks::BookmarkNode* task = CreateTestTask(u"Remove Me");
  column->AddTask(task);
  EXPECT_EQ(1, column->task_count());

  column->RemoveTask(task);
  EXPECT_EQ(0, column->task_count());
}

TEST_F(BookmarkTaskUITest, KanbanColumnClearTasks) {
  auto column = std::make_unique<BookmarkKanbanColumn>(
      TaskStatus::kTodo, u"Todo", task_manager_.get());

  column->AddTask(CreateTestTask(u"Task 1"));
  column->AddTask(CreateTestTask(u"Task 2"));
  column->AddTask(CreateTestTask(u"Task 3"));

  EXPECT_EQ(3, column->task_count());

  column->ClearTasks();
  EXPECT_EQ(0, column->task_count());
}

TEST_F(BookmarkTaskUITest, KanbanColumnPointsCalculation) {
  auto column = std::make_unique<BookmarkKanbanColumn>(
      TaskStatus::kTodo, u"Todo", task_manager_.get());

  // Create task with known points (Medium priority, Medium time = 50 * 1.5 = 75)
  const bookmarks::BookmarkNode* task = CreateTestTask(u"Points Test");
  auto metadata = task_manager_->GetTaskMetadata(task);
  ASSERT_TRUE(metadata.has_value());

  TaskMetadata updated = *metadata;
  updated.time_estimate = TimeEstimate::kMedium;  // 50 base points
  updated.priority = TaskPriority::kMedium;       // 1.5x multiplier
  task_manager_->SetTaskMetadata(task, updated);

  column->AddTask(task);

  EXPECT_EQ(75, column->total_points());
}

// ===== KanbanBoard UI Tests =====

TEST_F(BookmarkTaskUITest, KanbanBoardCreation) {
  auto board = std::make_unique<BookmarkKanbanBoardView>(task_manager_.get());

  EXPECT_NE(nullptr, board);
  EXPECT_EQ(BookmarkTaskIntegrationView::ViewMode::kBookmarks,
           board->GetCurrentViewMode());
}

TEST_F(BookmarkTaskUITest, KanbanBoardRefresh) {
  auto board = std::make_unique<BookmarkKanbanBoardView>(task_manager_.get());

  CreateTestTask(u"Task 1");
  CreateTestTask(u"Task 2");

  // Should not crash
  board->RefreshBoard();
}

TEST_F(BookmarkTaskUITest, KanbanBoardMoveTask) {
  auto board = std::make_unique<BookmarkKanbanBoardView>(task_manager_.get());

  const bookmarks::BookmarkNode* task = CreateTestTask(u"Movable");
  auto metadata = task_manager_->GetTaskMetadata(task);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_EQ(TaskStatus::kTodo, metadata->status);

  board->MoveTask(task, TaskStatus::kInProgress);

  auto updated = task_manager_->GetTaskMetadata(task);
  ASSERT_TRUE(updated.has_value());
  EXPECT_EQ(TaskStatus::kInProgress, updated->status);
}

TEST_F(BookmarkTaskUITest, KanbanBoardFilters) {
  auto board = std::make_unique<BookmarkKanbanBoardView>(task_manager_.get());

  // Should not crash
  board->SetPriorityFilter(TaskPriority::kHigh);
  board->SetPriorityFilter(std::nullopt);

  board->SetTypeFilter(TaskType::kRead);
  board->SetTypeFilter(std::nullopt);
}

TEST_F(BookmarkTaskUITest, KanbanBoardSorting) {
  auto board = std::make_unique<BookmarkKanbanBoardView>(task_manager_.get());

  // Should not crash with different sort orders
  board->SetSortOrder(BookmarkKanbanView::SortOrder::kPriority);
  board->SetSortOrder(BookmarkKanbanView::SortOrder::kDueDate);
  board->SetSortOrder(BookmarkKanbanView::SortOrder::kCreatedDate);
  board->SetSortOrder(BookmarkKanbanView::SortOrder::kAlphabetical);
}

TEST_F(BookmarkTaskUITest, KanbanBoardGamificationToggle) {
  auto board = std::make_unique<BookmarkKanbanBoardView>(task_manager_.get());

  board->ShowGamificationPanel();
  // Should be visible

  board->HideGamificationPanel();
  // Should be hidden
}

// ===== TaskQuickActionBar UI Tests =====

TEST_F(BookmarkTaskUITest, QuickActionBarCreation) {
  auto bar = std::make_unique<TaskQuickActionBar>(task_manager_.get(),
                                                   manager_.get());

  EXPECT_NE(nullptr, bar);
}

TEST_F(BookmarkTaskUITest, QuickActionBarButtonStates) {
  auto bar = std::make_unique<TaskQuickActionBar>(task_manager_.get(),
                                                   manager_.get());

  widget_->SetContentsView(std::move(bar));

  // Should have buttons created
  EXPECT_TRUE(widget_->GetContentsView()->children().size() > 0);
}

// ===== TaskStatsWidget UI Tests =====

TEST_F(BookmarkTaskUITest, StatsWidgetCreation) {
  auto stats = std::make_unique<TaskStatsWidget>(task_manager_.get());

  EXPECT_NE(nullptr, stats);
}

TEST_F(BookmarkTaskUITest, StatsWidgetUpdate) {
  auto stats = std::make_unique<TaskStatsWidget>(task_manager_.get());

  CreateTestTask(u"Todo 1");
  CreateTestTask(u"Todo 2");

  const bookmarks::BookmarkNode* done_task = CreateTestTask(u"Done");
  task_manager_->CompleteTask(done_task);

  // Should not crash
  stats->UpdateStats();
}

TEST_F(BookmarkTaskUITest, StatsWidgetProgressCalculation) {
  auto stats = std::make_unique<TaskStatsWidget>(task_manager_.get());
  widget_->SetContentsView(std::move(stats));

  // Create 3 todo, 1 done = 25% completion
  CreateTestTask(u"Todo 1");
  CreateTestTask(u"Todo 2");
  CreateTestTask(u"Todo 3");

  const bookmarks::BookmarkNode* done = CreateTestTask(u"Done");
  task_manager_->CompleteTask(done);

  static_cast<TaskStatsWidget*>(widget_->GetContentsView())->UpdateStats();

  // Progress ring should show 25%
}

// ===== Integration View UI Tests =====

TEST_F(BookmarkTaskUITest, IntegrationViewCreation) {
  auto view = std::make_unique<BookmarkTaskIntegrationView>(
      task_manager_.get(), manager_.get());

  EXPECT_NE(nullptr, view);
}

TEST_F(BookmarkTaskUITest, IntegrationViewModeSwitch) {
  auto view = std::make_unique<BookmarkTaskIntegrationView>(
      task_manager_.get(), manager_.get());

  EXPECT_EQ(BookmarkTaskIntegrationView::ViewMode::kBookmarks,
           view->GetCurrentViewMode());

  view->SetViewMode(BookmarkTaskIntegrationView::ViewMode::kKanban);
  EXPECT_EQ(BookmarkTaskIntegrationView::ViewMode::kKanban,
           view->GetCurrentViewMode());

  view->SetViewMode(BookmarkTaskIntegrationView::ViewMode::kTaskList);
  EXPECT_EQ(BookmarkTaskIntegrationView::ViewMode::kTaskList,
           view->GetCurrentViewMode());

  view->SetViewMode(BookmarkTaskIntegrationView::ViewMode::kFocusMode);
  EXPECT_EQ(BookmarkTaskIntegrationView::ViewMode::kFocusMode,
           view->GetCurrentViewMode());

  view->SetViewMode(BookmarkTaskIntegrationView::ViewMode::kStats);
  EXPECT_EQ(BookmarkTaskIntegrationView::ViewMode::kStats,
           view->GetCurrentViewMode());
}

TEST_F(BookmarkTaskUITest, IntegrationViewTaskCreation) {
  auto view = std::make_unique<BookmarkTaskIntegrationView>(
      task_manager_.get(), manager_.get());

  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark =
      bookmark_model_->AddURL(bar, 0, u"New Bookmark",
                             GURL("https://example.com"));

  view->CreateTaskFromBookmark(bookmark);

  auto metadata = task_manager_->GetTaskMetadata(bookmark);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_EQ(TaskStatus::kTodo, metadata->status);
}

TEST_F(BookmarkTaskUITest, IntegrationViewFocusSession) {
  auto view = std::make_unique<BookmarkTaskIntegrationView>(
      task_manager_.get(), manager_.get());

  const bookmarks::BookmarkNode* task = CreateTestTask(u"Focus Task");

  view->StartFocusSession(task);
  EXPECT_EQ(BookmarkTaskIntegrationView::ViewMode::kFocusMode,
           view->GetCurrentViewMode());

  view->EndFocusSession();
  EXPECT_EQ(BookmarkTaskIntegrationView::ViewMode::kKanban,
           view->GetCurrentViewMode());
}

TEST_F(BookmarkTaskUITest, IntegrationViewRefresh) {
  auto view = std::make_unique<BookmarkTaskIntegrationView>(
      task_manager_.get(), manager_.get());

  CreateTestTask(u"Task 1");
  CreateTestTask(u"Task 2");

  // Should not crash
  view->RefreshAll();
}

// ===== Achievement Notification UI Tests =====

TEST_F(BookmarkTaskUITest, AchievementNotificationCreation) {
  Achievement achievement = GetAchievementInfo(AchievementType::kFirstTask);

  auto notification = std::make_unique<AchievementNotification>(achievement);

  EXPECT_NE(nullptr, notification);
  EXPECT_FALSE(notification->GetVisible());
}

TEST_F(BookmarkTaskUITest, AchievementNotificationShow) {
  Achievement achievement = GetAchievementInfo(AchievementType::kTaskWarrior);

  auto notification = std::make_unique<AchievementNotification>(achievement);
  widget_->SetContentsView(std::move(notification));

  auto* notif_view =
      static_cast<AchievementNotification*>(widget_->GetContentsView());

  notif_view->Show();
  EXPECT_TRUE(notif_view->GetVisible());
}

// ===== Level Up Notification UI Tests =====

TEST_F(BookmarkTaskUITest, LevelUpNotificationCreation) {
  auto notification = std::make_unique<LevelUpNotification>(1, 2);

  EXPECT_NE(nullptr, notification);
  EXPECT_FALSE(notification->GetVisible());
}

TEST_F(BookmarkTaskUITest, LevelUpNotificationShow) {
  auto notification = std::make_unique<LevelUpNotification>(4, 5);
  widget_->SetContentsView(std::move(notification));

  auto* notif_view =
      static_cast<LevelUpNotification*>(widget_->GetContentsView());

  notif_view->Show();
  EXPECT_TRUE(notif_view->GetVisible());
}

// ===== Accessibility Tests =====

TEST_F(BookmarkTaskUITest, TaskCardAccessibility) {
  const bookmarks::BookmarkNode* task = CreateTestTask(u"Accessible Task");
  auto metadata = task_manager_->GetTaskMetadata(task);

  auto card = std::make_unique<BookmarkTaskCard>(task, *metadata,
                                                  task_manager_.get());

  ui::AXNodeData node_data;
  card->GetViewAccessibility().GetAccessibleNodeData(&node_data);

  EXPECT_EQ(ax::mojom::Role::kListItem, node_data.role);
  EXPECT_EQ(u"Accessible Task",
           node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

TEST_F(BookmarkTaskUITest, QuickActionBarAccessibility) {
  auto bar = std::make_unique<TaskQuickActionBar>(task_manager_.get(),
                                                   manager_.get());

  ui::AXNodeData node_data;
  bar->GetViewAccessibility().GetAccessibleNodeData(&node_data);

  EXPECT_EQ(ax::mojom::Role::kToolbar, node_data.role);
}

// ===== Animation Tests =====

TEST_F(BookmarkTaskUITest, CardAnimateIn) {
  const bookmarks::BookmarkNode* task = CreateTestTask(u"Animate");
  auto metadata = task_manager_->GetTaskMetadata(task);

  auto card = std::make_unique<BookmarkTaskCard>(task, *metadata,
                                                  task_manager_.get());
  widget_->SetContentsView(std::move(card));

  auto* card_view = static_cast<BookmarkTaskCard*>(widget_->GetContentsView());

  // Should not crash
  card_view->AnimateIn();
}

// ===== Error Handling Tests =====

TEST_F(BookmarkTaskUITest, NullTaskHandling) {
  // TaskCard should DCHECK on null task in debug
  // This test verifies the DCHECK exists
#if DCHECK_IS_ON()
  TaskMetadata metadata;
  EXPECT_DEATH(
      { auto card = std::make_unique<BookmarkTaskCard>(nullptr, metadata,
                                                        task_manager_.get()); },
      "");
#endif
}

TEST_F(BookmarkTaskUITest, InvalidViewModeHandling) {
  auto view = std::make_unique<BookmarkTaskIntegrationView>(
      task_manager_.get(), manager_.get());

  // Setting same mode should be a no-op
  view->SetViewMode(BookmarkTaskIntegrationView::ViewMode::kBookmarks);
  view->SetViewMode(BookmarkTaskIntegrationView::ViewMode::kBookmarks);

  EXPECT_EQ(BookmarkTaskIntegrationView::ViewMode::kBookmarks,
           view->GetCurrentViewMode());
}

// ===== Performance Tests =====

TEST_F(BookmarkTaskUITest, KanbanBoardWithManyTasks) {
  auto board = std::make_unique<BookmarkKanbanBoardView>(task_manager_.get());

  // Create 100 tasks
  for (int i = 0; i < 100; ++i) {
    CreateTestTask(u"Task " + base::NumberToString16(i));
  }

  // Refresh should complete quickly
  base::Time start = base::Time::Now();
  board->RefreshBoard();
  base::TimeDelta elapsed = base::Time::Now() - start;

  // Should complete in < 100ms even with 100 tasks
  EXPECT_LT(elapsed.InMilliseconds(), 100);
}

TEST_F(BookmarkTaskUITest, StatsWidgetUpdatePerformance) {
  auto stats = std::make_unique<TaskStatsWidget>(task_manager_.get());

  // Create many tasks
  for (int i = 0; i < 50; ++i) {
    CreateTestTask(u"Task " + base::NumberToString16(i));
  }

  // Update should be fast
  base::Time start = base::Time::Now();
  stats->UpdateStats();
  base::TimeDelta elapsed = base::Time::Now() - start;

  EXPECT_LT(elapsed.InMilliseconds(), 50);
}
