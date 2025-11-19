// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_smart_workspace.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "chrome/browser/ui/bookmarks/bookmark_task_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"

namespace {

class BookmarkSmartWorkspaceTest : public views::ViewsTestBase {
 protected:
  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create bookmark model
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();

    // Create managers
    bookmark_manager_ = std::make_unique<BookmarkManager>(
        bookmark_model_.get(), nullptr);
    task_manager_ = std::make_unique<BookmarkTaskManager>(
        bookmark_model_.get());

    // Create test bookmarks
    const bookmarks::BookmarkNode* bookmark_bar =
        bookmark_model_->bookmark_bar_node();

    test_bookmark_github_ = bookmark_model_->AddURL(
        bookmark_bar, 0, u"React Hooks Guide",
        GURL("https://github.com/facebook/react/docs/hooks"));

    test_bookmark_shopping_ = bookmark_model_->AddURL(
        bookmark_bar, 1, u"Running Shoes",
        GURL("https://www.amazon.com/running-shoes"));

    test_bookmark_tutorial_ = bookmark_model_->AddURL(
        bookmark_bar, 2, u"Learn TypeScript",
        GURL("https://www.typescriptlang.org/docs/handbook"));

    test_bookmark_article_ = bookmark_model_->AddURL(
        bookmark_bar, 3, u"Introduction to AI",
        GURL("https://medium.com/ai-intro-article"));

    // Add some with task metadata
    TaskMetadata github_task;
    github_task.type = TaskType::kLearn;
    github_task.priority = TaskPriority::kHigh;
    github_task.status = TaskStatus::kInProgress;
    github_task.due_date = base::Time::Now() + base::Days(1);
    task_manager_->SetTaskMetadata(test_bookmark_github_, github_task);

    TaskMetadata shopping_task;
    shopping_task.type = TaskType::kBuy;
    shopping_task.priority = TaskPriority::kMedium;
    shopping_task.status = TaskStatus::kTodo;
    task_manager_->SetTaskMetadata(test_bookmark_shopping_, shopping_task);
  }

  void TearDown() override {
    context_engine_.reset();
    quick_capture_.reset();
    task_card_.reset();
    task_manager_.reset();
    bookmark_manager_.reset();
    bookmark_model_.reset();
    ViewsTestBase::TearDown();
  }

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<BookmarkManager> bookmark_manager_;
  std::unique_ptr<BookmarkTaskManager> task_manager_;

  std::unique_ptr<ContextEngine> context_engine_;
  std::unique_ptr<QuickCaptureBar> quick_capture_;
  std::unique_ptr<VisualTaskCard> task_card_;

  raw_ptr<const bookmarks::BookmarkNode> test_bookmark_github_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> test_bookmark_shopping_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> test_bookmark_tutorial_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> test_bookmark_article_ = nullptr;

  base::test::TaskEnvironment task_environment_;
};

// ===== BrowsingContext Tests =====

TEST_F(BookmarkSmartWorkspaceTest, ActivityDetection_GitHub) {
  auto activity = BrowsingContext::DetectActivity(
      u"https://github.com/facebook/react",
      u"React Repository");

  EXPECT_EQ(BrowsingContext::ActivityType::kDeveloping, activity);
}

TEST_F(BookmarkSmartWorkspaceTest, ActivityDetection_Shopping) {
  auto activity = BrowsingContext::DetectActivity(
      u"https://www.amazon.com/products",
      u"Buy Running Shoes");

  EXPECT_EQ(BrowsingContext::ActivityType::kShopping, activity);
}

TEST_F(BookmarkSmartWorkspaceTest, ActivityDetection_Learning) {
  auto activity = BrowsingContext::DetectActivity(
      u"https://www.coursera.org/learn/machine-learning",
      u"Machine Learning Course");

  EXPECT_EQ(BrowsingContext::ActivityType::kLearning, activity);
}

TEST_F(BookmarkSmartWorkspaceTest, ActivityDetection_Reading) {
  auto activity = BrowsingContext::DetectActivity(
      u"https://medium.com/interesting-article",
      u"How to Build Better Software");

  EXPECT_EQ(BrowsingContext::ActivityType::kReading, activity);
}

TEST_F(BookmarkSmartWorkspaceTest, TimeContext_Morning) {
  // This test depends on actual time, so we just verify it returns a value
  auto time_context = BrowsingContext::GetTimeContext();

  EXPECT_TRUE(
      time_context == BrowsingContext::TimeContext::kMorning ||
      time_context == BrowsingContext::TimeContext::kAfternoon ||
      time_context == BrowsingContext::TimeContext::kEvening ||
      time_context == BrowsingContext::TimeContext::kNight ||
      time_context == BrowsingContext::TimeContext::kWeekend ||
      time_context == BrowsingContext::TimeContext::kWorkHours ||
      time_context == BrowsingContext::TimeContext::kOffHours);
}

TEST_F(BookmarkSmartWorkspaceTest, BrowsingContext_IsRelatedTo_SameDomain) {
  BrowsingContext context;
  context.current_url = u"https://github.com/facebook/react/issues";
  context.domain = u"github.com";

  bool related = context.IsRelatedTo(test_bookmark_github_);
  EXPECT_TRUE(related);
}

TEST_F(BookmarkSmartWorkspaceTest, BrowsingContext_IsRelatedTo_DifferentDomain) {
  BrowsingContext context;
  context.current_url = u"https://www.amazon.com/products";
  context.domain = u"amazon.com";

  bool related = context.IsRelatedTo(test_bookmark_github_);
  EXPECT_FALSE(related);
}

TEST_F(BookmarkSmartWorkspaceTest, BrowsingContext_RelevanceScore_SameDomain) {
  BrowsingContext context;
  context.current_url = u"https://github.com/facebook/react";
  context.domain = u"github.com";
  context.activity = BrowsingContext::ActivityType::kDeveloping;
  context.current_keywords = {u"react", u"hooks"};

  int score = context.CalculateRelevanceScore(test_bookmark_github_);

  // Should have high score: domain match (50) + keywords (20 each)
  EXPECT_GT(score, 50);
}

TEST_F(BookmarkSmartWorkspaceTest, BrowsingContext_RelevanceScore_NoMatch) {
  BrowsingContext context;
  context.current_url = u"https://www.example.com/unrelated";
  context.domain = u"example.com";
  context.activity = BrowsingContext::ActivityType::kOther;

  int score = context.CalculateRelevanceScore(test_bookmark_github_);

  // Should have low or zero score
  EXPECT_LT(score, 30);
}

// ===== QuickCaptureBar Tests =====

TEST_F(BookmarkSmartWorkspaceTest, QuickCaptureBar_Creation) {
  quick_capture_ = std::make_unique<QuickCaptureBar>(
      bookmark_manager_.get(), task_manager_.get());

  EXPECT_NE(nullptr, quick_capture_);
}

TEST_F(BookmarkSmartWorkspaceTest, QuickCaptureBar_DetectTaskType_Read) {
  quick_capture_ = std::make_unique<QuickCaptureBar>(
      bookmark_manager_.get(), task_manager_.get());

  TaskType type = quick_capture_->DetectTaskType(u"read this article later");
  EXPECT_EQ(TaskType::kRead, type);
}

TEST_F(BookmarkSmartWorkspaceTest, QuickCaptureBar_DetectTaskType_Watch) {
  quick_capture_ = std::make_unique<QuickCaptureBar>(
      bookmark_manager_.get(), task_manager_.get());

  TaskType type = quick_capture_->DetectTaskType(u"watch tutorial video");
  EXPECT_EQ(TaskType::kWatch, type);
}

TEST_F(BookmarkSmartWorkspaceTest, QuickCaptureBar_DetectTaskType_Buy) {
  quick_capture_ = std::make_unique<QuickCaptureBar>(
      bookmark_manager_.get(), task_manager_.get());

  TaskType type = quick_capture_->DetectTaskType(u"buy running shoes");
  EXPECT_EQ(TaskType::kBuy, type);
}

TEST_F(BookmarkSmartWorkspaceTest, QuickCaptureBar_DetectTaskType_Research) {
  quick_capture_ = std::make_unique<QuickCaptureBar>(
      bookmark_manager_.get(), task_manager_.get());

  TaskType type = quick_capture_->DetectTaskType(u"research machine learning");
  EXPECT_EQ(TaskType::kResearch, type);
}

TEST_F(BookmarkSmartWorkspaceTest, QuickCaptureBar_ParseNaturalLanguage_Simple) {
  quick_capture_ = std::make_unique<QuickCaptureBar>(
      bookmark_manager_.get(), task_manager_.get());

  TaskMetadata metadata;
  quick_capture_->ParseNaturalLanguage(u"read this article", metadata);

  EXPECT_EQ(TaskType::kRead, metadata.type);
  EXPECT_EQ(TaskStatus::kTodo, metadata.status);
  EXPECT_EQ(TimeEstimate::kMedium, metadata.time_estimate);
}

TEST_F(BookmarkSmartWorkspaceTest, QuickCaptureBar_ParseNaturalLanguage_WithPriority) {
  quick_capture_ = std::make_unique<QuickCaptureBar>(
      bookmark_manager_.get(), task_manager_.get());

  TaskMetadata metadata;
  quick_capture_->ParseNaturalLanguage(u"urgent: buy milk", metadata);

  EXPECT_EQ(TaskType::kBuy, metadata.type);
  EXPECT_EQ(TaskPriority::kHigh, metadata.priority);
}

TEST_F(BookmarkSmartWorkspaceTest, QuickCaptureBar_ExtractDueDate_Tomorrow) {
  quick_capture_ = std::make_unique<QuickCaptureBar>(
      bookmark_manager_.get(), task_manager_.get());

  std::optional<base::Time> due_date;
  quick_capture_->ExtractDueDate(u"finish report tomorrow", due_date);

  EXPECT_TRUE(due_date.has_value());

  base::TimeDelta delta = due_date.value() - base::Time::Now();
  EXPECT_GT(delta.InHours(), 20);  // Tomorrow is at least 20 hours away
  EXPECT_LT(delta.InHours(), 30);  // But less than 30 hours
}

TEST_F(BookmarkSmartWorkspaceTest, QuickCaptureBar_ExtractDueDate_Today) {
  quick_capture_ = std::make_unique<QuickCaptureBar>(
      bookmark_manager_.get(), task_manager_.get());

  std::optional<base::Time> due_date;
  quick_capture_->ExtractDueDate(u"do this today", due_date);

  EXPECT_TRUE(due_date.has_value());

  base::TimeDelta delta = due_date.value() - base::Time::Now();
  EXPECT_LT(delta.InHours(), 2);  // Today means soon
}

TEST_F(BookmarkSmartWorkspaceTest, QuickCaptureBar_ShowHide) {
  quick_capture_ = std::make_unique<QuickCaptureBar>(
      bookmark_manager_.get(), task_manager_.get());

  quick_capture_->ShowAndFocus();
  EXPECT_TRUE(quick_capture_->GetVisible());

  quick_capture_->Hide();
  EXPECT_FALSE(quick_capture_->GetVisible());
}

// ===== ContextEngine Tests =====

TEST_F(BookmarkSmartWorkspaceTest, ContextEngine_Creation) {
  context_engine_ = std::make_unique<ContextEngine>(
      bookmark_manager_.get(), task_manager_.get());

  EXPECT_NE(nullptr, context_engine_);
}

TEST_F(BookmarkSmartWorkspaceTest, ContextEngine_GetRelevantItems_GitHub) {
  context_engine_ = std::make_unique<ContextEngine>(
      bookmark_manager_.get(), task_manager_.get());

  BrowsingContext context;
  context.current_url = u"https://github.com/facebook/react";
  context.domain = u"github.com";
  context.activity = BrowsingContext::ActivityType::kDeveloping;

  auto relevant = context_engine_->GetRelevantItems(context, 10);

  // Should find the GitHub bookmark
  EXPECT_GT(relevant.size(), 0u);

  bool found_github = false;
  for (const auto* node : relevant) {
    if (node == test_bookmark_github_) {
      found_github = true;
      break;
    }
  }
  EXPECT_TRUE(found_github);
}

TEST_F(BookmarkSmartWorkspaceTest, ContextEngine_GetRelevantItems_MaxCount) {
  context_engine_ = std::make_unique<ContextEngine>(
      bookmark_manager_.get(), task_manager_.get());

  // Create many bookmarks
  const bookmarks::BookmarkNode* bookmark_bar =
      bookmark_model_->bookmark_bar_node();
  for (int i = 0; i < 20; ++i) {
    bookmark_model_->AddURL(
        bookmark_bar, i + 10,
        u"Test Bookmark " + base::NumberToString16(i),
        GURL("https://github.com/test" + base::NumberToString(i)));
  }

  BrowsingContext context;
  context.current_url = u"https://github.com/repos";
  context.domain = u"github.com";

  auto relevant = context_engine_->GetRelevantItems(context, 5);

  // Should respect max_count
  EXPECT_LE(relevant.size(), 5u);
}

TEST_F(BookmarkSmartWorkspaceTest, ContextEngine_GetRelatedBookmarks_SameDomain) {
  context_engine_ = std::make_unique<ContextEngine>(
      bookmark_manager_.get(), task_manager_.get());

  auto related = context_engine_->GetRelatedBookmarks(
      u"https://github.com/other/repo", 10);

  // Should find GitHub bookmarks
  bool found_github = false;
  for (const auto* node : related) {
    if (node == test_bookmark_github_) {
      found_github = true;
      break;
    }
  }
  EXPECT_TRUE(found_github);
}

TEST_F(BookmarkSmartWorkspaceTest, ContextEngine_IsSameDomain) {
  context_engine_ = std::make_unique<ContextEngine>(
      bookmark_manager_.get(), task_manager_.get());

  bool same = context_engine_->IsSameDomain(
      u"https://github.com/facebook/react",
      u"https://github.com/google/material");
  EXPECT_TRUE(same);

  bool different = context_engine_->IsSameDomain(
      u"https://github.com/repos",
      u"https://amazon.com/products");
  EXPECT_FALSE(different);
}

TEST_F(BookmarkSmartWorkspaceTest, ContextEngine_ExtractKeywords) {
  context_engine_ = std::make_unique<ContextEngine>(
      bookmark_manager_.get(), task_manager_.get());

  auto keywords = context_engine_->ExtractKeywords(
      u"Learn React Hooks for Modern Web Development");

  // Should extract meaningful keywords, filtering stop words
  EXPECT_GT(keywords.size(), 0u);

  // Should have "react" and "hooks"
  bool has_react = std::find(keywords.begin(), keywords.end(), u"react") != keywords.end();
  bool has_hooks = std::find(keywords.begin(), keywords.end(), u"hooks") != keywords.end();

  EXPECT_TRUE(has_react || has_hooks);

  // Should not have stop words like "for"
  bool has_for = std::find(keywords.begin(), keywords.end(), u"for") != keywords.end();
  EXPECT_FALSE(has_for);
}

// ===== VisualTaskCard Tests =====

TEST_F(BookmarkSmartWorkspaceTest, VisualTaskCard_Creation) {
  auto metadata = task_manager_->GetTaskMetadata(test_bookmark_github_);
  ASSERT_TRUE(metadata.has_value());

  task_card_ = std::make_unique<VisualTaskCard>(
      test_bookmark_github_, &metadata.value());

  EXPECT_NE(nullptr, task_card_);
  EXPECT_EQ(test_bookmark_github_, task_card_->bookmark());
}

TEST_F(BookmarkSmartWorkspaceTest, VisualTaskCard_NaturalDescription_Read) {
  TaskMetadata metadata;
  metadata.type = TaskType::kRead;

  task_card_ = std::make_unique<VisualTaskCard>(
      test_bookmark_article_, &metadata);

  std::u16string description = task_card_->GetNaturalDescription();
  EXPECT_EQ(u"📖 Read this article", description);
}

TEST_F(BookmarkSmartWorkspaceTest, VisualTaskCard_NaturalDescription_Watch) {
  TaskMetadata metadata;
  metadata.type = TaskType::kWatch;

  task_card_ = std::make_unique<VisualTaskCard>(
      test_bookmark_tutorial_, &metadata);

  std::u16string description = task_card_->GetNaturalDescription();
  EXPECT_EQ(u"📺 Watch this video", description);
}

TEST_F(BookmarkSmartWorkspaceTest, VisualTaskCard_NaturalDescription_Buy) {
  TaskMetadata metadata;
  metadata.type = TaskType::kBuy;

  task_card_ = std::make_unique<VisualTaskCard>(
      test_bookmark_shopping_, &metadata);

  std::u16string description = task_card_->GetNaturalDescription();
  EXPECT_EQ(u"🛒 Purchase this item", description);
}

TEST_F(BookmarkSmartWorkspaceTest, VisualTaskCard_TimeDisplay_Quick) {
  TaskMetadata metadata;
  metadata.time_estimate = TimeEstimate::kQuick;

  task_card_ = std::make_unique<VisualTaskCard>(
      test_bookmark_article_, &metadata);

  std::u16string time = task_card_->GetTimeDisplay();
  EXPECT_EQ(u"5 min", time);
}

TEST_F(BookmarkSmartWorkspaceTest, VisualTaskCard_TimeDisplay_WithDueDate) {
  TaskMetadata metadata;
  metadata.time_estimate = TimeEstimate::kMedium;
  metadata.due_date = base::Time::Now();  // Due today

  task_card_ = std::make_unique<VisualTaskCard>(
      test_bookmark_article_, &metadata);

  std::u16string time = task_card_->GetTimeDisplay();

  // Should contain time estimate and due date info
  EXPECT_TRUE(time.find(u"30 min") != std::u16string::npos);
  EXPECT_TRUE(time.find(u"Due today") != std::u16string::npos);
}

TEST_F(BookmarkSmartWorkspaceTest, VisualTaskCard_Progress) {
  TaskMetadata metadata;

  task_card_ = std::make_unique<VisualTaskCard>(
      test_bookmark_article_, &metadata);

  task_card_->SetProgress(50);
  // Progress is stored internally and shown on paint
  EXPECT_TRUE(true);  // No crash
}

// ===== Performance Tests =====

TEST_F(BookmarkSmartWorkspaceTest, Performance_ContextEngine_LargeBookmarkSet) {
  // Create 1000 bookmarks
  const bookmarks::BookmarkNode* bookmark_bar =
      bookmark_model_->bookmark_bar_node();

  for (int i = 0; i < 1000; ++i) {
    bookmark_model_->AddURL(
        bookmark_bar, i + 100,
        u"Bookmark " + base::NumberToString16(i),
        GURL("https://example.com/page" + base::NumberToString(i)));
  }

  context_engine_ = std::make_unique<ContextEngine>(
      bookmark_manager_.get(), task_manager_.get());

  BrowsingContext context;
  context.current_url = u"https://example.com/current";
  context.domain = u"example.com";

  base::Time start = base::Time::Now();
  auto relevant = context_engine_->GetRelevantItems(context, 10);
  base::TimeDelta elapsed = base::Time::Now() - start;

  // Should complete in < 200ms even with 1000 bookmarks
  EXPECT_LT(elapsed.InMilliseconds(), 200);
  EXPECT_LE(relevant.size(), 10u);
}

TEST_F(BookmarkSmartWorkspaceTest, Performance_QuickCapture_MultipleCaptures) {
  quick_capture_ = std::make_unique<QuickCaptureBar>(
      bookmark_manager_.get(), task_manager_.get());

  base::Time start = base::Time::Now();

  for (int i = 0; i < 100; ++i) {
    TaskMetadata metadata;
    quick_capture_->ParseNaturalLanguage(
        u"read article " + base::NumberToString16(i), metadata);
  }

  base::TimeDelta elapsed = base::Time::Now() - start;

  // Should parse 100 tasks in < 100ms (< 1ms per task)
  EXPECT_LT(elapsed.InMilliseconds(), 100);
}

// ===== Edge Cases & Error Handling =====

TEST_F(BookmarkSmartWorkspaceTest, EdgeCase_NullBookmark_RelevanceScore) {
  BrowsingContext context;
  context.current_url = u"https://example.com";

  int score = context.CalculateRelevanceScore(nullptr);
  EXPECT_EQ(0, score);
}

TEST_F(BookmarkSmartWorkspaceTest, EdgeCase_EmptyContext) {
  context_engine_ = std::make_unique<ContextEngine>(
      bookmark_manager_.get(), task_manager_.get());

  BrowsingContext context;  // Empty context

  auto relevant = context_engine_->GetRelevantItems(context, 10);
  // Should handle gracefully, may return empty or low-relevance items
  EXPECT_TRUE(true);  // No crash
}

TEST_F(BookmarkSmartWorkspaceTest, EdgeCase_EmptyNaturalLanguage) {
  quick_capture_ = std::make_unique<QuickCaptureBar>(
      bookmark_manager_.get(), task_manager_.get());

  TaskMetadata metadata;
  quick_capture_->ParseNaturalLanguage(u"", metadata);

  // Should have defaults
  EXPECT_EQ(TaskStatus::kTodo, metadata.status);
}

TEST_F(BookmarkSmartWorkspaceTest, EdgeCase_VeryLongText) {
  quick_capture_ = std::make_unique<QuickCaptureBar>(
      bookmark_manager_.get(), task_manager_.get());

  std::u16string long_text(1000, 'a');  // 1000 character string

  TaskMetadata metadata;
  quick_capture_->ParseNaturalLanguage(long_text, metadata);

  // Should handle without crashing
  EXPECT_TRUE(true);
}

TEST_F(BookmarkSmartWorkspaceTest, EdgeCase_SpecialCharacters) {
  quick_capture_ = std::make_unique<QuickCaptureBar>(
      bookmark_manager_.get(), task_manager_.get());

  TaskMetadata metadata;
  quick_capture_->ParseNaturalLanguage(
      u"Buy 🎁 gifts & things @home #important!!!", metadata);

  EXPECT_EQ(TaskType::kBuy, metadata.type);
}

// ===== Integration Tests =====

TEST_F(BookmarkSmartWorkspaceTest, Integration_ContextEngine_WithTaskMetadata) {
  context_engine_ = std::make_unique<ContextEngine>(
      bookmark_manager_.get(), task_manager_.get());

  BrowsingContext context;
  context.current_url = u"https://github.com/repos";
  context.domain = u"github.com";
  context.activity = BrowsingContext::ActivityType::kDeveloping;

  auto relevant = context_engine_->GetRelevantItems(context, 10);

  // GitHub bookmark should rank highly because:
  // 1. Same domain
  // 2. InProgress status (boosted)
  // 3. High priority (boosted)
  // 4. Due tomorrow (boosted)

  EXPECT_GT(relevant.size(), 0u);

  // First result should be the GitHub bookmark
  if (relevant.size() > 0) {
    EXPECT_EQ(test_bookmark_github_, relevant[0]);
  }
}

TEST_F(BookmarkSmartWorkspaceTest, Integration_QuickCapture_CreatesTask) {
  quick_capture_ = std::make_unique<QuickCaptureBar>(
      bookmark_manager_.get(), task_manager_.get());

  size_t initial_count = bookmark_model_->bookmark_bar_node()->children().size();

  quick_capture_->CaptureFromText(u"Read React tutorial");

  size_t final_count = bookmark_model_->bookmark_bar_node()->children().size();

  // Should have created a new bookmark
  EXPECT_EQ(initial_count + 1, final_count);
}

}  // namespace
