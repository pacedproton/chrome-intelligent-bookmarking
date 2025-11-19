// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_task_integration_view.h"

#include <memory>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/bookmarks/bookmark_kanban_view.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "chrome/browser/ui/bookmarks/bookmark_task_manager.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"

namespace {

// Animation constants
constexpr base::TimeDelta kTransitionDuration = base::Milliseconds(250);
constexpr base::TimeDelta kNotificationDuration = base::Seconds(3);
constexpr base::TimeDelta kCelebrationDuration = base::Seconds(2);

// Colors (Material Design 3)
constexpr SkColor kSuccessColor = SkColorSetRGB(76, 175, 80);  // Green
constexpr SkColor kInfoColor = SkColorSetRGB(33, 150, 243);    // Blue
constexpr SkColor kWarningColor = SkColorSetRGB(255, 152, 0);  // Orange

// Progress ring settings
constexpr int kProgressRingSize = 120;
constexpr int kProgressRingStrokeWidth = 8;

}  // namespace

// ===== TaskQuickActionBar Implementation =====

TaskQuickActionBar::TaskQuickActionBar(BookmarkTaskManager* task_manager,
                                       BookmarkManager* bookmark_manager)
    : task_manager_(task_manager), bookmark_manager_(bookmark_manager) {
  DCHECK(task_manager_);
  DCHECK(bookmark_manager_);
  CreateLayout();
}

TaskQuickActionBar::~TaskQuickActionBar() = default;

void TaskQuickActionBar::CreateLayout() {
  SetBackground(views::CreateSolidBackground(gfx::kGoogleGrey100));
  SetBorder(views::CreateSolidSidedBorder(
      gfx::Insets::TLBR(0, 0, 1, 0), gfx::kGoogleGrey300));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(8, 16), 12));

  // Convert all tabs button
  convert_tabs_button_ = AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&TaskQuickActionBar::OnConvertAllTabsClicked,
                         base::Unretained(this)),
      u"📥 Convert All Tabs"));
  convert_tabs_button_->SetTooltipText(
      u"Convert all open tabs to tasks (Ctrl+Shift+T)");

  // Close low priority tabs button
  close_tabs_button_ = AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&TaskQuickActionBar::OnCloseLowPriorityTabsClicked,
                         base::Unretained(this)),
      u"🗑️ Close Low Priority"));
  close_tabs_button_->SetTooltipText(
      u"Close all low priority task tabs");

  // Focus mode button
  focus_mode_button_ = AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&TaskQuickActionBar::OnStartFocusModeClicked,
                         base::Unretained(this)),
      u"🎯 Focus Mode"));
  focus_mode_button_->SetTooltipText(
      u"Start a 25-minute focus session (F6)");
  focus_mode_button_->SetProminent(true);

  // Stats button
  stats_button_ = AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&TaskQuickActionBar::OnShowStatsClicked,
                         base::Unretained(this)),
      u"📊 Stats"));
  stats_button_->SetTooltipText(u"View detailed statistics");

  // Accessibility
  GetViewAccessibility().SetRole(ax::mojom::Role::kToolbar);
  GetViewAccessibility().SetName(u"Task Quick Actions");

  UpdateButtonStates();
}

void TaskQuickActionBar::OnConvertAllTabsClicked() {
  DCHECK(task_manager_);
  task_manager_->ConvertAllTabsToTasks();
  UpdateButtonStates();

  DLOG(INFO) << "Converted all tabs to tasks";
}

void TaskQuickActionBar::OnCloseLowPriorityTabsClicked() {
  DCHECK(task_manager_);
  task_manager_->CloseLowPriorityTabs();
  UpdateButtonStates();

  DLOG(INFO) << "Closed low priority tabs";
}

void TaskQuickActionBar::OnStartFocusModeClicked() {
  // Would trigger focus mode in parent view
  DLOG(INFO) << "Focus mode requested";
}

void TaskQuickActionBar::OnShowStatsClicked() {
  // Would switch to stats view
  DLOG(INFO) << "Stats view requested";
}

void TaskQuickActionBar::UpdateButtonStates() {
  // Enable/disable buttons based on current state
  auto open_tasks = task_manager_->GetOpenTasks();
  close_tabs_button_->SetEnabled(!open_tasks.empty());
}

BEGIN_METADATA(TaskQuickActionBar)
END_METADATA

// ===== TaskStatsWidget Implementation =====

TaskStatsWidget::TaskStatsWidget(BookmarkTaskManager* task_manager)
    : task_manager_(task_manager) {
  DCHECK(task_manager_);
  CreateLayout();
  UpdateStats();
}

TaskStatsWidget::~TaskStatsWidget() = default;

void TaskStatsWidget::CreateLayout() {
  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  SetBorder(views::CreateRoundedRectBorder(1, 8, gfx::kGoogleGrey300));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(16), 24));

  CreateProgressRing();
  CreateStatsCards();
}

void TaskStatsWidget::CreateProgressRing() {
  progress_ring_ = AddChildView(std::make_unique<views::View>());
  progress_ring_->SetPreferredSize(gfx::Size(kProgressRingSize,
                                             kProgressRingSize));

  // Completion label in center
  completion_label_ = progress_ring_->AddChildView(
      std::make_unique<views::Label>(u"0%", views::style::CONTEXT_LABEL,
                                     views::style::STYLE_PRIMARY));
  completion_label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
}

void TaskStatsWidget::CreateStatsCards() {
  stats_cards_container_ =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  stats_cards_container_->SetOrientation(
      views::BoxLayout::Orientation::kHorizontal);
  stats_cards_container_->SetBetweenChildSpacing(16);

  // Todo count card
  auto* todo_card = stats_cards_container_->AddChildView(
      std::make_unique<views::BoxLayoutView>());
  todo_card->SetOrientation(views::BoxLayout::Orientation::kVertical);
  todo_count_ = todo_card->AddChildView(std::make_unique<views::Label>(
      u"0", views::style::CONTEXT_DIALOG_TITLE));
  todo_card->AddChildView(std::make_unique<views::Label>(
      u"Todo", views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));

  // In Progress count card
  auto* progress_card = stats_cards_container_->AddChildView(
      std::make_unique<views::BoxLayoutView>());
  progress_card->SetOrientation(views::BoxLayout::Orientation::kVertical);
  in_progress_count_ = progress_card->AddChildView(
      std::make_unique<views::Label>(u"0", views::style::CONTEXT_DIALOG_TITLE));
  progress_card->AddChildView(std::make_unique<views::Label>(
      u"In Progress", views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY));

  // Done count card
  auto* done_card = stats_cards_container_->AddChildView(
      std::make_unique<views::BoxLayoutView>());
  done_card->SetOrientation(views::BoxLayout::Orientation::kVertical);
  done_count_ = done_card->AddChildView(std::make_unique<views::Label>(
      u"0", views::style::CONTEXT_DIALOG_TITLE));
  done_card->AddChildView(std::make_unique<views::Label>(
      u"Done", views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));

  // Streak card
  auto* streak_card = stats_cards_container_->AddChildView(
      std::make_unique<views::BoxLayoutView>());
  streak_card->SetOrientation(views::BoxLayout::Orientation::kVertical);
  streak_count_ = streak_card->AddChildView(std::make_unique<views::Label>(
      u"🔥 0", views::style::CONTEXT_DIALOG_TITLE));
  streak_card->AddChildView(std::make_unique<views::Label>(
      u"Day Streak", views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY));
}

void TaskStatsWidget::UpdateStats() {
  DCHECK(task_manager_);

  // Get stats
  auto todo_tasks = task_manager_->GetTasksByStatus(TaskStatus::kTodo);
  auto progress_tasks = task_manager_->GetTasksByStatus(TaskStatus::kInProgress);
  auto done_tasks = task_manager_->GetTasksByStatus(TaskStatus::kDone);

  const GamificationProfile& profile = task_manager_->GetProfile();

  // Update counts
  todo_count_->SetText(base::NumberToString16(todo_tasks.size()));
  in_progress_count_->SetText(base::NumberToString16(progress_tasks.size()));
  done_count_->SetText(base::NumberToString16(done_tasks.size()));
  streak_count_->SetText(u"🔥 " +
                        base::NumberToString16(profile.current_streak));

  // Calculate completion percentage
  int total = todo_tasks.size() + progress_tasks.size() + done_tasks.size();
  int completion = 0;
  if (total > 0) {
    completion = (done_tasks.size() * 100) / total;
  }

  completion_label_->SetText(base::NumberToString16(completion) + u"%");

  SchedulePaint();
}

void TaskStatsWidget::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  if (!progress_ring_) {
    return;
  }

  // Draw progress ring
  gfx::Rect ring_bounds = progress_ring_->bounds();
  gfx::Point center(ring_bounds.CenterPoint());
  int radius = kProgressRingSize / 2 - kProgressRingStrokeWidth;

  // Background circle
  cc::PaintFlags bg_flags;
  bg_flags.setAntiAlias(true);
  bg_flags.setStyle(cc::PaintFlags::kStroke_Style);
  bg_flags.setStrokeWidth(kProgressRingStrokeWidth);
  bg_flags.setColor(gfx::kGoogleGrey200);
  canvas->DrawCircle(center, radius, bg_flags);

  // Progress arc
  auto todo_tasks = task_manager_->GetTasksByStatus(TaskStatus::kTodo);
  auto progress_tasks = task_manager_->GetTasksByStatus(TaskStatus::kInProgress);
  auto done_tasks = task_manager_->GetTasksByStatus(TaskStatus::kDone);

  int total = todo_tasks.size() + progress_tasks.size() + done_tasks.size();
  if (total > 0) {
    float completion = static_cast<float>(done_tasks.size()) /
                      static_cast<float>(total);

    cc::PaintFlags progress_flags;
    progress_flags.setAntiAlias(true);
    progress_flags.setStyle(cc::PaintFlags::kStroke_Style);
    progress_flags.setStrokeWidth(kProgressRingStrokeWidth);
    progress_flags.setColor(kSuccessColor);

    // Draw arc from top (270°) clockwise
    float sweep_angle = 360.0f * completion;
    gfx::RectF arc_rect(center.x() - radius, center.y() - radius,
                       radius * 2, radius * 2);

    SkPath path;
    path.addArc(gfx::RectFToSkRect(arc_rect), -90, sweep_angle);
    canvas->DrawPath(path, progress_flags);
  }
}

BEGIN_METADATA(TaskStatsWidget)
END_METADATA

// ===== BookmarkTaskIntegrationView Implementation =====

BookmarkTaskIntegrationView::BookmarkTaskIntegrationView(
    BookmarkTaskManager* task_manager,
    BookmarkManager* bookmark_manager)
    : task_manager_(task_manager), bookmark_manager_(bookmark_manager) {
  DCHECK(task_manager_);
  DCHECK(bookmark_manager_);

  focus_mode_ = std::make_unique<BookmarkFocusMode>(task_manager_);

  CreateLayout();
}

BookmarkTaskIntegrationView::~BookmarkTaskIntegrationView() = default;

void BookmarkTaskIntegrationView::CreateLayout() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  CreateQuickActionBar();
  CreateTabBar();
  CreateViewContainer();
  CreateStatsBar();

  // Start with bookmarks view
  SetViewMode(ViewMode::kBookmarks);
}

void BookmarkTaskIntegrationView::CreateQuickActionBar() {
  quick_action_bar_ = AddChildView(
      std::make_unique<TaskQuickActionBar>(task_manager_, bookmark_manager_));
}

void BookmarkTaskIntegrationView::CreateTabBar() {
  tab_bar_ = AddChildView(std::make_unique<views::TabbedPane>());

  tab_bar_->AddTab(u"📚 Bookmarks", CreateBookmarksView());
  tab_bar_->AddTab(u"📋 Kanban", CreateKanbanView());
  tab_bar_->AddTab(u"📝 Tasks", CreateTaskListView());
  tab_bar_->AddTab(u"🎯 Focus", CreateFocusModeView());
  tab_bar_->AddTab(u"📊 Stats", CreateStatsView());

  // Accessibility
  tab_bar_->GetViewAccessibility().SetRole(ax::mojom::Role::kTabList);
  tab_bar_->GetViewAccessibility().SetName(u"View Mode Selector");
}

void BookmarkTaskIntegrationView::CreateViewContainer() {
  view_container_ = AddChildView(std::make_unique<views::View>());
  view_container_->SetLayoutManager(std::make_unique<views::FillLayout>());
}

void BookmarkTaskIntegrationView::CreateStatsBar() {
  stats_bar_ = AddChildView(
      std::make_unique<TaskStatsWidget>(task_manager_));
}

std::unique_ptr<views::View>
BookmarkTaskIntegrationView::CreateBookmarksView() {
  auto view = std::make_unique<views::View>();
  // Would integrate with existing bookmark browser
  return view;
}

std::unique_ptr<BookmarkKanbanBoardView>
BookmarkTaskIntegrationView::CreateKanbanView() {
  return std::make_unique<BookmarkKanbanBoardView>(task_manager_);
}

std::unique_ptr<views::View>
BookmarkTaskIntegrationView::CreateTaskListView() {
  return std::make_unique<BookmarkTaskListView>(task_manager_);
}

std::unique_ptr<BookmarkFocusModeView>
BookmarkTaskIntegrationView::CreateFocusModeView() {
  return std::make_unique<BookmarkFocusModeView>(focus_mode_.get(),
                                                  task_manager_);
}

std::unique_ptr<TaskStatsWidget>
BookmarkTaskIntegrationView::CreateStatsView() {
  return std::make_unique<TaskStatsWidget>(task_manager_);
}

void BookmarkTaskIntegrationView::SetViewMode(ViewMode mode) {
  if (mode == current_mode_) {
    return;
  }

  current_mode_ = mode;

  // Update tab selection
  int tab_index = static_cast<int>(mode);
  if (tab_bar_) {
    tab_bar_->SelectTabAt(tab_index);
  }

  RefreshAll();

  DLOG(INFO) << "Switched to view mode: " << static_cast<int>(mode);
}

void BookmarkTaskIntegrationView::CreateTaskFromBookmark(
    const bookmarks::BookmarkNode* bookmark) {
  DCHECK(bookmark);
  DCHECK(task_manager_);

  TaskMetadata metadata;
  metadata.type = task_manager_->DetectTaskType(bookmark);
  metadata.time_estimate = task_manager_->EstimateTaskDuration(bookmark);
  metadata.status = TaskStatus::kTodo;
  metadata.priority = TaskPriority::kMedium;

  task_manager_->SetTaskMetadata(bookmark, metadata);

  RefreshAll();

  DLOG(INFO) << "Created task from bookmark: " << bookmark->GetTitle();
}

void BookmarkTaskIntegrationView::ConvertAllTabsToTasks() {
  DCHECK(task_manager_);
  task_manager_->ConvertAllTabsToTasks();
  RefreshAll();
}

void BookmarkTaskIntegrationView::StartFocusSession(
    const bookmarks::BookmarkNode* task) {
  DCHECK(task);
  DCHECK(focus_mode_);

  focus_mode_->StartFocusSession(task);
  SetViewMode(ViewMode::kFocusMode);
}

void BookmarkTaskIntegrationView::EndFocusSession() {
  DCHECK(focus_mode_);
  focus_mode_->EndFocusSession();
  SetViewMode(ViewMode::kKanban);
}

void BookmarkTaskIntegrationView::ShowAchievementNotification(
    const Achievement& achievement) {
  auto* notification = AddChildView(
      std::make_unique<AchievementNotification>(achievement));
  notification->Show();

  DLOG(INFO) << "Achievement unlocked: " << achievement.title;
}

void BookmarkTaskIntegrationView::ShowLevelUpNotification(int new_level) {
  const GamificationProfile& profile = task_manager_->GetProfile();
  auto* notification = AddChildView(
      std::make_unique<LevelUpNotification>(new_level - 1, new_level));
  notification->Show();

  DLOG(INFO) << "Level up! Now level " << new_level;
}

void BookmarkTaskIntegrationView::RefreshAll() {
  if (stats_bar_) {
    stats_bar_->UpdateStats();
  }

  if (kanban_view_ && current_mode_ == ViewMode::kKanban) {
    kanban_view_->RefreshBoard();
  }

  SchedulePaint();
}

BEGIN_METADATA(BookmarkTaskIntegrationView)
END_METADATA

// ===== AchievementNotification Implementation =====

AchievementNotification::AchievementNotification(const Achievement& achievement)
    : achievement_(achievement) {
  CreateLayout();
}

AchievementNotification::~AchievementNotification() = default;

void AchievementNotification::CreateLayout() {
  SetBackground(views::CreateRoundedRectBackground(kSuccessColor, 8));
  SetBorder(views::CreateEmptyBorder(gfx::Insets(12)));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 8));

  title_label_ = AddChildView(std::make_unique<views::Label>(
      u"🏆 Achievement Unlocked!", views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY));
  title_label_->SetEnabledColor(SK_ColorWHITE);

  description_label_ = AddChildView(std::make_unique<views::Label>(
      achievement_.title, views::style::CONTEXT_LABEL));
  description_label_->SetEnabledColor(SK_ColorWHITE);

  points_label_ = AddChildView(std::make_unique<views::Label>(
      u"+" + base::NumberToString16(achievement_.points) + u" pts",
      views::style::CONTEXT_LABEL));
  points_label_->SetEnabledColor(SK_ColorWHITE);
}

void AchievementNotification::Show() {
  SetVisible(true);
  AnimateIn();

  // Auto-dismiss after 3 seconds
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AchievementNotification::Dismiss,
                    base::Unretained(this)),
      kNotificationDuration);
}

void AchievementNotification::Dismiss() {
  AnimateOut();
}

void AchievementNotification::AnimateIn() {
  // Slide in from right with fade
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kTransitionDuration)
      .SetTransform(layer(), gfx::Transform())
      .SetOpacity(layer(), 1.0f);
}

void AchievementNotification::AnimateOut() {
  // Slide out to right with fade
  gfx::Transform slide_out;
  slide_out.Translate(300, 0);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kTransitionDuration)
      .SetTransform(layer(), slide_out)
      .SetOpacity(layer(), 0.0f)
      .Then()
      .SetDuration(base::TimeDelta())
      .SetVisibility(this, false);
}

void AchievementNotification::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  // Add shine effect
  cc::PaintFlags shine_flags;
  shine_flags.setAntiAlias(true);
  shine_flags.setColor(SkColorSetA(SK_ColorWHITE, 30));

  gfx::Rect shine_rect = GetLocalBounds();
  shine_rect.set_height(shine_rect.height() / 2);
  canvas->DrawRoundRect(shine_rect, 8, shine_flags);
}

BEGIN_METADATA(AchievementNotification)
END_METADATA

// ===== LevelUpNotification Implementation =====

LevelUpNotification::LevelUpNotification(int old_level, int new_level)
    : old_level_(old_level), new_level_(new_level) {
  CreateLayout();
}

LevelUpNotification::~LevelUpNotification() = default;

void LevelUpNotification::CreateLayout() {
  SetBackground(views::CreateRoundedRectBackground(kInfoColor, 12));
  SetBorder(views::CreateEmptyBorder(gfx::Insets(24)));

  level_label_ = AddChildView(std::make_unique<views::Label>(
      u"LEVEL " + base::NumberToString16(new_level_) + u"!",
      views::style::CONTEXT_DIALOG_TITLE));
  level_label_->SetEnabledColor(SK_ColorWHITE);
  level_label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
}

void LevelUpNotification::Show() {
  SetVisible(true);
  AnimateIn();
  PlayCelebrationEffect();

  // Auto-dismiss after 2 seconds
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LevelUpNotification::Dismiss, base::Unretained(this)),
      kCelebrationDuration);
}

void LevelUpNotification::Dismiss() {
  // Fade out
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kTransitionDuration)
      .SetOpacity(layer(), 0.0f)
      .Then()
      .SetDuration(base::TimeDelta())
      .SetVisibility(this, false);
}

void LevelUpNotification::AnimateIn() {
  // Scale up with bounce
  gfx::Transform scale;
  scale.Scale(1.2, 1.2);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::Milliseconds(150))
      .SetTransform(layer(), scale)
      .Then()
      .SetDuration(base::Milliseconds(100))
      .SetTransform(layer(), gfx::Transform());
}

void LevelUpNotification::PlayCelebrationEffect() {
  // Would trigger confetti/particle effect
  DLOG(INFO) << "🎉 Celebration effect!";
}

void LevelUpNotification::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  // Draw radial gradient effect
  gfx::Point center = GetLocalBounds().CenterPoint();
  cc::PaintFlags gradient_flags;
  gradient_flags.setAntiAlias(true);

  SkColor colors[] = {SkColorSetA(SK_ColorWHITE, 50),
                     SkColorSetA(kInfoColor, 0)};
  SkScalar positions[] = {0.0f, 1.0f};

  gradient_flags.setShader(cc::PaintShader::MakeRadialGradient(
      gfx::PointToSkPoint(center), 100.0f, colors, positions, 2,
      SkTileMode::kClamp));

  canvas->DrawCircle(center, 100, gradient_flags);
}

BEGIN_METADATA(LevelUpNotification)
END_METADATA
