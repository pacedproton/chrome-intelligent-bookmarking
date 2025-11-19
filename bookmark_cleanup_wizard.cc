// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_cleanup_wizard.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
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

// Extract domain from URL
std::u16string ExtractDomain(const GURL& url) {
  if (!url.is_valid()) {
    return u"";
  }
  return base::UTF8ToUTF16(url.host());
}

// Calculate similarity between two strings (simple word overlap)
float CalculateStringSimilarity(std::u16string_view a, std::u16string_view b) {
  if (a.empty() || b.empty()) {
    return 0.0f;
  }

  std::u16string lower_a = base::ToLowerASCII(a);
  std::u16string lower_b = base::ToLowerASCII(b);

  // Simple word-based similarity
  size_t common_chars = 0;
  size_t max_len = std::max(lower_a.length(), lower_b.length());

  for (size_t i = 0; i < std::min(lower_a.length(), lower_b.length()); ++i) {
    if (lower_a[i] == lower_b[i]) {
      ++common_chars;
    }
  }

  return static_cast<float>(common_chars) / static_cast<float>(max_len);
}

}  // namespace

// ===== BookmarkCleanupWizard Implementation =====

BookmarkCleanupWizard::BookmarkCleanupWizard(
    bookmarks::BookmarkModel* model,
    BookmarkManager* manager)
    : bookmark_model_(model), manager_(manager) {
  DCHECK(bookmark_model_);
  DCHECK(manager_);
}

BookmarkCleanupWizard::~BookmarkCleanupWizard() = default;

void BookmarkCleanupWizard::StartWizard() {
  current_step_ = WizardStep::kAnalysis;
  analysis_ = AnalyzeBookmarks();
  plan_ = GenerateCleanupPlan(analysis_.value());
}

CleanupAnalysis BookmarkCleanupWizard::AnalyzeBookmarks() {
  DCHECK(bookmark_model_);
  DCHECK(manager_);

  CleanupAnalysis analysis;

  // Collect all bookmarks
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;
  CollectAllBookmarks(bookmark_model_->root_node(), all_bookmarks);

  analysis.total_bookmarks = all_bookmarks.size();

  // Track URLs for duplicate detection
  std::map<GURL, std::vector<const bookmarks::BookmarkNode*>> url_map;

  // Analyze each bookmark
  base::Time two_years_ago = base::Time::Now() - base::Days(730);

  for (const auto* bookmark : all_bookmarks) {
    DCHECK(bookmark);
    DCHECK(bookmark->is_url());

    // Check for duplicates
    url_map[bookmark->url()].push_back(bookmark);

    // Check if orphaned (directly in bookmark bar or other bookmarks)
    const bookmarks::BookmarkNode* parent = bookmark->parent();
    if (parent && parent->is_permanent_node()) {
      ++analysis.orphaned;
    }

    // Check if rarely used (need metadata from manager)
    const BookmarkMetadata* metadata = manager_->GetMetadata(bookmark);
    if (metadata) {
      if (metadata->access_count == 0 ||
          metadata->last_accessed < two_years_ago) {
        ++analysis.rarely_used;
      }

      // Check if untagged
      if (metadata->tags.empty()) {
        ++analysis.untagged;
      }
    } else {
      // No metadata = never used
      ++analysis.rarely_used;
      ++analysis.untagged;
    }

    // Suggest categories based on domain
    std::u16string domain = ExtractDomain(bookmark->url());
    if (!domain.empty()) {
      analysis.suggested_categories[domain]++;
    }
  }

  // Count duplicates
  for (const auto& [url, bookmarks] : url_map) {
    if (bookmarks.size() > 1) {
      analysis.duplicates += bookmarks.size() - 1;  // Count extras
    }
  }

  // Collect and analyze folders
  std::vector<const bookmarks::BookmarkNode*> all_folders;
  CollectAllFolders(bookmark_model_->root_node(), all_folders);

  for (const auto* folder : all_folders) {
    DCHECK(folder);
    DCHECK(folder->is_folder());

    // Check if empty
    if (folder->children().empty()) {
      ++analysis.empty_folders;
    }
  }

  // Calculate health score (0-100)
  int score = 100;

  // Deduct points for issues (max 10 points per issue category)
  if (analysis.total_bookmarks > 0) {
    score -= std::min(10, static_cast<int>(
        (analysis.dead_links * 100) / analysis.total_bookmarks));
    score -= std::min(10, static_cast<int>(
        (analysis.duplicates * 100) / analysis.total_bookmarks));
    score -= std::min(10, static_cast<int>(
        (analysis.orphaned * 100) / analysis.total_bookmarks));
    score -= std::min(10, static_cast<int>(
        (analysis.rarely_used * 100) / analysis.total_bookmarks));
    score -= std::min(10, static_cast<int>(
        (analysis.untagged * 100) / analysis.total_bookmarks));
  }

  score -= std::min(10, static_cast<int>(analysis.empty_folders * 2));

  analysis.health_score = std::max(0, score);

  DLOG(INFO) << "Bookmark analysis complete: " << analysis.total_bookmarks
             << " total, health score: " << analysis.health_score;

  return analysis;
}

CleanupPlan BookmarkCleanupWizard::GenerateCleanupPlan(
    const CleanupAnalysis& analysis) {
  DCHECK(bookmark_model_);

  CleanupPlan plan;

  // Collect all bookmarks again for planning
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;
  CollectAllBookmarks(bookmark_model_->root_node(), all_bookmarks);

  // Track URLs for duplicate detection
  std::map<GURL, std::vector<const bookmarks::BookmarkNode*>> url_map;
  for (const auto* bookmark : all_bookmarks) {
    url_map[bookmark->url()].push_back(bookmark);
  }

  // Plan duplicate merging
  for (const auto& [url, bookmarks] : url_map) {
    if (bookmarks.size() > 1) {
      plan.duplicates_to_merge.push_back(bookmarks);
    }
  }

  // Plan archiving rarely used bookmarks
  base::Time two_years_ago = base::Time::Now() - base::Days(730);
  for (const auto* bookmark : all_bookmarks) {
    const BookmarkMetadata* metadata = manager_->GetMetadata(bookmark);
    if (metadata &&
        (metadata->access_count == 0 ||
         metadata->last_accessed < two_years_ago)) {
      plan.rarely_used_to_archive.push_back(bookmark);
    }
  }

  // Plan folder cleanup
  std::vector<const bookmarks::BookmarkNode*> all_folders;
  CollectAllFolders(bookmark_model_->root_node(), all_folders);

  for (const auto* folder : all_folders) {
    if (folder->children().empty()) {
      plan.empty_folders_to_delete.push_back(folder);
    }
  }

  // Plan category assignments based on domain
  for (const auto* bookmark : all_bookmarks) {
    std::u16string domain = ExtractDomain(bookmark->url());
    if (!domain.empty()) {
      plan.category_assignments[domain].push_back(bookmark);
    }
  }

  DLOG(INFO) << "Cleanup plan generated: " << plan.duplicates_to_merge.size()
             << " duplicate sets, " << plan.empty_folders_to_delete.size()
             << " empty folders";

  return plan;
}

void BookmarkCleanupWizard::ApplyCleanupPlan(const CleanupPlan& plan) {
  DCHECK(bookmark_model_);
  DCHECK(manager_);

  // Remove empty folders
  for (const auto* folder : plan.empty_folders_to_delete) {
    if (folder && folder->children().empty()) {
      bookmark_model_->Remove(folder, bookmarks::metrics::BookmarkEditSource::kUser);
    }
  }

  // Archive rarely used bookmarks
  if (!plan.rarely_used_to_archive.empty()) {
    // Create "Archive" folder if it doesn't exist
    const bookmarks::BookmarkNode* other =
        bookmark_model_->other_node();
    const bookmarks::BookmarkNode* archive_folder = nullptr;

    for (const auto& child : other->children()) {
      if (child->is_folder() && child->GetTitle() == u"Archive") {
        archive_folder = child.get();
        break;
      }
    }

    if (!archive_folder) {
      archive_folder = bookmark_model_->AddFolder(
          other, 0, u"Archive");
    }

    // Move bookmarks to archive
    for (const auto* bookmark : plan.rarely_used_to_archive) {
      if (bookmark) {
        bookmark_model_->Move(bookmark, archive_folder,
                            archive_folder->children().size());
        // Mark as archived in metadata
        manager_->SetArchived(bookmark, true);
      }
    }
  }

  // Note: Duplicate merging and category assignments would be done
  // interactively with user confirmation in a real UI

  DLOG(INFO) << "Cleanup plan applied";
}

void BookmarkCleanupWizard::AdvanceStep() {
  switch (current_step_) {
    case WizardStep::kAnalysis:
      current_step_ = WizardStep::kRemoveClutter;
      break;
    case WizardStep::kRemoveClutter:
      current_step_ = WizardStep::kAutoCategorize;
      break;
    case WizardStep::kAutoCategorize:
      current_step_ = WizardStep::kManualRefinement;
      break;
    case WizardStep::kManualRefinement:
      current_step_ = WizardStep::kFinalize;
      break;
    case WizardStep::kFinalize:
      // Already at end
      break;
  }
}

// ===== CleanupDashboard Implementation =====

CleanupDashboard::CleanupDashboard(bookmarks::BookmarkModel* model,
                                   BookmarkManager* manager)
    : bookmark_model_(model), manager_(manager) {
  DCHECK(bookmark_model_);
  DCHECK(manager_);
}

CleanupDashboard::~CleanupDashboard() = default;

CleanupDashboard::DashboardData CleanupDashboard::GenerateDashboard() {
  DCHECK(bookmark_model_);

  DashboardData data;
  data.organization_score = CalculateOrganizationScore();
  data.health_indicators = GetHealthIndicators();

  // Calculate folder distribution
  std::vector<const bookmarks::BookmarkNode*> all_folders;
  CollectAllFolders(bookmark_model_->root_node(), all_folders);

  for (const auto* folder : all_folders) {
    size_t bookmark_count = 0;
    std::vector<const bookmarks::BookmarkNode*> bookmarks;
    CollectAllBookmarks(folder, bookmarks);
    bookmark_count = bookmarks.size();

    if (bookmark_count > 0) {
      data.folder_distribution[folder->GetTitle()] = bookmark_count;
    }
  }

  // Calculate bookmark timeline (by year)
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;
  CollectAllBookmarks(bookmark_model_->root_node(), all_bookmarks);

  for (const auto* bookmark : all_bookmarks) {
    base::Time date_added = bookmark->date_added();
    base::Time::Exploded exploded;
    date_added.LocalExplode(&exploded);
    data.bookmark_timeline[exploded.year]++;
  }

  // Calculate category percentages
  std::map<std::u16string, size_t> domain_counts;
  size_t total = 0;

  for (const auto* bookmark : all_bookmarks) {
    std::u16string domain = ExtractDomain(bookmark->url());
    if (!domain.empty()) {
      domain_counts[domain]++;
      ++total;
    }
  }

  if (total > 0) {
    for (const auto& [domain, count] : domain_counts) {
      data.category_percentages[domain] =
          (static_cast<float>(count) / static_cast<float>(total)) * 100.0f;
    }
  }

  return data;
}

int CleanupDashboard::CalculateOrganizationScore() {
  DCHECK(bookmark_model_);
  DCHECK(manager_);

  int score = 100;

  // Collect all bookmarks
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;
  CollectAllBookmarks(bookmark_model_->root_node(), all_bookmarks);

  if (all_bookmarks.empty()) {
    return 100;  // Perfect score if no bookmarks
  }

  size_t total = all_bookmarks.size();

  // Deduct points for poor organization
  size_t orphaned = 0;
  size_t untagged = 0;
  size_t unfiled = 0;

  for (const auto* bookmark : all_bookmarks) {
    // Check if orphaned (in permanent node)
    const bookmarks::BookmarkNode* parent = bookmark->parent();
    if (parent && parent->is_permanent_node()) {
      ++orphaned;
    }

    // Check if untagged
    const BookmarkMetadata* metadata = manager_->GetMetadata(bookmark);
    if (!metadata || metadata->tags.empty()) {
      ++untagged;
    }

    // Check if not in a meaningful folder
    if (parent && parent->is_permanent_node()) {
      ++unfiled;
    }
  }

  // Deduct points (max 30 points per category)
  score -= std::min(30, static_cast<int>((orphaned * 100) / total));
  score -= std::min(30, static_cast<int>((untagged * 100) / total));
  score -= std::min(20, static_cast<int>((unfiled * 100) / total));

  // Check folder depth (too deep = bad)
  std::vector<const bookmarks::BookmarkNode*> all_folders;
  CollectAllFolders(bookmark_model_->root_node(), all_folders);

  int max_depth = 0;
  for (const auto* folder : all_folders) {
    int depth = 0;
    const bookmarks::BookmarkNode* node = folder;
    while (node && !node->is_permanent_node()) {
      ++depth;
      node = node->parent();
    }
    max_depth = std::max(max_depth, depth);
  }

  // Deduct points for excessive depth (> 5 levels)
  if (max_depth > 5) {
    score -= std::min(10, (max_depth - 5) * 2);
  }

  return std::max(0, score);
}

std::vector<CleanupDashboard::HealthIndicator>
CleanupDashboard::GetHealthIndicators() {
  DCHECK(bookmark_model_);
  DCHECK(manager_);

  std::vector<HealthIndicator> indicators;

  // Collect all bookmarks
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;
  CollectAllBookmarks(bookmark_model_->root_node(), all_bookmarks);

  if (all_bookmarks.empty()) {
    return indicators;
  }

  // Check for untagged bookmarks
  size_t untagged_count = 0;
  for (const auto* bookmark : all_bookmarks) {
    const BookmarkMetadata* metadata = manager_->GetMetadata(bookmark);
    if (!metadata || metadata->tags.empty()) {
      ++untagged_count;
    }
  }

  if (untagged_count > 0) {
    float percentage =
        (static_cast<float>(untagged_count) /
         static_cast<float>(all_bookmarks.size())) * 100.0f;
    HealthIndicator indicator;
    indicator.severity = percentage > 50.0f ? Severity::kError :
                        percentage > 25.0f ? Severity::kWarning :
                        Severity::kGood;
    indicator.message = base::NumberToString16(static_cast<int>(percentage)) +
                       u"% of bookmarks are untagged";
    indicator.affected_count = untagged_count;
    indicators.push_back(indicator);
  }

  // Check for never-visited bookmarks
  size_t never_visited = 0;
  for (const auto* bookmark : all_bookmarks) {
    const BookmarkMetadata* metadata = manager_->GetMetadata(bookmark);
    if (!metadata || metadata->access_count == 0) {
      ++never_visited;
    }
  }

  if (never_visited > 0) {
    float percentage =
        (static_cast<float>(never_visited) /
         static_cast<float>(all_bookmarks.size())) * 100.0f;
    HealthIndicator indicator;
    indicator.severity = percentage > 50.0f ? Severity::kWarning :
                        Severity::kGood;
    indicator.message = base::NumberToString16(static_cast<int>(percentage)) +
                       u"% of bookmarks have never been visited";
    indicator.affected_count = never_visited;
    indicators.push_back(indicator);
  }

  // Check for duplicates
  std::map<GURL, std::vector<const bookmarks::BookmarkNode*>> url_map;
  for (const auto* bookmark : all_bookmarks) {
    url_map[bookmark->url()].push_back(bookmark);
  }

  size_t duplicate_count = 0;
  for (const auto& [url, bookmarks] : url_map) {
    if (bookmarks.size() > 1) {
      duplicate_count += bookmarks.size() - 1;
    }
  }

  if (duplicate_count > 0) {
    HealthIndicator indicator;
    indicator.severity = duplicate_count > 10 ? Severity::kError :
                        duplicate_count > 5 ? Severity::kWarning :
                        Severity::kGood;
    indicator.message = base::NumberToString16(duplicate_count) +
                       u" duplicate bookmarks found";
    indicator.affected_count = duplicate_count;
    indicators.push_back(indicator);
  }

  return indicators;
}

// ===== OrphanInspector Implementation =====

OrphanInspector::OrphanInspector(bookmarks::BookmarkModel* model,
                                 BookmarkManager* manager)
    : bookmark_model_(model), manager_(manager) {
  DCHECK(bookmark_model_);
  DCHECK(manager_);
}

OrphanInspector::~OrphanInspector() = default;

std::vector<OrphanInspector::OrphanBookmark> OrphanInspector::FindOrphans() {
  DCHECK(bookmark_model_);

  std::vector<OrphanBookmark> orphans;

  // Collect all bookmarks
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;
  CollectAllBookmarks(bookmark_model_->root_node(), all_bookmarks);

  // Find orphans (bookmarks in permanent nodes)
  for (const auto* bookmark : all_bookmarks) {
    const bookmarks::BookmarkNode* parent = bookmark->parent();
    if (parent && parent->is_permanent_node()) {
      OrphanBookmark orphan;
      orphan.bookmark = bookmark;
      orphan.current_parent = parent;
      orphan.confidence = 0.0f;

      // Suggest folders
      std::u16string suggestion = SuggestFolder(bookmark);
      if (!suggestion.empty()) {
        orphan.suggested_folders.push_back(suggestion);
        orphan.confidence = 0.8f;  // High confidence for domain-based
      }

      orphans.push_back(orphan);
    }
  }

  DLOG(INFO) << "Found " << orphans.size() << " orphaned bookmarks";

  return orphans;
}

std::u16string OrphanInspector::SuggestFolder(
    const bookmarks::BookmarkNode* bookmark) {
  DCHECK(bookmark);
  DCHECK(bookmark_model_);

  // Extract domain from URL
  std::u16string domain = ExtractDomain(bookmark->url());
  if (domain.empty()) {
    return u"";
  }

  // Look for existing folders with similar bookmarks
  std::vector<const bookmarks::BookmarkNode*> all_folders;
  CollectAllFolders(bookmark_model_->root_node(), all_folders);

  std::map<const bookmarks::BookmarkNode*, int> folder_scores;

  for (const auto* folder : all_folders) {
    int score = 0;

    // Check bookmarks in this folder
    std::vector<const bookmarks::BookmarkNode*> folder_bookmarks;
    CollectAllBookmarks(folder, folder_bookmarks);

    for (const auto* fb : folder_bookmarks) {
      std::u16string fb_domain = ExtractDomain(fb->url());
      if (fb_domain == domain) {
        score += 10;  // Same domain = high score
      }
    }

    // Check folder name similarity
    float name_similarity = CalculateStringSimilarity(
        folder->GetTitle(), domain);
    score += static_cast<int>(name_similarity * 5.0f);

    if (score > 0) {
      folder_scores[folder] = score;
    }
  }

  // Find best folder
  const bookmarks::BookmarkNode* best_folder = nullptr;
  int best_score = 0;

  for (const auto& [folder, score] : folder_scores) {
    if (score > best_score) {
      best_score = score;
      best_folder = folder;
    }
  }

  if (best_folder) {
    return best_folder->GetTitle();
  }

  // If no good match, suggest creating a folder based on domain
  return domain;
}

void OrphanInspector::MoveOrphansToSuggestedFolders(
    const std::vector<OrphanBookmark>& orphans) {
  DCHECK(bookmark_model_);

  for (const auto& orphan : orphans) {
    if (orphan.suggested_folders.empty() || orphan.confidence < 0.5f) {
      continue;  // Skip low-confidence suggestions
    }

    std::u16string folder_name = orphan.suggested_folders[0];

    // Find or create the folder
    std::vector<const bookmarks::BookmarkNode*> all_folders;
    CollectAllFolders(bookmark_model_->root_node(), all_folders);

    const bookmarks::BookmarkNode* target_folder = nullptr;
    for (const auto* folder : all_folders) {
      if (folder->GetTitle() == folder_name) {
        target_folder = folder;
        break;
      }
    }

    if (!target_folder) {
      // Create new folder in bookmark bar
      const bookmarks::BookmarkNode* bar =
          bookmark_model_->bookmark_bar_node();
      target_folder = bookmark_model_->AddFolder(bar, 0, folder_name);
    }

    // Move bookmark
    if (target_folder && orphan.bookmark) {
      bookmark_model_->Move(orphan.bookmark, target_folder,
                          target_folder->children().size());
    }
  }

  DLOG(INFO) << "Moved " << orphans.size() << " orphaned bookmarks";
}
