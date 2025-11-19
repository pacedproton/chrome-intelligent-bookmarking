// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_cleanup_wizard.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "url/gurl.h"

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

}  // namespace

// ===== DuplicateMerger Implementation =====

DuplicateMerger::DuplicateMerger(bookmarks::BookmarkModel* model,
                                 BookmarkManager* manager)
    : bookmark_model_(model), manager_(manager) {
  DCHECK(bookmark_model_);
  DCHECK(manager_);
}

DuplicateMerger::~DuplicateMerger() = default;

std::vector<DuplicateMerger::DuplicateSet>
DuplicateMerger::FindDuplicateSets() {
  DCHECK(bookmark_model_);

  std::vector<DuplicateSet> duplicate_sets;

  // Collect all bookmarks
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;
  CollectAllBookmarks(bookmark_model_->root_node(), all_bookmarks);

  // Group by URL
  std::map<GURL, std::vector<const bookmarks::BookmarkNode*>> url_map;
  for (const auto* bookmark : all_bookmarks) {
    url_map[bookmark->url()].push_back(bookmark);
  }

  // Find duplicates
  for (const auto& [url, bookmarks] : url_map) {
    if (bookmarks.size() > 1) {
      DuplicateSet set;
      set.primary = SelectPrimary(bookmarks);

      for (const auto* bookmark : bookmarks) {
        if (bookmark != set.primary) {
          set.duplicates.push_back(bookmark);
        }
      }

      set.combined_metadata = PreviewMerge(set);
      duplicate_sets.push_back(set);
    }
  }

  DLOG(INFO) << "Found " << duplicate_sets.size() << " duplicate sets";

  return duplicate_sets;
}

const bookmarks::BookmarkNode* DuplicateMerger::SelectPrimary(
    const std::vector<const bookmarks::BookmarkNode*>& duplicates) {
  DCHECK(!duplicates.empty());
  DCHECK(manager_);

  const bookmarks::BookmarkNode* best = duplicates[0];
  int best_score = 0;

  for (const auto* bookmark : duplicates) {
    int score = 0;

    // Prefer bookmarks with metadata
    const BookmarkMetadata* metadata = manager_->GetMetadata(bookmark);
    if (metadata) {
      score += 10;
      score += metadata->access_count;  // More visits = better
      score += metadata->rating * 2;    // Higher rating = better
      score += metadata->tags.size() * 3;  // More tags = better
    }

    // Prefer bookmarks not in archive
    if (metadata && !metadata->archived) {
      score += 5;
    }

    // Prefer bookmarks in folders (not in root)
    const bookmarks::BookmarkNode* parent = bookmark->parent();
    if (parent && !parent->is_permanent_node()) {
      score += 5;
    }

    if (score > best_score) {
      best_score = score;
      best = bookmark;
    }
  }

  return best;
}

void DuplicateMerger::MergeDuplicates(const DuplicateSet& set) {
  DCHECK(bookmark_model_);
  DCHECK(manager_);
  DCHECK(set.primary);

  // Merge metadata into primary
  BookmarkMetadata merged_metadata;
  const BookmarkMetadata* primary_metadata =
      manager_->GetMetadata(set.primary);

  if (primary_metadata) {
    merged_metadata = *primary_metadata;
  }

  // Collect all tags
  std::set<std::u16string> all_tags;
  if (primary_metadata) {
    all_tags.insert(primary_metadata->tags.begin(),
                   primary_metadata->tags.end());
  }

  int total_access_count = primary_metadata ? primary_metadata->access_count : 0;
  int best_rating = primary_metadata ? primary_metadata->rating : 0;
  base::Time earliest_date = set.primary->date_added();
  base::Time latest_date = primary_metadata ? primary_metadata->last_modified :
                                             base::Time::Now();

  // Merge from duplicates
  for (const auto* duplicate : set.duplicates) {
    const BookmarkMetadata* dup_metadata = manager_->GetMetadata(duplicate);
    if (dup_metadata) {
      all_tags.insert(dup_metadata->tags.begin(), dup_metadata->tags.end());
      total_access_count += dup_metadata->access_count;
      best_rating = std::max(best_rating, dup_metadata->rating);
      latest_date = std::max(latest_date, dup_metadata->last_modified);
    }

    earliest_date = std::min(earliest_date, duplicate->date_added());

    // Remove duplicate
    bookmark_model_->Remove(duplicate,
        bookmarks::metrics::BookmarkEditSource::kUser);
  }

  // Update primary with merged metadata
  merged_metadata.tags.clear();
  merged_metadata.tags.insert(merged_metadata.tags.end(),
                              all_tags.begin(), all_tags.end());
  merged_metadata.access_count = total_access_count;
  merged_metadata.rating = best_rating;
  merged_metadata.last_modified = latest_date;

  // Set the merged metadata (this would need a SetMetadata method)
  // For now, just update individual fields
  for (const auto& tag : all_tags) {
    manager_->AddTag(set.primary, tag);
  }
  manager_->SetRating(set.primary, best_rating);

  DLOG(INFO) << "Merged " << set.duplicates.size() + 1 << " duplicates";
}

DuplicateMerger::MergedMetadata DuplicateMerger::PreviewMerge(
    const DuplicateSet& set) {
  DCHECK(manager_);
  DCHECK(set.primary);

  MergedMetadata merged;

  // Collect all tags
  std::set<std::u16string> all_tags;
  const BookmarkMetadata* primary_metadata =
      manager_->GetMetadata(set.primary);

  if (primary_metadata) {
    all_tags.insert(primary_metadata->tags.begin(),
                   primary_metadata->tags.end());
    merged.total_access_count = primary_metadata->access_count;
    merged.best_rating = primary_metadata->rating;
    merged.latest_date = primary_metadata->last_modified;
  } else {
    merged.total_access_count = 0;
    merged.best_rating = 0;
    merged.latest_date = base::Time::Now();
  }

  merged.earliest_date = set.primary->date_added();

  // Merge from duplicates
  for (const auto* duplicate : set.duplicates) {
    const BookmarkMetadata* dup_metadata = manager_->GetMetadata(duplicate);
    if (dup_metadata) {
      all_tags.insert(dup_metadata->tags.begin(), dup_metadata->tags.end());
      merged.total_access_count += dup_metadata->access_count;
      merged.best_rating = std::max(merged.best_rating, dup_metadata->rating);
      merged.latest_date = std::max(merged.latest_date,
                                   dup_metadata->last_modified);
    }

    merged.earliest_date = std::min(merged.earliest_date,
                                   duplicate->date_added());
  }

  merged.all_tags.assign(all_tags.begin(), all_tags.end());

  return merged;
}
