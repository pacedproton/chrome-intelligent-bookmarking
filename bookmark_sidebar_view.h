// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_SIDEBAR_VIEW_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_SIDEBAR_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class BoxLayoutView;
class Button;
class ImageButton;
class Label;
class ScrollView;
class Textfield;
class TreeView;
}  // namespace views

class BookmarkManager;

// Modern, minimal sidebar for bookmark browsing and navigation.
// Inspired by VS Code sidebar and modern file explorers.
//
// Features:
// - Collapsible/expandable (Ctrl+B)
// - Instant search at top
// - Tree view with folder icons
// - Quick access section (Favorites, Recent, Tags)
// - Right-click context menus
// - Drag-and-drop support
// - Keyboard navigation (arrow keys, Enter, Delete)
// - Responsive width (200-400px)
// - Auto-hide on small screens

// ===== Sidebar Header =====

class BookmarkSidebarHeader : public views::View {
  METADATA_HEADER(BookmarkSidebarHeader, views::View)

 public:
  explicit BookmarkSidebarHeader(BookmarkManager* manager);
  ~BookmarkSidebarHeader() override;

  // Search
  void FocusSearch();
  void ClearSearch();
  std::u16string GetSearchQuery() const;

  // Callbacks
  void SetOnSearchCallback(base::RepeatingCallback<void(std::u16string_view)> callback);
  void SetOnCollapseCallback(base::RepeatingClosure callback);

 private:
  void CreateLayout();
  void OnSearchChanged();
  void OnCollapseClicked();

  raw_ptr<BookmarkManager> manager_;

  // UI Components
  raw_ptr<views::Textfield> search_field_ = nullptr;
  raw_ptr<views::ImageButton> collapse_button_ = nullptr;
  raw_ptr<views::ImageButton> add_button_ = nullptr;

  // Callbacks
  base::RepeatingCallback<void(std::u16string_view)> search_callback_;
  base::RepeatingClosure collapse_callback_;
};

// ===== Quick Access Section =====

class BookmarkQuickAccessSection : public views::View {
  METADATA_HEADER(BookmarkQuickAccessSection, views::View)

 public:
  BookmarkQuickAccessSection(BookmarkManager* manager);
  ~BookmarkQuickAccessSection() override;

  void Refresh();

  // Callbacks
  void SetOnItemClickedCallback(
      base::RepeatingCallback<void(const bookmarks::BookmarkNode*)> callback);

 private:
  void CreateLayout();
  void CreateFavoritesSection();
  void CreateRecentSection();
  void CreateTagsSection();

  void OnItemClicked(const bookmarks::BookmarkNode* node);

  raw_ptr<BookmarkManager> manager_;

  raw_ptr<views::BoxLayoutView> favorites_list_ = nullptr;
  raw_ptr<views::BoxLayoutView> recent_list_ = nullptr;
  raw_ptr<views::BoxLayoutView> tags_list_ = nullptr;

  base::RepeatingCallback<void(const bookmarks::BookmarkNode*)> item_clicked_callback_;
};

// ===== Bookmark Tree Item =====

class BookmarkTreeItem : public views::View {
  METADATA_HEADER(BookmarkTreeItem, views::View)

 public:
  BookmarkTreeItem(const bookmarks::BookmarkNode* node,
                   BookmarkManager* manager);
  ~BookmarkTreeItem() override;

  const bookmarks::BookmarkNode* node() const { return node_; }

  // Expand/collapse for folders
  void SetExpanded(bool expanded);
  bool IsExpanded() const { return expanded_; }

  // Selection
  void SetSelected(bool selected);
  bool IsSelected() const { return selected_; }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

 private:
  void CreateLayout();
  void UpdateIcon();
  void ShowContextMenu(const gfx::Point& point);

  raw_ptr<const bookmarks::BookmarkNode> node_;
  raw_ptr<BookmarkManager> manager_;

  bool expanded_ = false;
  bool selected_ = false;
  bool hovered_ = false;

  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::ImageButton> expand_button_ = nullptr;
};

// ===== Bookmark Sidebar =====

class BookmarkSidebarView : public views::View {
  METADATA_HEADER(BookmarkSidebarView, views::View)

 public:
  explicit BookmarkSidebarView(BookmarkManager* manager);
  ~BookmarkSidebarView() override;

  // Sidebar state
  void Show();
  void Hide();
  void Toggle();
  bool IsVisible() const { return visible_; }

  // Width control
  void SetWidth(int width);
  int GetWidth() const { return current_width_; }

  // Navigation
  void NavigateToNode(const bookmarks::BookmarkNode* node);
  void NavigateUp();
  void NavigateToBookmarkBar();
  void NavigateToOtherBookmarks();

  // Search
  void FocusSearch();
  void Search(std::u16string_view query);
  void ClearSearch();

  // Selection
  void SelectNext();
  void SelectPrevious();
  void OpenSelected();
  void DeleteSelected();

  // Keyboard shortcuts (handled externally)
  bool HandleKeyEvent(const ui::KeyEvent& event);

 private:
  void CreateLayout();
  void CreateHeader();
  void CreateQuickAccess();
  void CreateTreeView();
  void CreateFooter();

  void OnSearchChanged(std::u16string_view query);
  void OnCollapseClicked();
  void OnItemClicked(const bookmarks::BookmarkNode* node);

  void RefreshTree();
  void AnimateShow();
  void AnimateHide();

  raw_ptr<BookmarkManager> manager_;

  // State
  bool visible_ = true;
  int current_width_ = 280;  // Default 280px
  std::vector<const bookmarks::BookmarkNode*> search_results_;
  raw_ptr<const bookmarks::BookmarkNode> selected_node_ = nullptr;

  // UI Components
  raw_ptr<BookmarkSidebarHeader> header_ = nullptr;
  raw_ptr<BookmarkQuickAccessSection> quick_access_ = nullptr;
  raw_ptr<views::ScrollView> tree_scroll_ = nullptr;
  raw_ptr<views::BoxLayoutView> tree_container_ = nullptr;
  raw_ptr<views::View> footer_ = nullptr;
};

// ===== Command Palette =====

class BookmarkCommandPalette : public views::View {
  METADATA_HEADER(BookmarkCommandPalette, views::View)

 public:
  BookmarkCommandPalette(BookmarkManager* manager);
  ~BookmarkCommandPalette() override;

  // Show/hide
  void Show();
  void Hide();
  void Toggle();

  // Search
  void FocusSearch();
  std::u16string GetQuery() const;

  // Navigation
  void SelectNext();
  void SelectPrevious();
  void OpenSelected();

  // Keyboard
  bool HandleKeyEvent(const ui::KeyEvent& event);

 private:
  void CreateLayout();
  void OnSearchChanged();
  void PerformFuzzySearch(std::u16string_view query);
  void UpdateResults();

  raw_ptr<BookmarkManager> manager_;

  // State
  std::vector<const bookmarks::BookmarkNode*> results_;
  int selected_index_ = 0;

  // UI Components
  raw_ptr<views::Textfield> search_field_ = nullptr;
  raw_ptr<views::ScrollView> results_scroll_ = nullptr;
  raw_ptr<views::BoxLayoutView> results_container_ = nullptr;

  // Fuzzy search scoring
  int CalculateFuzzyScore(std::u16string_view query,
                         std::u16string_view target) const;
};

// ===== Command Palette Result Item =====

class CommandPaletteResultItem : public views::View {
  METADATA_HEADER(CommandPaletteResultItem, views::View)

 public:
  CommandPaletteResultItem(const bookmarks::BookmarkNode* node,
                          int score,
                          std::u16string_view query);
  ~CommandPaletteResultItem() override;

  const bookmarks::BookmarkNode* node() const { return node_; }
  int score() const { return score_; }

  void SetSelected(bool selected);
  bool IsSelected() const { return selected_; }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;

 private:
  void CreateLayout();
  void HighlightMatches(std::u16string_view text, std::u16string_view query);

  raw_ptr<const bookmarks::BookmarkNode> node_;
  int score_;
  bool selected_ = false;

  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Label> path_label_ = nullptr;
};

// ===== Bookmark Context Menu =====

class BookmarkContextMenu {
 public:
  BookmarkContextMenu(const bookmarks::BookmarkNode* node,
                      BookmarkManager* manager);
  ~BookmarkContextMenu();

  enum class Command {
    kOpen,
    kOpenInNewTab,
    kOpenInNewWindow,
    kOpenInIncognito,
    kEdit,
    kDelete,
    kCut,
    kCopy,
    kPaste,
    kAddFolder,
    kAddBookmark,
    kSortByName,
    kSortByDate,
  };

  void Show(const gfx::Point& point, gfx::NativeView parent);

 private:
  void ExecuteCommand(Command command);

  raw_ptr<const bookmarks::BookmarkNode> node_;
  raw_ptr<BookmarkManager> manager_;
};

// ===== Keyboard Shortcuts Handler =====

class BookmarkKeyboardShortcuts {
 public:
  static bool IsSidebarToggle(const ui::KeyEvent& event);      // Ctrl+B
  static bool IsCommandPalette(const ui::KeyEvent& event);     // Ctrl+Shift+B
  static bool IsSearch(const ui::KeyEvent& event);             // Ctrl+F
  static bool IsNewBookmark(const ui::KeyEvent& event);        // Ctrl+D
  static bool IsNewFolder(const ui::KeyEvent& event);          // Ctrl+Shift+F
  static bool IsNavigateUp(const ui::KeyEvent& event);         // Alt+Up
  static bool IsDelete(const ui::KeyEvent& event);             // Delete
  static bool IsSelectAll(const ui::KeyEvent& event);          // Ctrl+A
  static bool IsRefresh(const ui::KeyEvent& event);            // F5
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_SIDEBAR_VIEW_H_
