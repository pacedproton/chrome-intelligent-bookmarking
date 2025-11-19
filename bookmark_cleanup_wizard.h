// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_CLEANUP_WIZARD_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_CLEANUP_WIZARD_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

// ===== Phase 1: Foundation =====

// Analysis results from bookmark collection scan.
struct CleanupAnalysis {
  size_t total_bookmarks = 0;
  size_t dead_links = 0;
  size_t duplicates = 0;
  size_t orphaned = 0;  // In root, not in folders
  size_t rarely_used = 0;  // < 1 visit in 2 years
  size_t empty_folders = 0;
  size_t untagged = 0;
  int health_score = 0;  // 0-100
  std::map<std::u16string, size_t> suggested_categories;
};

// Cleanup plan describing what actions to take.
struct CleanupPlan {
  std::vector<const bookmarks::BookmarkNode*> dead_links_to_remove;
  std::vector<std::vector<const bookmarks::BookmarkNode*>> duplicates_to_merge;
  std::vector<const bookmarks::BookmarkNode*> rarely_used_to_archive;
  std::vector<const bookmarks::BookmarkNode*> empty_folders_to_delete;
  std::map<std::u16string, std::vector<const bookmarks::BookmarkNode*>>
      category_assignments;
};

// Wizard to guide users through bookmark cleanup process.
class BookmarkCleanupWizard {
 public:
  enum class WizardStep {
    kAnalysis,
    kRemoveClutter,
    kAutoCategorize,
    kManualRefinement,
    kFinalize
  };

  explicit BookmarkCleanupWizard(bookmarks::BookmarkModel* model,
                                  BookmarkManager* manager);
  ~BookmarkCleanupWizard();

  BookmarkCleanupWizard(const BookmarkCleanupWizard&) = delete;
  BookmarkCleanupWizard& operator=(const BookmarkCleanupWizard&) = delete;

  // Start the cleanup wizard.
  void StartWizard();

  // Perform comprehensive analysis.
  [[nodiscard]] CleanupAnalysis AnalyzeBookmarks();

  // Generate cleanup plan based on analysis.
  [[nodiscard]] CleanupPlan GenerateCleanupPlan(
      const CleanupAnalysis& analysis);

  // Apply the cleanup plan.
  void ApplyCleanupPlan(const CleanupPlan& plan);

  // Get current wizard step.
  [[nodiscard]] WizardStep GetCurrentStep() const { return current_step_; }

  // Advance to next step.
  void AdvanceStep();

 private:
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<BookmarkManager> manager_;
  WizardStep current_step_ = WizardStep::kAnalysis;
  std::optional<CleanupAnalysis> analysis_;
  std::optional<CleanupPlan> plan_;
};

// Dashboard showing cleanup insights and health metrics.
class CleanupDashboard {
 public:
  struct HealthIndicator {
    enum Severity { kGood, kWarning, kError };
    Severity severity;
    std::u16string message;
    size_t affected_count;
  };

  struct DashboardData {
    int organization_score;  // 0-100
    std::map<std::u16string, size_t> folder_distribution;
    std::vector<HealthIndicator> health_indicators;
    std::map<int, size_t> bookmark_timeline;  // year -> count
    std::map<std::u16string, float> category_percentages;
  };

  explicit CleanupDashboard(bookmarks::BookmarkModel* model,
                           BookmarkManager* manager);
  ~CleanupDashboard();

  // Generate complete dashboard data.
  [[nodiscard]] DashboardData GenerateDashboard();

  // Calculate organization score (0-100).
  [[nodiscard]] int CalculateOrganizationScore();

  // Get health indicators.
  [[nodiscard]] std::vector<HealthIndicator> GetHealthIndicators();

 private:
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<BookmarkManager> manager_;
};

// Inspector for finding and organizing orphaned bookmarks.
class OrphanInspector {
 public:
  struct OrphanBookmark {
    const bookmarks::BookmarkNode* bookmark;
    const bookmarks::BookmarkNode* current_parent;
    std::vector<std::u16string> suggested_folders;
    float confidence;  // 0.0-1.0
  };

  explicit OrphanInspector(bookmarks::BookmarkModel* model,
                          BookmarkManager* manager);
  ~OrphanInspector();

  // Find all orphaned bookmarks (in root or poorly organized).
  [[nodiscard]] std::vector<OrphanBookmark> FindOrphans();

  // Suggest destination folder for a bookmark.
  [[nodiscard]] std::u16string SuggestFolder(
      const bookmarks::BookmarkNode* bookmark);

  // Move orphans to suggested folders.
  void MoveOrphansToSuggestedFolders(
      const std::vector<OrphanBookmark>& orphans);

 private:
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<BookmarkManager> manager_;
};

// ===== Phase 2: Auto-Categorization =====

// Smart categorization engine for automatic bookmark organization.
class SmartCategorizer {
 public:
  struct Category {
    std::u16string name;
    std::u16string description;
    std::vector<const bookmarks::BookmarkNode*> bookmarks;
    float confidence;  // 0.0-1.0
  };

  explicit SmartCategorizer(bookmarks::BookmarkModel* model,
                           BookmarkManager* manager);
  ~SmartCategorizer();

  // Analyze and suggest categories.
  [[nodiscard]] std::vector<Category> SuggestCategories(
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks);

  // Group bookmarks by domain.
  [[nodiscard]] std::map<std::u16string,
                        std::vector<const bookmarks::BookmarkNode*>>
  GroupByDomain(const std::vector<const bookmarks::BookmarkNode*>& bookmarks);

  // Group bookmarks by URL patterns.
  [[nodiscard]] std::map<std::u16string,
                        std::vector<const bookmarks::BookmarkNode*>>
  GroupByPattern(const std::vector<const bookmarks::BookmarkNode*>& bookmarks);

  // Group bookmarks by time period.
  [[nodiscard]] std::map<std::u16string,
                        std::vector<const bookmarks::BookmarkNode*>>
  GroupByTimePeriod(
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks);

  // Detect URL pattern type (docs, api, tutorial, etc.).
  [[nodiscard]] std::u16string DetectURLPattern(const GURL& url);

 private:
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<BookmarkManager> manager_;
};

// ===== Phase 3: Advanced Features =====

// Enhanced duplicate detection and merging with metadata preservation.
class DuplicateMerger {
 public:
  struct MergedMetadata {
    std::vector<std::u16string> all_tags;
    int total_access_count;
    int best_rating;
    base::Time earliest_date;
    base::Time latest_date;
  };

  struct DuplicateSet {
    const bookmarks::BookmarkNode* primary;  // Best one to keep
    std::vector<const bookmarks::BookmarkNode*> duplicates;
    MergedMetadata combined_metadata;
  };

  explicit DuplicateMerger(bookmarks::BookmarkModel* model,
                          BookmarkManager* manager);
  ~DuplicateMerger();

  // Find all duplicate sets.
  [[nodiscard]] std::vector<DuplicateSet> FindDuplicateSets();

  // Select best bookmark to keep from duplicates.
  [[nodiscard]] const bookmarks::BookmarkNode* SelectPrimary(
      const std::vector<const bookmarks::BookmarkNode*>& duplicates);

  // Merge metadata from duplicates into primary.
  void MergeDuplicates(const DuplicateSet& set);

  // Preview merge result without applying.
  [[nodiscard]] MergedMetadata PreviewMerge(const DuplicateSet& set);

 private:
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<BookmarkManager> manager_;
};

// Analyzer and optimizer for folder structure.
class FolderOptimizer {
 public:
  struct FolderAnalysis {
    int max_depth;
    int avg_bookmarks_per_folder;
    std::vector<const bookmarks::BookmarkNode*> oversized_folders;
    std::vector<const bookmarks::BookmarkNode*> empty_folders;
    std::vector<std::pair<const bookmarks::BookmarkNode*,
                         const bookmarks::BookmarkNode*>> similar_folders;
  };

  struct Optimization {
    enum Type {
      kFlattenHierarchy,
      kSplitLargeFolder,
      kMergeSimilarFolders,
      kRemoveEmptyFolders
    };

    Type type;
    std::vector<const bookmarks::BookmarkNode*> affected_folders;
    std::u16string description;
  };

  explicit FolderOptimizer(bookmarks::BookmarkModel* model,
                          BookmarkManager* manager);
  ~FolderOptimizer();

  // Analyze folder structure.
  [[nodiscard]] FolderAnalysis AnalyzeStructure();

  // Suggest optimizations.
  [[nodiscard]] std::vector<Optimization> SuggestOptimizations();

  // Apply optimization.
  void ApplyOptimization(const Optimization& opt);

  // Calculate folder depth.
  [[nodiscard]] int CalculateFolderDepth(
      const bookmarks::BookmarkNode* folder);

 private:
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<BookmarkManager> manager_;
};

// Bulk tag suggestion engine.
class BulkTagSuggester {
 public:
  struct TagSuggestion {
    const bookmarks::BookmarkNode* bookmark;
    std::vector<std::u16string> suggested_tags;
    float confidence;  // 0.0-1.0
  };

  explicit BulkTagSuggester(bookmarks::BookmarkModel* model,
                           BookmarkManager* manager);
  ~BulkTagSuggester();

  // Generate tag suggestions for untagged bookmarks.
  [[nodiscard]] std::vector<TagSuggestion> SuggestTags(
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks);

  // Extract tags from URL.
  [[nodiscard]] std::vector<std::u16string> ExtractTagsFromURL(
      const GURL& url);

  // Extract tags from title.
  [[nodiscard]] std::vector<std::u16string> ExtractTagsFromTitle(
      std::u16string_view title);

  // Apply suggested tags.
  void ApplyTagSuggestions(const std::vector<TagSuggestion>& suggestions);

 private:
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<BookmarkManager> manager_;
};

// ===== Phase 4: Quick Organizer =====

// Quick keyboard-driven organization mode for power users.
class QuickOrganizer {
 public:
  static constexpr size_t kQuickFolderSlots = 9;

  struct QuickAction {
    enum Type { kMove, kTag, kDelete, kArchive, kFavorite };

    Type type;
    std::u16string parameter;  // folder name, tag, etc.
  };

  explicit QuickOrganizer(bookmarks::BookmarkModel* model,
                         BookmarkManager* manager);
  ~QuickOrganizer();

  // Type-ahead folder search.
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*> SearchFolders(
      std::u16string_view query);

  // Quick move to numbered slot (1-9).
  void MoveToQuickFolder(
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks,
      int slot);

  // Set quick folder slot.
  void SetQuickFolder(const bookmarks::BookmarkNode* folder, int slot);

  // Get quick folder for slot.
  [[nodiscard]] const bookmarks::BookmarkNode* GetQuickFolder(int slot) const;

 private:
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<BookmarkManager> manager_;
  std::array<raw_ptr<const bookmarks::BookmarkNode>, kQuickFolderSlots>
      quick_folders_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_CLEANUP_WIZARD_H_
