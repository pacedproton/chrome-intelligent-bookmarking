// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TASK_MANAGER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TASK_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"

// Task-based bookmark management system that replaces tab hoarding with
// organized, gameified task workflows. Inspired by modern task managers
// like Bluebird Focus, this system provides Kanban boards, progress tracking,
// and achievements to make bookmark management engaging and productive.
//
// Key Features:
// - Convert open tabs to tasks automatically
// - Kanban board view (Todo/In Progress/Done/Snoozed)
// - Gamification (points, streaks, levels, achievements)
// - Focus mode for distraction-free work
// - Task scheduling and reminders
// - Visual progress tracking

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

class BookmarkManager;

// ===== Task Data Model =====

// Task status representing lifecycle states
enum class TaskStatus {
  kTodo,        // Not started
  kInProgress,  // Currently working on
  kDone,        // Completed
  kSnoozed,     // Temporarily hidden
  kArchived,    // Completed and archived
};

// Task priority levels
enum class TaskPriority {
  kLow,
  kMedium,
  kHigh,
  kCritical,
};

// Task type categorization
enum class TaskType {
  kRead,          // Article, blog post, documentation
  kWatch,         // Video, tutorial
  kLearn,         // Course, tutorial series
  kWork,          // Work-related task
  kResearch,      // Research, comparison
  kBuy,           // Shopping, comparison
  kIdea,          // Ideas, inspiration
  kReference,     // Reference material
  kOther,         // Uncategorized
};

// Time estimate for task completion
enum class TimeEstimate {
  kQuick,      // < 5 minutes
  kShort,      // 5-15 minutes
  kMedium,     // 15-30 minutes
  kLong,       // 30-60 minutes
  kVeryLong,   // > 1 hour
};

// Task metadata extending BookmarkMetadata
struct TaskMetadata {
  TaskStatus status = TaskStatus::kTodo;
  TaskPriority priority = TaskPriority::kMedium;
  TaskType type = TaskType::kRead;
  TimeEstimate time_estimate = TimeEstimate::kMedium;

  // Scheduling
  std::optional<base::Time> due_date;
  std::optional<base::Time> start_date;
  std::optional<base::Time> completed_date;
  std::optional<base::Time> snoozed_until;

  // Progress tracking
  int progress_percent = 0;  // 0-100
  base::Time last_worked_on;
  base::TimeDelta total_time_spent;

  // Task-specific fields
  std::u16string notes;
  std::vector<std::u16string> checklist_items;
  std::vector<bool> checklist_completed;

  // Context
  bool opened_as_tab = false;  // Currently open as tab
  int tab_index = -1;          // Tab index if opened

  // Recurrence (for recurring tasks)
  bool is_recurring = false;
  base::TimeDelta recurrence_interval;
};

// ===== Gamification System =====

// Achievement types
enum class AchievementType {
  // Completion achievements
  kFirstTask,              // Complete first task
  kTaskWarrior,           // Complete 10 tasks
  kTaskMaster,            // Complete 100 tasks
  kTaskLegend,            // Complete 1000 tasks

  // Streak achievements
  kWeekStreak,            // 7-day streak
  kMonthStreak,           // 30-day streak
  kYearStreak,            // 365-day streak

  // Speed achievements
  kSpeedDemon,            // Complete 10 tasks in 1 day
  kProductivityBeast,     // Complete 50 tasks in 1 week

  // Organization achievements
  kOrganizer,             // Organize 100 bookmarks
  kCurator,               // Create 10 smart folders
  kArchivist,             // Archive 50 old tasks

  // Special achievements
  kEarlyBird,             // Complete task before 8am
  kNightOwl,              // Complete task after 10pm
  kWeekendWarrior,        // Complete 20 weekend tasks
  kZeroInbox,             // Clear all todo tasks
};

struct Achievement {
  AchievementType type;
  std::u16string title;
  std::u16string description;
  int points;
  base::Time earned_date;
  bool unlocked = false;
};

// User gamification profile
struct GamificationProfile {
  // Level and XP
  int level = 1;
  int experience_points = 0;
  int points_to_next_level = 100;

  // Statistics
  int total_tasks_completed = 0;
  int current_streak = 0;
  int longest_streak = 0;
  base::Time streak_start_date;
  base::Time last_task_completion;

  // Points breakdown
  int total_points_earned = 0;
  std::map<std::u16string, int> points_by_category;

  // Achievements
  std::vector<Achievement> achievements;
  int achievements_unlocked = 0;

  // Daily/weekly stats
  int tasks_today = 0;
  int tasks_this_week = 0;
  int tasks_this_month = 0;
};

// ===== Task Manager =====

class BookmarkTaskManager {
 public:
  BookmarkTaskManager(bookmarks::BookmarkModel* model,
                      BookmarkManager* manager);
  ~BookmarkTaskManager();

  BookmarkTaskManager(const BookmarkTaskManager&) = delete;
  BookmarkTaskManager& operator=(const BookmarkTaskManager&) = delete;

  // Task CRUD operations
  void SetTaskMetadata(const bookmarks::BookmarkNode* bookmark,
                      const TaskMetadata& metadata);
  [[nodiscard]] std::optional<TaskMetadata> GetTaskMetadata(
      const bookmarks::BookmarkNode* bookmark) const;
  void RemoveTaskMetadata(const bookmarks::BookmarkNode* bookmark);

  // Task state transitions
  void StartTask(const bookmarks::BookmarkNode* bookmark);
  void CompleteTask(const bookmarks::BookmarkNode* bookmark);
  void SnoozeTask(const bookmarks::BookmarkNode* bookmark, base::TimeDelta duration);
  void UnsnoozeTask(const bookmarks::BookmarkNode* bookmark);
  void ArchiveTask(const bookmarks::BookmarkNode* bookmark);

  // Task queries
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*> GetTasksByStatus(
      TaskStatus status) const;
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*> GetTasksByPriority(
      TaskPriority priority) const;
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*> GetDueTasks() const;
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*> GetOverdueTasks() const;
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*> GetTasksForToday() const;

  // Tab conversion
  void ConvertTabToTask(const bookmarks::BookmarkNode* bookmark,
                       int tab_index);
  void OpenTaskAsTab(const bookmarks::BookmarkNode* bookmark);
  void CloseTaskTab(const bookmarks::BookmarkNode* bookmark);
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*> GetOpenTasks() const;

  // Bulk operations
  void ConvertAllTabsToTasks();
  void CloseLowPriorityTabs();
  void ReopenTasksAsTabGroup(const std::vector<const bookmarks::BookmarkNode*>& tasks);

  // Auto-categorization
  TaskType DetectTaskType(const bookmarks::BookmarkNode* bookmark) const;
  TimeEstimate EstimateTaskDuration(const bookmarks::BookmarkNode* bookmark) const;

  // Gamification
  GamificationProfile& GetProfile() { return profile_; }
  const GamificationProfile& GetProfile() const { return profile_; }
  void AwardPoints(int points, std::u16string_view category);
  void CheckAchievements();
  void UpdateStreak();

 private:
  // Task metadata storage
  std::map<const bookmarks::BookmarkNode*, TaskMetadata> task_metadata_;

  // Gamification profile
  GamificationProfile profile_;

  // Dependencies
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<BookmarkManager> manager_;

  // Helper methods
  void SaveProfile();
  void LoadProfile();
  void UnlockAchievement(AchievementType type);
  int CalculatePointsForTask(const TaskMetadata& metadata);
  void UpdateLevel();
};

// ===== Kanban Board View =====

struct KanbanColumn {
  TaskStatus status;
  std::u16string title;
  std::vector<const bookmarks::BookmarkNode*> tasks;
  int task_count = 0;
  int total_points = 0;
};

struct KanbanBoard {
  std::vector<KanbanColumn> columns;
  int total_tasks = 0;
  int completion_percentage = 0;

  // View filters
  std::optional<TaskPriority> filter_priority;
  std::optional<TaskType> filter_type;
  bool show_snoozed = false;
  bool show_archived = false;
};

class BookmarkKanbanView {
 public:
  BookmarkKanbanView(BookmarkTaskManager* task_manager);
  ~BookmarkKanbanView();

  // Board generation
  [[nodiscard]] KanbanBoard GenerateBoard();
  void MoveTaskToColumn(const bookmarks::BookmarkNode* task, TaskStatus new_status);

  // Filters
  void SetPriorityFilter(std::optional<TaskPriority> priority);
  void SetTypeFilter(std::optional<TaskType> type);
  void SetShowSnoozed(bool show);
  void SetShowArchived(bool show);

  // Sorting
  enum class SortOrder {
    kPriority,
    kDueDate,
    kCreatedDate,
    kAlphabetical,
  };
  void SetSortOrder(SortOrder order);

 private:
  raw_ptr<BookmarkTaskManager> task_manager_;

  // View state
  std::optional<TaskPriority> filter_priority_;
  std::optional<TaskType> filter_type_;
  bool show_snoozed_ = false;
  bool show_archived_ = false;
  SortOrder sort_order_ = SortOrder::kPriority;

  // Helper methods
  bool ShouldShowTask(const bookmarks::BookmarkNode* task) const;
  void SortTasks(std::vector<const bookmarks::BookmarkNode*>& tasks) const;
};

// ===== Focus Mode =====

struct FocusSession {
  const bookmarks::BookmarkNode* task = nullptr;
  base::Time start_time;
  base::TimeDelta duration;  // Pomodoro-style duration
  bool active = false;
  int breaks_taken = 0;

  // Session stats
  int distractions_blocked = 0;
  std::vector<std::u16string> notes_taken;
};

class BookmarkFocusMode {
 public:
  BookmarkFocusMode(BookmarkTaskManager* task_manager);
  ~BookmarkFocusMode();

  // Focus session management
  void StartFocusSession(const bookmarks::BookmarkNode* task,
                        base::TimeDelta duration = base::Minutes(25));
  void EndFocusSession();
  void PauseSession();
  void ResumeSession();
  void TakeBreak(base::TimeDelta break_duration = base::Minutes(5));

  // Session state
  [[nodiscard]] const FocusSession* GetCurrentSession() const {
    return current_session_.get();
  }
  [[nodiscard]] bool IsSessionActive() const;
  [[nodiscard]] base::TimeDelta GetRemainingTime() const;

  // Distraction blocking
  void BlockDistractingSites(bool block);
  void AllowSite(std::u16string_view domain);

 private:
  raw_ptr<BookmarkTaskManager> task_manager_;
  std::unique_ptr<FocusSession> current_session_;
  std::vector<std::u16string> blocked_sites_;
  std::vector<std::u16string> allowed_sites_;
};

// ===== Task Scheduler =====

struct TaskReminder {
  const bookmarks::BookmarkNode* task;
  base::Time reminder_time;
  std::u16string message;
  bool shown = false;
};

class BookmarkTaskScheduler {
 public:
  BookmarkTaskScheduler(BookmarkTaskManager* task_manager);
  ~BookmarkTaskScheduler();

  // Scheduling
  void ScheduleTask(const bookmarks::BookmarkNode* task,
                   base::Time start_time,
                   std::optional<base::Time> due_time);
  void SetReminder(const bookmarks::BookmarkNode* task,
                  base::Time reminder_time,
                  std::u16string_view message);
  void SnoozeReminder(const bookmarks::BookmarkNode* task,
                     base::TimeDelta duration);

  // Recurring tasks
  void MakeRecurring(const bookmarks::BookmarkNode* task,
                    base::TimeDelta interval);
  void CreateNextRecurrence(const bookmarks::BookmarkNode* task);

  // Query
  [[nodiscard]] std::vector<TaskReminder> GetPendingReminders() const;
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*> GetScheduledTasks() const;

  // Processing
  void ProcessReminders();  // Called periodically to check for due reminders

 private:
  raw_ptr<BookmarkTaskManager> task_manager_;
  std::vector<TaskReminder> reminders_;

  void ShowNotification(const TaskReminder& reminder);
};

// ===== Helper Functions =====

// Convert enum to string for UI display
std::u16string TaskStatusToString(TaskStatus status);
std::u16string TaskPriorityToString(TaskPriority priority);
std::u16string TaskTypeToString(TaskType type);
std::u16string TimeEstimateToString(TimeEstimate estimate);

// Parse string to enum
std::optional<TaskStatus> StringToTaskStatus(std::u16string_view str);
std::optional<TaskPriority> StringToTaskPriority(std::u16string_view str);
std::optional<TaskType> StringToTaskType(std::u16string_view str);

// Calculate points based on task completion
int CalculateTaskPoints(const TaskMetadata& metadata);

// Get achievement info
Achievement GetAchievementInfo(AchievementType type);

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_TASK_MANAGER_H_
