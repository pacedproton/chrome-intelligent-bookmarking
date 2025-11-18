// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_DUAL_PANE_BOOKMARK_MANAGER_VIEW_H_
#define CHROME_BROWSER_UI_BOOKMARKS_DUAL_PANE_BOOKMARK_MANAGER_VIEW_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "ui/base/models/tree_node_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/controls/tree/tree_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace views {
class Label;
class Textfield;
class SplitView;
class ScrollView;
}  // namespace views

// Column types for bookmark table view
enum class BookmarkColumn {
  kTitle,
  kURL,
  kDateAdded,
  kDateModified,
  kRating,
  kTags,
  kVisitCount,
};

// Sort order for bookmarks
struct BookmarkSortDescriptor {
  BookmarkSortOrder order = BookmarkSortOrder::kAlphabetical;
};

// Undo/Redo action types
enum class UndoActionType {
  kDelete,
  kMove,
  kRename,
  kEdit,
  kBatchTag,
  kBatchArchive,
};

// Undo/Redo action descriptor
struct UndoAction {
  UndoActionType type;
  std::vector<int64_t> affected_node_ids;
  std::string description;
  base::Value::Dict saved_state;
};

// Dual-pane bookmark manager view with advanced features.
//
// This view provides a comprehensive bookmark management interface:
// - Dual-pane layout for easy organization
// - Drag-and-drop between panes
// - Bulk selection and operations
// - Undo/Redo support
// - Keyboard shortcuts
// - Quick preview pane
// - Column customization
// - Advanced filtering and search
// - Tree view with expand/collapse
//
// Usage:
//   auto* manager_view = new DualPaneBookmarkManagerView(bookmark_model);
//   widget->SetContentsView(manager_view);
//
// Keyboard shortcuts:
//   Ctrl+F: Focus search
//   Ctrl+Z: Undo
//   Ctrl+Y: Redo
//   Ctrl+A: Select all
//   Delete: Delete selected
//   Ctrl+D: Duplicate
//   F2: Rename
//   Ctrl+T: Add tags
//   Space: Quick preview
class DualPaneBookmarkManagerView : public views::WidgetDelegateView,
                                    public views::TextfieldController,
                                    public views::TreeViewController,
                                    public views::TableViewObserver {
 public:
  explicit DualPaneBookmarkManagerView(bookmarks::BookmarkModel* model);
  ~DualPaneBookmarkManagerView() override;

  DualPaneBookmarkManagerView(const DualPaneBookmarkManagerView&) = delete;
  DualPaneBookmarkManagerView& operator=(const DualPaneBookmarkManagerView&) =
      delete;

  // ===== View Management =====

  // Show the manager in a new window
  static void Show(bookmarks::BookmarkModel* model);

  // Get current selection
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  GetSelectedBookmarks() const;

  // ===== Drag and Drop =====

  // Enable/disable drag and drop
  void SetDragDropEnabled(bool enabled);

  // ===== Bulk Operations =====

  // Select all bookmarks in current view
  void SelectAll();

  // Clear selection
  void ClearSelection();

  // Delete selected bookmarks
  void DeleteSelected();

  // Duplicate selected bookmarks
  void DuplicateSelected();

  // Add tag to selected bookmarks
  void AddTagToSelected(std::u16string_view tag);

  // Archive selected bookmarks
  void ArchiveSelected();

  // ===== Undo/Redo =====

  // Undo last action
  void Undo();

  // Redo last undone action
  void Redo();

  // Check if undo is available
  [[nodiscard]] bool CanUndo() const;

  // Check if redo is available
  [[nodiscard]] bool CanRedo() const;

  // ===== Search and Filter =====

  // Focus search box
  void FocusSearch();

  // Apply filter
  void SetFilter(const BookmarkFilter& filter);

  // Clear filter
  void ClearFilter();

  // ===== View Configuration =====

  // Show/hide columns
  void SetColumnVisible(BookmarkColumn column, bool visible);

  // Set sort order
  void SetSortOrder(const BookmarkSortDescriptor& sort);

  // Show/hide preview pane
  void SetPreviewPaneVisible(bool visible);

  // Set pane split ratio (0.0 - 1.0)
  void SetPaneSplitRatio(float ratio);

  // ===== Quick Actions =====

  // Open selected bookmarks
  void OpenSelected();

  // Open selected in new tab
  void OpenSelectedInNewTab();

  // Open selected in incognito
  void OpenSelectedInIncognito();

  // Export selected to JSON
  [[nodiscard]] std::string ExportSelected();

  // Find duplicates in current folder
  void FindDuplicatesInCurrentFolder();

  // views::WidgetDelegateView:
  std::u16string GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  void WindowClosing() override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                      const std::u16string& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                     const ui::KeyEvent& key_event) override;

  // views::TreeViewController:
  void OnTreeViewSelectionChanged(views::TreeView* tree_view) override;
  bool CanEdit(views::TreeView* tree_view,
              ui::TreeModelNode* node) override;

  // views::TableViewObserver:
  void OnSelectionChanged() override;
  void OnDoubleClick() override;

 private:
  // Initialize UI components
  void InitializeUI();

  // Create toolbar
  std::unique_ptr<views::View> CreateToolbar();

  // Create left pane (tree view)
  std::unique_ptr<views::View> CreateLeftPane();

  // Create right pane (table view)
  std::unique_ptr<views::View> CreateRightPane();

  // Create bottom toolbar
  std::unique_ptr<views::View> CreateBottomToolbar();

  // Create preview pane
  std::unique_ptr<views::View> CreatePreviewPane();

  // Create UI polish components
  std::unique_ptr<views::View> CreateLoadingState();
  std::unique_ptr<views::View> CreateEmptyState(bool is_filtered);
  std::unique_ptr<views::View> CreateErrorState(std::u16string_view error_message);

  // Update bookmark list based on current folder
  void UpdateBookmarkList();

  // Show/hide different UI states
  void ShowLoadingState();
  void ShowEmptyState(bool is_filtered);
  void ShowErrorState(std::u16string_view error_message);
  void ShowContentState();

  // Update status bar with health score
  void UpdateStatusBar();
  void SetHealthScore(int score);

  // Update preview pane with selected bookmark
  void UpdatePreview();

  // Apply current sort order
  void ApplySortOrder();

  // Handle keyboard shortcuts
  bool HandleKeyboardShortcut(const ui::KeyEvent& event);

  // Add undo action
  void RecordUndoAction(UndoActionType type,
                       const std::vector<int64_t>& node_ids,
                       std::string_view description);

  // Perform undo action
  void PerformUndo(const UndoAction& action);

  // Perform redo action
  void PerformRedo(const UndoAction& action);

  // Drag and drop handlers
  void OnDragEntered();
  void OnDragUpdated(const ui::DropTargetEvent& event);
  void OnDragExited();
  bool OnPerformDrop(const ui::DropTargetEvent& event);

  // Button click handlers
  void OnNewFolderClicked();
  void OnDeleteClicked();
  void OnExportClicked();
  void OnImportClicked();
  void OnFindDuplicatesClicked();
  void OnSettingsClicked();

  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<BookmarkManager> manager_;

  // UI Components
  raw_ptr<views::Textfield> search_box_ = nullptr;
  raw_ptr<views::TreeView> tree_view_ = nullptr;
  raw_ptr<views::TableView> table_view_ = nullptr;
  raw_ptr<views::SplitView> split_view_ = nullptr;
  raw_ptr<views::View> preview_pane_ = nullptr;
  raw_ptr<views::Label> status_label_ = nullptr;

  // UI Polish Components
  raw_ptr<views::View> loading_state_view_ = nullptr;
  raw_ptr<views::View> empty_state_view_ = nullptr;
  raw_ptr<views::View> error_state_view_ = nullptr;
  raw_ptr<views::View> content_container_ = nullptr;

  // Preview pane components
  raw_ptr<views::Label> preview_title_ = nullptr;
  raw_ptr<views::Label> preview_url_ = nullptr;
  raw_ptr<views::Label> preview_tags_ = nullptr;
  raw_ptr<views::Label> preview_description_ = nullptr;
  raw_ptr<views::Label> preview_stats_ = nullptr;

  // State
  raw_ptr<const bookmarks::BookmarkNode> current_folder_ = nullptr;
  BookmarkFilter current_filter_;
  BookmarkSortDescriptor sort_descriptor_;
  std::vector<BookmarkColumn> visible_columns_;
  bool preview_pane_visible_ = true;
  bool drag_drop_enabled_ = true;

  // Currently displayed bookmarks (after filtering and sorting)
  std::vector<const bookmarks::BookmarkNode*> displayed_bookmarks_;

  // Undo/Redo stacks
  std::vector<UndoAction> undo_stack_;
  std::vector<UndoAction> redo_stack_;
  static constexpr size_t kMaxUndoStackSize = 100;

  base::WeakPtrFactory<DualPaneBookmarkManagerView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_DUAL_PANE_BOOKMARK_MANAGER_VIEW_H_
