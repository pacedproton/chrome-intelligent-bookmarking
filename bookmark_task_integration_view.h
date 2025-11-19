// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TASK_INTEGRATION_VIEW_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TASK_INTEGRATION_VIEW_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/bookmarks/bookmark_kanban_view.h"
#include "chrome/browser/ui/bookmarks/bookmark_task_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class BoxLayoutView;
class Button;
class Label;
class TabbedPane;
}  // namespace views

class BookmarkManager;
class BookmarkTaskManager;
class BookmarkKanbanBoardView;
class BookmarkFocusModeView;

// Comprehensive integration view that combines bookmark management with
// task-based workflows. Provides seamless switching between:
// - Traditional bookmark browsing
// - Kanban task board
// - Focus mode
// - Task list views
//
// This is the main entry point for the task management UI, designed to
// integrate smoothly with existing bookmark functionality.

// ===== Quick Action Bar =====

class TaskQuickActionBar : public views::View {
  METADATA_HEADER(TaskQuickActionBar, views::View)

 public:
  TaskQuickActionBar(BookmarkTaskManager* task_manager,
                     BookmarkManager* bookmark_manager);
  ~TaskQuickActionBar() override;

  // Quick actions
  void OnConvertAllTabsClicked();
  void OnCloseLowPriorityTabsClicked();
  void OnStartFocusModeClicked();
  void OnShowStatsClicked();

 private:
  void CreateLayout();
  void UpdateButtonStates();

  raw_ptr<BookmarkTaskManager> task_manager_;
  raw_ptr<BookmarkManager> bookmark_manager_;

  // Buttons
  raw_ptr<views::Button> convert_tabs_button_ = nullptr;
  raw_ptr<views::Button> close_tabs_button_ = nullptr;
  raw_ptr<views::Button> focus_mode_button_ = nullptr;
  raw_ptr<views::Button> stats_button_ = nullptr;
};

// ===== Task Stats Widget =====

class TaskStatsWidget : public views::View {
  METADATA_HEADER(TaskStatsWidget, views::View)

 public:
  explicit TaskStatsWidget(BookmarkTaskManager* task_manager);
  ~TaskStatsWidget() override;

  void UpdateStats();

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  void CreateLayout();
  void CreateProgressRing();
  void CreateStatsCards();

  raw_ptr<BookmarkTaskManager> task_manager_;

  // Components
  raw_ptr<views::View> progress_ring_ = nullptr;
  raw_ptr<views::Label> completion_label_ = nullptr;
  raw_ptr<views::BoxLayoutView> stats_cards_container_ = nullptr;

  // Stats
  raw_ptr<views::Label> todo_count_ = nullptr;
  raw_ptr<views::Label> in_progress_count_ = nullptr;
  raw_ptr<views::Label> done_count_ = nullptr;
  raw_ptr<views::Label> streak_count_ = nullptr;
};

// ===== Main Integration View =====

class BookmarkTaskIntegrationView : public views::View {
  METADATA_HEADER(BookmarkTaskIntegrationView, views::View)

 public:
  BookmarkTaskIntegrationView(BookmarkTaskManager* task_manager,
                              BookmarkManager* bookmark_manager);
  ~BookmarkTaskIntegrationView() override;

  // View mode switching
  enum class ViewMode {
    kBookmarks,    // Traditional bookmark browser
    kKanban,       // Kanban board view
    kTaskList,     // List view of tasks
    kFocusMode,    // Full-screen focus mode
    kStats,        // Statistics dashboard
  };

  void SetViewMode(ViewMode mode);
  ViewMode GetCurrentViewMode() const { return current_mode_; }

  // Task operations
  void CreateTaskFromBookmark(const bookmarks::BookmarkNode* bookmark);
  void CreateTaskFromCurrentTab();
  void ConvertAllTabsToTasks();
  void OpenTaskAsTab(const bookmarks::BookmarkNode* task);

  // Focus mode
  void StartFocusSession(const bookmarks::BookmarkNode* task);
  void EndFocusSession();

  // Gamification
  void ShowAchievementNotification(const Achievement& achievement);
  void ShowLevelUpNotification(int new_level);

  // Refresh
  void RefreshAll();

 private:
  void CreateLayout();
  void CreateTabBar();
  void CreateViewContainer();
  void CreateQuickActionBar();
  void CreateStatsBar();

  void SwitchToView(ViewMode mode);
  void AnimateTransition(views::View* from, views::View* to);

  // View creation
  std::unique_ptr<views::View> CreateBookmarksView();
  std::unique_ptr<BookmarkKanbanBoardView> CreateKanbanView();
  std::unique_ptr<views::View> CreateTaskListView();
  std::unique_ptr<BookmarkFocusModeView> CreateFocusModeView();
  std::unique_ptr<TaskStatsWidget> CreateStatsView();

  // Dependencies
  raw_ptr<BookmarkTaskManager> task_manager_;
  raw_ptr<BookmarkManager> bookmark_manager_;
  std::unique_ptr<BookmarkFocusMode> focus_mode_;

  // Current view mode
  ViewMode current_mode_ = ViewMode::kBookmarks;

  // UI Components
  raw_ptr<views::TabbedPane> tab_bar_ = nullptr;
  raw_ptr<views::View> view_container_ = nullptr;
  raw_ptr<TaskQuickActionBar> quick_action_bar_ = nullptr;
  raw_ptr<TaskStatsWidget> stats_bar_ = nullptr;

  // View instances (cached)
  raw_ptr<views::View> bookmarks_view_ = nullptr;
  raw_ptr<BookmarkKanbanBoardView> kanban_view_ = nullptr;
  raw_ptr<views::View> task_list_view_ = nullptr;
  raw_ptr<BookmarkFocusModeView> focus_mode_view_ = nullptr;
  raw_ptr<TaskStatsWidget> stats_view_ = nullptr;
};

// ===== Achievement Notification =====

class AchievementNotification : public views::View {
  METADATA_HEADER(AchievementNotification, views::View)

 public:
  explicit AchievementNotification(const Achievement& achievement);
  ~AchievementNotification() override;

  void Show();
  void Dismiss();

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  void CreateLayout();
  void AnimateIn();
  void AnimateOut();

  Achievement achievement_;

  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Label> description_label_ = nullptr;
  raw_ptr<views::Label> points_label_ = nullptr;
  raw_ptr<views::View> badge_icon_ = nullptr;
};

// ===== Level Up Notification =====

class LevelUpNotification : public views::View {
  METADATA_HEADER(LevelUpNotification, views::View)

 public:
  LevelUpNotification(int old_level, int new_level);
  ~LevelUpNotification() override;

  void Show();
  void Dismiss();

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  void CreateLayout();
  void AnimateIn();
  void PlayCelebrationEffect();

  int old_level_;
  int new_level_;

  raw_ptr<views::Label> level_label_ = nullptr;
  raw_ptr<views::View> celebration_effect_ = nullptr;
};

// ===== Task Creation Dialog =====

class TaskCreationDialog : public views::View {
  METADATA_HEADER(TaskCreationDialog, views::View)

 public:
  TaskCreationDialog(BookmarkTaskManager* task_manager,
                    const bookmarks::BookmarkNode* bookmark);
  ~TaskCreationDialog() override;

  // Show/hide
  void Show();
  void Close();

  // Callbacks
  void OnCreateClicked();
  void OnCancelClicked();

 private:
  void CreateLayout();
  void PopulateFromBookmark();
  TaskMetadata GatherMetadata();

  raw_ptr<BookmarkTaskManager> task_manager_;
  raw_ptr<const bookmarks::BookmarkNode> bookmark_;

  // Form fields (dropdowns, inputs)
  // Implementation would include actual controls
  TaskMetadata pending_metadata_;
};

// ===== Batch Task Operations Dialog =====

class BatchTaskOperationsDialog : public views::View {
  METADATA_HEADER(BatchTaskOperationsDialog, views::View)

 public:
  BatchTaskOperationsDialog(
      BookmarkTaskManager* task_manager,
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks);
  ~BatchTaskOperationsDialog() override;

  enum class Operation {
    kSetPriority,
    kSetType,
    kSetDueDate,
    kArchive,
    kDelete,
    kAddTags,
  };

  void SetOperation(Operation op);
  void Apply();

 private:
  void CreateLayout();
  void UpdatePreview();

  raw_ptr<BookmarkTaskManager> task_manager_;
  std::vector<const bookmarks::BookmarkNode*> bookmarks_;
  Operation current_operation_ = Operation::kSetPriority;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TASK_INTEGRATION_VIEW_H_
