// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_SMART_WORKSPACE_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_SMART_WORKSPACE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "chrome/browser/ui/bookmarks/bookmark_task_manager.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"

// ============================================================================
// SMART WORKSPACE - The Ultimate Task + Bookmark Integration
// ============================================================================
//
// This system completely replaces:
// - Traditional task managers (Todoist, Things, etc.)
// - Tab hoarding (50+ open tabs as placeholders)
// - External todo lists
// - All other task management methods
//
// Design Philosophy:
// 1. **Zero Friction**: Capture tasks/bookmarks in < 2 seconds
// 2. **Context-Aware**: Show right items at right time
// 3. **Always Accessible**: Floating panel, never hidden
// 4. **Visual**: Previews, progress, natural recognition
// 5. **Smart**: Auto-categorize, auto-suggest, auto-organize
// 6. **Natural Flow**: Not lists or boards, but natural task flow
// 7. **Extreme Usability**: So good you never need anything else
//
// Key Components:
// - SmartFloatingWorkspace: Main always-visible interface
// - QuickCaptureBar: 1-click task/bookmark creation from anywhere
// - ContextEngine: Shows relevant items based on current activity
// - VisualTaskFlow: Natural task progression view
// - SmartGrouping: Auto-organize by project/context
// - AddressBarIntegration: Quick access from omnibox
// - FloatingAccessButton: Minimized state on screen edge

namespace bookmarks {
class BookmarkNode;
class BookmarkModel;
}  // namespace bookmarks

// ===== Context Detection =====

// Detects current browsing context to surface relevant bookmarks/tasks
class BrowsingContext {
 public:
  enum class ActivityType {
    kResearching,    // Reading articles, documentation
    kShopping,       // E-commerce sites
    kWorking,        // Work-related domains (docs, email, project tools)
    kLearning,       // Educational sites, tutorials, courses
    kEntertainment,  // Videos, social media, news
    kPlanning,       // Calendar, planning tools
    kDeveloping,     // GitHub, Stack Overflow, dev tools
    kReading,        // Articles, blogs, long-form content
    kCommunicating,  // Email, chat, messaging
    kOther
  };

  enum class TimeContext {
    kMorning,      // 5am-12pm: Planning, email, standup tasks
    kAfternoon,    // 12pm-5pm: Deep work, focused tasks
    kEvening,      // 5pm-10pm: Lighter tasks, reading, learning
    kNight,        // 10pm+: Quick tasks, browsing, cleanup
    kWeekend,      // Different task priorities
    kWorkHours,    // 9-5 weekday
    kOffHours      // Outside work hours
  };

  std::u16string current_url;
  std::u16string current_title;
  std::u16string domain;
  ActivityType activity = ActivityType::kOther;
  TimeContext time_context = TimeContext::kMorning;
  std::vector<std::u16string> recent_domains;  // Last 5 domains visited
  std::vector<std::u16string> current_keywords;  // Extracted from page
  base::Time session_start;
  int tabs_open_count = 0;

  [[nodiscard]] static ActivityType DetectActivity(
      std::u16string_view url,
      std::u16string_view title);
  [[nodiscard]] static TimeContext GetTimeContext();
  [[nodiscard]] bool IsRelatedTo(const bookmarks::BookmarkNode* bookmark) const;
  [[nodiscard]] int CalculateRelevanceScore(
      const bookmarks::BookmarkNode* bookmark) const;
};

// ===== Smart Grouping =====

// Auto-groups bookmarks/tasks by project, topic, or context
struct SmartGroup {
  std::u16string name;  // Auto-generated or user-named
  std::u16string icon;  // Emoji or icon
  SkColor color;        // Visual identifier

  std::vector<const bookmarks::BookmarkNode*> items;

  enum class GroupType {
    kProject,     // Related by project (e.g., "Website Redesign")
    kDomain,      // Same domain (e.g., all GitHub)
    kTopic,       // Same topic (e.g., "React Learning")
    kTimeframe,   // Due this week/today/overdue
    kFrequent,    // Frequently accessed together
    kRecent,      // Recently added
    kContext      // Same activity type (all shopping, all reading)
  };
  GroupType type = GroupType::kProject;

  base::Time created;
  base::Time last_accessed;
  int access_count = 0;

  // Smart metrics
  float completion_rate = 0.0f;  // For task groups
  int items_added_this_week = 0;
  bool is_active = true;  // Has activity in last 7 days
};

// ===== Visual Task Card =====

// Visual card with thumbnail, progress, natural display
class VisualTaskCard : public views::View {
 public:
  METADATA_HEADER(VisualTaskCard);

  VisualTaskCard(const bookmarks::BookmarkNode* bookmark,
                 const TaskMetadata* task_metadata);
  ~VisualTaskCard() override;

  // Visual elements
  void SetThumbnail(const gfx::ImageSkia& thumbnail);
  void SetProgress(int percent);
  void UpdateTimeEstimate(base::TimeDelta remaining);

  // Natural language display
  std::u16string GetNaturalDescription() const;  // "Read this article"
  std::u16string GetTimeDisplay() const;  // "5 min read", "2 hours", "tomorrow"

  // Quick actions (hover overlay)
  void ShowQuickActions();
  void HideQuickActions();

  // Gestures
  void OnSwipeRight();  // Complete
  void OnSwipeLeft();   // Snooze
  void OnLongPress();   // Open options

  [[nodiscard]] const bookmarks::BookmarkNode* bookmark() const {
    return bookmark_;
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

 private:
  void CreateLayout();
  void UpdateVisuals();

  raw_ptr<const bookmarks::BookmarkNode> bookmark_;
  raw_ptr<const TaskMetadata> task_metadata_;

  gfx::ImageSkia thumbnail_;
  int progress_percent_ = 0;
  bool showing_quick_actions_ = false;
  bool is_hovered_ = false;
};

// ===== Quick Capture Bar =====

// Instant task/bookmark creation with natural language
class QuickCaptureBar : public views::View,
                        public views::TextfieldController {
 public:
  METADATA_HEADER(QuickCaptureBar);

  explicit QuickCaptureBar(BookmarkManager* manager,
                          BookmarkTaskManager* task_manager);
  ~QuickCaptureBar() override;

  // Natural language capture
  // "Read this later" -> Creates bookmark with Read task type
  // "Buy running shoes" -> Creates bookmark to current page with Buy type
  // "Research React hooks" -> Creates task with Research type
  // "Due tomorrow: finish report" -> Creates task with due date
  void CaptureFromText(std::u16string_view text);

  // One-click capture from current page
  void CaptureCurrentPage(const std::u16string& natural_action);

  // Show with focus (keyboard shortcut or button)
  void ShowAndFocus();
  void Hide();

  // Auto-suggest as user types
  void ShowSuggestions(std::u16string_view partial_text);

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                      const std::u16string& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                     const ui::KeyEvent& event) override;

 private:
  void CreateLayout();
  void ParseNaturalLanguage(std::u16string_view text,
                           TaskMetadata& out_metadata);
  void ExtractDueDate(std::u16string_view text,
                     std::optional<base::Time>& out_date);
  TaskType DetectTaskType(std::u16string_view text);

  raw_ptr<BookmarkManager> manager_;
  raw_ptr<BookmarkTaskManager> task_manager_;
  raw_ptr<views::Textfield> input_field_;
  raw_ptr<views::View> suggestions_container_;

  std::vector<std::u16string> recent_captures_;
};

// ===== Context Engine =====

// Surfaces relevant bookmarks/tasks based on current context
class ContextEngine {
 public:
  explicit ContextEngine(BookmarkManager* manager,
                        BookmarkTaskManager* task_manager);
  ~ContextEngine();

  // Get relevant items for current context
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  GetRelevantItems(const BrowsingContext& context, int max_count = 10);

  // Get suggested tasks for current time/activity
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  GetSuggestedTasks(const BrowsingContext& context);

  // Get related bookmarks to current page
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  GetRelatedBookmarks(std::u16string_view current_url, int max_count = 5);

  // Smart "continue where you left off"
  [[nodiscard]] std::vector<const bookmarks::BookmarkNode*>
  GetContinueWorkingTasks();

  // Proactive suggestions
  struct Suggestion {
    std::u16string message;  // "Continue reading this article?"
    const bookmarks::BookmarkNode* bookmark;
    float confidence;  // 0-1
    std::u16string reason;  // "You were reading this yesterday"
  };
  [[nodiscard]] std::vector<Suggestion> GetProactiveSuggestions(
      const BrowsingContext& context);

 private:
  int CalculateContextScore(const bookmarks::BookmarkNode* bookmark,
                           const BrowsingContext& context);
  bool IsSameDomain(std::u16string_view url1, std::u16string_view url2);
  std::vector<std::u16string> ExtractKeywords(std::u16string_view text);

  raw_ptr<BookmarkManager> manager_;
  raw_ptr<BookmarkTaskManager> task_manager_;

  // Learning data
  std::map<std::u16string, std::vector<const bookmarks::BookmarkNode*>>
      domain_clusters_;
  std::map<BrowsingContext::TimeContext, std::vector<const bookmarks::BookmarkNode*>>
      time_patterns_;
};

// ===== Visual Task Flow =====

// Natural task progression view (better than kanban for everyday use)
class VisualTaskFlowView : public views::View {
 public:
  METADATA_HEADER(VisualTaskFlowView);

  explicit VisualTaskFlowView(BookmarkTaskManager* task_manager);
  ~VisualTaskFlowView() override;

  // Flow sections (not rigid columns like kanban)
  enum class FlowSection {
    kNow,        // Do right now (0-3 items, focused)
    kNext,       // Do next (3-5 items)
    kSoon,       // Do soon (5-10 items)
    kSomeday,    // Someday/maybe (collapsed by default)
    kWaiting     // Waiting on something (collapsed)
  };

  // Natural operations
  void MoveToNow(const bookmarks::BookmarkNode* task);
  void MoveToNext(const bookmarks::BookmarkNode* task);
  void PushToSoon(const bookmarks::BookmarkNode* task);
  void Complete(const bookmarks::BookmarkNode* task);

  // Smart auto-arrangement
  void AutoArrange();  // Moves items based on priority, due date, estimates
  void SuggestNext();  // Highlight suggested next task

  // Focus mode (show only "Now" section, full screen)
  void EnterFocusMode();
  void ExitFocusMode();

  // Bulk operations
  void ClearCompleted();
  void SnoozeAll();  // Move all to tomorrow
  void ReviewWeekly();  // Weekly review mode

 private:
  void CreateLayout();
  void UpdateFlowSections();
  void CalculateOptimalArrangement();

  raw_ptr<BookmarkTaskManager> task_manager_;

  raw_ptr<views::View> now_section_;
  raw_ptr<views::View> next_section_;
  raw_ptr<views::View> soon_section_;
  raw_ptr<views::View> someday_section_;
  raw_ptr<views::View> waiting_section_;

  bool in_focus_mode_ = false;
  std::optional<const bookmarks::BookmarkNode*> suggested_next_;
};

// ===== Smart Floating Workspace =====

// Main interface - always accessible, context-aware
class SmartFloatingWorkspace : public views::View {
 public:
  METADATA_HEADER(SmartFloatingWorkspace);

  SmartFloatingWorkspace(BookmarkManager* manager,
                        BookmarkTaskManager* task_manager);
  ~SmartFloatingWorkspace() override;

  enum class DisplayMode {
    kMinimized,   // Just floating button on edge
    kCompact,     // Quick view with 3-5 items
    kExpanded,    // Full workspace with all sections
    kFocus        // Focus mode (current task only)
  };

  // State management
  void SetDisplayMode(DisplayMode mode);
  void ToggleExpanded();
  void AutoShow();   // Show based on context
  void AutoHide();   // Hide after inactivity

  // Position (floats on screen edge)
  enum class Position {
    kRightEdge,   // Default
    kLeftEdge,
    kBottomEdge,
    kTopRight,    // Corner
    kTopLeft
  };
  void SetPosition(Position position);

  // Update context (called when page changes, time changes)
  void UpdateContext(const BrowsingContext& context);

  // Quick actions (available in all modes)
  void QuickCapture();         // Show capture bar
  void QuickComplete();        // Complete top task
  void QuickSnooze();          // Snooze top task
  void ShowRelated();          // Show related bookmarks

  // Smart features
  void EnableSmartSuggestions(bool enabled);
  void EnableAutoArrange(bool enabled);
  void EnableProactiveNotifications(bool enabled);

  // Current context display
  void ShowContextualItems();  // Show items relevant to current page
  void ShowTimeBasedItems();   // Show items for current time
  void ShowRecentActivity();   // Show recent bookmarks/tasks

 private:
  void CreateLayout();
  void CreateMinimizedView();
  void CreateCompactView();
  void CreateExpandedView();
  void CreateFocusView();

  void AnimateTransition(DisplayMode from, DisplayMode to);
  void UpdateVisibleItems();
  void RefreshGroups();

  raw_ptr<BookmarkManager> manager_;
  raw_ptr<BookmarkTaskManager> task_manager_;

  std::unique_ptr<QuickCaptureBar> capture_bar_;
  std::unique_ptr<ContextEngine> context_engine_;
  std::unique_ptr<VisualTaskFlowView> task_flow_;

  DisplayMode current_mode_ = DisplayMode::kMinimized;
  Position current_position_ = Position::kRightEdge;
  BrowsingContext current_context_;

  std::vector<SmartGroup> smart_groups_;

  bool smart_suggestions_enabled_ = true;
  bool auto_arrange_enabled_ = true;
  bool proactive_notifications_enabled_ = true;

  base::Time last_interaction_;
};

// ===== Floating Access Button =====

// Small always-visible button that expands workspace
class FloatingAccessButton : public views::Button {
 public:
  METADATA_HEADER(FloatingAccessButton);

  explicit FloatingAccessButton(PressedCallback callback);
  ~FloatingAccessButton() override;

  // Visual indicators
  void SetTaskCount(int count);       // Show number of pending tasks
  void SetUrgentIndicator(bool urgent);  // Pulsing red dot for urgent
  void ShowSuggestionBadge(bool show);   // Badge for new suggestions

  // Animations
  void Pulse();  // Attract attention
  void Glow();   // Subtle glow when relevant items available

  // views::Button:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

 private:
  int task_count_ = 0;
  bool urgent_indicator_ = false;
  bool suggestion_badge_ = false;
  bool is_glowing_ = false;
};

// ===== Address Bar Integration =====

// Integrate bookmarks/tasks into address bar
class AddressBarBookmarkIntegration {
 public:
  AddressBarBookmarkIntegration(BookmarkManager* manager,
                               BookmarkTaskManager* task_manager,
                               ContextEngine* context_engine);
  ~AddressBarBookmarkIntegration();

  // Inject relevant bookmarks/tasks into omnibox suggestions
  struct BookmarkSuggestion {
    const bookmarks::BookmarkNode* bookmark;
    std::u16string display_text;  // "📖 Read: React Hooks Guide"
    std::u16string secondary_text;  // "5 min · Due tomorrow"
    int relevance_score;  // 0-1500 (omnibox scoring)
    std::u16string icon;  // Emoji or icon identifier
  };

  [[nodiscard]] std::vector<BookmarkSuggestion> GetSuggestionsForQuery(
      std::u16string_view query,
      const BrowsingContext& context);

  // Quick actions in address bar
  // Type "task: buy milk" -> Creates task
  // Type "read: " -> Shows reading list
  // Type "work: " -> Shows work bookmarks
  [[nodiscard]] std::vector<BookmarkSuggestion> GetQuickActionSuggestions(
      std::u16string_view query);

 private:
  raw_ptr<BookmarkManager> manager_;
  raw_ptr<BookmarkTaskManager> task_manager_;
  raw_ptr<ContextEngine> context_engine_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_SMART_WORKSPACE_H_
