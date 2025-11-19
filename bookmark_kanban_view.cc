// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_kanban_view.h"

#include <memory>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/bookmarks/bookmark_task_manager.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace {

// Card dimensions
constexpr int kCardWidth = 280;
constexpr int kCardMinHeight = 120;
constexpr int kCardPadding = 12;
constexpr int kCardCornerRadius = 8;
constexpr int kCardSpacing = 8;

// Column dimensions
constexpr int kColumnWidth = 300;
constexpr int kColumnPadding = 16;
constexpr int kColumnSpacing = 16;

// Priority colors (Material Design 3)
constexpr SkColor kLowPriorityColor = SkColorSetRGB(76, 175, 80);     // Green
constexpr SkColor kMediumPriorityColor = SkColorSetRGB(255, 193, 7);  // Amber
constexpr SkColor kHighPriorityColor = SkColorSetRGB(255, 152, 0);    // Orange
constexpr SkColor kCriticalPriorityColor = SkColorSetRGB(244, 67, 54); // Red

// Get priority color
SkColor GetPriorityColor(TaskPriority priority) {
  switch (priority) {
    case TaskPriority::kLow:
      return kLowPriorityColor;
    case TaskPriority::kMedium:
      return kMediumPriorityColor;
    case TaskPriority::kHigh:
      return kHighPriorityColor;
    case TaskPriority::kCritical:
      return kCriticalPriorityColor;
  }
  return kMediumPriorityColor;
}

}  // namespace

// ===== BookmarkTaskCard Implementation =====

BookmarkTaskCard::BookmarkTaskCard(const bookmarks::BookmarkNode* task,
                                   const TaskMetadata& metadata,
                                   BookmarkTaskManager* task_manager)
    : task_(task), metadata_(metadata), task_manager_(task_manager) {
  DCHECK(task_);
  DCHECK(task_manager_);
  CreateLayout();
}

BookmarkTaskCard::~BookmarkTaskCard() = default;

void BookmarkTaskCard::CreateLayout() {
  SetBackground(views::CreateRoundedRectBackground(SK_ColorWHITE,
                                                   kCardCornerRadius));
  SetBorder(views::CreateRoundedRectBorder(
      1, kCardCornerRadius, gfx::kGoogleGrey300));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(kCardPadding),
      kCardSpacing));

  // Priority indicator (colored bar on left)
  priority_indicator_ = AddChildView(std::make_unique<views::View>());
  priority_indicator_->SetBackground(
      views::CreateSolidBackground(GetPriorityColor(metadata_.priority)));
  priority_indicator_->SetPreferredSize(gfx::Size(4, 60));

  // Title
  title_label_ = AddChildView(std::make_unique<views::Label>(
      task_->GetTitle(), views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY));
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetMultiLine(true);
  title_label_->SetMaxLines(2);

  // Task type badge
  type_label_ = AddChildView(std::make_unique<views::Label>(
      TaskTypeToString(metadata_.type), views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY));
  type_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Time estimate
  time_estimate_label_ = AddChildView(std::make_unique<views::Label>(
      TimeEstimateToString(metadata_.time_estimate),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  time_estimate_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Points
  int points = CalculateTaskPoints(metadata_);
  std::u16string points_text = u"⭐ " + base::NumberToString16(points) + u" pts";
  points_label_ = AddChildView(std::make_unique<views::Label>(
      points_text, views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  points_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Quick actions (hidden by default)
  quick_actions_container_ = AddChildView(std::make_unique<views::View>());
  quick_actions_container_->SetVisible(false);

  // Accessibility
  GetViewAccessibility().SetRole(ax::mojom::Role::kListItem);
  GetViewAccessibility().SetName(task_->GetTitle());
}

gfx::Size BookmarkTaskCard::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kCardWidth, kCardMinHeight);
}

void BookmarkTaskCard::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  // Draw shadow when dragging
  if (is_dragging_) {
    cc::PaintFlags flags;
    flags.setColor(SkColorSetA(SK_ColorBLACK, 50));
    flags.setAntiAlias(true);
    canvas->DrawRoundRect(GetLocalBounds(), kCardCornerRadius, flags);
  }

  // Draw hover effect
  if (is_hovered_ && !is_dragging_) {
    cc::PaintFlags flags;
    flags.setColor(SkColorSetA(gfx::kGoogleBlue300, 30));
    flags.setAntiAlias(true);
    canvas->DrawRoundRect(GetLocalBounds(), kCardCornerRadius, flags);
  }
}

bool BookmarkTaskCard::OnMousePressed(const ui::MouseEvent& event) {
  drag_start_point_ = event.location();
  return true;
}

bool BookmarkTaskCard::OnMouseDragged(const ui::MouseEvent& event) {
  if (!is_dragging_) {
    gfx::Vector2d delta = event.location() - drag_start_point_;
    if (delta.Length() > 5) {  // Drag threshold
      is_dragging_ = true;
      SchedulePaint();
    }
  }
  return true;
}

void BookmarkTaskCard::OnMouseReleased(const ui::MouseEvent& event) {
  if (is_dragging_) {
    is_dragging_ = false;
    SchedulePaint();
  }
}

void BookmarkTaskCard::OnMouseEntered(const ui::MouseEvent& event) {
  is_hovered_ = true;
  ShowQuickActions();
  SchedulePaint();
}

void BookmarkTaskCard::OnMouseExited(const ui::MouseEvent& event) {
  is_hovered_ = false;
  HideQuickActions();
  SchedulePaint();
}

void BookmarkTaskCard::UpdateMetadata(const TaskMetadata& metadata) {
  metadata_ = metadata;
  UpdatePriorityIndicator();
  type_label_->SetText(TaskTypeToString(metadata_.type));
  time_estimate_label_->SetText(TimeEstimateToString(metadata_.time_estimate));

  int points = CalculateTaskPoints(metadata_);
  points_label_->SetText(u"⭐ " + base::NumberToString16(points) + u" pts");
}

void BookmarkTaskCard::UpdatePriorityIndicator() {
  priority_indicator_->SetBackground(
      views::CreateSolidBackground(GetPriorityColor(metadata_.priority)));
}

void BookmarkTaskCard::ShowQuickActions() {
  if (quick_actions_container_) {
    quick_actions_container_->SetVisible(true);
  }
}

void BookmarkTaskCard::HideQuickActions() {
  if (quick_actions_container_) {
    quick_actions_container_->SetVisible(false);
  }
}

void BookmarkTaskCard::SetDragging(bool dragging) {
  is_dragging_ = dragging;
  SchedulePaint();
}

BEGIN_METADATA(BookmarkTaskCard)
END_METADATA

// ===== BookmarkKanbanColumn Implementation =====

BookmarkKanbanColumn::BookmarkKanbanColumn(TaskStatus status,
                                           std::u16string_view title,
                                           BookmarkTaskManager* task_manager)
    : status_(status), title_(title), task_manager_(task_manager) {
  DCHECK(task_manager_);

  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetInsideBorderInsets(gfx::Insets(kColumnPadding));
  SetBetweenChildSpacing(kCardSpacing);
  SetBackground(views::CreateSolidBackground(gfx::kGoogleGrey100));
  SetBorder(views::CreateRoundedRectBorder(1, 8, gfx::kGoogleGrey300));

  CreateHeader();
  CreateTasksContainer();
}

BookmarkKanbanColumn::~BookmarkKanbanColumn() = default;

void BookmarkKanbanColumn::CreateHeader() {
  header_container_ = AddChildView(std::make_unique<views::BoxLayoutView>());
  header_container_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  header_container_->SetBetweenChildSpacing(8);

  // Title
  title_label_ = header_container_->AddChildView(
      std::make_unique<views::Label>(title_, views::style::CONTEXT_LABEL,
                                     views::style::STYLE_PRIMARY));
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Count badge
  count_label_ = header_container_->AddChildView(std::make_unique<views::Label>(
      u"0", views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));

  // Points
  points_label_ = header_container_->AddChildView(
      std::make_unique<views::Label>(u"0 pts", views::style::CONTEXT_LABEL,
                                     views::style::STYLE_SECONDARY));
}

void BookmarkKanbanColumn::CreateTasksContainer() {
  tasks_scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  tasks_scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);

  tasks_container_ =
      tasks_scroll_view_->SetContents(std::make_unique<views::BoxLayoutView>());
  tasks_container_->SetOrientation(views::BoxLayout::Orientation::kVertical);
  tasks_container_->SetBetweenChildSpacing(kCardSpacing);
}

void BookmarkKanbanColumn::SetTasks(
    const std::vector<const bookmarks::BookmarkNode*>& tasks) {
  ClearTasks();

  for (const auto* task : tasks) {
    AddTask(task);
  }
}

void BookmarkKanbanColumn::AddTask(const bookmarks::BookmarkNode* task) {
  DCHECK(task);

  auto metadata_opt = task_manager_->GetTaskMetadata(task);
  if (!metadata_opt) {
    return;
  }

  tasks_container_->AddChildView(std::make_unique<BookmarkTaskCard>(
      task, *metadata_opt, task_manager_));

  UpdateStats();
}

void BookmarkKanbanColumn::RemoveTask(const bookmarks::BookmarkNode* task) {
  DCHECK(task);

  // Find and remove the card
  for (auto* child : tasks_container_->children()) {
    auto* card = static_cast<BookmarkTaskCard*>(child);
    if (card->task() == task) {
      tasks_container_->RemoveChildView(card);
      delete card;
      break;
    }
  }

  UpdateStats();
}

void BookmarkKanbanColumn::ClearTasks() {
  tasks_container_->RemoveAllChildViews();
  UpdateStats();
}

void BookmarkKanbanColumn::UpdateStats() {
  task_count_ = tasks_container_->children().size();
  total_points_ = 0;

  for (auto* child : tasks_container_->children()) {
    auto* card = static_cast<BookmarkTaskCard*>(child);
    total_points_ += CalculateTaskPoints(card->metadata());
  }

  UpdateHeaderText();
}

void BookmarkKanbanColumn::UpdateHeaderText() {
  count_label_->SetText(base::NumberToString16(task_count_));
  points_label_->SetText(base::NumberToString16(total_points_) + u" pts");
}

BEGIN_METADATA(BookmarkKanbanColumn)
END_METADATA

// ===== BookmarkKanbanBoardView Implementation =====

BookmarkKanbanBoardView::BookmarkKanbanBoardView(
    BookmarkTaskManager* task_manager)
    : task_manager_(task_manager) {
  DCHECK(task_manager_);
  kanban_view_ = std::make_unique<BookmarkKanbanView>(task_manager_);
  CreateLayout();
}

BookmarkKanbanBoardView::~BookmarkKanbanBoardView() = default;

void BookmarkKanbanBoardView::CreateLayout() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  CreateToolbar();
  CreateColumnsContainer();
  CreateGamificationPanel();
  CreateStatsBar();

  RefreshBoard();
}

void BookmarkKanbanBoardView::CreateToolbar() {
  toolbar_ = AddChildView(std::make_unique<views::BoxLayoutView>());
  toolbar_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  toolbar_->SetBetweenChildSpacing(16);
  toolbar_->SetInsideBorderInsets(gfx::Insets::VH(8, 16));

  // Add filter buttons, sort dropdown, etc.
  // (Buttons omitted for brevity - would use views::MdTextButton)
}

void BookmarkKanbanBoardView::CreateColumnsContainer() {
  columns_container_ = AddChildView(std::make_unique<views::BoxLayoutView>());
  columns_container_->SetOrientation(
      views::BoxLayout::Orientation::kHorizontal);
  columns_container_->SetBetweenChildSpacing(kColumnSpacing);
  columns_container_->SetInsideBorderInsets(gfx::Insets(16));

  // Create columns
  todo_column_ = columns_container_->AddChildView(
      std::make_unique<BookmarkKanbanColumn>(TaskStatus::kTodo, u"Todo",
                                            task_manager_));
  in_progress_column_ = columns_container_->AddChildView(
      std::make_unique<BookmarkKanbanColumn>(TaskStatus::kInProgress,
                                            u"In Progress", task_manager_));
  done_column_ = columns_container_->AddChildView(
      std::make_unique<BookmarkKanbanColumn>(TaskStatus::kDone, u"Done",
                                            task_manager_));
  snoozed_column_ = columns_container_->AddChildView(
      std::make_unique<BookmarkKanbanColumn>(TaskStatus::kSnoozed, u"Snoozed",
                                            task_manager_));
}

void BookmarkKanbanBoardView::CreateGamificationPanel() {
  gamification_panel_ = AddChildView(std::make_unique<views::View>());
  gamification_panel_->SetVisible(showing_gamification_);

  // Add level, XP bar, streak, achievements
  // (Implementation omitted for brevity)
}

void BookmarkKanbanBoardView::CreateStatsBar() {
  stats_bar_ = AddChildView(std::make_unique<views::View>());

  // Add total tasks, completion %, tasks today
  // (Implementation omitted for brevity)
}

void BookmarkKanbanBoardView::RefreshBoard() {
  KanbanBoard board = kanban_view_->GenerateBoard();

  // Update each column
  for (const auto& column : board.columns) {
    BookmarkKanbanColumn* view_column = nullptr;

    switch (column.status) {
      case TaskStatus::kTodo:
        view_column = todo_column_;
        break;
      case TaskStatus::kInProgress:
        view_column = in_progress_column_;
        break;
      case TaskStatus::kDone:
        view_column = done_column_;
        break;
      case TaskStatus::kSnoozed:
        view_column = snoozed_column_;
        break;
      default:
        continue;
    }

    if (view_column) {
      view_column->SetTasks(column.tasks);
    }
  }

  UpdateGamificationDisplay();
  UpdateStatsBar();
}

void BookmarkKanbanBoardView::MoveTask(const bookmarks::BookmarkNode* task,
                                       TaskStatus new_status) {
  kanban_view_->MoveTaskToColumn(task, new_status);
  RefreshBoard();
}

void BookmarkKanbanBoardView::UpdateGamificationDisplay() {
  const GamificationProfile& profile = task_manager_->GetProfile();

  if (level_label_) {
    level_label_->SetText(u"Level " + base::NumberToString16(profile.level));
  }

  if (xp_label_) {
    xp_label_->SetText(base::NumberToString16(profile.experience_points) +
                      u" / " +
                      base::NumberToString16(profile.points_to_next_level) +
                      u" XP");
  }

  if (streak_label_) {
    streak_label_->SetText(u"🔥 " +
                          base::NumberToString16(profile.current_streak) +
                          u" day streak");
  }
}

void BookmarkKanbanBoardView::UpdateStatsBar() {
  const GamificationProfile& profile = task_manager_->GetProfile();

  if (total_tasks_label_) {
    total_tasks_label_->SetText(
        base::NumberToString16(profile.total_tasks_completed) +
        u" tasks completed");
  }

  if (tasks_today_label_) {
    tasks_today_label_->SetText(base::NumberToString16(profile.tasks_today) +
                               u" today");
  }
}

void BookmarkKanbanBoardView::SetPriorityFilter(
    std::optional<TaskPriority> priority) {
  kanban_view_->SetPriorityFilter(priority);
  RefreshBoard();
}

void BookmarkKanbanBoardView::SetTypeFilter(std::optional<TaskType> type) {
  kanban_view_->SetTypeFilter(type);
  RefreshBoard();
}

void BookmarkKanbanBoardView::SetSortOrder(
    BookmarkKanbanView::SortOrder order) {
  kanban_view_->SetSortOrder(order);
  RefreshBoard();
}

void BookmarkKanbanBoardView::ShowGamificationPanel() {
  showing_gamification_ = true;
  if (gamification_panel_) {
    gamification_panel_->SetVisible(true);
  }
}

void BookmarkKanbanBoardView::HideGamificationPanel() {
  showing_gamification_ = false;
  if (gamification_panel_) {
    gamification_panel_->SetVisible(false);
  }
}

BEGIN_METADATA(BookmarkKanbanBoardView)
END_METADATA
