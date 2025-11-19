// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_alternative_views.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// Design constants
constexpr int kTreeItemHeight = 32;
constexpr int kTreeIndentSize = 20;
constexpr int kWorkspaceCardWidth = 300;
constexpr int kWorkspaceCardHeight = 200;
constexpr int kSpacing = 12;
constexpr int kBorderRadius = 8;

// Colors
constexpr SkColor kTreeItemHover = SkColorSetARGB(8, 0, 0, 0);
constexpr SkColor kTreeItemSelected = SkColorSetARGB(24, 66, 133, 244);
constexpr SkColor kAccentColor = SkColorSetRGB(66, 133, 244);
constexpr SkColor kFolderColor = SkColorSetRGB(251, 188, 5);

}  // namespace

// ===== ProjectTreeItem =====

ProjectTreeItem::ProjectTreeItem(ProjectFolder* folder, int depth_level)
    : folder_(folder), depth_level_(depth_level) {
  DCHECK(folder_);
  CreateLayout();
}

ProjectTreeItem::~ProjectTreeItem() = default;

void ProjectTreeItem::CreateLayout() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(4, depth_level_ * kTreeIndentSize), 8));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  SetPreferredSize(gfx::Size(0, kTreeItemHeight));

  // Expand button
  expand_button_ = AddChildView(std::make_unique<views::View>());
  expand_button_->SetPreferredSize(gfx::Size(16, 16));

  // Folder icon
  auto* icon = AddChildView(std::make_unique<views::Label>(
      folder_->icon.empty() ? u"📁" : folder_->icon));

  // Name
  name_label_ = AddChildView(std::make_unique<views::Label>(
      folder_->name, views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY));
  name_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->SetFlexForView(name_label_, 1);

  // Stats
  stats_label_ = AddChildView(std::make_unique<views::Label>());
  stats_label_->SetEnabledColor(SkColorSetRGB(95, 99, 104));
  UpdateStats();

  // Progress bar
  progress_bar_ = AddChildView(std::make_unique<views::View>());
  progress_bar_->SetPreferredSize(gfx::Size(60, 4));
  UpdateProgressBar();
}

void ProjectTreeItem::SetExpanded(bool expanded) {
  if (folder_->is_expanded == expanded) {
    return;
  }
  folder_->is_expanded = expanded;
  SchedulePaint();
}

void ProjectTreeItem::SetSelected(bool selected) {
  if (folder_->is_selected == selected) {
    return;
  }
  folder_->is_selected = selected;
  SchedulePaint();
}

void ProjectTreeItem::UpdateStats() {
  if (!stats_label_) {
    return;
  }

  std::u16string stats = base::NumberToString16(folder_->total_tasks);
  if (folder_->overdue_count > 0) {
    stats += u" • " + base::NumberToString16(folder_->overdue_count) + u" overdue";
  }

  stats_label_->SetText(stats);
}

void ProjectTreeItem::UpdateProgressBar() {
  // Progress bar is rendered in OnPaint
}

void ProjectTreeItem::SetDepth(int depth) {
  depth_level_ = depth;
  InvalidateLayout();
}

void ProjectTreeItem::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  // Background
  if (folder_->is_selected) {
    cc::PaintFlags flags;
    flags.setColor(kTreeItemSelected);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawRoundRect(GetLocalBounds(), kBorderRadius, flags);
  } else if (is_hovered_) {
    cc::PaintFlags flags;
    flags.setColor(kTreeItemHover);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawRoundRect(GetLocalBounds(), kBorderRadius, flags);
  }

  // Progress bar (if any tasks)
  if (folder_->total_tasks > 0 && progress_bar_) {
    gfx::Rect progress_bounds = progress_bar_->bounds();
    int filled_width = static_cast<int>(
        progress_bounds.width() * folder_->completion_percent / 100.0f);

    cc::PaintFlags bg_flags;
    bg_flags.setColor(SkColorSetARGB(30, 0, 0, 0));
    bg_flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawRoundRect(progress_bounds, 2, bg_flags);

    if (filled_width > 0) {
      gfx::Rect filled_rect = progress_bounds;
      filled_rect.set_width(filled_width);

      cc::PaintFlags filled_flags;
      filled_flags.setColor(kAccentColor);
      filled_flags.setStyle(cc::PaintFlags::kFill_Style);
      canvas->DrawRoundRect(filled_rect, 2, filled_flags);
    }
  }
}

bool ProjectTreeItem::OnMousePressed(const ui::MouseEvent& event) {
  SetSelected(true);
  return true;
}

void ProjectTreeItem::OnMouseEntered(const ui::MouseEvent& event) {
  is_hovered_ = true;
  SchedulePaint();
}

void ProjectTreeItem::OnMouseExited(const ui::MouseEvent& event) {
  is_hovered_ = false;
  SchedulePaint();
}

BEGIN_METADATA(ProjectTreeItem)
END_METADATA

// ===== BookmarkHierarchicalOrganizer =====

BookmarkHierarchicalOrganizer::BookmarkHierarchicalOrganizer(
    BookmarkManager* manager,
    BookmarkTaskManager* task_manager)
    : manager_(manager), task_manager_(task_manager) {
  DCHECK(manager_);
  DCHECK(task_manager_);

  // Create root folder
  root_folder_ = std::make_unique<ProjectFolder>();
  root_folder_->name = u"All Tasks";
  root_folder_->icon = u"📚";
  root_folder_->color = kAccentColor;
  root_folder_->created = base::Time::Now();
  current_folder_ = root_folder_.get();

  CreateLayout();
}

BookmarkHierarchicalOrganizer::~BookmarkHierarchicalOrganizer() = default;

void BookmarkHierarchicalOrganizer::CreateLayout() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  CreateToolbar();
  CreateBreadcrumb();
  CreateTreeView();
}

void BookmarkHierarchicalOrganizer::CreateToolbar() {
  toolbar_ = AddChildView(std::make_unique<views::View>());
  auto* layout = toolbar_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(kSpacing), kSpacing));

  // New folder button
  auto* new_folder_btn = toolbar_->AddChildView(
      std::make_unique<views::LabelButton>(
          base::BindRepeating([](BookmarkHierarchicalOrganizer* self) {
            self->CreateFolder(u"New Project");
          }, this),
          u"📁 New Folder"));

  // Auto-organize buttons
  auto* auto_org_btn = toolbar_->AddChildView(
      std::make_unique<views::LabelButton>(
          base::BindRepeating([](BookmarkHierarchicalOrganizer* self) {
            self->AutoOrganizeByDomain();
          }, this),
          u"🔄 Auto-Organize"));

  toolbar_->SetBorder(views::CreateSolidSidedBorder(
      gfx::Insets::TLBR(0, 0, 1, 0), SkColorSetARGB(20, 0, 0, 0)));
}

void BookmarkHierarchicalOrganizer::CreateBreadcrumb() {
  breadcrumb_container_ = AddChildView(std::make_unique<views::View>());
  breadcrumb_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(kSpacing), 4));
  breadcrumb_container_->SetBorder(views::CreateSolidSidedBorder(
      gfx::Insets::TLBR(0, 0, 1, 0), SkColorSetARGB(20, 0, 0, 0)));

  RefreshBreadcrumb();
}

void BookmarkHierarchicalOrganizer::CreateTreeView() {
  auto* scroll = AddChildView(std::make_unique<views::ScrollView>());
  scroll->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);

  tree_container_ = scroll->SetContents(
      std::make_unique<views::View>());
  tree_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  auto* layout = GetLayoutManager()->AsBoxLayout();
  layout->SetFlexForView(scroll, 1);

  RefreshTree();
}

ProjectFolder* BookmarkHierarchicalOrganizer::CreateFolder(
    const std::u16string& name,
    ProjectFolder* parent) {
  DCHECK(!name.empty());

  if (!parent) {
    parent = current_folder_;
  }

  auto folder = std::make_unique<ProjectFolder>();
  folder->name = name;
  folder->icon = u"📁";
  folder->color = kFolderColor;
  folder->created = base::Time::Now();
  folder->last_modified = folder->created;

  // Create backing bookmark folder
  if (manager_->model()) {
    const bookmarks::BookmarkNode* parent_node =
        parent->bookmark_folder ? parent->bookmark_folder
                                : manager_->model()->bookmark_bar_node();

    folder->bookmark_folder = manager_->model()->AddFolder(
        parent_node, 0, name);
  }

  ProjectFolder* folder_ptr = folder.get();
  parent->subfolders.push_back(std::move(folder));

  RefreshTree();
  DLOG(INFO) << "Created folder: " << name;

  return folder_ptr;
}

void BookmarkHierarchicalOrganizer::DeleteFolder(ProjectFolder* folder) {
  DCHECK(folder);
  DCHECK(folder != root_folder_.get());  // Can't delete root

  // Remove from bookmark model
  if (folder->bookmark_folder && manager_->model()) {
    manager_->model()->Remove(folder->bookmark_folder,
                             bookmarks::metrics::BookmarkEditSource::kUser);
  }

  // Remove from parent (implementation would go here)
  RefreshTree();
  DLOG(INFO) << "Deleted folder";
}

void BookmarkHierarchicalOrganizer::AddTaskToFolder(
    const bookmarks::BookmarkNode* task,
    ProjectFolder* folder) {
  DCHECK(task);
  DCHECK(folder);

  folder->tasks.push_back(task);
  folder->total_tasks++;

  UpdateFolderStats(folder);
  RefreshTree();

  DLOG(INFO) << "Added task to folder: " << folder->name;
}

void BookmarkHierarchicalOrganizer::NavigateToFolder(ProjectFolder* folder) {
  DCHECK(folder);

  current_folder_ = folder;
  RefreshBreadcrumb();
  RefreshTree();
}

void BookmarkHierarchicalOrganizer::NavigateUp() {
  // Navigate to parent (implementation would track parent pointers)
  RefreshBreadcrumb();
  RefreshTree();
}

void BookmarkHierarchicalOrganizer::NavigateToRoot() {
  current_folder_ = root_folder_.get();
  RefreshBreadcrumb();
  RefreshTree();
}

void BookmarkHierarchicalOrganizer::AutoOrganizeByDomain() {
  DLOG(INFO) << "Auto-organizing by domain";

  // Group tasks by domain
  std::map<std::u16string, std::vector<const bookmarks::BookmarkNode*>>
      domain_groups;

  for (const auto* task : current_folder_->tasks) {
    std::string url = task->url().spec();
    // Extract domain
    size_t start = url.find("://");
    if (start != std::string::npos) {
      start += 3;
      size_t end = url.find("/", start);
      std::string domain = url.substr(start, end - start);
      domain_groups[base::UTF8ToUTF16(domain)].push_back(task);
    }
  }

  // Create folders for each domain
  for (const auto& [domain, tasks] : domain_groups) {
    if (tasks.size() >= 2) {  // Only create folder if multiple tasks
      ProjectFolder* domain_folder = CreateFolder(domain, current_folder_);
      for (const auto* task : tasks) {
        AddTaskToFolder(task, domain_folder);
      }
    }
  }

  RefreshTree();
}

void BookmarkHierarchicalOrganizer::AutoOrganizeByDueDate() {
  // Create Today/This Week/This Month folders
  ProjectFolder* today = CreateFolder(u"Due Today", current_folder_);
  ProjectFolder* week = CreateFolder(u"This Week", current_folder_);
  ProjectFolder* month = CreateFolder(u"This Month", current_folder_);

  base::Time now = base::Time::Now();

  for (const auto* task : current_folder_->tasks) {
    auto metadata = task_manager_->GetTaskMetadata(task);
    if (metadata.has_value() && metadata->due_date.has_value()) {
      base::TimeDelta delta = metadata->due_date.value() - now;

      if (delta.InDays() == 0) {
        AddTaskToFolder(task, today);
      } else if (delta.InDays() < 7) {
        AddTaskToFolder(task, week);
      } else if (delta.InDays() < 30) {
        AddTaskToFolder(task, month);
      }
    }
  }

  RefreshTree();
}

void BookmarkHierarchicalOrganizer::ExpandAll() {
  // Recursively expand all folders
  std::function<void(ProjectFolder*)> expand_recursive =
      [&](ProjectFolder* folder) {
        folder->is_expanded = true;
        for (const auto& subfolder : folder->subfolders) {
          expand_recursive(subfolder.get());
        }
      };

  expand_recursive(root_folder_.get());
  RefreshTree();
}

void BookmarkHierarchicalOrganizer::CollapseAll() {
  std::function<void(ProjectFolder*)> collapse_recursive =
      [&](ProjectFolder* folder) {
        folder->is_expanded = false;
        for (const auto& subfolder : folder->subfolders) {
          collapse_recursive(subfolder.get());
        }
      };

  collapse_recursive(root_folder_.get());
  RefreshTree();
}

void BookmarkHierarchicalOrganizer::RefreshTree() {
  if (!tree_container_) {
    return;
  }

  tree_container_->RemoveAllChildViews();

  // Recursively add tree items
  std::function<void(ProjectFolder*, int)> add_items =
      [&](ProjectFolder* folder, int depth) {
        auto* item = tree_container_->AddChildView(
            std::make_unique<ProjectTreeItem>(folder, depth));

        if (folder->is_expanded) {
          for (const auto& subfolder : folder->subfolders) {
            add_items(subfolder.get(), depth + 1);
          }
        }
      };

  add_items(root_folder_.get(), 0);
}

void BookmarkHierarchicalOrganizer::RefreshBreadcrumb() {
  if (!breadcrumb_container_) {
    return;
  }

  breadcrumb_container_->RemoveAllChildViews();

  // Add breadcrumb items (implementation would track path)
  auto* label = breadcrumb_container_->AddChildView(
      std::make_unique<views::Label>(
          current_folder_->name, views::style::CONTEXT_LABEL,
          views::style::STYLE_PRIMARY));
}

void BookmarkHierarchicalOrganizer::UpdateFolderStats(ProjectFolder* folder) {
  DCHECK(folder);

  CalculateStatsRecursive(folder);
}

void BookmarkHierarchicalOrganizer::CalculateStatsRecursive(
    ProjectFolder* folder) {
  folder->total_tasks = folder->tasks.size();
  folder->completed_tasks = 0;
  folder->overdue_count = 0;
  folder->due_today_count = 0;
  folder->high_priority_count = 0;

  base::Time now = base::Time::Now();

  // Count task states
  for (const auto* task : folder->tasks) {
    auto metadata = task_manager_->GetTaskMetadata(task);
    if (metadata.has_value()) {
      if (metadata->status == TaskStatus::kDone) {
        folder->completed_tasks++;
      }

      if (metadata->due_date.has_value()) {
        base::TimeDelta delta = metadata->due_date.value() - now;
        if (delta.InDays() < 0) {
          folder->overdue_count++;
        } else if (delta.InDays() == 0) {
          folder->due_today_count++;
        }
      }

      if (metadata->priority == TaskPriority::kHigh ||
          metadata->priority == TaskPriority::kCritical) {
        folder->high_priority_count++;
      }
    }
  }

  // Recurse to subfolders
  for (const auto& subfolder : folder->subfolders) {
    CalculateStatsRecursive(subfolder.get());
    folder->total_tasks += subfolder->total_tasks;
    folder->completed_tasks += subfolder->completed_tasks;
    folder->overdue_count += subfolder->overdue_count;
  }

  // Calculate completion percentage
  if (folder->total_tasks > 0) {
    folder->completion_percent = static_cast<float>(folder->completed_tasks) /
                                folder->total_tasks * 100.0f;
  }

  folder->last_modified = base::Time::Now();
}

BEGIN_METADATA(BookmarkHierarchicalOrganizer)
END_METADATA

// ===== WindowWorkspaceCard =====

WindowWorkspaceCard::WindowWorkspaceCard(WindowWorkspace* workspace)
    : workspace_(workspace) {
  DCHECK(workspace_);
  CreateLayout();
}

WindowWorkspaceCard::~WindowWorkspaceCard() = default;

void WindowWorkspaceCard::CreateLayout() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(kSpacing), kSpacing));

  SetPreferredSize(gfx::Size(kWorkspaceCardWidth, kWorkspaceCardHeight));
  SetBackground(views::CreateRoundedRectBackground(SK_ColorWHITE, kBorderRadius));
  SetBorder(views::CreateRoundedRectBorder(
      1, kBorderRadius, SkColorSetARGB(20, 0, 0, 0)));

  // Title
  title_label_ = AddChildView(std::make_unique<views::Label>(
      workspace_->name, views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY));
  title_label_->SetFontList(gfx::FontList().Derive(2, gfx::Font::NORMAL,
                                                   gfx::Font::Weight::SEMIBOLD));

  // Tab preview container
  tab_preview_container_ = AddChildView(std::make_unique<views::View>());
  tab_preview_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(), 2));

  layout->SetFlexForView(tab_preview_container_, 1);

  CreateTabPreview();

  // Stats
  std::u16string stats = base::NumberToString16(workspace_->tab_count) +
                        u" tabs • " +
                        base::NumberToString16(static_cast<int>(
                            workspace_->completion_percent)) + u"% complete";

  stats_label_ = AddChildView(std::make_unique<views::Label>(
      stats, views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  stats_label_->SetEnabledColor(SkColorSetRGB(95, 99, 104));
}

void WindowWorkspaceCard::CreateTabPreview() {
  if (!tab_preview_container_) {
    return;
  }

  tab_preview_container_->RemoveAllChildViews();

  // Show first 5 tabs as preview
  int preview_count = std::min(5, static_cast<int>(workspace_->tabs.size()));
  for (int i = 0; i < preview_count; ++i) {
    const auto& tab = workspace_->tabs[i];

    auto* tab_item = tab_preview_container_->AddChildView(
        std::make_unique<views::View>());
    auto* tab_layout = tab_item->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            gfx::Insets::VH(2, 4), 4));

    // Favicon placeholder
    auto* favicon = tab_item->AddChildView(
        std::make_unique<views::View>());
    favicon->SetPreferredSize(gfx::Size(16, 16));

    // Title (truncated)
    auto* title = tab_item->AddChildView(std::make_unique<views::Label>(
        tab.title, views::style::CONTEXT_LABEL));
    title->SetElideBehavior(gfx::ELIDE_TAIL);
    tab_layout->SetFlexForView(title, 1);

    tab_item->SetBackground(
        views::CreateRoundedRectBackground(SkColorSetARGB(10, 0, 0, 0), 4));
  }

  if (workspace_->tabs.size() > 5) {
    auto* more = tab_preview_container_->AddChildView(
        std::make_unique<views::Label>(
            u"+" + base::NumberToString16(workspace_->tabs.size() - 5) +
            u" more..."));
    more->SetEnabledColor(SkColorSetRGB(95, 99, 104));
  }
}

void WindowWorkspaceCard::Restore() {
  DLOG(INFO) << "Restoring workspace: " << workspace_->name;
  workspace_->access_count++;
  workspace_->last_used = base::Time::Now();
}

void WindowWorkspaceCard::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  if (is_hovered_) {
    cc::PaintFlags flags;
    flags.setColor(SkColorSetARGB(8, 0, 0, 0));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawRoundRect(GetLocalBounds(), kBorderRadius, flags);
  }
}

bool WindowWorkspaceCard::OnMousePressed(const ui::MouseEvent& event) {
  Restore();
  return true;
}

void WindowWorkspaceCard::OnMouseEntered(const ui::MouseEvent& event) {
  is_hovered_ = true;
  SchedulePaint();
}

void WindowWorkspaceCard::OnMouseExited(const ui::MouseEvent& event) {
  is_hovered_ = false;
  SchedulePaint();
}

BEGIN_METADATA(WindowWorkspaceCard)
END_METADATA

// ===== WindowTabMirrorView =====

WindowTabMirrorView::WindowTabMirrorView(BookmarkManager* manager,
                                         BookmarkTaskManager* task_manager)
    : manager_(manager), task_manager_(task_manager) {
  DCHECK(manager_);
  DCHECK(task_manager_);
  CreateLayout();
}

WindowTabMirrorView::~WindowTabMirrorView() = default;

void WindowTabMirrorView::CreateLayout() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  CreateToolbar();
  CreateWorkspaceGrid();
}

void WindowTabMirrorView::CreateToolbar() {
  toolbar_ = AddChildView(std::make_unique<views::View>());
  toolbar_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(kSpacing), kSpacing));

  // Capture current window button
  auto* capture_btn = toolbar_->AddChildView(
      std::make_unique<views::LabelButton>(
          base::BindRepeating([](WindowTabMirrorView* self) {
            self->CaptureCurrentWindow(u"Captured Window");
          }, this),
          u"💾 Capture Window"));

  toolbar_->SetBorder(views::CreateSolidSidedBorder(
      gfx::Insets::TLBR(0, 0, 1, 0), SkColorSetARGB(20, 0, 0, 0)));
}

void WindowTabMirrorView::CreateWorkspaceGrid() {
  auto* scroll = AddChildView(std::make_unique<views::ScrollView>());

  workspace_container_ = scroll->SetContents(
      std::make_unique<views::View>());
  workspace_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets(kSpacing), kSpacing));

  auto* layout = GetLayoutManager()->AsBoxLayout();
  layout->SetFlexForView(scroll, 1);
}

WindowWorkspace* WindowTabMirrorView::CreateWorkspace(
    const std::u16string& name) {
  DCHECK(!name.empty());

  auto workspace = std::make_unique<WindowWorkspace>();
  workspace->name = name;
  workspace->theme_color = kAccentColor;
  workspace->created = base::Time::Now();
  workspace->last_used = workspace->created;
  workspace->last_saved = workspace->created;

  WindowWorkspace* workspace_ptr = workspace.get();
  workspaces_.push_back(std::move(workspace));

  RefreshWorkspaces();

  DLOG(INFO) << "Created workspace: " << name;

  return workspace_ptr;
}

void WindowTabMirrorView::CaptureCurrentWindow(
    const std::u16string& workspace_name) {
  WindowWorkspace* workspace = CreateWorkspace(workspace_name);

  // In a full implementation, this would capture actual browser window state
  // For now, create sample tabs
  WindowWorkspace::TabState tab;
  tab.title = u"Example Tab";
  tab.url = u"https://example.com";
  tab.is_active = true;

  workspace->tabs.push_back(tab);
  workspace->tab_count = workspace->tabs.size();

  RefreshWorkspaces();

  DLOG(INFO) << "Captured current window as: " << workspace_name;
}

void WindowTabMirrorView::RestoreWorkspace(WindowWorkspace* workspace) {
  DCHECK(workspace);

  DLOG(INFO) << "Restoring workspace: " << workspace->name
            << " with " << workspace->tab_count << " tabs";

  workspace->access_count++;
  workspace->last_used = base::Time::Now();

  // In full implementation, this would open a new browser window
  // and restore all tabs
}

void WindowTabMirrorView::RefreshWorkspaces() {
  if (!workspace_container_) {
    return;
  }

  workspace_container_->RemoveAllChildViews();

  // Sort workspaces
  std::vector<WindowWorkspace*> sorted_workspaces;
  for (const auto& workspace : workspaces_) {
    sorted_workspaces.push_back(workspace.get());
  }

  if (current_sort_order_ == SortOrder::kMostRecent) {
    std::sort(sorted_workspaces.begin(), sorted_workspaces.end(),
             [](const WindowWorkspace* a, const WindowWorkspace* b) {
               return a->last_used > b->last_used;
             });
  }

  // Add workspace cards
  for (auto* workspace : sorted_workspaces) {
    workspace_container_->AddChildView(
        std::make_unique<WindowWorkspaceCard>(workspace));
  }
}

BEGIN_METADATA(WindowTabMirrorView)
END_METADATA
