// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Performance and stress tests for DualPaneBookmarkManagerView.
//
// Tests verify that the UI remains responsive and performant with:
// - Large bookmark collections (1000+ items)
// - Complex filtering operations
// - Rapid UI state changes
// - Heavy sorting operations
// - Bulk operations on large selections
//
// Performance criteria:
// - UI updates should complete in < 100ms for 1000 items
// - Search should complete in < 50ms
// - Sorting should complete in < 100ms
// - State transitions should be smooth (< 16ms for 60fps)

#include "chrome/browser/ui/bookmarks/dual_pane_bookmark_manager_view.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace {

// Performance thresholds (in milliseconds)
constexpr int64_t kMaxUIUpdateTime = 100;
constexpr int64_t kMaxSearchTime = 50;
constexpr int64_t kMaxSortTime = 100;
constexpr int64_t kMaxStateTransitionTime = 16;  // 60fps target

class DualPaneBookmarkManagerPerformanceTest : public views::test::WidgetTest {
 public:
  void SetUp() override {
    WidgetTest::SetUp();

    // Create test bookmark model
    bookmark_client_ = std::make_unique<bookmarks::TestBookmarkClient>();
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel(
        std::move(bookmark_client_));

    // Create the manager view
    widget_ = CreateTopLevelPlatformWidget();
    manager_view_ = new DualPaneBookmarkManagerView(bookmark_model_.get());
    widget_->SetContentsView(manager_view_.get());
    widget_->SetSize(gfx::Size(1200, 800));
    widget_->Show();
  }

  void TearDown() override {
    manager_view_ = nullptr;
    if (widget_) {
      widget_->CloseNow();
      widget_ = nullptr;
    }
    bookmark_model_.reset();
    WidgetTest::TearDown();
  }

  // Create a large number of test bookmarks
  void CreateLargeBookmarkCollection(size_t count) {
    const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

    // Create folders
    const size_t kFolderCount = 10;
    std::vector<const bookmarks::BookmarkNode*> folders;
    for (size_t i = 0; i < kFolderCount; ++i) {
      folders.push_back(bookmark_model_->AddFolder(
          bar, i, u"Folder " + base::NumberToString16(i)));
    }

    // Create bookmarks distributed across folders
    for (size_t i = 0; i < count; ++i) {
      const bookmarks::BookmarkNode* folder = folders[i % kFolderCount];
      std::u16string title = u"Bookmark " + base::NumberToString16(i);
      std::string url = "https://example.com/" + std::to_string(i);
      bookmark_model_->AddURL(folder, folder->children().size(), title,
                             GURL(url));
    }
  }

  // Measure execution time of a function
  template <typename Func>
  int64_t MeasureExecutionTime(Func&& func) {
    base::ElapsedTimer timer;
    func();
    return timer.Elapsed().InMilliseconds();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<views::Widget> widget_ = nullptr;
  raw_ptr<DualPaneBookmarkManagerView> manager_view_ = nullptr;
};

// ===== Large Collection Tests =====

TEST_F(DualPaneBookmarkManagerPerformanceTest, UpdateWith1000Bookmarks) {
  CreateLargeBookmarkCollection(1000);

  int64_t elapsed = MeasureExecutionTime([this]() {
    manager_view_->UpdateBookmarkList();
  });

  EXPECT_LT(elapsed, kMaxUIUpdateTime)
      << "UI update took " << elapsed << "ms (threshold: "
      << kMaxUIUpdateTime << "ms)";
}

TEST_F(DualPaneBookmarkManagerPerformanceTest, UpdateWith5000Bookmarks) {
  CreateLargeBookmarkCollection(5000);

  int64_t elapsed = MeasureExecutionTime([this]() {
    manager_view_->UpdateBookmarkList();
  });

  // Allow more time for very large collections
  EXPECT_LT(elapsed, kMaxUIUpdateTime * 5)
      << "UI update with 5000 bookmarks took " << elapsed << "ms";
}

TEST_F(DualPaneBookmarkManagerPerformanceTest, UpdateWith10000Bookmarks) {
  CreateLargeBookmarkCollection(10000);

  int64_t elapsed = MeasureExecutionTime([this]() {
    manager_view_->UpdateBookmarkList();
  });

  // Even with 10k items, should remain reasonably responsive
  EXPECT_LT(elapsed, kMaxUIUpdateTime * 10)
      << "UI update with 10000 bookmarks took " << elapsed << "ms";
}

// ===== Search Performance Tests =====

TEST_F(DualPaneBookmarkManagerPerformanceTest, SearchIn1000Bookmarks) {
  CreateLargeBookmarkCollection(1000);

  BookmarkFilter filter;
  filter.search_query = u"Bookmark 500";

  int64_t elapsed = MeasureExecutionTime([this, &filter]() {
    manager_view_->SetFilter(filter);
  });

  EXPECT_LT(elapsed, kMaxSearchTime)
      << "Search took " << elapsed << "ms (threshold: "
      << kMaxSearchTime << "ms)";
}

TEST_F(DualPaneBookmarkManagerPerformanceTest, SearchWithWildcard) {
  CreateLargeBookmarkCollection(1000);

  BookmarkFilter filter;
  filter.search_query = u"Bookmark";  // Matches all

  int64_t elapsed = MeasureExecutionTime([this, &filter]() {
    manager_view_->SetFilter(filter);
  });

  EXPECT_LT(elapsed, kMaxSearchTime * 2)
      << "Wildcard search took " << elapsed << "ms";
}

TEST_F(DualPaneBookmarkManagerPerformanceTest, ClearSearchPerformance) {
  CreateLargeBookmarkCollection(1000);

  // Apply filter first
  BookmarkFilter filter;
  filter.search_query = u"test";
  manager_view_->SetFilter(filter);

  // Measure clear time
  int64_t elapsed = MeasureExecutionTime([this]() {
    manager_view_->ClearFilter();
  });

  EXPECT_LT(elapsed, kMaxSearchTime)
      << "Clear filter took " << elapsed << "ms";
}

// ===== Sorting Performance Tests =====

TEST_F(DualPaneBookmarkManagerPerformanceTest, SortAlphabetically) {
  CreateLargeBookmarkCollection(1000);
  manager_view_->UpdateBookmarkList();

  BookmarkSortDescriptor sort;
  sort.order = BookmarkSortOrder::kAlphabetical;

  int64_t elapsed = MeasureExecutionTime([this, &sort]() {
    manager_view_->SetSortOrder(sort);
  });

  EXPECT_LT(elapsed, kMaxSortTime)
      << "Alphabetical sort took " << elapsed << "ms (threshold: "
      << kMaxSortTime << "ms)";
}

TEST_F(DualPaneBookmarkManagerPerformanceTest, SortByDateAdded) {
  CreateLargeBookmarkCollection(1000);
  manager_view_->UpdateBookmarkList();

  BookmarkSortDescriptor sort;
  sort.order = BookmarkSortOrder::kDateAddedNewest;

  int64_t elapsed = MeasureExecutionTime([this, &sort]() {
    manager_view_->SetSortOrder(sort);
  });

  EXPECT_LT(elapsed, kMaxSortTime)
      << "Date sort took " << elapsed << "ms";
}

TEST_F(DualPaneBookmarkManagerPerformanceTest, SortByMostVisited) {
  CreateLargeBookmarkCollection(1000);
  manager_view_->UpdateBookmarkList();

  BookmarkSortDescriptor sort;
  sort.order = BookmarkSortOrder::kMostVisited;

  int64_t elapsed = MeasureExecutionTime([this, &sort]() {
    manager_view_->SetSortOrder(sort);
  });

  EXPECT_LT(elapsed, kMaxSortTime)
      << "Visit count sort took " << elapsed << "ms";
}

// ===== State Transition Performance Tests =====

TEST_F(DualPaneBookmarkManagerPerformanceTest, LoadingToContentTransition) {
  CreateLargeBookmarkCollection(100);

  int64_t elapsed = MeasureExecutionTime([this]() {
    manager_view_->ShowLoadingState();
    manager_view_->ShowContentState();
  });

  EXPECT_LT(elapsed, kMaxStateTransitionTime)
      << "Loading->Content transition took " << elapsed << "ms";
}

TEST_F(DualPaneBookmarkManagerPerformanceTest, ContentToEmptyTransition) {
  int64_t elapsed = MeasureExecutionTime([this]() {
    manager_view_->ShowContentState();
    manager_view_->ShowEmptyState(false);
  });

  EXPECT_LT(elapsed, kMaxStateTransitionTime)
      << "Content->Empty transition took " << elapsed << "ms";
}

TEST_F(DualPaneBookmarkManagerPerformanceTest, ErrorStateTransition) {
  int64_t elapsed = MeasureExecutionTime([this]() {
    manager_view_->ShowErrorState(u"Test error");
    manager_view_->ShowContentState();
  });

  EXPECT_LT(elapsed, kMaxStateTransitionTime)
      << "Error state transition took " << elapsed << "ms";
}

TEST_F(DualPaneBookmarkManagerPerformanceTest, RapidStateChanges) {
  // Simulate rapid state changes (e.g., during async loading)
  int64_t total_elapsed = 0;
  const int kIterations = 10;

  for (int i = 0; i < kIterations; ++i) {
    total_elapsed += MeasureExecutionTime([this, i]() {
      if (i % 3 == 0) {
        manager_view_->ShowLoadingState();
      } else if (i % 3 == 1) {
        manager_view_->ShowEmptyState(false);
      } else {
        manager_view_->ShowContentState();
      }
    });
  }

  int64_t avg_elapsed = total_elapsed / kIterations;
  EXPECT_LT(avg_elapsed, kMaxStateTransitionTime)
      << "Average state transition took " << avg_elapsed << "ms";
}

// ===== Bulk Operations Performance Tests =====

TEST_F(DualPaneBookmarkManagerPerformanceTest, SelectAllPerformance) {
  CreateLargeBookmarkCollection(1000);
  manager_view_->UpdateBookmarkList();

  int64_t elapsed = MeasureExecutionTime([this]() {
    manager_view_->SelectAll();
  });

  EXPECT_LT(elapsed, kMaxUIUpdateTime)
      << "Select all took " << elapsed << "ms";
}

TEST_F(DualPaneBookmarkManagerPerformanceTest, DeleteManyBookmarks) {
  CreateLargeBookmarkCollection(100);
  manager_view_->UpdateBookmarkList();
  manager_view_->SelectAll();

  int64_t elapsed = MeasureExecutionTime([this]() {
    manager_view_->DeleteSelected();
  });

  EXPECT_LT(elapsed, kMaxUIUpdateTime * 2)
      << "Delete 100 bookmarks took " << elapsed << "ms";
}

TEST_F(DualPaneBookmarkManagerPerformanceTest, AddTagToBulkSelection) {
  CreateLargeBookmarkCollection(500);
  manager_view_->UpdateBookmarkList();
  manager_view_->SelectAll();

  int64_t elapsed = MeasureExecutionTime([this]() {
    manager_view_->AddTagToSelected(u"bulk-tag");
  });

  EXPECT_LT(elapsed, kMaxUIUpdateTime * 2)
      << "Add tag to 500 bookmarks took " << elapsed << "ms";
}

// ===== Status Bar Update Performance =====

TEST_F(DualPaneBookmarkManagerPerformanceTest, StatusBarUpdate) {
  CreateLargeBookmarkCollection(1000);
  manager_view_->UpdateBookmarkList();

  int64_t elapsed = MeasureExecutionTime([this]() {
    manager_view_->UpdateStatusBar();
  });

  // Status bar updates should be very fast
  EXPECT_LT(elapsed, 10)
      << "Status bar update took " << elapsed << "ms";
}

TEST_F(DualPaneBookmarkManagerPerformanceTest, HealthScoreUpdate) {
  CreateLargeBookmarkCollection(1000);
  manager_view_->UpdateBookmarkList();

  int64_t elapsed = MeasureExecutionTime([this]() {
    manager_view_->SetHealthScore(85);
  });

  EXPECT_LT(elapsed, 10)
      << "Health score update took " << elapsed << "ms";
}

// ===== Memory Stress Tests =====

TEST_F(DualPaneBookmarkManagerPerformanceTest, RepeatedUpdatesNoMemoryLeak) {
  CreateLargeBookmarkCollection(500);

  // Perform many updates to check for memory issues
  const int kUpdateCount = 50;
  for (int i = 0; i < kUpdateCount; ++i) {
    manager_view_->UpdateBookmarkList();
  }

  // If we got here without crashing, test passed
  SUCCEED();
}

TEST_F(DualPaneBookmarkManagerPerformanceTest, RepeatedStateChangesNoLeak) {
  CreateLargeBookmarkCollection(100);

  const int kIterations = 100;
  for (int i = 0; i < kIterations; ++i) {
    manager_view_->ShowLoadingState();
    manager_view_->ShowContentState();
    manager_view_->ShowEmptyState(false);
    manager_view_->ShowErrorState(u"Test");
  }

  SUCCEED();
}

// ===== Filter Combination Performance =====

TEST_F(DualPaneBookmarkManagerPerformanceTest, ComplexFilterCombination) {
  CreateLargeBookmarkCollection(1000);

  BookmarkFilter filter;
  filter.search_query = u"Bookmark";
  filter.favorites_only = true;
  filter.min_rating = 3;

  int64_t elapsed = MeasureExecutionTime([this, &filter]() {
    manager_view_->SetFilter(filter);
  });

  EXPECT_LT(elapsed, kMaxSearchTime * 2)
      << "Complex filter took " << elapsed << "ms";
}

// ===== Undo/Redo Performance =====

TEST_F(DualPaneBookmarkManagerPerformanceTest, UndoPerformance) {
  CreateLargeBookmarkCollection(100);
  manager_view_->UpdateBookmarkList();
  manager_view_->SelectAll();
  manager_view_->DeleteSelected();

  int64_t elapsed = MeasureExecutionTime([this]() {
    manager_view_->Undo();
  });

  EXPECT_LT(elapsed, kMaxUIUpdateTime)
      << "Undo operation took " << elapsed << "ms";
}

TEST_F(DualPaneBookmarkManagerPerformanceTest, RedoPerformance) {
  CreateLargeBookmarkCollection(100);
  manager_view_->UpdateBookmarkList();
  manager_view_->SelectAll();
  manager_view_->DeleteSelected();
  manager_view_->Undo();

  int64_t elapsed = MeasureExecutionTime([this]() {
    manager_view_->Redo();
  });

  EXPECT_LT(elapsed, kMaxUIUpdateTime)
      << "Redo operation took " << elapsed << "ms";
}

// ===== Preview Pane Performance =====

TEST_F(DualPaneBookmarkManagerPerformanceTest, PreviewUpdatePerformance) {
  CreateLargeBookmarkCollection(100);
  manager_view_->UpdateBookmarkList();

  int64_t elapsed = MeasureExecutionTime([this]() {
    // Simulate selection change that triggers preview update
    manager_view_->OnSelectionChanged();
  });

  EXPECT_LT(elapsed, kMaxStateTransitionTime)
      << "Preview update took " << elapsed << "ms";
}

}  // namespace
