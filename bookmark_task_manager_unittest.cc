// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_task_manager.h"

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

class BookmarkTaskManagerTest : public testing::Test {
 public:
  void SetUp() override {
    bookmark_client_ = std::make_unique<bookmarks::TestBookmarkClient>();
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel(
        std::move(bookmark_client_));
    manager_ = std::make_unique<BookmarkManager>(bookmark_model_.get());
    task_manager_ = std::make_unique<BookmarkTaskManager>(bookmark_model_.get(),
                                                          manager_.get());
  }

  void TearDown() override {
    task_manager_.reset();
    manager_.reset();
    bookmark_model_.reset();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::TestBookmarkClient> bookmark_client_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<BookmarkManager> manager_;
  std::unique_ptr<BookmarkTaskManager> task_manager_;
};

// ===== Task Metadata Tests =====

TEST_F(BookmarkTaskManagerTest, SetAndGetTaskMetadata) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* task =
      bookmark_model_->AddURL(bar, 0, u"Test Task", GURL("https://example.com"));

  TaskMetadata metadata;
  metadata.status = TaskStatus::kTodo;
  metadata.priority = TaskPriority::kHigh;
  metadata.type = TaskType::kRead;

  task_manager_->SetTaskMetadata(task, metadata);

  auto retrieved = task_manager_->GetTaskMetadata(task);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(TaskStatus::kTodo, retrieved->status);
  EXPECT_EQ(TaskPriority::kHigh, retrieved->priority);
  EXPECT_EQ(TaskType::kRead, retrieved->type);
}

TEST_F(BookmarkTaskManagerTest, RemoveTaskMetadata) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* task =
      bookmark_model_->AddURL(bar, 0, u"Test", GURL("https://example.com"));

  TaskMetadata metadata;
  task_manager_->SetTaskMetadata(task, metadata);

  ASSERT_TRUE(task_manager_->GetTaskMetadata(task).has_value());

  task_manager_->RemoveTaskMetadata(task);

  EXPECT_FALSE(task_manager_->GetTaskMetadata(task).has_value());
}

// ===== Task Lifecycle Tests =====

TEST_F(BookmarkTaskManagerTest, StartTask) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* task =
      bookmark_model_->AddURL(bar, 0, u"Test", GURL("https://example.com"));

  TaskMetadata metadata;
  metadata.status = TaskStatus::kTodo;
  task_manager_->SetTaskMetadata(task, metadata);

  task_manager_->StartTask(task);

  auto retrieved = task_manager_->GetTaskMetadata(task);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(TaskStatus::kInProgress, retrieved->status);
  EXPECT_TRUE(retrieved->start_date.has_value());
  EXPECT_FALSE(retrieved->last_worked_on.is_null());
}

TEST_F(BookmarkTaskManagerTest, CompleteTask) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* task =
      bookmark_model_->AddURL(bar, 0, u"Test", GURL("https://example.com"));

  TaskMetadata metadata;
  metadata.status = TaskStatus::kInProgress;
  metadata.time_estimate = TimeEstimate::kMedium;
  metadata.priority = TaskPriority::kHigh;
  task_manager_->SetTaskMetadata(task, metadata);

  int initial_points = task_manager_->GetProfile().total_points_earned;

  task_manager_->CompleteTask(task);

  auto retrieved = task_manager_->GetTaskMetadata(task);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(TaskStatus::kDone, retrieved->status);
  EXPECT_TRUE(retrieved->completed_date.has_value());
  EXPECT_EQ(100, retrieved->progress_percent);

  // Check points were awarded
  EXPECT_GT(task_manager_->GetProfile().total_points_earned, initial_points);
  EXPECT_EQ(1, task_manager_->GetProfile().total_tasks_completed);
}

TEST_F(BookmarkTaskManagerTest, SnoozeTask) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* task =
      bookmark_model_->AddURL(bar, 0, u"Test", GURL("https://example.com"));

  TaskMetadata metadata;
  metadata.status = TaskStatus::kTodo;
  task_manager_->SetTaskMetadata(task, metadata);

  task_manager_->SnoozeTask(task, base::Hours(2));

  auto retrieved = task_manager_->GetTaskMetadata(task);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(TaskStatus::kSnoozed, retrieved->status);
  EXPECT_TRUE(retrieved->snoozed_until.has_value());
}

TEST_F(BookmarkTaskManagerTest, UnsnoozeTask) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* task =
      bookmark_model_->AddURL(bar, 0, u"Test", GURL("https://example.com"));

  TaskMetadata metadata;
  metadata.status = TaskStatus::kSnoozed;
  metadata.snoozed_until = base::Time::Now() + base::Hours(1);
  task_manager_->SetTaskMetadata(task, metadata);

  task_manager_->UnsnoozeTask(task);

  auto retrieved = task_manager_->GetTaskMetadata(task);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(TaskStatus::kTodo, retrieved->status);
  EXPECT_FALSE(retrieved->snoozed_until.has_value());
}

TEST_F(BookmarkTaskManagerTest, ArchiveTask) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* task =
      bookmark_model_->AddURL(bar, 0, u"Test", GURL("https://example.com"));

  TaskMetadata metadata;
  metadata.status = TaskStatus::kDone;
  task_manager_->SetTaskMetadata(task, metadata);

  task_manager_->ArchiveTask(task);

  auto retrieved = task_manager_->GetTaskMetadata(task);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(TaskStatus::kArchived, retrieved->status);
}

// ===== Task Query Tests =====

TEST_F(BookmarkTaskManagerTest, GetTasksByStatus) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  // Create tasks with different statuses
  const bookmarks::BookmarkNode* todo_task =
      bookmark_model_->AddURL(bar, 0, u"Todo", GURL("https://example.com/1"));
  const bookmarks::BookmarkNode* progress_task =
      bookmark_model_->AddURL(bar, 1, u"Progress", GURL("https://example.com/2"));
  const bookmarks::BookmarkNode* done_task =
      bookmark_model_->AddURL(bar, 2, u"Done", GURL("https://example.com/3"));

  TaskMetadata todo_meta;
  todo_meta.status = TaskStatus::kTodo;
  task_manager_->SetTaskMetadata(todo_task, todo_meta);

  TaskMetadata progress_meta;
  progress_meta.status = TaskStatus::kInProgress;
  task_manager_->SetTaskMetadata(progress_task, progress_meta);

  TaskMetadata done_meta;
  done_meta.status = TaskStatus::kDone;
  task_manager_->SetTaskMetadata(done_task, done_meta);

  auto todo_tasks = task_manager_->GetTasksByStatus(TaskStatus::kTodo);
  EXPECT_EQ(1u, todo_tasks.size());
  EXPECT_EQ(todo_task, todo_tasks[0]);

  auto progress_tasks = task_manager_->GetTasksByStatus(TaskStatus::kInProgress);
  EXPECT_EQ(1u, progress_tasks.size());

  auto done_tasks = task_manager_->GetTasksByStatus(TaskStatus::kDone);
  EXPECT_EQ(1u, done_tasks.size());
}

TEST_F(BookmarkTaskManagerTest, GetTasksByPriority) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  const bookmarks::BookmarkNode* high_task =
      bookmark_model_->AddURL(bar, 0, u"High", GURL("https://example.com/1"));
  const bookmarks::BookmarkNode* low_task =
      bookmark_model_->AddURL(bar, 1, u"Low", GURL("https://example.com/2"));

  TaskMetadata high_meta;
  high_meta.priority = TaskPriority::kHigh;
  task_manager_->SetTaskMetadata(high_task, high_meta);

  TaskMetadata low_meta;
  low_meta.priority = TaskPriority::kLow;
  task_manager_->SetTaskMetadata(low_task, low_meta);

  auto high_tasks = task_manager_->GetTasksByPriority(TaskPriority::kHigh);
  EXPECT_EQ(1u, high_tasks.size());
  EXPECT_EQ(high_task, high_tasks[0]);
}

TEST_F(BookmarkTaskManagerTest, GetDueTasks) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  const bookmarks::BookmarkNode* due_task =
      bookmark_model_->AddURL(bar, 0, u"Due", GURL("https://example.com"));

  TaskMetadata metadata;
  metadata.due_date = base::Time::Now() - base::Hours(1);  // Overdue
  metadata.status = TaskStatus::kTodo;
  task_manager_->SetTaskMetadata(due_task, metadata);

  auto due_tasks = task_manager_->GetDueTasks();
  EXPECT_EQ(1u, due_tasks.size());
  EXPECT_EQ(due_task, due_tasks[0]);
}

TEST_F(BookmarkTaskManagerTest, GetOverdueTasks) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  const bookmarks::BookmarkNode* overdue_task =
      bookmark_model_->AddURL(bar, 0, u"Overdue", GURL("https://example.com"));

  TaskMetadata metadata;
  metadata.due_date = base::Time::Now() - base::Days(2);  // 2 days overdue
  metadata.status = TaskStatus::kTodo;
  task_manager_->SetTaskMetadata(overdue_task, metadata);

  auto overdue_tasks = task_manager_->GetOverdueTasks();
  EXPECT_EQ(1u, overdue_tasks.size());
}

// ===== Tab-to-Task Conversion Tests =====

TEST_F(BookmarkTaskManagerTest, ConvertTabToTask) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* task =
      bookmark_model_->AddURL(bar, 0, u"Article",
                             GURL("https://blog.example.com/article"));

  task_manager_->ConvertTabToTask(task, 5);  // Tab index 5

  auto metadata = task_manager_->GetTaskMetadata(task);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_TRUE(metadata->opened_as_tab);
  EXPECT_EQ(5, metadata->tab_index);
  EXPECT_EQ(TaskType::kRead, metadata->type);  // Auto-detected
}

TEST_F(BookmarkTaskManagerTest, DetectTaskTypeFromURL) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  // Test video detection
  const bookmarks::BookmarkNode* video_task =
      bookmark_model_->AddURL(bar, 0, u"Tutorial",
                             GURL("https://youtube.com/watch?v=123"));
  EXPECT_EQ(TaskType::kWatch, task_manager_->DetectTaskType(video_task));

  // Test learning detection
  const bookmarks::BookmarkNode* course_task =
      bookmark_model_->AddURL(bar, 1, u"React Course",
                             GURL("https://example.com/course/react"));
  EXPECT_EQ(TaskType::kLearn, task_manager_->DetectTaskType(course_task));

  // Test documentation detection
  const bookmarks::BookmarkNode* docs_task =
      bookmark_model_->AddURL(bar, 2, u"API Docs",
                             GURL("https://docs.example.com/api"));
  EXPECT_EQ(TaskType::kRead, task_manager_->DetectTaskType(docs_task));

  // Test shopping detection
  const bookmarks::BookmarkNode* shop_task =
      bookmark_model_->AddURL(bar, 3, u"Buy Laptop",
                             GURL("https://amazon.com/product/123"));
  EXPECT_EQ(TaskType::kBuy, task_manager_->DetectTaskType(shop_task));
}

TEST_F(BookmarkTaskManagerTest, EstimateTaskDuration) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  // Videos are typically long
  const bookmarks::BookmarkNode* video =
      bookmark_model_->AddURL(bar, 0, u"Video",
                             GURL("https://youtube.com/watch"));
  EXPECT_EQ(TimeEstimate::kLong, task_manager_->EstimateTaskDuration(video));

  // Shopping is typically short
  const bookmarks::BookmarkNode* shop =
      bookmark_model_->AddURL(bar, 1, u"Shop",
                             GURL("https://amazon.com/product"));
  EXPECT_EQ(TimeEstimate::kShort, task_manager_->EstimateTaskDuration(shop));

  // Learning is very long
  const bookmarks::BookmarkNode* course =
      bookmark_model_->AddURL(bar, 2, u"Course",
                             GURL("https://example.com/course/react"));
  EXPECT_EQ(TimeEstimate::kVeryLong, task_manager_->EstimateTaskDuration(course));
}

// ===== Gamification Tests =====

TEST_F(BookmarkTaskManagerTest, AwardPoints) {
  int initial_points = task_manager_->GetProfile().total_points_earned;

  task_manager_->AwardPoints(100, u"Test");

  EXPECT_EQ(initial_points + 100,
           task_manager_->GetProfile().total_points_earned);
  EXPECT_EQ(100, task_manager_->GetProfile().experience_points);
}

TEST_F(BookmarkTaskManagerTest, LevelUp) {
  // Award enough points to level up
  task_manager_->AwardPoints(150, u"Test");  // Exceeds initial 100 pts threshold

  EXPECT_EQ(2, task_manager_->GetProfile().level);
  EXPECT_EQ(50, task_manager_->GetProfile().experience_points);  // Overflow
  EXPECT_GT(task_manager_->GetProfile().points_to_next_level, 100);  // Increased
}

TEST_F(BookmarkTaskManagerTest, StreakTracking) {
  // Complete first task
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* task1 =
      bookmark_model_->AddURL(bar, 0, u"Task 1", GURL("https://example.com/1"));

  TaskMetadata metadata;
  metadata.status = TaskStatus::kInProgress;
  task_manager_->SetTaskMetadata(task1, metadata);

  task_manager_->CompleteTask(task1);

  EXPECT_EQ(1, task_manager_->GetProfile().current_streak);
  EXPECT_EQ(1, task_manager_->GetProfile().longest_streak);
}

TEST_F(BookmarkTaskManagerTest, FirstTaskAchievement) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* task =
      bookmark_model_->AddURL(bar, 0, u"First Task", GURL("https://example.com"));

  TaskMetadata metadata;
  metadata.status = TaskStatus::kInProgress;
  task_manager_->SetTaskMetadata(task, metadata);

  task_manager_->CompleteTask(task);

  // Check achievement was unlocked
  const auto& profile = task_manager_->GetProfile();
  EXPECT_EQ(1, profile.achievements_unlocked);

  // Find First Task achievement
  bool found_first_task = false;
  for (const auto& achievement : profile.achievements) {
    if (achievement.type == AchievementType::kFirstTask &&
        achievement.unlocked) {
      found_first_task = true;
      break;
    }
  }
  EXPECT_TRUE(found_first_task);
}

TEST_F(BookmarkTaskManagerTest, CalculateTaskPoints) {
  TaskMetadata quick_low;
  quick_low.time_estimate = TimeEstimate::kQuick;
  quick_low.priority = TaskPriority::kLow;
  EXPECT_EQ(10, CalculateTaskPoints(quick_low));  // 10 * 1.0

  TaskMetadata medium_high;
  medium_high.time_estimate = TimeEstimate::kMedium;
  medium_high.priority = TaskPriority::kHigh;
  EXPECT_EQ(100, CalculateTaskPoints(medium_high));  // 50 * 2.0

  TaskMetadata long_critical;
  long_critical.time_estimate = TimeEstimate::kVeryLong;
  long_critical.priority = TaskPriority::kCritical;
  EXPECT_EQ(600, CalculateTaskPoints(long_critical));  // 200 * 3.0
}

// ===== Kanban View Tests =====

class BookmarkKanbanViewTest : public BookmarkTaskManagerTest {
 public:
  void SetUp() override {
    BookmarkTaskManagerTest::SetUp();
    kanban_view_ = std::make_unique<BookmarkKanbanView>(task_manager_.get());
  }

 protected:
  std::unique_ptr<BookmarkKanbanView> kanban_view_;
};

TEST_F(BookmarkKanbanViewTest, GenerateBoard) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  // Create tasks in different statuses
  const bookmarks::BookmarkNode* todo =
      bookmark_model_->AddURL(bar, 0, u"Todo", GURL("https://example.com/1"));
  const bookmarks::BookmarkNode* progress =
      bookmark_model_->AddURL(bar, 1, u"Progress", GURL("https://example.com/2"));

  TaskMetadata todo_meta;
  todo_meta.status = TaskStatus::kTodo;
  task_manager_->SetTaskMetadata(todo, todo_meta);

  TaskMetadata progress_meta;
  progress_meta.status = TaskStatus::kInProgress;
  task_manager_->SetTaskMetadata(progress, progress_meta);

  KanbanBoard board = kanban_view_->GenerateBoard();

  EXPECT_EQ(4u, board.columns.size());  // Todo, InProgress, Done, Snoozed
  EXPECT_EQ(2, board.total_tasks);
  EXPECT_EQ(0, board.completion_percentage);  // No done tasks
}

TEST_F(BookmarkKanbanViewTest, MoveTaskToColumn) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* task =
      bookmark_model_->AddURL(bar, 0, u"Task", GURL("https://example.com"));

  TaskMetadata metadata;
  metadata.status = TaskStatus::kTodo;
  task_manager_->SetTaskMetadata(task, metadata);

  kanban_view_->MoveTaskToColumn(task, TaskStatus::kInProgress);

  auto updated = task_manager_->GetTaskMetadata(task);
  ASSERT_TRUE(updated.has_value());
  EXPECT_EQ(TaskStatus::kInProgress, updated->status);
}

TEST_F(BookmarkKanbanViewTest, FilterByPriority) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  const bookmarks::BookmarkNode* high =
      bookmark_model_->AddURL(bar, 0, u"High", GURL("https://example.com/1"));
  const bookmarks::BookmarkNode* low =
      bookmark_model_->AddURL(bar, 1, u"Low", GURL("https://example.com/2"));

  TaskMetadata high_meta;
  high_meta.priority = TaskPriority::kHigh;
  high_meta.status = TaskStatus::kTodo;
  task_manager_->SetTaskMetadata(high, high_meta);

  TaskMetadata low_meta;
  low_meta.priority = TaskPriority::kLow;
  low_meta.status = TaskStatus::kTodo;
  task_manager_->SetTaskMetadata(low, low_meta);

  kanban_view_->SetPriorityFilter(TaskPriority::kHigh);

  KanbanBoard board = kanban_view_->GenerateBoard();
  EXPECT_EQ(1, board.total_tasks);  // Only high priority shown
}

TEST_F(BookmarkKanbanViewTest, FilterByType) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();

  const bookmarks::BookmarkNode* read =
      bookmark_model_->AddURL(bar, 0, u"Article", GURL("https://example.com/1"));
  const bookmarks::BookmarkNode* watch =
      bookmark_model_->AddURL(bar, 1, u"Video", GURL("https://example.com/2"));

  TaskMetadata read_meta;
  read_meta.type = TaskType::kRead;
  read_meta.status = TaskStatus::kTodo;
  task_manager_->SetTaskMetadata(read, read_meta);

  TaskMetadata watch_meta;
  watch_meta.type = TaskType::kWatch;
  watch_meta.status = TaskStatus::kTodo;
  task_manager_->SetTaskMetadata(watch, watch_meta);

  kanban_view_->SetTypeFilter(TaskType::kRead);

  KanbanBoard board = kanban_view_->GenerateBoard();
  EXPECT_EQ(1, board.total_tasks);  // Only read tasks shown
}

// ===== Focus Mode Tests =====

class BookmarkFocusModeTest : public BookmarkTaskManagerTest {
 public:
  void SetUp() override {
    BookmarkTaskManagerTest::SetUp();
    focus_mode_ = std::make_unique<BookmarkFocusMode>(task_manager_.get());
  }

 protected:
  std::unique_ptr<BookmarkFocusMode> focus_mode_;
};

TEST_F(BookmarkFocusModeTest, StartFocusSession) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* task =
      bookmark_model_->AddURL(bar, 0, u"Focus Task", GURL("https://example.com"));

  TaskMetadata metadata;
  metadata.status = TaskStatus::kTodo;
  task_manager_->SetTaskMetadata(task, metadata);

  focus_mode_->StartFocusSession(task, base::Minutes(25));

  EXPECT_TRUE(focus_mode_->IsSessionActive());
  ASSERT_NE(nullptr, focus_mode_->GetCurrentSession());
  EXPECT_EQ(task, focus_mode_->GetCurrentSession()->task);
  EXPECT_EQ(base::Minutes(25), focus_mode_->GetCurrentSession()->duration);
}

TEST_F(BookmarkFocusModeTest, EndFocusSession) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* task =
      bookmark_model_->AddURL(bar, 0, u"Focus Task", GURL("https://example.com"));

  TaskMetadata metadata;
  metadata.status = TaskStatus::kTodo;
  task_manager_->SetTaskMetadata(task, metadata);

  focus_mode_->StartFocusSession(task);
  focus_mode_->EndFocusSession();

  EXPECT_FALSE(focus_mode_->IsSessionActive());
  EXPECT_EQ(nullptr, focus_mode_->GetCurrentSession());

  // Check task metadata was updated with time spent
  auto updated = task_manager_->GetTaskMetadata(task);
  ASSERT_TRUE(updated.has_value());
  EXPECT_FALSE(updated->last_worked_on.is_null());
}

TEST_F(BookmarkFocusModeTest, PauseAndResumeSession) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* task =
      bookmark_model_->AddURL(bar, 0, u"Task", GURL("https://example.com"));

  TaskMetadata metadata;
  task_manager_->SetTaskMetadata(task, metadata);

  focus_mode_->StartFocusSession(task);
  EXPECT_TRUE(focus_mode_->IsSessionActive());

  focus_mode_->PauseSession();
  EXPECT_FALSE(focus_mode_->IsSessionActive());

  focus_mode_->ResumeSession();
  EXPECT_TRUE(focus_mode_->IsSessionActive());
}

TEST_F(BookmarkFocusModeTest, TakeBreak) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* task =
      bookmark_model_->AddURL(bar, 0, u"Task", GURL("https://example.com"));

  TaskMetadata metadata;
  task_manager_->SetTaskMetadata(task, metadata);

  focus_mode_->StartFocusSession(task);

  int initial_breaks = focus_mode_->GetCurrentSession()->breaks_taken;
  focus_mode_->TakeBreak(base::Minutes(5));

  EXPECT_EQ(initial_breaks + 1, focus_mode_->GetCurrentSession()->breaks_taken);
  EXPECT_FALSE(focus_mode_->IsSessionActive());  // Paused during break
}

// ===== Task Scheduler Tests =====

class BookmarkTaskSchedulerTest : public BookmarkTaskManagerTest {
 public:
  void SetUp() override {
    BookmarkTaskManagerTest::SetUp();
    scheduler_ = std::make_unique<BookmarkTaskScheduler>(task_manager_.get());
  }

 protected:
  std::unique_ptr<BookmarkTaskScheduler> scheduler_;
};

TEST_F(BookmarkTaskSchedulerTest, ScheduleTask) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* task =
      bookmark_model_->AddURL(bar, 0, u"Scheduled", GURL("https://example.com"));

  base::Time start = base::Time::Now() + base::Days(1);
  base::Time due = base::Time::Now() + base::Days(7);

  scheduler_->ScheduleTask(task, start, due);

  auto metadata = task_manager_->GetTaskMetadata(task);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_EQ(start, metadata->start_date);
  EXPECT_EQ(due, metadata->due_date);
}

TEST_F(BookmarkTaskSchedulerTest, SetReminder) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* task =
      bookmark_model_->AddURL(bar, 0, u"Task", GURL("https://example.com"));

  base::Time reminder_time = base::Time::Now() + base::Hours(2);
  scheduler_->SetReminder(task, reminder_time, u"Time to start!");

  auto reminders = scheduler_->GetPendingReminders();
  EXPECT_EQ(0u, reminders.size());  // Not due yet
}

TEST_F(BookmarkTaskSchedulerTest, MakeRecurring) {
  const bookmarks::BookmarkNode* bar = bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* task =
      bookmark_model_->AddURL(bar, 0, u"Weekly Task", GURL("https://example.com"));

  scheduler_->MakeRecurring(task, base::Days(7));

  auto metadata = task_manager_->GetTaskMetadata(task);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_TRUE(metadata->is_recurring);
  EXPECT_EQ(base::Days(7), metadata->recurrence_interval);
}

// ===== Helper Function Tests =====

TEST_F(BookmarkTaskManagerTest, TaskStatusToString) {
  EXPECT_EQ(u"Todo", TaskStatusToString(TaskStatus::kTodo));
  EXPECT_EQ(u"In Progress", TaskStatusToString(TaskStatus::kInProgress));
  EXPECT_EQ(u"Done", TaskStatusToString(TaskStatus::kDone));
  EXPECT_EQ(u"Snoozed", TaskStatusToString(TaskStatus::kSnoozed));
  EXPECT_EQ(u"Archived", TaskStatusToString(TaskStatus::kArchived));
}

TEST_F(BookmarkTaskManagerTest, TaskPriorityToString) {
  EXPECT_EQ(u"Low", TaskPriorityToString(TaskPriority::kLow));
  EXPECT_EQ(u"Medium", TaskPriorityToString(TaskPriority::kMedium));
  EXPECT_EQ(u"High", TaskPriorityToString(TaskPriority::kHigh));
  EXPECT_EQ(u"Critical", TaskPriorityToString(TaskPriority::kCritical));
}

TEST_F(BookmarkTaskManagerTest, TaskTypeToString) {
  EXPECT_EQ(u"Read", TaskTypeToString(TaskType::kRead));
  EXPECT_EQ(u"Watch", TaskTypeToString(TaskType::kWatch));
  EXPECT_EQ(u"Learn", TaskTypeToString(TaskType::kLearn));
  EXPECT_EQ(u"Work", TaskTypeToString(TaskType::kWork));
}

TEST_F(BookmarkTaskManagerTest, GetAchievementInfo) {
  Achievement first_task = GetAchievementInfo(AchievementType::kFirstTask);
  EXPECT_EQ(u"First Steps", first_task.title);
  EXPECT_EQ(50, first_task.points);

  Achievement task_master = GetAchievementInfo(AchievementType::kTaskMaster);
  EXPECT_EQ(u"Task Master", task_master.title);
  EXPECT_EQ(500, task_master.points);

  Achievement year_streak = GetAchievementInfo(AchievementType::kYearStreak);
  EXPECT_EQ(u"Year Legend", year_streak.title);
  EXPECT_EQ(10000, year_streak.points);
}
