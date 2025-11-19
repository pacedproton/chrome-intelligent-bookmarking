// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_sidebar_view.h"

#include <algorithm>
#include <utility>

#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"

namespace {

// Design constants
constexpr int kSidebarDefaultWidth = 280;
constexpr int kSidebarMinWidth = 200;
constexpr int kSidebarMaxWidth = 400;
constexpr int kHeaderHeight = 56;
constexpr int kSearchFieldHeight = 36;
constexpr int kQuickAccessItemHeight = 32;
constexpr int kTreeItemHeight = 28;
constexpr int kIconSize = 16;
constexpr int kSpacingSmall = 4;
constexpr int kSpacingMedium = 8;
constexpr int kSpacingLarge = 12;
constexpr int kBorderRadius = 6;
constexpr int kAnimationDurationMs = 250;

// Colors (Material Design 3)
constexpr SkColor kBackgroundColor = SkColorSetRGB(250, 250, 250);
constexpr SkColor kHoverColor = SkColorSetARGB(12, 0, 0, 0);
constexpr SkColor kSelectedColor = SkColorSetARGB(24, 66, 133, 244);
constexpr SkColor kBorderColor = SkColorSetARGB(20, 0, 0, 0);
constexpr SkColor kPrimaryColor = SkColorSetRGB(66, 133, 244);
constexpr SkColor kTextPrimary = SkColorSetRGB(32, 33, 36);
constexpr SkColor kTextSecondary = SkColorSetRGB(95, 99, 104);

// Fuzzy search scoring
constexpr int kExactMatchBonus = 100;
constexpr int kStartMatchBonus = 50;
constexpr int kCamelCaseBonus = 30;
constexpr int kConsecutiveBonus = 15;
constexpr int kGapPenalty = -5;

}  // namespace

// ===== BookmarkSidebarHeader =====

BookmarkSidebarHeader::BookmarkSidebarHeader(BookmarkManager* manager)
    : manager_(manager) {
  CreateLayout();
}

BookmarkSidebarHeader::~BookmarkSidebarHeader() = default;

void BookmarkSidebarHeader::CreateLayout() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(kSpacingMedium),
      kSpacingMedium));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  SetBackground(views::CreateSolidBackground(kBackgroundColor));
  SetBorder(views::CreateSolidSidedBorder(
      gfx::Insets::TLBR(0, 0, 1, 0), kBorderColor));

  // Top row: Title + Collapse button
  auto* top_row = AddChildView(std::make_unique<views::BoxLayoutView>());
  top_row->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  top_row->SetBetweenChildSpacing(kSpacingMedium);
  top_row->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);

  auto* title = top_row->AddChildView(std::make_unique<views::Label>(
      u"Bookmarks", views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY));
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetFontList(gfx::FontList().Derive(2, gfx::Font::NORMAL,
                                              gfx::Font::Weight::MEDIUM));

  top_row->SetFlexForView(title, 1);

  // Add button
  add_button_ = top_row->AddChildView(std::make_unique<views::ImageButton>(
      base::BindRepeating(&BookmarkSidebarHeader::OnCollapseClicked,
                         base::Unretained(this))));
  add_button_->SetTooltipText(u"Add bookmark");

  // Collapse button
  collapse_button_ = top_row->AddChildView(std::make_unique<views::ImageButton>(
      base::BindRepeating(&BookmarkSidebarHeader::OnCollapseClicked,
                         base::Unretained(this))));
  collapse_button_->SetTooltipText(u"Collapse sidebar (Ctrl+B)");

  // Search field
  search_field_ = AddChildView(std::make_unique<views::Textfield>());
  search_field_->SetPlaceholderText(u"Search bookmarks...");
  search_field_->SetController(this);
  search_field_->SetBackgroundColor(SK_ColorWHITE);
  search_field_->SetBorder(views::CreateRoundedRectBorder(
      1, kBorderRadius, kBorderColor));
  search_field_->SetPreferredSize(
      gfx::Size(0, kSearchFieldHeight));
}

void BookmarkSidebarHeader::FocusSearch() {
  if (search_field_) {
    search_field_->RequestFocus();
  }
}

void BookmarkSidebarHeader::ClearSearch() {
  if (search_field_) {
    search_field_->SetText(u"");
  }
}

std::u16string BookmarkSidebarHeader::GetSearchQuery() const {
  return search_field_ ? search_field_->GetText() : u"";
}

void BookmarkSidebarHeader::SetOnSearchCallback(
    base::RepeatingCallback<void(std::u16string_view)> callback) {
  search_callback_ = std::move(callback);
}

void BookmarkSidebarHeader::SetOnCollapseCallback(
    base::RepeatingClosure callback) {
  collapse_callback_ = std::move(callback);
}

void BookmarkSidebarHeader::OnSearchChanged() {
  if (search_callback_) {
    search_callback_.Run(GetSearchQuery());
  }
}

void BookmarkSidebarHeader::OnCollapseClicked() {
  if (collapse_callback_) {
    collapse_callback_.Run();
  }
}

void BookmarkSidebarHeader::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  OnSearchChanged();
}

BEGIN_METADATA(BookmarkSidebarHeader)
END_METADATA

// ===== BookmarkQuickAccessSection =====

BookmarkQuickAccessSection::BookmarkQuickAccessSection(
    BookmarkManager* manager)
    : manager_(manager) {
  CreateLayout();
}

BookmarkQuickAccessSection::~BookmarkQuickAccessSection() = default;

void BookmarkQuickAccessSection::CreateLayout() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(kSpacingMedium),
      kSpacingSmall));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  SetBackground(views::CreateSolidBackground(kBackgroundColor));

  CreateFavoritesSection();
  CreateRecentSection();
  CreateTagsSection();
}

void BookmarkQuickAccessSection::CreateFavoritesSection() {
  auto* section = AddChildView(std::make_unique<views::BoxLayoutView>());
  section->SetOrientation(views::BoxLayout::Orientation::kVertical);
  section->SetBetweenChildSpacing(kSpacingSmall);

  auto* header = section->AddChildView(std::make_unique<views::Label>(
      u"Favorites", views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY));
  header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  header->SetFontList(
      gfx::FontList().Derive(0, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  header->SetEnabledColor(kTextSecondary);

  favorites_list_ = section->AddChildView(std::make_unique<views::BoxLayoutView>());
  favorites_list_->SetOrientation(views::BoxLayout::Orientation::kVertical);
  favorites_list_->SetBetweenChildSpacing(2);
}

void BookmarkQuickAccessSection::CreateRecentSection() {
  auto* section = AddChildView(std::make_unique<views::BoxLayoutView>());
  section->SetOrientation(views::BoxLayout::Orientation::kVertical);
  section->SetBetweenChildSpacing(kSpacingSmall);
  section->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kSpacingLarge, 0, 0, 0)));

  auto* header = section->AddChildView(std::make_unique<views::Label>(
      u"Recent", views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY));
  header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  header->SetFontList(
      gfx::FontList().Derive(0, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  header->SetEnabledColor(kTextSecondary);

  recent_list_ = section->AddChildView(std::make_unique<views::BoxLayoutView>());
  recent_list_->SetOrientation(views::BoxLayout::Orientation::kVertical);
  recent_list_->SetBetweenChildSpacing(2);
}

void BookmarkQuickAccessSection::CreateTagsSection() {
  auto* section = AddChildView(std::make_unique<views::BoxLayoutView>());
  section->SetOrientation(views::BoxLayout::Orientation::kVertical);
  section->SetBetweenChildSpacing(kSpacingSmall);
  section->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kSpacingLarge, 0, 0, 0)));

  auto* header = section->AddChildView(std::make_unique<views::Label>(
      u"Tags", views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY));
  header->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  header->SetFontList(
      gfx::FontList().Derive(0, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  header->SetEnabledColor(kTextSecondary);

  tags_list_ = section->AddChildView(std::make_unique<views::BoxLayoutView>());
  tags_list_->SetOrientation(views::BoxLayout::Orientation::kVertical);
  tags_list_->SetBetweenChildSpacing(2);
}

void BookmarkQuickAccessSection::Refresh() {
  if (!manager_->model()) {
    return;
  }

  // Clear existing items
  favorites_list_->RemoveAllChildViews();
  recent_list_->RemoveAllChildViews();
  tags_list_->RemoveAllChildViews();

  // Populate favorites (starred bookmarks)
  // In a full implementation, this would query metadata
  auto* bookmark_bar = manager_->model()->bookmark_bar_node();
  if (bookmark_bar && bookmark_bar->children().size() > 0) {
    int count = 0;
    for (const auto& child : bookmark_bar->children()) {
      if (count >= 5) break;  // Show top 5

      auto* item = favorites_list_->AddChildView(
          std::make_unique<views::LabelButton>(
              base::BindRepeating(&BookmarkQuickAccessSection::OnItemClicked,
                                 base::Unretained(this), child.get()),
              child->GetTitle()));
      item->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      item->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(4, 8)));
      item->SetBackground(
          views::CreateThemedRoundedRectBackground(kBackgroundColor, 4));
      count++;
    }
  }

  // Populate recent (last 5 visited)
  // This would integrate with history service in full implementation
  auto* other = manager_->model()->other_node();
  if (other && other->children().size() > 0) {
    int count = 0;
    for (const auto& child : other->children()) {
      if (count >= 5) break;

      auto* item = recent_list_->AddChildView(
          std::make_unique<views::LabelButton>(
              base::BindRepeating(&BookmarkQuickAccessSection::OnItemClicked,
                                 base::Unretained(this), child.get()),
              child->GetTitle()));
      item->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      item->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(4, 8)));
      item->SetBackground(
          views::CreateThemedRoundedRectBackground(kBackgroundColor, 4));
      count++;
    }
  }
}

void BookmarkQuickAccessSection::SetOnItemClickedCallback(
    base::RepeatingCallback<void(const bookmarks::BookmarkNode*)> callback) {
  item_clicked_callback_ = std::move(callback);
}

void BookmarkQuickAccessSection::OnItemClicked(
    const bookmarks::BookmarkNode* node) {
  if (item_clicked_callback_) {
    item_clicked_callback_.Run(node);
  }
}

BEGIN_METADATA(BookmarkQuickAccessSection)
END_METADATA

// ===== BookmarkTreeItem =====

BookmarkTreeItem::BookmarkTreeItem(const bookmarks::BookmarkNode* node,
                                   BookmarkManager* manager)
    : node_(node), manager_(manager) {
  CreateLayout();
}

BookmarkTreeItem::~BookmarkTreeItem() = default;

void BookmarkTreeItem::CreateLayout() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(kSpacingSmall, kSpacingMedium), kSpacingSmall));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  SetPreferredSize(gfx::Size(0, kTreeItemHeight));

  // Expand button (for folders)
  if (node_->is_folder()) {
    expand_button_ = AddChildView(std::make_unique<views::ImageButton>(
        base::BindRepeating(&BookmarkTreeItem::SetExpanded,
                           base::Unretained(this), !expanded_)));
    expand_button_->SetPreferredSize(gfx::Size(kIconSize, kIconSize));
  } else {
    // Spacer for alignment
    auto* spacer = AddChildView(std::make_unique<views::View>());
    spacer->SetPreferredSize(gfx::Size(kIconSize, kIconSize));
  }

  // Icon
  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetPreferredSize(gfx::Size(kIconSize, kIconSize));
  UpdateIcon();

  // Title
  title_ = AddChildView(std::make_unique<views::Label>(
      node_->GetTitle(), views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetElideBehavior(gfx::ELIDE_TAIL);
  layout->SetFlexForView(title_, 1);
}

void BookmarkTreeItem::UpdateIcon() {
  // In a full implementation, this would set folder/bookmark icons
  // For now, just placeholder
}

void BookmarkTreeItem::SetExpanded(bool expanded) {
  if (expanded_ == expanded) {
    return;
  }
  expanded_ = expanded;

  // Update expand button icon
  if (expand_button_) {
    // Icon would be updated here
  }

  // Notify parent to show/hide children
  parent()->InvalidateLayout();
}

void BookmarkTreeItem::SetSelected(bool selected) {
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  SchedulePaint();
}

void BookmarkTreeItem::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  if (selected_) {
    cc::PaintFlags flags;
    flags.setColor(kSelectedColor);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawRoundRect(GetLocalBounds(), kBorderRadius, flags);
  } else if (hovered_) {
    cc::PaintFlags flags;
    flags.setColor(kHoverColor);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawRoundRect(GetLocalBounds(), kBorderRadius, flags);
  }
}

bool BookmarkTreeItem::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsRightMouseButton()) {
    ShowContextMenu(event.location());
    return true;
  }

  SetSelected(true);
  return true;
}

void BookmarkTreeItem::OnMouseEntered(const ui::MouseEvent& event) {
  hovered_ = true;
  SchedulePaint();
}

void BookmarkTreeItem::OnMouseExited(const ui::MouseEvent& event) {
  hovered_ = false;
  SchedulePaint();
}

void BookmarkTreeItem::ShowContextMenu(const gfx::Point& point) {
  // Context menu implementation would go here
}

BEGIN_METADATA(BookmarkTreeItem)
END_METADATA

// ===== BookmarkSidebarView =====

BookmarkSidebarView::BookmarkSidebarView(BookmarkManager* manager)
    : manager_(manager), current_width_(kSidebarDefaultWidth) {
  CreateLayout();
}

BookmarkSidebarView::~BookmarkSidebarView() = default;

void BookmarkSidebarView::CreateLayout() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  SetPreferredSize(gfx::Size(current_width_, 0));
  SetBackground(views::CreateSolidBackground(kBackgroundColor));
  SetBorder(views::CreateSolidSidedBorder(
      gfx::Insets::TLBR(0, 0, 0, 1), kBorderColor));

  CreateHeader();
  CreateQuickAccess();
  CreateTreeView();
  CreateFooter();
}

void BookmarkSidebarView::CreateHeader() {
  header_ = AddChildView(std::make_unique<BookmarkSidebarHeader>(manager_));
  header_->SetOnSearchCallback(base::BindRepeating(
      &BookmarkSidebarView::OnSearchChanged, base::Unretained(this)));
  header_->SetOnCollapseCallback(base::BindRepeating(
      &BookmarkSidebarView::OnCollapseClicked, base::Unretained(this)));
}

void BookmarkSidebarView::CreateQuickAccess() {
  quick_access_ = AddChildView(
      std::make_unique<BookmarkQuickAccessSection>(manager_));
  quick_access_->SetOnItemClickedCallback(base::BindRepeating(
      &BookmarkSidebarView::OnItemClicked, base::Unretained(this)));
  quick_access_->Refresh();
}

void BookmarkSidebarView::CreateTreeView() {
  tree_scroll_ = AddChildView(std::make_unique<views::ScrollView>());
  tree_scroll_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  tree_scroll_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kEnabled);

  tree_container_ = tree_scroll_->SetContents(
      std::make_unique<views::BoxLayoutView>());
  tree_container_->SetOrientation(views::BoxLayout::Orientation::kVertical);
  tree_container_->SetBetweenChildSpacing(2);
  tree_container_->SetInsideBorderInsets(gfx::Insets(kSpacingMedium));

  auto* layout = GetLayoutManager()->AsBoxLayout();
  layout->SetFlexForView(tree_scroll_, 1);

  RefreshTree();
}

void BookmarkSidebarView::CreateFooter() {
  footer_ = AddChildView(std::make_unique<views::View>());
  footer_->SetLayoutManager(std::make_unique<views::FillLayout>());
  footer_->SetPreferredSize(gfx::Size(0, 40));
  footer_->SetBackground(views::CreateSolidBackground(kBackgroundColor));
  footer_->SetBorder(views::CreateSolidSidedBorder(
      gfx::Insets::TLBR(1, 0, 0, 0), kBorderColor));

  // Storage info or quick stats could go here
  auto* label = footer_->AddChildView(std::make_unique<views::Label>(
      u"", views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  label->SetEnabledColor(kTextSecondary);
}

void BookmarkSidebarView::RefreshTree() {
  if (!manager_->model()) {
    return;
  }

  tree_container_->RemoveAllChildViews();

  // Add bookmark bar folder
  auto* bookmark_bar = manager_->model()->bookmark_bar_node();
  if (bookmark_bar) {
    auto* item = tree_container_->AddChildView(
        std::make_unique<BookmarkTreeItem>(bookmark_bar, manager_));

    // Add children
    for (const auto& child : bookmark_bar->children()) {
      tree_container_->AddChildView(
          std::make_unique<BookmarkTreeItem>(child.get(), manager_));
    }
  }

  // Add other bookmarks folder
  auto* other = manager_->model()->other_node();
  if (other) {
    auto* item = tree_container_->AddChildView(
        std::make_unique<BookmarkTreeItem>(other, manager_));

    for (const auto& child : other->children()) {
      tree_container_->AddChildView(
          std::make_unique<BookmarkTreeItem>(child.get(), manager_));
    }
  }
}

void BookmarkSidebarView::Show() {
  if (visible_) {
    return;
  }
  visible_ = true;
  AnimateShow();
}

void BookmarkSidebarView::Hide() {
  if (!visible_) {
    return;
  }
  visible_ = false;
  AnimateHide();
}

void BookmarkSidebarView::Toggle() {
  if (visible_) {
    Hide();
  } else {
    Show();
  }
}

void BookmarkSidebarView::SetWidth(int width) {
  width = std::clamp(width, kSidebarMinWidth, kSidebarMaxWidth);
  if (current_width_ == width) {
    return;
  }
  current_width_ = width;
  SetPreferredSize(gfx::Size(current_width_, 0));
  InvalidateLayout();
}

void BookmarkSidebarView::NavigateToNode(const bookmarks::BookmarkNode* node) {
  if (!node) {
    return;
  }

  selected_node_ = node;

  // Open the URL if it's a bookmark
  if (!node->is_folder()) {
    manager_->OpenBookmark(node);
  }
}

void BookmarkSidebarView::NavigateUp() {
  if (selected_node_ && selected_node_->parent()) {
    NavigateToNode(selected_node_->parent());
  }
}

void BookmarkSidebarView::NavigateToBookmarkBar() {
  if (manager_->model()) {
    NavigateToNode(manager_->model()->bookmark_bar_node());
  }
}

void BookmarkSidebarView::NavigateToOtherBookmarks() {
  if (manager_->model()) {
    NavigateToNode(manager_->model()->other_node());
  }
}

void BookmarkSidebarView::FocusSearch() {
  if (header_) {
    header_->FocusSearch();
  }
}

void BookmarkSidebarView::Search(std::u16string_view query) {
  if (query.empty()) {
    search_results_.clear();
    RefreshTree();
    return;
  }

  search_results_.clear();

  // Simple search implementation
  // In full version, this would use fuzzy matching
  if (!manager_->model()) {
    return;
  }

  std::u16string lower_query = base::i18n::ToLower(query);

  auto search_node = [&](const bookmarks::BookmarkNode* node, auto& self) -> void {
    std::u16string lower_title = base::i18n::ToLower(node->GetTitle());
    if (lower_title.find(lower_query) != std::u16string::npos) {
      search_results_.push_back(node);
    }

    if (node->is_folder()) {
      for (const auto& child : node->children()) {
        self(child.get(), self);
      }
    }
  };

  search_node(manager_->model()->bookmark_bar_node(), search_node);
  search_node(manager_->model()->other_node(), search_node);

  // Update tree to show only results
  tree_container_->RemoveAllChildViews();
  for (const auto* result : search_results_) {
    tree_container_->AddChildView(
        std::make_unique<BookmarkTreeItem>(result, manager_));
  }
}

void BookmarkSidebarView::ClearSearch() {
  if (header_) {
    header_->ClearSearch();
  }
  Search(u"");
}

void BookmarkSidebarView::SelectNext() {
  // Navigate to next item in tree
  // Full implementation would track current selection
}

void BookmarkSidebarView::SelectPrevious() {
  // Navigate to previous item in tree
}

void BookmarkSidebarView::OpenSelected() {
  if (selected_node_) {
    NavigateToNode(selected_node_);
  }
}

void BookmarkSidebarView::DeleteSelected() {
  if (selected_node_ && manager_->model()) {
    manager_->model()->Remove(selected_node_,
                             bookmarks::metrics::BookmarkEditSource::kUser);
    selected_node_ = nullptr;
    RefreshTree();
  }
}

bool BookmarkSidebarView::HandleKeyEvent(const ui::KeyEvent& event) {
  // Handle keyboard shortcuts
  if (BookmarkKeyboardShortcuts::IsSidebarToggle(event)) {
    Toggle();
    return true;
  }

  if (BookmarkKeyboardShortcuts::IsSearch(event)) {
    FocusSearch();
    return true;
  }

  if (BookmarkKeyboardShortcuts::IsNavigateUp(event)) {
    NavigateUp();
    return true;
  }

  if (BookmarkKeyboardShortcuts::IsDelete(event)) {
    DeleteSelected();
    return true;
  }

  // Arrow key navigation
  if (event.key_code() == ui::VKEY_DOWN) {
    SelectNext();
    return true;
  }

  if (event.key_code() == ui::VKEY_UP) {
    SelectPrevious();
    return true;
  }

  if (event.key_code() == ui::VKEY_RETURN) {
    OpenSelected();
    return true;
  }

  return false;
}

void BookmarkSidebarView::OnSearchChanged(std::u16string_view query) {
  Search(query);
}

void BookmarkSidebarView::OnCollapseClicked() {
  Toggle();
}

void BookmarkSidebarView::OnItemClicked(const bookmarks::BookmarkNode* node) {
  NavigateToNode(node);
}

void BookmarkSidebarView::AnimateShow() {
  SetVisible(true);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::Milliseconds(kAnimationDurationMs))
      .SetOpacity(this, 1.0f);
}

void BookmarkSidebarView::AnimateHide() {
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::Milliseconds(kAnimationDurationMs))
      .SetOpacity(this, 0.0f)
      .Then()
      .SetDuration(base::Milliseconds(0))
      .SetVisibility(this, false);
}

BEGIN_METADATA(BookmarkSidebarView)
END_METADATA

// ===== BookmarkCommandPalette =====

BookmarkCommandPalette::BookmarkCommandPalette(BookmarkManager* manager)
    : manager_(manager) {
  CreateLayout();
}

BookmarkCommandPalette::~BookmarkCommandPalette() = default;

void BookmarkCommandPalette::CreateLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Semi-transparent backdrop
  SetBackground(views::CreateSolidBackground(
      SkColorSetARGB(180, 0, 0, 0)));

  // Center container
  auto* container = AddChildView(std::make_unique<views::BoxLayoutView>());
  container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  container->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
  container->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(80, 0, 0, 0)));

  // Search box container
  auto* search_container = container->AddChildView(
      std::make_unique<views::BoxLayoutView>());
  search_container->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  search_container->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(0, 100)));

  // Search field
  search_field_ = search_container->AddChildView(
      std::make_unique<views::Textfield>());
  search_field_->SetPlaceholderText(
      u"Search bookmarks... (fuzzy search enabled)");
  search_field_->SetController(this);
  search_field_->SetBackgroundColor(SK_ColorWHITE);
  search_field_->SetBorder(views::CreateRoundedRectBorder(
      1, kBorderRadius, kBorderColor));
  search_field_->SetPreferredSize(gfx::Size(600, 48));
  search_field_->SetFontList(
      gfx::FontList().Derive(4, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL));

  // Results scroll view
  results_scroll_ = container->AddChildView(
      std::make_unique<views::ScrollView>());
  results_scroll_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  results_scroll_->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  results_scroll_->SetBorder(views::CreateRoundedRectBorder(
      1, kBorderRadius, kBorderColor));
  results_scroll_->SetPreferredSize(gfx::Size(600, 400));

  results_container_ = results_scroll_->SetContents(
      std::make_unique<views::BoxLayoutView>());
  results_container_->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  results_container_->SetBetweenChildSpacing(1);

  SetVisible(false);
}

void BookmarkCommandPalette::Show() {
  SetVisible(true);
  FocusSearch();
}

void BookmarkCommandPalette::Hide() {
  SetVisible(false);
  search_field_->SetText(u"");
  results_.clear();
  results_container_->RemoveAllChildViews();
}

void BookmarkCommandPalette::Toggle() {
  if (GetVisible()) {
    Hide();
  } else {
    Show();
  }
}

void BookmarkCommandPalette::FocusSearch() {
  if (search_field_) {
    search_field_->RequestFocus();
  }
}

std::u16string BookmarkCommandPalette::GetQuery() const {
  return search_field_ ? search_field_->GetText() : u"";
}

void BookmarkCommandPalette::SelectNext() {
  if (results_.empty()) {
    return;
  }
  selected_index_ = (selected_index_ + 1) % results_.size();
  UpdateResults();
}

void BookmarkCommandPalette::SelectPrevious() {
  if (results_.empty()) {
    return;
  }
  selected_index_ = (selected_index_ - 1 + results_.size()) % results_.size();
  UpdateResults();
}

void BookmarkCommandPalette::OpenSelected() {
  if (selected_index_ >= 0 &&
      selected_index_ < static_cast<int>(results_.size())) {
    manager_->OpenBookmark(results_[selected_index_]);
    Hide();
  }
}

bool BookmarkCommandPalette::HandleKeyEvent(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_ESCAPE) {
    Hide();
    return true;
  }

  if (event.key_code() == ui::VKEY_DOWN) {
    SelectNext();
    return true;
  }

  if (event.key_code() == ui::VKEY_UP) {
    SelectPrevious();
    return true;
  }

  if (event.key_code() == ui::VKEY_RETURN) {
    OpenSelected();
    return true;
  }

  return false;
}

void BookmarkCommandPalette::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  OnSearchChanged();
}

void BookmarkCommandPalette::OnSearchChanged() {
  PerformFuzzySearch(GetQuery());
}

void BookmarkCommandPalette::PerformFuzzySearch(std::u16string_view query) {
  results_.clear();
  selected_index_ = 0;

  if (query.empty() || !manager_->model()) {
    UpdateResults();
    return;
  }

  // Collect all bookmarks with scores
  struct ScoredBookmark {
    const bookmarks::BookmarkNode* node;
    int score;
  };
  std::vector<ScoredBookmark> scored_results;

  std::u16string lower_query = base::i18n::ToLower(query);

  auto score_node = [&](const bookmarks::BookmarkNode* node, auto& self) -> void {
    if (!node->is_folder()) {
      int score = CalculateFuzzyScore(lower_query,
                                     base::i18n::ToLower(node->GetTitle()));
      if (score > 0) {
        scored_results.push_back({node, score});
      }
    }

    if (node->is_folder()) {
      for (const auto& child : node->children()) {
        self(child.get(), self);
      }
    }
  };

  score_node(manager_->model()->bookmark_bar_node(), score_node);
  score_node(manager_->model()->other_node(), score_node);

  // Sort by score (descending)
  std::sort(scored_results.begin(), scored_results.end(),
           [](const ScoredBookmark& a, const ScoredBookmark& b) {
             return a.score > b.score;
           });

  // Take top 50 results
  int count = std::min(50, static_cast<int>(scored_results.size()));
  for (int i = 0; i < count; ++i) {
    results_.push_back(scored_results[i].node);
  }

  UpdateResults();
}

void BookmarkCommandPalette::UpdateResults() {
  results_container_->RemoveAllChildViews();

  if (results_.empty()) {
    auto* empty = results_container_->AddChildView(
        std::make_unique<views::Label>(
            u"No bookmarks found", views::style::CONTEXT_LABEL,
            views::style::STYLE_SECONDARY));
    empty->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    empty->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(40, 0)));
    return;
  }

  for (size_t i = 0; i < results_.size(); ++i) {
    auto* item = results_container_->AddChildView(
        std::make_unique<CommandPaletteResultItem>(
            results_[i], 100 - i, GetQuery()));
    item->SetSelected(i == static_cast<size_t>(selected_index_));
  }
}

int BookmarkCommandPalette::CalculateFuzzyScore(
    std::u16string_view query,
    std::u16string_view target) const {
  if (query.empty()) {
    return 0;
  }

  // Exact match
  if (target.find(query) != std::u16string::npos) {
    return kExactMatchBonus + static_cast<int>(target.length() - query.length());
  }

  // Start match
  if (target.starts_with(query)) {
    return kStartMatchBonus + static_cast<int>(target.length() - query.length());
  }

  // Fuzzy match
  int score = 0;
  size_t target_idx = 0;
  size_t last_match = std::u16string::npos;

  for (size_t query_idx = 0; query_idx < query.length(); ++query_idx) {
    char16_t qc = query[query_idx];
    bool found = false;

    for (; target_idx < target.length(); ++target_idx) {
      if (target[target_idx] == qc) {
        found = true;

        // Consecutive match bonus
        if (last_match != std::u16string::npos &&
            target_idx == last_match + 1) {
          score += kConsecutiveBonus;
        }

        // CamelCase bonus
        if (target_idx > 0 &&
            std::isupper(target[target_idx]) &&
            std::islower(target[target_idx - 1])) {
          score += kCamelCaseBonus;
        }

        last_match = target_idx;
        target_idx++;
        break;
      }
      score += kGapPenalty;
    }

    if (!found) {
      return 0;  // Character not found
    }
  }

  return std::max(1, score);
}

BEGIN_METADATA(BookmarkCommandPalette)
END_METADATA

// ===== CommandPaletteResultItem =====

CommandPaletteResultItem::CommandPaletteResultItem(
    const bookmarks::BookmarkNode* node,
    int score,
    std::u16string_view query)
    : node_(node), score_(score) {
  CreateLayout();
  HighlightMatches(node->GetTitle(), query);
}

CommandPaletteResultItem::~CommandPaletteResultItem() = default;

void CommandPaletteResultItem::CreateLayout() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kSpacingMedium, kSpacingLarge), kSpacingSmall));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  SetPreferredSize(gfx::Size(0, 60));

  // Title
  title_label_ = AddChildView(std::make_unique<views::Label>(
      node_->GetTitle(), views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY));
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetFontList(
      gfx::FontList().Derive(2, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));

  // Path/URL
  std::u16string path_text = base::UTF8ToUTF16(node_->url().spec());
  path_label_ = AddChildView(std::make_unique<views::Label>(
      path_text, views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY));
  path_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  path_label_->SetEnabledColor(kTextSecondary);
  path_label_->SetElideBehavior(gfx::ELIDE_MIDDLE);
}

void CommandPaletteResultItem::HighlightMatches(
    std::u16string_view text,
    std::u16string_view query) {
  // In a full implementation, this would highlight matching characters
  // For now, just display the text as-is
}

void CommandPaletteResultItem::SetSelected(bool selected) {
  if (selected_ == selected) {
    return;
  }
  selected_ = selected;
  SchedulePaint();
}

void CommandPaletteResultItem::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  if (selected_) {
    cc::PaintFlags flags;
    flags.setColor(kSelectedColor);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawRect(GetLocalBounds(), flags);
  }
}

bool CommandPaletteResultItem::OnMousePressed(const ui::MouseEvent& event) {
  SetSelected(true);
  return true;
}

BEGIN_METADATA(CommandPaletteResultItem)
END_METADATA

// ===== BookmarkContextMenu =====

BookmarkContextMenu::BookmarkContextMenu(
    const bookmarks::BookmarkNode* node,
    BookmarkManager* manager)
    : node_(node), manager_(manager) {}

BookmarkContextMenu::~BookmarkContextMenu() = default;

void BookmarkContextMenu::Show(const gfx::Point& point,
                               gfx::NativeView parent) {
  // Full context menu implementation would go here
  // This would use ui::SimpleMenuModel and views::MenuRunner
}

void BookmarkContextMenu::ExecuteCommand(Command command) {
  switch (command) {
    case Command::kOpen:
      manager_->OpenBookmark(node_);
      break;
    case Command::kOpenInNewTab:
      // Implementation
      break;
    case Command::kEdit:
      // Show edit dialog
      break;
    case Command::kDelete:
      if (manager_->model()) {
        manager_->model()->Remove(node_,
                                 bookmarks::metrics::BookmarkEditSource::kUser);
      }
      break;
    // ... other commands
  }
}

// ===== BookmarkKeyboardShortcuts =====

bool BookmarkKeyboardShortcuts::IsSidebarToggle(const ui::KeyEvent& event) {
  return event.IsControlDown() && !event.IsShiftDown() &&
         event.key_code() == ui::VKEY_B;
}

bool BookmarkKeyboardShortcuts::IsCommandPalette(const ui::KeyEvent& event) {
  return event.IsControlDown() && event.IsShiftDown() &&
         event.key_code() == ui::VKEY_B;
}

bool BookmarkKeyboardShortcuts::IsSearch(const ui::KeyEvent& event) {
  return event.IsControlDown() && !event.IsShiftDown() &&
         event.key_code() == ui::VKEY_F;
}

bool BookmarkKeyboardShortcuts::IsNewBookmark(const ui::KeyEvent& event) {
  return event.IsControlDown() && !event.IsShiftDown() &&
         event.key_code() == ui::VKEY_D;
}

bool BookmarkKeyboardShortcuts::IsNewFolder(const ui::KeyEvent& event) {
  return event.IsControlDown() && event.IsShiftDown() &&
         event.key_code() == ui::VKEY_F;
}

bool BookmarkKeyboardShortcuts::IsNavigateUp(const ui::KeyEvent& event) {
  return event.IsAltDown() && event.key_code() == ui::VKEY_UP;
}

bool BookmarkKeyboardShortcuts::IsDelete(const ui::KeyEvent& event) {
  return event.key_code() == ui::VKEY_DELETE;
}

bool BookmarkKeyboardShortcuts::IsSelectAll(const ui::KeyEvent& event) {
  return event.IsControlDown() && event.key_code() == ui::VKEY_A;
}

bool BookmarkKeyboardShortcuts::IsRefresh(const ui::KeyEvent& event) {
  return event.key_code() == ui::VKEY_F5;
}
