// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_KANBAN_VIEW_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_KANBAN_VIEW_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/bookmarks/bookmark_task_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class BoxLayoutView;
class Label;
class ScrollView;
}  // namespace views

class BookmarkTaskManager;

// Visual Kanban board for task management with drag-and-drop support.
// Displays tasks in columns by status (Todo/In Progress/Done/Snoozed).
// Inspired by Trello, Notion, and modern task managers with gameified elements.
//
// Features:
// - Drag-and-drop task cards between columns
// - Visual progress indicators (points, completion %)
// - Priority color coding
// - Quick actions on hover
// - Smooth animations
// - Accessible keyboard navigation

// ===== Task Card View =====

class BookmarkTaskCard : public views::View {
  METADATA_HEADER(BookmarkTaskCard, views::View)

 public:
  BookmarkTaskCard(const bookmarks::BookmarkNode* task,
                  const TaskMetadata& metadata,
                  BookmarkTaskManager* task_manager);
  ~BookmarkTaskCard() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // Accessors
  const bookmarks::BookmarkNode* task() const { return task_; }
  const TaskMetadata& metadata() const { return metadata_; }

  // Actions
  void UpdateMetadata(const TaskMetadata& metadata);
  void SetDragging(bool dragging);
  void AnimateIn();
  void AnimateOut(base::OnceClosure callback);

 private:
  void CreateLayout();
  void UpdatePriorityIndicator();
  void ShowQuickActions();
  void HideQuickActions();

  // Task data
  raw_ptr<const bookmarks::BookmarkNode> task_;
  TaskMetadata metadata_;
  raw_ptr<BookmarkTaskManager> task_manager_;

  // UI components
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Label> type_label_ = nullptr;
  raw_ptr<views::Label> time_estimate_label_ = nullptr;
  raw_ptr<views::Label> points_label_ = nullptr;
  raw_ptr<views::View> priority_indicator_ = nullptr;
  raw_ptr<views::View> quick_actions_container_ = nullptr;

  // State
  bool is_dragging_ = false;
  bool is_hovered_ = false;
  gfx::Point drag_start_point_;
};

// ===== Kanban Column View =====

class BookmarkKanbanColumn : public views::BoxLayoutView {
  METADATA_HEADER(BookmarkKanbanColumn, views::BoxLayoutView)

 public:
  BookmarkKanbanColumn(TaskStatus status,
                      std::u16string_view title,
                      BookmarkTaskManager* task_manager);
  ~BookmarkKanbanColumn() override;

  // Column management
  void SetTasks(const std::vector<const bookmarks::BookmarkNode*>& tasks);
  void AddTask(const bookmarks::BookmarkNode* task);
  void RemoveTask(const bookmarks::BookmarkNode* task);
  void ClearTasks();

  // Drag-and-drop
  bool CanAcceptDrop(const ui::DropTargetEvent& event) const;
  void OnDragEntered(const ui::DropTargetEvent& event);
  void OnDragExited();
  int OnPerformDrop(const ui::DropTargetEvent& event);

  // Accessors
  TaskStatus status() const { return status_; }
  int task_count() const { return task_count_; }
  int total_points() const { return total_points_; }

 private:
  void CreateHeader();
  void CreateTasksContainer();
  void UpdateStats();
  void UpdateHeaderText();

  // Column data
  TaskStatus status_;
  std::u16string title_;
  raw_ptr<BookmarkTaskManager> task_manager_;

  // Stats
  int task_count_ = 0;
  int total_points_ = 0;

  // UI components
  raw_ptr<views::BoxLayoutView> header_container_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Label> count_label_ = nullptr;
  raw_ptr<views::Label> points_label_ = nullptr;
  raw_ptr<views::ScrollView> tasks_scroll_view_ = nullptr;
  raw_ptr<views::BoxLayoutView> tasks_container_ = nullptr;

  // State
  bool is_drag_target_ = false;
};

// ===== Main Kanban Board View =====

class BookmarkKanbanBoardView : public views::View {
  METADATA_HEADER(BookmarkKanbanBoardView, views::View)

 public:
  explicit BookmarkKanbanBoardView(BookmarkTaskManager* task_manager);
  ~BookmarkKanbanBoardView() override;

  // Board management
  void RefreshBoard();
  void SetPriorityFilter(std::optional<TaskPriority> priority);
  void SetTypeFilter(std::optional<TaskType> type);
  void SetSortOrder(BookmarkKanbanView::SortOrder order);

  // Actions
  void MoveTask(const bookmarks::BookmarkNode* task, TaskStatus new_status);
  void OpenTask(const bookmarks::BookmarkNode* task);
  void DeleteTask(const bookmarks::BookmarkNode* task);

  // Gamification display
  void ShowGamificationPanel();
  void HideGamificationPanel();

 private:
  void CreateLayout();
  void CreateToolbar();
  void CreateColumnsContainer();
  void CreateGamificationPanel();
  void CreateStatsBar();

  void UpdateBoard();
  void UpdateGamificationDisplay();
  void UpdateStatsBar();

  // Dependencies
  raw_ptr<BookmarkTaskManager> task_manager_;
  std::unique_ptr<BookmarkKanbanView> kanban_view_;

  // UI components
  raw_ptr<views::BoxLayoutView> toolbar_ = nullptr;
  raw_ptr<views::BoxLayoutView> columns_container_ = nullptr;
  raw_ptr<views::View> gamification_panel_ = nullptr;
  raw_ptr<views::View> stats_bar_ = nullptr;

  // Columns
  raw_ptr<BookmarkKanbanColumn> todo_column_ = nullptr;
  raw_ptr<BookmarkKanbanColumn> in_progress_column_ = nullptr;
  raw_ptr<BookmarkKanbanColumn> done_column_ = nullptr;
  raw_ptr<BookmarkKanbanColumn> snoozed_column_ = nullptr;

  // Gamification widgets
  raw_ptr<views::Label> level_label_ = nullptr;
  raw_ptr<views::Label> xp_label_ = nullptr;
  raw_ptr<views::Label> streak_label_ = nullptr;
  raw_ptr<views::View> achievements_container_ = nullptr;

  // Stats widgets
  raw_ptr<views::Label> total_tasks_label_ = nullptr;
  raw_ptr<views::Label> completion_percentage_label_ = nullptr;
  raw_ptr<views::Label> tasks_today_label_ = nullptr;

  // State
  bool showing_gamification_ = true;
};

// ===== Focus Mode View =====

class BookmarkFocusModeView : public views::View {
  METADATA_HEADER(BookmarkFocusModeView, views::View)

 public:
  BookmarkFocusModeView(BookmarkFocusMode* focus_mode,
                       BookmarkTaskManager* task_manager);
  ~BookmarkFocusModeView() override;

  // Focus session management
  void StartSession(const bookmarks::BookmarkNode* task,
                   base::TimeDelta duration = base::Minutes(25));
  void EndSession();
  void PauseSession();
  void ResumeSession();
  void TakeBreak();

  // Display
  void UpdateTimer();
  void ShowSessionComplete();

 private:
  void CreateLayout();
  void CreateTaskDisplay();
  void CreateTimerDisplay();
  void CreateControls();
  void CreateBreakPanel();

  void OnTimerTick();
  void UpdateTimerDisplay(base::TimeDelta remaining);

  // Dependencies
  raw_ptr<BookmarkFocusMode> focus_mode_;
  raw_ptr<BookmarkTaskManager> task_manager_;

  // UI components
  raw_ptr<views::Label> task_title_label_ = nullptr;
  raw_ptr<views::Label> timer_label_ = nullptr;
  raw_ptr<views::Label> session_type_label_ = nullptr;
  raw_ptr<views::View> progress_bar_ = nullptr;
  raw_ptr<views::BoxLayoutView> controls_container_ = nullptr;
  raw_ptr<views::View> break_panel_ = nullptr;

  // Timer state
  std::unique_ptr<base::RepeatingTimer> timer_;
};

// ===== Achievement Badge View =====

class AchievementBadgeView : public views::View {
  METADATA_HEADER(AchievementBadgeView, views::View)

 public:
  AchievementBadgeView(const Achievement& achievement, bool show_locked);
  ~AchievementBadgeView() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  void AnimateUnlock();

 private:
  void CreateTooltip();

  Achievement achievement_;
  bool show_locked_;
  bool is_hovered_ = false;
};

// ===== Task List View (Alternative to Kanban) =====

class BookmarkTaskListView : public views::View {
  METADATA_HEADER(BookmarkTaskListView, views::View)

 public:
  BookmarkTaskListView(BookmarkTaskManager* task_manager);
  ~BookmarkTaskListView() override;

  // View modes
  enum class ViewMode {
    kAll,           // All tasks
    kToday,         // Tasks due today
    kOverdue,       // Overdue tasks
    kPriority,      // High priority tasks
    kInProgress,    // Currently working on
  };

  void SetViewMode(ViewMode mode);
  void RefreshList();

  // Sorting
  void SetSortOrder(BookmarkKanbanView::SortOrder order);
  void SetGroupBy(std::optional<TaskType> type);

 private:
  void CreateLayout();
  void UpdateList();

  raw_ptr<BookmarkTaskManager> task_manager_;
  ViewMode view_mode_ = ViewMode::kAll;
  BookmarkKanbanView::SortOrder sort_order_ =
      BookmarkKanbanView::SortOrder::kPriority;

  raw_ptr<views::ScrollView> scroll_view_ = nullptr;
  raw_ptr<views::BoxLayoutView> tasks_container_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_KANBAN_VIEW_H_
