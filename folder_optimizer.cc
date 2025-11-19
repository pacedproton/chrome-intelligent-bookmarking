// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_cleanup_wizard.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace {

// Helper to recursively collect all bookmark nodes
void CollectAllBookmarks(const bookmarks::BookmarkNode* node,
                        std::vector<const bookmarks::BookmarkNode*>& out) {
  if (!node) {
    return;
  }

  if (node->is_url()) {
    out.push_back(node);
  }

  for (const auto& child : node->children()) {
    CollectAllBookmarks(child.get(), out);
  }
}

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

// Calculate similarity between folder names
float CalculateFolderSimilarity(std::u16string_view a, std::u16string_view b) {
  if (a.empty() || b.empty()) {
    return 0.0f;
  }

  std::u16string lower_a = base::ToLowerASCII(a);
  std::u16string lower_b = base::ToLowerASCII(b);

  // Simple character overlap similarity
  size_t common_chars = 0;
  for (size_t i = 0; i < std::min(lower_a.length(), lower_b.length()); ++i) {
    if (lower_a[i] == lower_b[i]) {
      ++common_chars;
    }
  }

  size_t max_len = std::max(lower_a.length(), lower_b.length());
  return static_cast<float>(common_chars) / static_cast<float>(max_len);
}

}  // namespace

// ===== FolderOptimizer Implementation =====

FolderOptimizer::FolderOptimizer(bookmarks::BookmarkModel* model,
                                 BookmarkManager* manager)
    : bookmark_model_(model), manager_(manager) {
  DCHECK(bookmark_model_);
  DCHECK(manager_);
}

FolderOptimizer::~FolderOptimizer() = default;

FolderOptimizer::FolderAnalysis FolderOptimizer::AnalyzeStructure() {
  DCHECK(bookmark_model_);

  FolderAnalysis analysis;
  analysis.max_depth = 0;
  analysis.avg_bookmarks_per_folder = 0;

  // Collect all folders
  std::vector<const bookmarks::BookmarkNode*> all_folders;
  CollectAllFolders(bookmark_model_->root_node(), all_folders);

  if (all_folders.empty()) {
    return analysis;
  }

  // Calculate max depth
  for (const auto* folder : all_folders) {
    int depth = CalculateFolderDepth(folder);
    analysis.max_depth = std::max(analysis.max_depth, depth);
  }

  // Analyze each folder
  size_t total_bookmarks = 0;
  const size_t kOversizedThreshold = 50;  // More than 50 bookmarks = oversized

  for (const auto* folder : all_folders) {
    std::vector<const bookmarks::BookmarkNode*> folder_bookmarks;
    CollectAllBookmarks(folder, folder_bookmarks);
    size_t bookmark_count = folder_bookmarks.size();

    total_bookmarks += bookmark_count;

    // Check for oversized folders
    if (bookmark_count > kOversizedThreshold) {
      analysis.oversized_folders.push_back(folder);
    }

    // Check for empty folders
    if (folder->children().empty()) {
      analysis.empty_folders.push_back(folder);
    }
  }

  analysis.avg_bookmarks_per_folder = total_bookmarks / all_folders.size();

  // Find similar folders
  const float kSimilarityThreshold = 0.7f;
  for (size_t i = 0; i < all_folders.size(); ++i) {
    for (size_t j = i + 1; j < all_folders.size(); ++j) {
      float similarity = CalculateFolderSimilarity(
          all_folders[i]->GetTitle(),
          all_folders[j]->GetTitle());

      if (similarity >= kSimilarityThreshold) {
        analysis.similar_folders.push_back(
            std::make_pair(all_folders[i], all_folders[j]));
      }
    }
  }

  DLOG(INFO) << "Folder analysis: max_depth=" << analysis.max_depth
             << ", avg_bookmarks=" << analysis.avg_bookmarks_per_folder
             << ", oversized=" << analysis.oversized_folders.size()
             << ", empty=" << analysis.empty_folders.size()
             << ", similar=" << analysis.similar_folders.size();

  return analysis;
}

std::vector<FolderOptimizer::Optimization>
FolderOptimizer::SuggestOptimizations() {
  DCHECK(bookmark_model_);

  std::vector<Optimization> optimizations;
  FolderAnalysis analysis = AnalyzeStructure();

  // Suggest removing empty folders
  if (!analysis.empty_folders.empty()) {
    Optimization opt;
    opt.type = Optimization::Type::kRemoveEmptyFolders;
    opt.affected_folders = analysis.empty_folders;
    opt.description = u"Remove " +
        base::NumberToString16(analysis.empty_folders.size()) +
        u" empty folders";
    optimizations.push_back(opt);
  }

  // Suggest splitting oversized folders
  for (const auto* folder : analysis.oversized_folders) {
    Optimization opt;
    opt.type = Optimization::Type::kSplitLargeFolder;
    opt.affected_folders.push_back(folder);
    opt.description = u"Split large folder \"" + folder->GetTitle() +
        u"\" into smaller folders";
    optimizations.push_back(opt);
  }

  // Suggest merging similar folders
  for (const auto& [folder1, folder2] : analysis.similar_folders) {
    Optimization opt;
    opt.type = Optimization::Type::kMergeSimilarFolders;
    opt.affected_folders.push_back(folder1);
    opt.affected_folders.push_back(folder2);
    opt.description = u"Merge similar folders \"" + folder1->GetTitle() +
        u"\" and \"" + folder2->GetTitle() + u"\"";
    optimizations.push_back(opt);
  }

  // Suggest flattening deep hierarchies
  if (analysis.max_depth > 5) {
    Optimization opt;
    opt.type = Optimization::Type::kFlattenHierarchy;
    opt.description = u"Flatten folder hierarchy (current depth: " +
        base::NumberToString16(analysis.max_depth) + u")";
    optimizations.push_back(opt);
  }

  DLOG(INFO) << "Suggested " << optimizations.size() << " optimizations";

  return optimizations;
}

void FolderOptimizer::ApplyOptimization(const Optimization& opt) {
  DCHECK(bookmark_model_);

  switch (opt.type) {
    case Optimization::Type::kRemoveEmptyFolders:
      for (const auto* folder : opt.affected_folders) {
        if (folder && folder->children().empty()) {
          bookmark_model_->Remove(folder,
              bookmarks::metrics::BookmarkEditSource::kUser);
        }
      }
      DLOG(INFO) << "Removed " << opt.affected_folders.size()
                 << " empty folders";
      break;

    case Optimization::Type::kSplitLargeFolder:
      if (!opt.affected_folders.empty()) {
        const bookmarks::BookmarkNode* folder = opt.affected_folders[0];
        if (folder) {
          // Collect bookmarks
          std::vector<const bookmarks::BookmarkNode*> bookmarks;
          CollectAllBookmarks(folder, bookmarks);

          // Create two new folders
          const bookmarks::BookmarkNode* parent = folder->parent();
          if (parent) {
            const bookmarks::BookmarkNode* folder1 =
                bookmark_model_->AddFolder(parent, parent->children().size(),
                    folder->GetTitle() + u" (1)");
            const bookmarks::BookmarkNode* folder2 =
                bookmark_model_->AddFolder(parent, parent->children().size(),
                    folder->GetTitle() + u" (2)");

            // Move half to each folder
            size_t half = bookmarks.size() / 2;
            for (size_t i = 0; i < bookmarks.size(); ++i) {
              const bookmarks::BookmarkNode* target =
                  i < half ? folder1 : folder2;
              bookmark_model_->Move(bookmarks[i], target,
                                  target->children().size());
            }

            // Remove original folder
            bookmark_model_->Remove(folder,
                bookmarks::metrics::BookmarkEditSource::kUser);
          }
        }
      }
      DLOG(INFO) << "Split large folder";
      break;

    case Optimization::Type::kMergeSimilarFolders:
      if (opt.affected_folders.size() >= 2) {
        const bookmarks::BookmarkNode* folder1 = opt.affected_folders[0];
        const bookmarks::BookmarkNode* folder2 = opt.affected_folders[1];

        if (folder1 && folder2) {
          // Move all bookmarks from folder2 to folder1
          while (!folder2->children().empty()) {
            const auto& child = folder2->children()[0];
            bookmark_model_->Move(child.get(), folder1,
                                folder1->children().size());
          }

          // Remove folder2
          bookmark_model_->Remove(folder2,
              bookmarks::metrics::BookmarkEditSource::kUser);
        }
      }
      DLOG(INFO) << "Merged similar folders";
      break;

    case Optimization::Type::kFlattenHierarchy:
      // This would require more complex logic to flatten deep hierarchies
      // For now, just log
      DLOG(INFO) << "Flatten hierarchy optimization not yet implemented";
      break;
  }
}

int FolderOptimizer::CalculateFolderDepth(
    const bookmarks::BookmarkNode* folder) {
  DCHECK(folder);

  int depth = 0;
  const bookmarks::BookmarkNode* node = folder;

  while (node && !node->is_permanent_node()) {
    ++depth;
    node = node->parent();
  }

  return depth;
}
