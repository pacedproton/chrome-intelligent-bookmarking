// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_task_manager.h"

#include "base/logging.h"
#include "base/time/time.h"

// ===== BookmarkFocusMode Implementation =====

BookmarkFocusMode::BookmarkFocusMode(BookmarkTaskManager* task_manager)
    : task_manager_(task_manager) {
  DCHECK(task_manager_);
}

BookmarkFocusMode::~BookmarkFocusMode() {
  if (IsSessionActive()) {
    EndFocusSession();
  }
}

void BookmarkFocusMode::StartFocusSession(
    const bookmarks::BookmarkNode* task,
    base::TimeDelta duration) {
  DCHECK(task);

  if (current_session_) {
    DLOG(WARNING) << "Already in a focus session, ending previous session";
    EndFocusSession();
  }

  current_session_ = std::make_unique<FocusSession>();
  current_session_->task = task;
  current_session_->start_time = base::Time::Now();
  current_session_->duration = duration;
  current_session_->active = true;
  current_session_->breaks_taken = 0;
  current_session_->distractions_blocked = 0;

  // Mark task as in progress
  task_manager_->StartTask(task);

  DLOG(INFO) << "Started focus session for " << duration.InMinutes()
             << " minutes on task: " << task->GetTitle();
}

void BookmarkFocusMode::EndFocusSession() {
  if (!current_session_) {
    return;
  }

  base::Time now = base::Time::Now();
  base::TimeDelta elapsed = now - current_session_->start_time;

  // Update task metadata with time spent
  auto metadata_opt = task_manager_->GetTaskMetadata(current_session_->task);
  if (metadata_opt) {
    TaskMetadata metadata = *metadata_opt;
    metadata.total_time_spent += elapsed;
    metadata.last_worked_on = now;
    task_manager_->SetTaskMetadata(current_session_->task, metadata);
  }

  DLOG(INFO) << "Ended focus session. Duration: " << elapsed.InMinutes()
             << " minutes | Breaks: " << current_session_->breaks_taken
             << " | Distractions blocked: "
             << current_session_->distractions_blocked;

  current_session_.reset();
}

void BookmarkFocusMode::PauseSession() {
  if (!current_session_ || !current_session_->active) {
    return;
  }

  current_session_->active = false;
  DLOG(INFO) << "Paused focus session";
}

void BookmarkFocusMode::ResumeSession() {
  if (!current_session_ || current_session_->active) {
    return;
  }

  current_session_->active = true;
  DLOG(INFO) << "Resumed focus session";
}

void BookmarkFocusMode::TakeBreak(base::TimeDelta break_duration) {
  if (!current_session_) {
    return;
  }

  PauseSession();
  current_session_->breaks_taken++;

  DLOG(INFO) << "Taking break for " << break_duration.InMinutes() << " minutes";

  // In a real implementation, we would:
  // 1. Set a timer to auto-resume after break_duration
  // 2. Show a notification when break is over
  // 3. Track break time separately
}

bool BookmarkFocusMode::IsSessionActive() const {
  return current_session_ && current_session_->active;
}

base::TimeDelta BookmarkFocusMode::GetRemainingTime() const {
  if (!current_session_) {
    return base::TimeDelta();
  }

  base::Time now = base::Time::Now();
  base::TimeDelta elapsed = now - current_session_->start_time;
  base::TimeDelta remaining = current_session_->duration - elapsed;

  return remaining.is_positive() ? remaining : base::TimeDelta();
}

void BookmarkFocusMode::BlockDistractingSites(bool block) {
  if (!current_session_) {
    return;
  }

  // In a real implementation, this would:
  // 1. Integrate with Chrome's content settings
  // 2. Block navigation to known distracting sites
  // 3. Show a "Stay focused!" message when blocked
  // 4. Increment distractions_blocked counter

  DLOG(INFO) << (block ? "Enabled" : "Disabled") << " distraction blocking";
}

void BookmarkFocusMode::AllowSite(std::u16string_view domain) {
  allowed_sites_.push_back(std::u16string(domain));
  DLOG(INFO) << "Allowed site: " << domain;
}

// ===== BookmarkTaskScheduler Implementation =====

BookmarkTaskScheduler::BookmarkTaskScheduler(
    BookmarkTaskManager* task_manager)
    : task_manager_(task_manager) {
  DCHECK(task_manager_);
}

BookmarkTaskScheduler::~BookmarkTaskScheduler() = default;

void BookmarkTaskScheduler::ScheduleTask(
    const bookmarks::BookmarkNode* task,
    base::Time start_time,
    std::optional<base::Time> due_time) {
  DCHECK(task);

  auto metadata_opt = task_manager_->GetTaskMetadata(task);
  TaskMetadata metadata = metadata_opt.value_or(TaskMetadata());

  metadata.start_date = start_time;
  metadata.due_date = due_time;

  task_manager_->SetTaskMetadata(task, metadata);

  DLOG(INFO) << "Scheduled task: " << task->GetTitle();
}

void BookmarkTaskScheduler::SetReminder(
    const bookmarks::BookmarkNode* task,
    base::Time reminder_time,
    std::u16string_view message) {
  DCHECK(task);

  TaskReminder reminder;
  reminder.task = task;
  reminder.reminder_time = reminder_time;
  reminder.message = std::u16string(message);
  reminder.shown = false;

  reminders_.push_back(reminder);

  DLOG(INFO) << "Set reminder for task: " << task->GetTitle()
             << " at " << reminder_time;
}

void BookmarkTaskScheduler::SnoozeReminder(
    const bookmarks::BookmarkNode* task,
    base::TimeDelta duration) {
  DCHECK(task);

  // Find and update reminder
  for (auto& reminder : reminders_) {
    if (reminder.task == task && !reminder.shown) {
      reminder.reminder_time = base::Time::Now() + duration;
      DLOG(INFO) << "Snoozed reminder for " << duration.InMinutes()
                 << " minutes";
      return;
    }
  }
}

void BookmarkTaskScheduler::MakeRecurring(
    const bookmarks::BookmarkNode* task,
    base::TimeDelta interval) {
  DCHECK(task);

  auto metadata_opt = task_manager_->GetTaskMetadata(task);
  TaskMetadata metadata = metadata_opt.value_or(TaskMetadata());

  metadata.is_recurring = true;
  metadata.recurrence_interval = interval;

  task_manager_->SetTaskMetadata(task, metadata);

  DLOG(INFO) << "Made task recurring with interval: "
             << interval.InDays() << " days";
}

void BookmarkTaskScheduler::CreateNextRecurrence(
    const bookmarks::BookmarkNode* task) {
  DCHECK(task);

  auto metadata_opt = task_manager_->GetTaskMetadata(task);
  if (!metadata_opt || !metadata_opt->is_recurring) {
    return;
  }

  const TaskMetadata& metadata = *metadata_opt;

  // In a real implementation, we would:
  // 1. Clone the bookmark
  // 2. Set new due date = current due date + recurrence_interval
  // 3. Reset completion status
  // 4. Preserve other metadata

  DLOG(INFO) << "Created next recurrence for task";
}

std::vector<TaskReminder> BookmarkTaskScheduler::GetPendingReminders() const {
  std::vector<TaskReminder> pending;
  base::Time now = base::Time::Now();

  for (const auto& reminder : reminders_) {
    if (!reminder.shown && reminder.reminder_time <= now) {
      pending.push_back(reminder);
    }
  }

  return pending;
}

std::vector<const bookmarks::BookmarkNode*>
BookmarkTaskScheduler::GetScheduledTasks() const {
  std::vector<const bookmarks::BookmarkNode*> scheduled;

  // Get all tasks with start or due dates
  // In a real implementation, iterate through task metadata

  return scheduled;
}

void BookmarkTaskScheduler::ProcessReminders() {
  std::vector<TaskReminder> pending = GetPendingReminders();

  for (auto& reminder : pending) {
    ShowNotification(reminder);

    // Mark as shown
    for (auto& r : reminders_) {
      if (r.task == reminder.task &&
          r.reminder_time == reminder.reminder_time) {
        r.shown = true;
        break;
      }
    }
  }
}

void BookmarkTaskScheduler::ShowNotification(const TaskReminder& reminder) {
  DCHECK(reminder.task);

  // In a real implementation, this would:
  // 1. Create a Chrome notification
  // 2. Include task title and custom message
  // 3. Add action buttons (Complete, Snooze, Dismiss)
  // 4. Play notification sound

  DLOG(INFO) << "Reminder: " << reminder.message
             << " | Task: " << reminder.task->GetTitle();
}
