// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_ALTERNATIVE_VIEWS_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_ALTERNATIVE_VIEWS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "chrome/browser/ui/bookmarks/bookmark_task_manager.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"

// ============================================================================
// ALTERNATIVE TASK VIEWS FOR DIFFERENT WORKING STYLES
// ============================================================================
//
// These views complement the Smart Workspace and Kanban systems by providing
// alternative interfaces for different types of people:
//
// 1. **Bookmark Hierarchical Organizer** - For structured thinkers
//    - Think: macOS Finder, Windows Explorer for tasks
//    - Perfect for: Project managers, organizers, hierarchical thinkers
//    - Key feature: Deep folder nesting with visual tree
//
// 2. **Window/Tab Mirror View** - For context switchers
//    - Think: macOS Mission Control, Windows Task View for browser sessions
//    - Perfect for: Multi-taskers, researchers, people with many projects
//    - Key feature: Save/restore entire browser window states as workspaces

namespace bookmarks {
class BookmarkNode;
class BookmarkModel;
}  // namespace bookmarks

// ===== Bookmark Hierarchical Organizer =====

// Project/folder structure for hierarchical organization
struct ProjectFolder {
  std::u16string name;
  std::u16string icon;  // Emoji or icon
  SkColor color;

  const bookmarks::BookmarkNode* bookmark_folder;  // Backing storage

  std::vector<const bookmarks::BookmarkNode*> tasks;  // Tasks in this folder
  std::vector<std::unique_ptr<ProjectFolder>> subfolders;  // Nested folders

  // Metadata
  base::Time created;
  base::Time last_modified;
  int total_tasks = 0;         // Including subfolders
  int completed_tasks = 0;     // Including subfolders
  float completion_percent = 0.0f;

  // Tree expansion state
  bool is_expanded = true;
  bool is_selected = false;

  // Quick stats
  int overdue_count = 0;
  int due_today_count = 0;
  int high_priority_count = 0;
};

// Tree item in the hierarchical view
class ProjectTreeItem : public views::View {
 public:
  METADATA_HEADER(ProjectTreeItem);

  ProjectTreeItem(ProjectFolder* folder, int depth_level);
  ~ProjectTreeItem() override;

  // Expansion
  void SetExpanded(bool expanded);
  [[nodiscard]] bool IsExpanded() const { return folder_->is_expanded; }

  // Selection
  void SetSelected(bool selected);
  [[nodiscard]] bool IsSelected() const { return folder_->is_selected; }

  // Visual
  void UpdateStats();  // Refresh displayed stats
  void SetDepth(int depth);  // Indentation level

  [[nodiscard]] ProjectFolder* folder() const { return folder_; }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

 private:
  void CreateLayout();
  void UpdateProgressBar();

  raw_ptr<ProjectFolder> folder_;
  int depth_level_ = 0;
  bool is_hovered_ = false;

  raw_ptr<views::View> expand_button_;
  raw_ptr<views::Label> name_label_;
  raw_ptr<views::Label> stats_label_;
  raw_ptr<views::View> progress_bar_;
};

// Main hierarchical organizer view
class BookmarkHierarchicalOrganizer : public views::View {
 public:
  METADATA_HEADER(BookmarkHierarchicalOrganizer);

  explicit BookmarkHierarchicalOrganizer(BookmarkManager* manager,
                                         BookmarkTaskManager* task_manager);
  ~BookmarkHierarchicalOrganizer() override;

  // Folder operations
  ProjectFolder* CreateFolder(const std::u16string& name,
                             ProjectFolder* parent = nullptr);
  void DeleteFolder(ProjectFolder* folder);
  void RenameFolder(ProjectFolder* folder, const std::u16string& new_name);
  void MoveFolder(ProjectFolder* folder, ProjectFolder* new_parent);

  // Task operations
  void AddTaskToFolder(const bookmarks::BookmarkNode* task,
                      ProjectFolder* folder);
  void MoveTaskToFolder(const bookmarks::BookmarkNode* task,
                       ProjectFolder* from,
                       ProjectFolder* to);

  // Navigation
  void NavigateToFolder(ProjectFolder* folder);
  void NavigateUp();  // Go to parent folder
  void NavigateToRoot();

  // Search within hierarchy
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  SearchInFolder(ProjectFolder* folder, std::u16string_view query,
                bool include_subfolders = true);

  // Expansion
  void ExpandAll();
  void CollapseAll();
  void ExpandFolder(ProjectFolder* folder);
  void CollapseFolder(ProjectFolder* folder);

  // Smart organization
  void AutoOrganizeByDomain();    // Group tasks by website domain
  void AutoOrganizeByDueDate();   // Create Today/Week/Month folders
  void AutoOrganizeByPriority();  // Create High/Medium/Low folders
  void AutoOrganizeByType();      // Create Read/Watch/Buy folders

  // Drag and drop
  void EnableDragDrop(bool enabled);
  void OnTaskDragged(const bookmarks::BookmarkNode* task,
                    ProjectFolder* target_folder);

  // View modes
  enum class ViewMode {
    kTree,        // Full tree view (default)
    kList,        // Flat list with folder context
    kMiller,      // Miller columns (macOS Finder style)
    kBreadcrumb   // Breadcrumb navigation
  };
  void SetViewMode(ViewMode mode);

  [[nodiscard]] ProjectFolder* current_folder() const {
    return current_folder_;
  }
  [[nodiscard]] ProjectFolder* root_folder() const { return root_folder_.get(); }

 private:
  void CreateLayout();
  void CreateToolbar();
  void CreateBreadcrumb();
  void CreateTreeView();
  void CreateTaskListArea();

  void RefreshTree();
  void RefreshBreadcrumb();
  void UpdateFolderStats(ProjectFolder* folder);

  // Recursively calculate stats
  void CalculateStatsRecursive(ProjectFolder* folder);

  raw_ptr<BookmarkManager> manager_;
  raw_ptr<BookmarkTaskManager> task_manager_;

  std::unique_ptr<ProjectFolder> root_folder_;
  raw_ptr<ProjectFolder> current_folder_ = nullptr;
  ViewMode current_view_mode_ = ViewMode::kTree;

  raw_ptr<views::View> toolbar_;
  raw_ptr<views::View> breadcrumb_container_;
  raw_ptr<views::View> tree_container_;
  raw_ptr<views::View> task_list_;

  bool drag_drop_enabled_ = true;
};

// ===== Window/Tab Mirror View =====

// Represents a saved browser window state
struct WindowWorkspace {
  std::u16string name;           // "Website Redesign Project"
  std::u16string description;    // Optional description
  SkColor theme_color;           // Visual identification

  // Window state
  struct TabState {
    std::u16string title;
    std::u16string url;
    const bookmarks::BookmarkNode* task_bookmark;  // Linked task
    gfx::ImageSkia favicon;
    bool is_pinned = false;
    bool is_active = false;
  };

  std::vector<TabState> tabs;
  gfx::Rect window_bounds;  // Position and size
  bool is_maximized = false;
  bool is_minimized = false;

  // Metadata
  base::Time created;
  base::Time last_used;
  base::Time last_saved;
  int access_count = 0;

  // Quick stats
  int tab_count = 0;
  int completed_tasks = 0;
  int total_tasks = 0;
  float completion_percent = 0.0f;

  // Smart features
  bool auto_save = true;          // Save changes automatically
  bool restore_on_startup = false;  // Open on browser start
  std::vector<std::u16string> tags;  // For organization
};

// Visual card for a window workspace
class WindowWorkspaceCard : public views::View {
 public:
  METADATA_HEADER(WindowWorkspaceCard);

  explicit WindowWorkspaceCard(WindowWorkspace* workspace);
  ~WindowWorkspaceCard() override;

  // Actions
  void Restore();        // Open this window workspace
  void Update();         // Update from current window state
  void Delete();         // Delete this workspace
  void Pin();            // Pin to top of list
  void Archive();        // Archive (hide from main view)

  // Visual
  void SetThumbnail(const gfx::ImageSkia& thumbnail);
  void ShowQuickActions(bool show);

  [[nodiscard]] WindowWorkspace* workspace() const { return workspace_; }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

 private:
  void CreateLayout();
  void CreateTabPreview();  // Mini tab strip preview
  void UpdatePreview();

  raw_ptr<WindowWorkspace> workspace_;
  gfx::ImageSkia thumbnail_;
  bool is_hovered_ = false;
  bool showing_quick_actions_ = false;

  raw_ptr<views::View> tab_preview_container_;
  raw_ptr<views::Label> title_label_;
  raw_ptr<views::Label> stats_label_;
};

// Main window/tab mirror view
class WindowTabMirrorView : public views::View {
 public:
  METADATA_HEADER(WindowTabMirrorView);

  explicit WindowTabMirrorView(BookmarkManager* manager,
                               BookmarkTaskManager* task_manager);
  ~WindowTabMirrorView() override;

  // Workspace management
  WindowWorkspace* CreateWorkspace(const std::u16string& name);
  void DeleteWorkspace(WindowWorkspace* workspace);
  void RenameWorkspace(WindowWorkspace* workspace,
                      const std::u16string& new_name);

  // Capture operations
  void CaptureCurrentWindow(const std::u16string& workspace_name);
  void CaptureAllWindows();  // Save all open windows
  void UpdateWorkspace(WindowWorkspace* workspace);  // Update from current state

  // Restore operations
  void RestoreWorkspace(WindowWorkspace* workspace);
  void RestoreMultipleWorkspaces(
      const std::vector<WindowWorkspace*>& workspaces);

  // Smart features
  void AutoSaveCurrentWindow();  // Periodically save current window
  void SuggestWorkspaceFromTabs();  // Suggest creating workspace from current tabs

  // Session management
  void SaveSession(const std::u16string& session_name);  // Save all workspaces
  void LoadSession(const std::u16string& session_name);  // Load saved session
  [[nodiscard]] std::vector<std::u16string> GetSavedSessions() const;

  // Organization
  enum class SortOrder {
    kMostRecent,
    kMostUsed,
    kAlphabetical,
    kTabCount,
    kCompletion
  };
  void SetSortOrder(SortOrder order);

  // Filters
  void FilterByTags(const std::vector<std::u16string>& tags);
  void ShowPinnedOnly(bool pinned_only);

  // View modes
  enum class ViewMode {
    kGrid,       // Grid of workspace cards (default)
    kList,       // Vertical list
    kTimeline,   // Timeline by last used
    kGrouped     // Grouped by tags
  };
  void SetViewMode(ViewMode mode);

  // Quick actions
  void MergeWindows(const std::vector<WindowWorkspace*>& workspaces);
  void SplitWorkspace(WindowWorkspace* workspace,
                     const std::vector<int>& tab_indices);

  [[nodiscard]] const std::vector<std::unique_ptr<WindowWorkspace>>&
  workspaces() const {
    return workspaces_;
  }

 private:
  void CreateLayout();
  void CreateToolbar();
  void CreateWorkspaceGrid();
  void RefreshWorkspaces();

  // Capture helpers
  WindowWorkspace::TabState CaptureTab(/* tab info */);
  gfx::ImageSkia CaptureWindowThumbnail(/* window */);

  raw_ptr<BookmarkManager> manager_;
  raw_ptr<BookmarkTaskManager> task_manager_;

  std::vector<std::unique_ptr<WindowWorkspace>> workspaces_;
  SortOrder current_sort_order_ = SortOrder::kMostRecent;
  ViewMode current_view_mode_ = ViewMode::kGrid;

  raw_ptr<views::View> toolbar_;
  raw_ptr<views::View> workspace_container_;

  bool auto_save_enabled_ = true;
  base::TimeDelta auto_save_interval_ = base::Minutes(5);
};

// ===== Integration Helper =====

// Manages switching between different task view modes
class TaskViewSwitcher : public views::View {
 public:
  METADATA_HEADER(TaskViewSwitcher);

  TaskViewSwitcher(BookmarkManager* manager,
                  BookmarkTaskManager* task_manager);
  ~TaskViewSwitcher() override;

  enum class ViewType {
    kSmartWorkspace,      // Floating context-aware (default)
    kKanban,              // Visual board
    kTaskFlow,            // Now/Next/Soon progression
    kSidebar,             // Tree-based sidebar
    kHierarchical,        // Project folders (NEW)
    kWindowMirror         // Window/tab workspaces (NEW)
  };

  // Switch views
  void SwitchTo(ViewType type);
  [[nodiscard]] ViewType current_view() const { return current_view_; }

  // Quick actions available in all views
  void QuickCapture();
  void QuickSearch();
  void QuickFilter();

  // User preferences
  void SetDefaultView(ViewType type);
  void RememberLastView(bool remember);

  // Smart suggestions
  [[nodiscard]] ViewType SuggestBestView() const;  // Based on user behavior

 private:
  void CreateLayout();
  void ShowViewSelector();  // Dropdown to switch views

  raw_ptr<BookmarkManager> manager_;
  raw_ptr<BookmarkTaskManager> task_manager_;

  std::unique_ptr<BookmarkHierarchicalOrganizer> hierarchical_view_;
  std::unique_ptr<WindowTabMirrorView> window_mirror_view_;

  ViewType current_view_ = ViewType::kSmartWorkspace;
  ViewType default_view_ = ViewType::kSmartWorkspace;
  bool remember_last_view_ = true;

  raw_ptr<views::View> view_container_;
  raw_ptr<views::View> view_selector_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_ALTERNATIVE_VIEWS_H_
