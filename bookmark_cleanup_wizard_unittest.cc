// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_cleanup_wizard.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"

class BookmarkCleanupWizardTest : public testing::Test {
 public:
  void SetUp() override {
    bookmark_client_ = std::make_unique<bookmarks::TestBookmarkClient>();
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel(
        std::move(bookmark_client_));
    manager_ = std::make_unique<BookmarkManager>(bookmark_model_.get());
    wizard_ = std::make_unique<BookmarkCleanupWizard>(bookmark_model_.get(),
                                                       manager_.get());
  }

  void TearDown() override {
    wizard_.reset();
    manager_.reset();
    bookmark_model_.reset();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<BookmarkManager> manager_;
  std::unique_ptr<BookmarkCleanupWizard> wizard_;
};

// ===== BookmarkCleanupWizard Tests =====

TEST_F(BookmarkCleanupWizardTest, AnalyzeEmptyBookmarks) {
  CleanupAnalysis analysis = wizard_->AnalyzeBookmarks();

  EXPECT_EQ(0u, analysis.total_bookmarks);
  EXPECT_EQ(0u, analysis.duplicates);
  EXPECT_EQ(0u, analysis.orphaned);
  EXPECT_EQ(0u, analysis.empty_folders);
  EXPECT_EQ(100, analysis.health_score);  // Perfect score for no bookmarks
}

TEST_F(BookmarkCleanupWizardTest, AnalyzeBasicBookmarks) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  // Add some bookmarks
  bookmark_model_->AddURL(bar, 0, u"Bookmark 1", GURL("https://example.com/1"));
  bookmark_model_->AddURL(bar, 1, u"Bookmark 2", GURL("https://example.com/2"));

  CleanupAnalysis analysis = wizard_->AnalyzeBookmarks();

  EXPECT_EQ(2u, analysis.total_bookmarks);
  EXPECT_EQ(2u, analysis.orphaned);  // Both in permanent node
}

TEST_F(BookmarkCleanupWizardTest, DetectDuplicates) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  // Add duplicate bookmarks
  bookmark_model_->AddURL(bar, 0, u"Bookmark", GURL("https://example.com"));
  bookmark_model_->AddURL(bar, 1, u"Duplicate", GURL("https://example.com"));
  bookmark_model_->AddURL(bar, 2, u"Another", GURL("https://example.com"));

  CleanupAnalysis analysis = wizard_->AnalyzeBookmarks();

  EXPECT_EQ(3u, analysis.total_bookmarks);
  EXPECT_EQ(2u, analysis.duplicates);  // 3 bookmarks = 2 duplicates
}

TEST_F(BookmarkCleanupWizardTest, DetectEmptyFolders) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  // Add empty folders
  bookmark_model_->AddFolder(bar, 0, u"Empty 1");
  bookmark_model_->AddFolder(bar, 1, u"Empty 2");

  CleanupAnalysis analysis = wizard_->AnalyzeBookmarks();

  EXPECT_EQ(2u, analysis.empty_folders);
}

TEST_F(BookmarkCleanupWizardTest, GenerateCleanupPlan) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  // Add duplicates
  bookmark_model_->AddURL(bar, 0, u"Bookmark", GURL("https://example.com"));
  bookmark_model_->AddURL(bar, 1, u"Duplicate", GURL("https://example.com"));

  // Add empty folder
  bookmark_model_->AddFolder(bar, 2, u"Empty");

  CleanupAnalysis analysis = wizard_->AnalyzeBookmarks();
  CleanupPlan plan = wizard_->GenerateCleanupPlan(analysis);

  EXPECT_EQ(1u, plan.duplicates_to_merge.size());
  EXPECT_EQ(1u, plan.empty_folders_to_delete.size());
}

TEST_F(BookmarkCleanupWizardTest, ApplyCleanupPlan) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  // Add empty folders
  bookmark_model_->AddFolder(bar, 0, u"Empty 1");
  bookmark_model_->AddFolder(bar, 1, u"Empty 2");

  CleanupAnalysis analysis = wizard_->AnalyzeBookmarks();
  CleanupPlan plan = wizard_->GenerateCleanupPlan(analysis);

  EXPECT_EQ(2u, plan.empty_folders_to_delete.size());

  wizard_->ApplyCleanupPlan(plan);

  // Verify folders were removed
  EXPECT_EQ(0u, bar->children().size());
}

TEST_F(BookmarkCleanupWizardTest, WizardStepProgression) {
  EXPECT_EQ(BookmarkCleanupWizard::WizardStep::kAnalysis,
           wizard_->GetCurrentStep());

  wizard_->AdvanceStep();
  EXPECT_EQ(BookmarkCleanupWizard::WizardStep::kRemoveClutter,
           wizard_->GetCurrentStep());

  wizard_->AdvanceStep();
  EXPECT_EQ(BookmarkCleanupWizard::WizardStep::kAutoCategorize,
           wizard_->GetCurrentStep());

  wizard_->AdvanceStep();
  EXPECT_EQ(BookmarkCleanupWizard::WizardStep::kManualRefinement,
           wizard_->GetCurrentStep());

  wizard_->AdvanceStep();
  EXPECT_EQ(BookmarkCleanupWizard::WizardStep::kFinalize,
           wizard_->GetCurrentStep());
}

TEST_F(BookmarkCleanupWizardTest, StartWizard) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  bookmark_model_->AddURL(bar, 0, u"Test", GURL("https://example.com"));

  wizard_->StartWizard();

  EXPECT_EQ(BookmarkCleanupWizard::WizardStep::kAnalysis,
           wizard_->GetCurrentStep());
}

// ===== CleanupDashboard Tests =====

class CleanupDashboardTest : public testing::Test {
 public:
  void SetUp() override {
    bookmark_client_ = std::make_unique<bookmarks::TestBookmarkClient>();
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel(
        std::move(bookmark_client_));
    manager_ = std::make_unique<BookmarkManager>(bookmark_model_.get());
    dashboard_ = std::make_unique<CleanupDashboard>(bookmark_model_.get(),
                                                     manager_.get());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<BookmarkManager> manager_;
  std::unique_ptr<CleanupDashboard> dashboard_;
};

TEST_F(CleanupDashboardTest, EmptyDashboard) {
  CleanupDashboard::DashboardData data = dashboard_->GenerateDashboard();

  EXPECT_EQ(100, data.organization_score);  // Perfect for empty
  EXPECT_TRUE(data.health_indicators.empty());
}

TEST_F(CleanupDashboardTest, CalculateOrganizationScore) {
  int score = dashboard_->CalculateOrganizationScore();
  EXPECT_EQ(100, score);  // Perfect score for no bookmarks
}

TEST_F(CleanupDashboardTest, HealthIndicatorsForUntagged) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  // Add untagged bookmarks
  bookmark_model_->AddURL(bar, 0, u"Test 1", GURL("https://example.com/1"));
  bookmark_model_->AddURL(bar, 1, u"Test 2", GURL("https://example.com/2"));

  std::vector<CleanupDashboard::HealthIndicator> indicators =
      dashboard_->GetHealthIndicators();

  // Should have indicator for untagged bookmarks
  EXPECT_FALSE(indicators.empty());

  bool found_untagged = false;
  for (const auto& indicator : indicators) {
    if (indicator.message.find(u"untagged") != std::u16string::npos) {
      found_untagged = true;
      EXPECT_EQ(2u, indicator.affected_count);
    }
  }
  EXPECT_TRUE(found_untagged);
}

TEST_F(CleanupDashboardTest, HealthIndicatorsForDuplicates) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  // Add duplicates
  bookmark_model_->AddURL(bar, 0, u"Test", GURL("https://example.com"));
  bookmark_model_->AddURL(bar, 1, u"Dup", GURL("https://example.com"));

  std::vector<CleanupDashboard::HealthIndicator> indicators =
      dashboard_->GetHealthIndicators();

  bool found_duplicates = false;
  for (const auto& indicator : indicators) {
    if (indicator.message.find(u"duplicate") != std::u16string::npos) {
      found_duplicates = true;
      EXPECT_EQ(1u, indicator.affected_count);
    }
  }
  EXPECT_TRUE(found_duplicates);
}

TEST_F(CleanupDashboardTest, GenerateCompleteDesktop) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder =
      bookmark_model_->AddFolder(bar, 0, u"Work");

  bookmark_model_->AddURL(folder, 0, u"Test", GURL("https://example.com"));

  CleanupDashboard::DashboardData data = dashboard_->GenerateDashboard();

  EXPECT_GT(data.organization_score, 0);
  EXPECT_FALSE(data.folder_distribution.empty());
}

// ===== OrphanInspector Tests =====

class OrphanInspectorTest : public testing::Test {
 public:
  void SetUp() override {
    bookmark_client_ = std::make_unique<bookmarks::TestBookmarkClient>();
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel(
        std::move(bookmark_client_));
    manager_ = std::make_unique<BookmarkManager>(bookmark_model_.get());
    inspector_ = std::make_unique<OrphanInspector>(bookmark_model_.get(),
                                                    manager_.get());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<BookmarkManager> manager_;
  std::unique_ptr<OrphanInspector> inspector_;
};

TEST_F(OrphanInspectorTest, FindNoOrphans) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder =
      bookmark_model_->AddFolder(bar, 0, u"Folder");

  // Bookmark in folder = not orphaned
  bookmark_model_->AddURL(folder, 0, u"Test", GURL("https://example.com"));

  std::vector<OrphanInspector::OrphanBookmark> orphans =
      inspector_->FindOrphans();

  EXPECT_EQ(0u, orphans.size());
}

TEST_F(OrphanInspectorTest, FindOrphansInRoot) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  // Bookmark directly in bar = orphaned
  bookmark_model_->AddURL(bar, 0, u"Orphan", GURL("https://example.com"));

  std::vector<OrphanInspector::OrphanBookmark> orphans =
      inspector_->FindOrphans();

  EXPECT_EQ(1u, orphans.size());
  EXPECT_EQ(bar, orphans[0].current_parent);
}

TEST_F(OrphanInspectorTest, SuggestFolderByDomain) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder =
      bookmark_model_->AddFolder(bar, 0, u"github");

  // Add existing bookmark in folder
  bookmark_model_->AddURL(folder, 0, u"Repo 1",
                         GURL("https://github.com/test1"));

  // Add orphan with same domain
  const bookmarks::BookmarkNode* orphan = bookmark_model_->AddURL(
      bar, 1, u"Repo 2", GURL("https://github.com/test2"));

  std::u16string suggestion = inspector_->SuggestFolder(orphan);

  EXPECT_FALSE(suggestion.empty());
  // Should suggest "github" folder
  EXPECT_EQ(u"github", suggestion);
}

TEST_F(OrphanInspectorTest, MoveOrphansToSuggestedFolders) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder =
      bookmark_model_->AddFolder(bar, 0, u"Work");

  const bookmarks::BookmarkNode* orphan =
      bookmark_model_->AddURL(bar, 1, u"Test", GURL("https://example.com"));

  std::vector<OrphanInspector::OrphanBookmark> orphans;
  OrphanInspector::OrphanBookmark orphan_info;
  orphan_info.bookmark = orphan;
  orphan_info.current_parent = bar;
  orphan_info.suggested_folders.push_back(u"Work");
  orphan_info.confidence = 0.9f;
  orphans.push_back(orphan_info);

  inspector_->MoveOrphansToSuggestedFolders(orphans);

  // Verify bookmark was moved
  EXPECT_EQ(folder, orphan->parent());
}

// ===== SmartCategorizer Tests =====

class SmartCategorizerTest : public testing::Test {
 public:
  void SetUp() override {
    bookmark_client_ = std::make_unique<bookmarks::TestBookmarkClient>();
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel(
        std::move(bookmark_client_));
    manager_ = std::make_unique<BookmarkManager>(bookmark_model_.get());
    categorizer_ = std::make_unique<SmartCategorizer>(bookmark_model_.get(),
                                                       manager_.get());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<BookmarkManager> manager_;
  std::unique_ptr<SmartCategorizer> categorizer_;
};

TEST_F(SmartCategorizerTest, GroupByDomain) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  const bookmarks::BookmarkNode* b1 = bookmark_model_->AddURL(
      bar, 0, u"Test 1", GURL("https://github.com/test1"));
  const bookmarks::BookmarkNode* b2 = bookmark_model_->AddURL(
      bar, 1, u"Test 2", GURL("https://github.com/test2"));
  const bookmarks::BookmarkNode* b3 = bookmark_model_->AddURL(
      bar, 2, u"Test 3", GURL("https://example.com/test"));

  std::vector<const bookmarks::BookmarkNode*> bookmarks = {b1, b2, b3};
  auto groups = categorizer_->GroupByDomain(bookmarks);

  EXPECT_EQ(2u, groups.size());
  EXPECT_EQ(2u, groups[u"github.com"].size());
  EXPECT_EQ(1u, groups[u"example.com"].size());
}

TEST_F(SmartCategorizerTest, DetectURLPattern) {
  EXPECT_EQ(u"Development",
           categorizer_->DetectURLPattern(GURL("https://github.com/test")));
  EXPECT_EQ(u"Development",
           categorizer_->DetectURLPattern(
               GURL("https://stackoverflow.com/questions/123")));
  EXPECT_EQ(u"Videos",
           categorizer_->DetectURLPattern(GURL("https://youtube.com/watch")));
}

TEST_F(SmartCategorizerTest, SuggestCategories) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  // Add 3 bookmarks from same domain
  const bookmarks::BookmarkNode* b1 = bookmark_model_->AddURL(
      bar, 0, u"Repo 1", GURL("https://github.com/test1"));
  const bookmarks::BookmarkNode* b2 = bookmark_model_->AddURL(
      bar, 1, u"Repo 2", GURL("https://github.com/test2"));
  const bookmarks::BookmarkNode* b3 = bookmark_model_->AddURL(
      bar, 2, u"Repo 3", GURL("https://github.com/test3"));

  std::vector<const bookmarks::BookmarkNode*> bookmarks = {b1, b2, b3};
  std::vector<SmartCategorizer::Category> categories =
      categorizer_->SuggestCategories(bookmarks);

  EXPECT_FALSE(categories.empty());
  // Should suggest github.com category
  bool found_github = false;
  for (const auto& category : categories) {
    if (category.name == u"github.com") {
      found_github = true;
      EXPECT_EQ(3u, category.bookmarks.size());
    }
  }
  EXPECT_TRUE(found_github);
}

TEST_F(SmartCategorizerTest, GroupByTimePeriod) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  const bookmarks::BookmarkNode* b1 = bookmark_model_->AddURL(
      bar, 0, u"Test 1", GURL("https://example.com/1"));
  const bookmarks::BookmarkNode* b2 = bookmark_model_->AddURL(
      bar, 1, u"Test 2", GURL("https://example.com/2"));

  std::vector<const bookmarks::BookmarkNode*> bookmarks = {b1, b2};
  auto groups = categorizer_->GroupByTimePeriod(bookmarks);

  EXPECT_FALSE(groups.empty());
  // All recent bookmarks should be in "This Week"
  EXPECT_EQ(2u, groups[u"This Week"].size());
}

// ===== DuplicateMerger Tests =====

class DuplicateMergerTest : public testing::Test {
 public:
  void SetUp() override {
    bookmark_client_ = std::make_unique<bookmarks::TestBookmarkClient>();
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel(
        std::move(bookmark_client_));
    manager_ = std::make_unique<BookmarkManager>(bookmark_model_.get());
    merger_ = std::make_unique<DuplicateMerger>(bookmark_model_.get(),
                                                 manager_.get());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<BookmarkManager> manager_;
  std::unique_ptr<DuplicateMerger> merger_;
};

TEST_F(DuplicateMergerTest, FindNoDuplicates) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  bookmark_model_->AddURL(bar, 0, u"Test 1", GURL("https://example.com/1"));
  bookmark_model_->AddURL(bar, 1, u"Test 2", GURL("https://example.com/2"));

  std::vector<DuplicateMerger::DuplicateSet> sets =
      merger_->FindDuplicateSets();

  EXPECT_EQ(0u, sets.size());
}

TEST_F(DuplicateMergerTest, FindDuplicates) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  bookmark_model_->AddURL(bar, 0, u"Test", GURL("https://example.com"));
  bookmark_model_->AddURL(bar, 1, u"Dup", GURL("https://example.com"));

  std::vector<DuplicateMerger::DuplicateSet> sets =
      merger_->FindDuplicateSets();

  EXPECT_EQ(1u, sets.size());
  EXPECT_EQ(1u, sets[0].duplicates.size());
}

TEST_F(DuplicateMergerTest, SelectPrimaryByMetadata) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  const bookmarks::BookmarkNode* b1 =
      bookmark_model_->AddURL(bar, 0, u"Basic", GURL("https://example.com"));
  const bookmarks::BookmarkNode* b2 =
      bookmark_model_->AddURL(bar, 1, u"Better", GURL("https://example.com"));

  // Add metadata to b2
  manager_->AddTag(b2, u"important");
  manager_->SetRating(b2, 5);

  std::vector<const bookmarks::BookmarkNode*> duplicates = {b1, b2};
  const bookmarks::BookmarkNode* primary = merger_->SelectPrimary(duplicates);

  // b2 should be selected as primary due to better metadata
  EXPECT_EQ(b2, primary);
}

TEST_F(DuplicateMergerTest, PreviewMerge) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  const bookmarks::BookmarkNode* b1 =
      bookmark_model_->AddURL(bar, 0, u"Test 1", GURL("https://example.com"));
  const bookmarks::BookmarkNode* b2 =
      bookmark_model_->AddURL(bar, 1, u"Test 2", GURL("https://example.com"));

  manager_->AddTag(b1, u"tag1");
  manager_->AddTag(b2, u"tag2");
  manager_->SetRating(b1, 3);
  manager_->SetRating(b2, 5);

  DuplicateMerger::DuplicateSet set;
  set.primary = b1;
  set.duplicates.push_back(b2);

  DuplicateMerger::MergedMetadata merged = merger_->PreviewMerge(set);

  EXPECT_EQ(2u, merged.all_tags.size());
  EXPECT_EQ(5, merged.best_rating);
}

// ===== FolderOptimizer Tests =====

class FolderOptimizerTest : public testing::Test {
 public:
  void SetUp() override {
    bookmark_client_ = std::make_unique<bookmarks::TestBookmarkClient>();
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel(
        std::move(bookmark_client_));
    manager_ = std::make_unique<BookmarkManager>(bookmark_model_.get());
    optimizer_ = std::make_unique<FolderOptimizer>(bookmark_model_.get(),
                                                    manager_.get());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<BookmarkManager> manager_;
  std::unique_ptr<FolderOptimizer> optimizer_;
};

TEST_F(FolderOptimizerTest, AnalyzeEmptyStructure) {
  FolderOptimizer::FolderAnalysis analysis = optimizer_->AnalyzeStructure();

  EXPECT_EQ(0, analysis.max_depth);
  EXPECT_TRUE(analysis.empty_folders.empty());
  EXPECT_TRUE(analysis.oversized_folders.empty());
}

TEST_F(FolderOptimizerTest, DetectEmptyFolders) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  bookmark_model_->AddFolder(bar, 0, u"Empty 1");
  bookmark_model_->AddFolder(bar, 1, u"Empty 2");

  FolderOptimizer::FolderAnalysis analysis = optimizer_->AnalyzeStructure();

  EXPECT_EQ(2u, analysis.empty_folders.size());
}

TEST_F(FolderOptimizerTest, CalculateFolderDepth) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  const bookmarks::BookmarkNode* l1 =
      bookmark_model_->AddFolder(bar, 0, u"Level 1");
  const bookmarks::BookmarkNode* l2 =
      bookmark_model_->AddFolder(l1, 0, u"Level 2");
  const bookmarks::BookmarkNode* l3 =
      bookmark_model_->AddFolder(l2, 0, u"Level 3");

  EXPECT_EQ(1, optimizer_->CalculateFolderDepth(l1));
  EXPECT_EQ(2, optimizer_->CalculateFolderDepth(l2));
  EXPECT_EQ(3, optimizer_->CalculateFolderDepth(l3));
}

TEST_F(FolderOptimizerTest, SuggestOptimizations) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  // Add empty folders
  bookmark_model_->AddFolder(bar, 0, u"Empty");

  std::vector<FolderOptimizer::Optimization> optimizations =
      optimizer_->SuggestOptimizations();

  EXPECT_FALSE(optimizations.empty());

  bool found_remove_empty = false;
  for (const auto& opt : optimizations) {
    if (opt.type == FolderOptimizer::Optimization::Type::kRemoveEmptyFolders) {
      found_remove_empty = true;
    }
  }
  EXPECT_TRUE(found_remove_empty);
}

// ===== BulkTagSuggester Tests =====

class BulkTagSuggesterTest : public testing::Test {
 public:
  void SetUp() override {
    bookmark_client_ = std::make_unique<bookmarks::TestBookmarkClient>();
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel(
        std::move(bookmark_client_));
    manager_ = std::make_unique<BookmarkManager>(bookmark_model_.get());
    suggester_ = std::make_unique<BulkTagSuggester>(bookmark_model_.get(),
                                                     manager_.get());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<BookmarkManager> manager_;
  std::unique_ptr<BulkTagSuggester> suggester_;
};

TEST_F(BulkTagSuggesterTest, ExtractTagsFromURL) {
  std::vector<std::u16string> tags = suggester_->ExtractTagsFromURL(
      GURL("https://github.com/chromium/chromium"));

  EXPECT_FALSE(tags.empty());
  // Should extract "github" and "development"
  bool found_github = false;
  bool found_dev = false;
  for (const auto& tag : tags) {
    if (tag == u"github") found_github = true;
    if (tag == u"development") found_dev = true;
  }
  EXPECT_TRUE(found_github);
  EXPECT_TRUE(found_dev);
}

TEST_F(BulkTagSuggesterTest, ExtractTagsFromTitle) {
  std::vector<std::u16string> tags = suggester_->ExtractTagsFromTitle(
      u"Chrome Browser Development Guide");

  EXPECT_FALSE(tags.empty());
  // Should extract meaningful words (not stop words)
  bool found_chrome = false;
  bool found_browser = false;
  for (const auto& tag : tags) {
    if (tag == u"chrome") found_chrome = true;
    if (tag == u"browser") found_browser = true;
  }
  EXPECT_TRUE(found_chrome);
  EXPECT_TRUE(found_browser);
}

TEST_F(BulkTagSuggesterTest, SuggestTagsForUntagged) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  const bookmarks::BookmarkNode* b1 = bookmark_model_->AddURL(
      bar, 0, u"Chrome Development", GURL("https://github.com/chromium"));

  std::vector<const bookmarks::BookmarkNode*> bookmarks = {b1};
  std::vector<BulkTagSuggester::TagSuggestion> suggestions =
      suggester_->SuggestTags(bookmarks);

  EXPECT_EQ(1u, suggestions.size());
  EXPECT_FALSE(suggestions[0].suggested_tags.empty());
  EXPECT_GT(suggestions[0].confidence, 0.0f);
}

TEST_F(BulkTagSuggesterTest, SkipAlreadyTagged) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  const bookmarks::BookmarkNode* b1 = bookmark_model_->AddURL(
      bar, 0, u"Test", GURL("https://example.com"));

  // Add tag
  manager_->AddTag(b1, u"existing");

  std::vector<const bookmarks::BookmarkNode*> bookmarks = {b1};
  std::vector<BulkTagSuggester::TagSuggestion> suggestions =
      suggester_->SuggestTags(bookmarks);

  // Should skip already tagged bookmark
  EXPECT_EQ(0u, suggestions.size());
}

// ===== QuickOrganizer Tests =====

class QuickOrganizerTest : public testing::Test {
 public:
  void SetUp() override {
    bookmark_client_ = std::make_unique<bookmarks::TestBookmarkClient>();
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel(
        std::move(bookmark_client_));
    manager_ = std::make_unique<BookmarkManager>(bookmark_model_.get());
    organizer_ = std::make_unique<QuickOrganizer>(bookmark_model_.get(),
                                                   manager_.get());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<BookmarkManager> manager_;
  std::unique_ptr<QuickOrganizer> organizer_;
};

TEST_F(QuickOrganizerTest, SearchFolders) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  bookmark_model_->AddFolder(bar, 0, u"Work");
  bookmark_model_->AddFolder(bar, 1, u"Personal");
  bookmark_model_->AddFolder(bar, 2, u"Projects");

  std::vector<const bookmarks::BookmarkNode*> results =
      organizer_->SearchFolders(u"wor");

  EXPECT_FALSE(results.empty());
  // Should find "Work" folder
  EXPECT_EQ(u"Work", results[0]->GetTitle());
}

TEST_F(QuickOrganizerTest, SetAndGetQuickFolder) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder =
      bookmark_model_->AddFolder(bar, 0, u"Quick 1");

  organizer_->SetQuickFolder(folder, 0);

  EXPECT_EQ(folder, organizer_->GetQuickFolder(0));
}

TEST_F(QuickOrganizerTest, MoveToQuickFolder) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder =
      bookmark_model_->AddFolder(bar, 0, u"Target");

  const bookmarks::BookmarkNode* b1 =
      bookmark_model_->AddURL(bar, 1, u"Test 1", GURL("https://example.com/1"));
  const bookmarks::BookmarkNode* b2 =
      bookmark_model_->AddURL(bar, 2, u"Test 2", GURL("https://example.com/2"));

  organizer_->SetQuickFolder(folder, 0);

  std::vector<const bookmarks::BookmarkNode*> bookmarks = {b1, b2};
  organizer_->MoveToQuickFolder(bookmarks, 0);

  // Verify bookmarks were moved
  EXPECT_EQ(folder, b1->parent());
  EXPECT_EQ(folder, b2->parent());
  EXPECT_EQ(2u, folder->children().size());
}

TEST_F(QuickOrganizerTest, FuzzyFolderSearch) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  bookmark_model_->AddFolder(bar, 0, u"Development");
  bookmark_model_->AddFolder(bar, 1, u"Documentation");

  std::vector<const bookmarks::BookmarkNode*> results =
      organizer_->SearchFolders(u"dev");

  EXPECT_FALSE(results.empty());
  EXPECT_EQ(u"Development", results[0]->GetTitle());
}
