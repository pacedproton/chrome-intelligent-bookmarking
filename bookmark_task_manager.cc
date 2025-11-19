// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_task_manager.h"

#include <algorithm>
#include <memory>
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

// Points awarded for different task completions
constexpr int kQuickTaskPoints = 10;
constexpr int kShortTaskPoints = 25;
constexpr int kMediumTaskPoints = 50;
constexpr int kLongTaskPoints = 100;
constexpr int kVeryLongTaskPoints = 200;

// Priority multipliers
constexpr float kLowPriorityMultiplier = 1.0f;
constexpr float kMediumPriorityMultiplier = 1.5f;
constexpr float kHighPriorityMultiplier = 2.0f;
constexpr float kCriticalPriorityMultiplier = 3.0f;

// Bonus points
constexpr int kStreakBonusPoints = 50;
constexpr int kEarlyCompletionBonus = 25;
constexpr int kWeekendBonus = 15;

}  // namespace

// ===== Helper Functions Implementation =====

std::u16string TaskStatusToString(TaskStatus status) {
  switch (status) {
    case TaskStatus::kTodo:
      return u"Todo";
    case TaskStatus::kInProgress:
      return u"In Progress";
    case TaskStatus::kDone:
      return u"Done";
    case TaskStatus::kSnoozed:
      return u"Snoozed";
    case TaskStatus::kArchived:
      return u"Archived";
  }
  return u"Unknown";
}

std::u16string TaskPriorityToString(TaskPriority priority) {
  switch (priority) {
    case TaskPriority::kLow:
      return u"Low";
    case TaskPriority::kMedium:
      return u"Medium";
    case TaskPriority::kHigh:
      return u"High";
    case TaskPriority::kCritical:
      return u"Critical";
  }
  return u"Unknown";
}

std::u16string TaskTypeToString(TaskType type) {
  switch (type) {
    case TaskType::kRead:
      return u"Read";
    case TaskType::kWatch:
      return u"Watch";
    case TaskType::kLearn:
      return u"Learn";
    case TaskType::kWork:
      return u"Work";
    case TaskType::kResearch:
      return u"Research";
    case TaskType::kBuy:
      return u"Buy";
    case TaskType::kIdea:
      return u"Idea";
    case TaskType::kReference:
      return u"Reference";
    case TaskType::kOther:
      return u"Other";
  }
  return u"Unknown";
}

std::u16string TimeEstimateToString(TimeEstimate estimate) {
  switch (estimate) {
    case TimeEstimate::kQuick:
      return u"Quick (< 5 min)";
    case TimeEstimate::kShort:
      return u"Short (5-15 min)";
    case TimeEstimate::kMedium:
      return u"Medium (15-30 min)";
    case TimeEstimate::kLong:
      return u"Long (30-60 min)";
    case TimeEstimate::kVeryLong:
      return u"Very Long (> 1 hr)";
  }
  return u"Unknown";
}

int CalculateTaskPoints(const TaskMetadata& metadata) {
  int base_points = 0;

  // Base points by time estimate
  switch (metadata.time_estimate) {
    case TimeEstimate::kQuick:
      base_points = kQuickTaskPoints;
      break;
    case TimeEstimate::kShort:
      base_points = kShortTaskPoints;
      break;
    case TimeEstimate::kMedium:
      base_points = kMediumTaskPoints;
      break;
    case TimeEstimate::kLong:
      base_points = kLongTaskPoints;
      break;
    case TimeEstimate::kVeryLong:
      base_points = kVeryLongTaskPoints;
      break;
  }

  // Apply priority multiplier
  float multiplier = kMediumPriorityMultiplier;
  switch (metadata.priority) {
    case TaskPriority::kLow:
      multiplier = kLowPriorityMultiplier;
      break;
    case TaskPriority::kMedium:
      multiplier = kMediumPriorityMultiplier;
      break;
    case TaskPriority::kHigh:
      multiplier = kHighPriorityMultiplier;
      break;
    case TaskPriority::kCritical:
      multiplier = kCriticalPriorityMultiplier;
      break;
  }

  return static_cast<int>(base_points * multiplier);
}

Achievement GetAchievementInfo(AchievementType type) {
  Achievement achievement;
  achievement.type = type;
  achievement.unlocked = false;

  switch (type) {
    case AchievementType::kFirstTask:
      achievement.title = u"First Steps";
      achievement.description = u"Complete your first task";
      achievement.points = 50;
      break;
    case AchievementType::kTaskWarrior:
      achievement.title = u"Task Warrior";
      achievement.description = u"Complete 10 tasks";
      achievement.points = 100;
      break;
    case AchievementType::kTaskMaster:
      achievement.title = u"Task Master";
      achievement.description = u"Complete 100 tasks";
      achievement.points = 500;
      break;
    case AchievementType::kTaskLegend:
      achievement.title = u"Task Legend";
      achievement.description = u"Complete 1000 tasks";
      achievement.points = 5000;
      break;
    case AchievementType::kWeekStreak:
      achievement.title = u"Week Warrior";
      achievement.description = u"Maintain a 7-day streak";
      achievement.points = 200;
      break;
    case AchievementType::kMonthStreak:
      achievement.title = u"Monthly Master";
      achievement.description = u"Maintain a 30-day streak";
      achievement.points = 1000;
      break;
    case AchievementType::kYearStreak:
      achievement.title = u"Year Legend";
      achievement.description = u"Maintain a 365-day streak";
      achievement.points = 10000;
      break;
    case AchievementType::kSpeedDemon:
      achievement.title = u"Speed Demon";
      achievement.description = u"Complete 10 tasks in one day";
      achievement.points = 300;
      break;
    case AchievementType::kProductivityBeast:
      achievement.title = u"Productivity Beast";
      achievement.description = u"Complete 50 tasks in one week";
      achievement.points = 1500;
      break;
    case AchievementType::kOrganizer:
      achievement.title = u"The Organizer";
      achievement.description = u"Organize 100 bookmarks";
      achievement.points = 250;
      break;
    case AchievementType::kCurator:
      achievement.title = u"The Curator";
      achievement.description = u"Create 10 smart folders";
      achievement.points = 300;
      break;
    case AchievementType::kArchivist:
      achievement.title = u"The Archivist";
      achievement.description = u"Archive 50 old tasks";
      achievement.points = 200;
      break;
    case AchievementType::kEarlyBird:
      achievement.title = u"Early Bird";
      achievement.description = u"Complete a task before 8 AM";
      achievement.points = 100;
      break;
    case AchievementType::kNightOwl:
      achievement.title = u"Night Owl";
      achievement.description = u"Complete a task after 10 PM";
      achievement.points = 100;
      break;
    case AchievementType::kWeekendWarrior:
      achievement.title = u"Weekend Warrior";
      achievement.description = u"Complete 20 weekend tasks";
      achievement.points = 250;
      break;
    case AchievementType::kZeroInbox:
      achievement.title = u"Zero Inbox";
      achievement.description = u"Clear all todo tasks";
      achievement.points = 500;
      break;
  }

  return achievement;
}

// ===== BookmarkTaskManager Implementation =====

BookmarkTaskManager::BookmarkTaskManager(bookmarks::BookmarkModel* model,
                                         BookmarkManager* manager)
    : bookmark_model_(model), manager_(manager) {
  DCHECK(bookmark_model_);
  DCHECK(manager_);
  LoadProfile();
}

BookmarkTaskManager::~BookmarkTaskManager() {
  SaveProfile();
}

void BookmarkTaskManager::SetTaskMetadata(
    const bookmarks::BookmarkNode* bookmark,
    const TaskMetadata& metadata) {
  DCHECK(bookmark);
  task_metadata_[bookmark] = metadata;
}

std::optional<TaskMetadata> BookmarkTaskManager::GetTaskMetadata(
    const bookmarks::BookmarkNode* bookmark) const {
  DCHECK(bookmark);
  auto it = task_metadata_.find(bookmark);
  if (it != task_metadata_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void BookmarkTaskManager::RemoveTaskMetadata(
    const bookmarks::BookmarkNode* bookmark) {
  DCHECK(bookmark);
  task_metadata_.erase(bookmark);
}

void BookmarkTaskManager::StartTask(const bookmarks::BookmarkNode* bookmark) {
  DCHECK(bookmark);

  auto metadata_opt = GetTaskMetadata(bookmark);
  if (!metadata_opt) {
    return;
  }

  TaskMetadata metadata = *metadata_opt;
  metadata.status = TaskStatus::kInProgress;
  metadata.last_worked_on = base::Time::Now();

  if (!metadata.start_date) {
    metadata.start_date = base::Time::Now();
  }

  SetTaskMetadata(bookmark, metadata);

  DLOG(INFO) << "Started task: " << bookmark->GetTitle();
}

void BookmarkTaskManager::CompleteTask(
    const bookmarks::BookmarkNode* bookmark) {
  DCHECK(bookmark);

  auto metadata_opt = GetTaskMetadata(bookmark);
  if (!metadata_opt) {
    return;
  }

  TaskMetadata metadata = *metadata_opt;
  metadata.status = TaskStatus::kDone;
  metadata.completed_date = base::Time::Now();
  metadata.progress_percent = 100;

  SetTaskMetadata(bookmark, metadata);

  // Award points
  int points = CalculateTaskPoints(metadata);
  AwardPoints(points, TaskTypeToString(metadata.type));

  // Update stats
  profile_.total_tasks_completed++;
  profile_.tasks_today++;
  profile_.tasks_this_week++;
  profile_.tasks_this_month++;
  profile_.last_task_completion = base::Time::Now();

  // Update streak
  UpdateStreak();

  // Check for achievements
  CheckAchievements();

  DLOG(INFO) << "Completed task: " << bookmark->GetTitle()
             << " | Points: " << points;
}

void BookmarkTaskManager::SnoozeTask(const bookmarks::BookmarkNode* bookmark,
                                     base::TimeDelta duration) {
  DCHECK(bookmark);

  auto metadata_opt = GetTaskMetadata(bookmark);
  if (!metadata_opt) {
    return;
  }

  TaskMetadata metadata = *metadata_opt;
  metadata.status = TaskStatus::kSnoozed;
  metadata.snoozed_until = base::Time::Now() + duration;

  SetTaskMetadata(bookmark, metadata);

  DLOG(INFO) << "Snoozed task for " << duration.InMinutes() << " minutes";
}

void BookmarkTaskManager::UnsnoozeTask(
    const bookmarks::BookmarkNode* bookmark) {
  DCHECK(bookmark);

  auto metadata_opt = GetTaskMetadata(bookmark);
  if (!metadata_opt) {
    return;
  }

  TaskMetadata metadata = *metadata_opt;
  if (metadata.status == TaskStatus::kSnoozed) {
    metadata.status = TaskStatus::kTodo;
    metadata.snoozed_until = std::nullopt;
    SetTaskMetadata(bookmark, metadata);
  }
}

void BookmarkTaskManager::ArchiveTask(
    const bookmarks::BookmarkNode* bookmark) {
  DCHECK(bookmark);

  auto metadata_opt = GetTaskMetadata(bookmark);
  if (!metadata_opt) {
    return;
  }

  TaskMetadata metadata = *metadata_opt;
  metadata.status = TaskStatus::kArchived;
  SetTaskMetadata(bookmark, metadata);
}

std::vector<const bookmarks::BookmarkNode*>
BookmarkTaskManager::GetTasksByStatus(TaskStatus status) const {
  std::vector<const bookmarks::BookmarkNode*> tasks;

  for (const auto& [bookmark, metadata] : task_metadata_) {
    if (metadata.status == status) {
      tasks.push_back(bookmark);
    }
  }

  return tasks;
}

std::vector<const bookmarks::BookmarkNode*>
BookmarkTaskManager::GetTasksByPriority(TaskPriority priority) const {
  std::vector<const bookmarks::BookmarkNode*> tasks;

  for (const auto& [bookmark, metadata] : task_metadata_) {
    if (metadata.priority == priority) {
      tasks.push_back(bookmark);
    }
  }

  return tasks;
}

std::vector<const bookmarks::BookmarkNode*>
BookmarkTaskManager::GetDueTasks() const {
  std::vector<const bookmarks::BookmarkNode*> tasks;
  base::Time now = base::Time::Now();

  for (const auto& [bookmark, metadata] : task_metadata_) {
    if (metadata.due_date && *metadata.due_date <= now &&
        metadata.status != TaskStatus::kDone &&
        metadata.status != TaskStatus::kArchived) {
      tasks.push_back(bookmark);
    }
  }

  return tasks;
}

std::vector<const bookmarks::BookmarkNode*>
BookmarkTaskManager::GetOverdueTasks() const {
  std::vector<const bookmarks::BookmarkNode*> tasks;
  base::Time now = base::Time::Now();

  for (const auto& [bookmark, metadata] : task_metadata_) {
    if (metadata.due_date && *metadata.due_date < now &&
        metadata.status != TaskStatus::kDone &&
        metadata.status != TaskStatus::kArchived) {
      tasks.push_back(bookmark);
    }
  }

  return tasks;
}

std::vector<const bookmarks::BookmarkNode*>
BookmarkTaskManager::GetTasksForToday() const {
  std::vector<const bookmarks::BookmarkNode*> tasks;
  base::Time now = base::Time::Now();
  base::Time::Exploded now_exploded;
  now.LocalExplode(&now_exploded);

  for (const auto& [bookmark, metadata] : task_metadata_) {
    if (metadata.due_date) {
      base::Time::Exploded due_exploded;
      metadata.due_date->LocalExplode(&due_exploded);

      if (now_exploded.year == due_exploded.year &&
          now_exploded.month == due_exploded.month &&
          now_exploded.day_of_month == due_exploded.day_of_month &&
          metadata.status != TaskStatus::kDone &&
          metadata.status != TaskStatus::kArchived) {
        tasks.push_back(bookmark);
      }
    }
  }

  return tasks;
}

void BookmarkTaskManager::ConvertTabToTask(
    const bookmarks::BookmarkNode* bookmark,
    int tab_index) {
  DCHECK(bookmark);

  auto metadata_opt = GetTaskMetadata(bookmark);
  TaskMetadata metadata = metadata_opt.value_or(TaskMetadata());

  metadata.opened_as_tab = true;
  metadata.tab_index = tab_index;

  // Auto-detect task type if not set
  if (!metadata_opt) {
    metadata.type = DetectTaskType(bookmark);
    metadata.time_estimate = EstimateTaskDuration(bookmark);
  }

  SetTaskMetadata(bookmark, metadata);
}

void BookmarkTaskManager::OpenTaskAsTab(
    const bookmarks::BookmarkNode* bookmark) {
  DCHECK(bookmark);

  // This would integrate with browser's tab system
  // For now, just mark as opened
  auto metadata_opt = GetTaskMetadata(bookmark);
  if (!metadata_opt) {
    return;
  }

  TaskMetadata metadata = *metadata_opt;
  metadata.opened_as_tab = true;
  SetTaskMetadata(bookmark, metadata);
}

void BookmarkTaskManager::CloseTaskTab(
    const bookmarks::BookmarkNode* bookmark) {
  DCHECK(bookmark);

  auto metadata_opt = GetTaskMetadata(bookmark);
  if (!metadata_opt) {
    return;
  }

  TaskMetadata metadata = *metadata_opt;
  metadata.opened_as_tab = false;
  metadata.tab_index = -1;
  SetTaskMetadata(bookmark, metadata);
}

std::vector<const bookmarks::BookmarkNode*>
BookmarkTaskManager::GetOpenTasks() const {
  std::vector<const bookmarks::BookmarkNode*> tasks;

  for (const auto& [bookmark, metadata] : task_metadata_) {
    if (metadata.opened_as_tab) {
      tasks.push_back(bookmark);
    }
  }

  return tasks;
}

TaskType BookmarkTaskManager::DetectTaskType(
    const bookmarks::BookmarkNode* bookmark) const {
  DCHECK(bookmark);

  std::u16string url_str = base::UTF8ToUTF16(bookmark->url().spec());
  std::u16string title = bookmark->GetTitle();
  std::u16string combined = base::ToLowerASCII(url_str + u" " + title);

  // Pattern matching
  if (combined.find(u"youtube") != std::u16string::npos ||
      combined.find(u"video") != std::u16string::npos ||
      combined.find(u"watch") != std::u16string::npos) {
    return TaskType::kWatch;
  }

  if (combined.find(u"course") != std::u16string::npos ||
      combined.find(u"tutorial") != std::u16string::npos ||
      combined.find(u"learn") != std::u16string::npos) {
    return TaskType::kLearn;
  }

  if (combined.find(u"docs") != std::u16string::npos ||
      combined.find(u"documentation") != std::u16string::npos ||
      combined.find(u"api") != std::u16string::npos ||
      combined.find(u"blog") != std::u16string::npos ||
      combined.find(u"article") != std::u16string::npos) {
    return TaskType::kRead;
  }

  if (combined.find(u"amazon") != std::u16string::npos ||
      combined.find(u"shop") != std::u16string::npos ||
      combined.find(u"buy") != std::u16string::npos) {
    return TaskType::kBuy;
  }

  if (combined.find(u"github") != std::u16string::npos ||
      combined.find(u"jira") != std::u16string::npos ||
      combined.find(u"confluence") != std::u16string::npos) {
    return TaskType::kWork;
  }

  return TaskType::kOther;
}

TimeEstimate BookmarkTaskManager::EstimateTaskDuration(
    const bookmarks::BookmarkNode* bookmark) const {
  DCHECK(bookmark);

  TaskType type = DetectTaskType(bookmark);

  // Estimate based on type
  switch (type) {
    case TaskType::kWatch:
      return TimeEstimate::kLong;  // Videos tend to be longer
    case TaskType::kLearn:
      return TimeEstimate::kVeryLong;  // Courses are lengthy
    case TaskType::kRead:
      return TimeEstimate::kMedium;  // Articles average length
    case TaskType::kWork:
      return TimeEstimate::kLong;  // Work tasks usually complex
    case TaskType::kBuy:
      return TimeEstimate::kShort;  // Quick shopping
    case TaskType::kIdea:
      return TimeEstimate::kQuick;  // Quick review
    case TaskType::kReference:
      return TimeEstimate::kQuick;  // Quick lookup
    default:
      return TimeEstimate::kMedium;
  }
}

void BookmarkTaskManager::AwardPoints(int points,
                                      std::u16string_view category) {
  profile_.experience_points += points;
  profile_.total_points_earned += points;
  profile_.points_by_category[std::u16string(category)] += points;

  // Check for level up
  UpdateLevel();

  DLOG(INFO) << "Awarded " << points << " points in " << category;
}

void BookmarkTaskManager::UpdateLevel() {
  while (profile_.experience_points >= profile_.points_to_next_level) {
    profile_.experience_points -= profile_.points_to_next_level;
    profile_.level++;

    // Exponential scaling: each level requires 10% more points
    profile_.points_to_next_level =
        static_cast<int>(profile_.points_to_next_level * 1.1);

    DLOG(INFO) << "Level up! Now level " << profile_.level;
  }
}

void BookmarkTaskManager::UpdateStreak() {
  base::Time now = base::Time::Now();
  base::Time::Exploded now_exploded;
  now.LocalExplode(&now_exploded);

  if (profile_.last_task_completion.is_null()) {
    // First task ever
    profile_.current_streak = 1;
    profile_.streak_start_date = now;
    return;
  }

  base::Time::Exploded last_exploded;
  profile_.last_task_completion.LocalExplode(&last_exploded);

  // Check if same day
  if (now_exploded.year == last_exploded.year &&
      now_exploded.month == last_exploded.month &&
      now_exploded.day_of_month == last_exploded.day_of_month) {
    // Same day, streak continues
    return;
  }

  // Check if consecutive day
  base::TimeDelta diff = now - profile_.last_task_completion;
  if (diff.InDays() == 1) {
    profile_.current_streak++;
    profile_.longest_streak =
        std::max(profile_.longest_streak, profile_.current_streak);

    // Award streak bonus
    if (profile_.current_streak % 7 == 0) {
      AwardPoints(kStreakBonusPoints, u"Streak Bonus");
    }
  } else if (diff.InDays() > 1) {
    // Streak broken
    profile_.current_streak = 1;
    profile_.streak_start_date = now;
  }
}

void BookmarkTaskManager::CheckAchievements() {
  // Initialize achievements if empty
  if (profile_.achievements.empty()) {
    for (int i = 0; i <= static_cast<int>(AchievementType::kZeroInbox); ++i) {
      Achievement achievement =
          GetAchievementInfo(static_cast<AchievementType>(i));
      profile_.achievements.push_back(achievement);
    }
  }

  // Check completion achievements
  if (profile_.total_tasks_completed == 1) {
    UnlockAchievement(AchievementType::kFirstTask);
  } else if (profile_.total_tasks_completed == 10) {
    UnlockAchievement(AchievementType::kTaskWarrior);
  } else if (profile_.total_tasks_completed == 100) {
    UnlockAchievement(AchievementType::kTaskMaster);
  } else if (profile_.total_tasks_completed == 1000) {
    UnlockAchievement(AchievementType::kTaskLegend);
  }

  // Check streak achievements
  if (profile_.current_streak == 7) {
    UnlockAchievement(AchievementType::kWeekStreak);
  } else if (profile_.current_streak == 30) {
    UnlockAchievement(AchievementType::kMonthStreak);
  } else if (profile_.current_streak == 365) {
    UnlockAchievement(AchievementType::kYearStreak);
  }

  // Check speed achievements
  if (profile_.tasks_today == 10) {
    UnlockAchievement(AchievementType::kSpeedDemon);
  }
  if (profile_.tasks_this_week == 50) {
    UnlockAchievement(AchievementType::kProductivityBeast);
  }

  // Check time-based achievements
  base::Time now = base::Time::Now();
  base::Time::Exploded now_exploded;
  now.LocalExplode(&now_exploded);

  if (now_exploded.hour < 8) {
    UnlockAchievement(AchievementType::kEarlyBird);
  } else if (now_exploded.hour >= 22) {
    UnlockAchievement(AchievementType::kNightOwl);
  }

  // Check zero inbox
  auto todo_tasks = GetTasksByStatus(TaskStatus::kTodo);
  if (todo_tasks.empty() && profile_.total_tasks_completed > 0) {
    UnlockAchievement(AchievementType::kZeroInbox);
  }
}

void BookmarkTaskManager::UnlockAchievement(AchievementType type) {
  for (auto& achievement : profile_.achievements) {
    if (achievement.type == type && !achievement.unlocked) {
      achievement.unlocked = true;
      achievement.earned_date = base::Time::Now();
      profile_.achievements_unlocked++;

      // Award points
      AwardPoints(achievement.points, u"Achievement");

      DLOG(INFO) << "Achievement unlocked: " << achievement.title
                 << " | Points: " << achievement.points;
      break;
    }
  }
}

void BookmarkTaskManager::SaveProfile() {
  // TODO: Implement profile persistence to disk/preferences
  DLOG(INFO) << "Saving gamification profile (level " << profile_.level << ")";
}

void BookmarkTaskManager::LoadProfile() {
  // TODO: Implement profile loading from disk/preferences
  DLOG(INFO) << "Loading gamification profile";
}

// ===== BookmarkKanbanView Implementation =====

BookmarkKanbanView::BookmarkKanbanView(BookmarkTaskManager* task_manager)
    : task_manager_(task_manager) {
  DCHECK(task_manager_);
}

BookmarkKanbanView::~BookmarkKanbanView() = default;

KanbanBoard BookmarkKanbanView::GenerateBoard() {
  DCHECK(task_manager_);

  KanbanBoard board;

  // Create columns for each status
  const TaskStatus statuses[] = {
      TaskStatus::kTodo,
      TaskStatus::kInProgress,
      TaskStatus::kDone,
      TaskStatus::kSnoozed,
  };

  for (TaskStatus status : statuses) {
    if (status == TaskStatus::kSnoozed && !show_snoozed_) {
      continue;
    }

    KanbanColumn column;
    column.status = status;
    column.title = TaskStatusToString(status);
    column.tasks = task_manager_->GetTasksByStatus(status);

    // Apply filters
    std::vector<const bookmarks::BookmarkNode*> filtered_tasks;
    for (const auto* task : column.tasks) {
      if (ShouldShowTask(task)) {
        filtered_tasks.push_back(task);
      }
    }
    column.tasks = filtered_tasks;

    // Sort tasks
    SortTasks(column.tasks);

    column.task_count = column.tasks.size();

    // Calculate points
    for (const auto* task : column.tasks) {
      auto metadata_opt = task_manager_->GetTaskMetadata(task);
      if (metadata_opt) {
        column.total_points += CalculateTaskPoints(*metadata_opt);
      }
    }

    board.columns.push_back(column);
    board.total_tasks += column.task_count;
  }

  // Calculate completion percentage
  int done_count = 0;
  for (const auto& column : board.columns) {
    if (column.status == TaskStatus::kDone) {
      done_count = column.task_count;
    }
  }

  if (board.total_tasks > 0) {
    board.completion_percentage = (done_count * 100) / board.total_tasks;
  }

  return board;
}

void BookmarkKanbanView::MoveTaskToColumn(
    const bookmarks::BookmarkNode* task,
    TaskStatus new_status) {
  DCHECK(task_manager_);
  DCHECK(task);

  auto metadata_opt = task_manager_->GetTaskMetadata(task);
  if (!metadata_opt) {
    return;
  }

  TaskMetadata metadata = *metadata_opt;
  TaskStatus old_status = metadata.status;
  metadata.status = new_status;

  // Handle status-specific updates
  if (new_status == TaskStatus::kDone && old_status != TaskStatus::kDone) {
    task_manager_->CompleteTask(task);
  } else if (new_status == TaskStatus::kInProgress) {
    task_manager_->StartTask(task);
  } else {
    task_manager_->SetTaskMetadata(task, metadata);
  }

  DLOG(INFO) << "Moved task from " << TaskStatusToString(old_status)
             << " to " << TaskStatusToString(new_status);
}

bool BookmarkKanbanView::ShouldShowTask(
    const bookmarks::BookmarkNode* task) const {
  DCHECK(task_manager_);
  DCHECK(task);

  auto metadata_opt = task_manager_->GetTaskMetadata(task);
  if (!metadata_opt) {
    return false;
  }

  const TaskMetadata& metadata = *metadata_opt;

  // Apply priority filter
  if (filter_priority_ && metadata.priority != *filter_priority_) {
    return false;
  }

  // Apply type filter
  if (filter_type_ && metadata.type != *filter_type_) {
    return false;
  }

  return true;
}

void BookmarkKanbanView::SortTasks(
    std::vector<const bookmarks::BookmarkNode*>& tasks) const {
  DCHECK(task_manager_);

  std::sort(tasks.begin(), tasks.end(),
           [this](const bookmarks::BookmarkNode* a,
                  const bookmarks::BookmarkNode* b) {
             auto a_meta = task_manager_->GetTaskMetadata(a);
             auto b_meta = task_manager_->GetTaskMetadata(b);

             if (!a_meta || !b_meta) {
               return false;
             }

             switch (sort_order_) {
               case SortOrder::kPriority:
                 return static_cast<int>(a_meta->priority) >
                        static_cast<int>(b_meta->priority);
               case SortOrder::kDueDate:
                 if (a_meta->due_date && b_meta->due_date) {
                   return *a_meta->due_date < *b_meta->due_date;
                 }
                 return a_meta->due_date.has_value();
               case SortOrder::kCreatedDate:
                 return a->date_added() > b->date_added();
               case SortOrder::kAlphabetical:
                 return a->GetTitle() < b->GetTitle();
             }
             return false;
           });
}

void BookmarkKanbanView::SetPriorityFilter(
    std::optional<TaskPriority> priority) {
  filter_priority_ = priority;
}

void BookmarkKanbanView::SetTypeFilter(std::optional<TaskType> type) {
  filter_type_ = type;
}

void BookmarkKanbanView::SetShowSnoozed(bool show) {
  show_snoozed_ = show;
}

void BookmarkKanbanView::SetShowArchived(bool show) {
  show_archived_ = show;
}

void BookmarkKanbanView::SetSortOrder(SortOrder order) {
  sort_order_ = order;
}
