// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_cleanup_wizard.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace {

// Helper to recursively collect all folders
void CollectAllFolders(const bookmarks::BookmarkNode* node,
                      std::vector<const bookmarks::BookmarkNode*>& out) {
  if (!node) {
    return;
  }

  if (node->is_folder() && !node->is_permanent_node()) {
    out.push_back(node);
  }

  for (const auto& child : node->children()) {
    CollectAllFolders(child.get(), out);
  }
}

// Calculate fuzzy match score
int CalculateFuzzyMatchScore(std::u16string_view query,
                             std::u16string_view target) {
  if (query.empty() || target.empty()) {
    return 0;
  }

  std::u16string lower_query = base::ToLowerASCII(query);
  std::u16string lower_target = base::ToLowerASCII(target);

  // Exact match
  if (lower_query == lower_target) {
    return 100;
  }

  // Starts with
  if (lower_target.find(lower_query) == 0) {
    return 90;
  }

  // Contains
  if (lower_target.find(lower_query) != std::u16string::npos) {
    return 70;
  }

  // Fuzzy match (all characters present in order)
  size_t target_pos = 0;
  for (char16_t c : lower_query) {
    size_t found = lower_target.find(c, target_pos);
    if (found == std::u16string::npos) {
      return 0;  // Character not found
    }
    target_pos = found + 1;
  }

  return 50;  // Fuzzy match
}

}  // namespace

// ===== QuickOrganizer Implementation =====

QuickOrganizer::QuickOrganizer(bookmarks::BookmarkModel* model,
                               BookmarkManager* manager)
    : bookmark_model_(model), manager_(manager) {
  DCHECK(bookmark_model_);
  DCHECK(manager_);

  // Initialize quick folders to nullptr
  quick_folders_.fill(nullptr);
}

QuickOrganizer::~QuickOrganizer() = default;

std::vector<const bookmarks::BookmarkNode*> QuickOrganizer::SearchFolders(
    std::u16string_view query) {
  DCHECK(bookmark_model_);

  std::vector<const bookmarks::BookmarkNode*> results;

  if (query.empty()) {
    return results;
  }

  // Collect all folders
  std::vector<const bookmarks::BookmarkNode*> all_folders;
  CollectAllFolders(bookmark_model_->root_node(), all_folders);

  // Score and filter folders
  std::vector<std::pair<const bookmarks::BookmarkNode*, int>> scored_folders;

  for (const auto* folder : all_folders) {
    int score = CalculateFuzzyMatchScore(query, folder->GetTitle());
    if (score > 0) {
      scored_folders.push_back(std::make_pair(folder, score));
    }
  }

  // Sort by score (descending)
  std::sort(scored_folders.begin(), scored_folders.end(),
           [](const auto& a, const auto& b) {
             return a.second > b.second;
           });

  // Return top 10 results
  const size_t kMaxResults = 10;
  for (size_t i = 0; i < std::min(kMaxResults, scored_folders.size()); ++i) {
    results.push_back(scored_folders[i].first);
  }

  DLOG(INFO) << "Folder search for '" << query << "' returned "
             << results.size() << " results";

  return results;
}

void QuickOrganizer::MoveToQuickFolder(
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks,
    int slot) {
  DCHECK(bookmark_model_);
  DCHECK(slot >= 0 && slot < static_cast<int>(kQuickFolderSlots));

  const bookmarks::BookmarkNode* folder = GetQuickFolder(slot);
  if (!folder) {
    DLOG(WARNING) << "Quick folder slot " << slot << " is not set";
    return;
  }

  // Move all bookmarks to the folder
  for (const auto* bookmark : bookmarks) {
    if (bookmark && bookmark->is_url()) {
      bookmark_model_->Move(bookmark, folder, folder->children().size());
    }
  }

  DLOG(INFO) << "Moved " << bookmarks.size() << " bookmarks to quick folder "
             << slot;
}

void QuickOrganizer::SetQuickFolder(const bookmarks::BookmarkNode* folder,
                                    int slot) {
  DCHECK(folder);
  DCHECK(folder->is_folder());
  DCHECK(slot >= 0 && slot < static_cast<int>(kQuickFolderSlots));

  quick_folders_[slot] = folder;

  DLOG(INFO) << "Set quick folder slot " << slot << " to '"
             << folder->GetTitle() << "'";
}

const bookmarks::BookmarkNode* QuickOrganizer::GetQuickFolder(int slot) const {
  DCHECK(slot >= 0 && slot < static_cast<int>(kQuickFolderSlots));
  return quick_folders_[slot];
}
