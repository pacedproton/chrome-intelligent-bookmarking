// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_cleanup_wizard.h"

#include <algorithm>
#include <map>
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

// Common URL patterns for categorization
struct URLPattern {
  std::u16string pattern;
  std::u16string category;
};

const URLPattern kURLPatterns[] = {
    {u"github.com", u"Development"},
    {u"stackoverflow.com", u"Development"},
    {u"docs.", u"Documentation"},
    {u"/api/", u"API Reference"},
    {u"/tutorial", u"Tutorials"},
    {u"/blog", u"Blogs"},
    {u"youtube.com", u"Videos"},
    {u"reddit.com", u"Social Media"},
    {u"twitter.com", u"Social Media"},
    {u"facebook.com", u"Social Media"},
    {u"linkedin.com", u"Professional"},
    {u"news", u"News"},
    {u"wikipedia.org", u"Reference"},
    {u"/download", u"Downloads"},
    {u"shop", u"Shopping"},
    {u"amazon.com", u"Shopping"},
};

// Extract domain from URL
std::u16string ExtractDomain(const GURL& url) {
  if (!url.is_valid()) {
    return u"";
  }
  return base::UTF8ToUTF16(url.host());
}

// Get time period string
std::u16string GetTimePeriod(base::Time time) {
  base::Time now = base::Time::Now();
  base::TimeDelta delta = now - time;

  if (delta.InDays() < 7) {
    return u"This Week";
  } else if (delta.InDays() < 30) {
    return u"This Month";
  } else if (delta.InDays() < 90) {
    return u"Last 3 Months";
  } else if (delta.InDays() < 365) {
    return u"This Year";
  } else {
    base::Time::Exploded exploded;
    time.LocalExplode(&exploded);
    return base::NumberToString16(exploded.year);
  }
}

}  // namespace

// ===== SmartCategorizer Implementation =====

SmartCategorizer::SmartCategorizer(bookmarks::BookmarkModel* model,
                                   BookmarkManager* manager)
    : bookmark_model_(model), manager_(manager) {
  DCHECK(bookmark_model_);
  DCHECK(manager_);
}

SmartCategorizer::~SmartCategorizer() = default;

std::vector<SmartCategorizer::Category> SmartCategorizer::SuggestCategories(
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks) {
  DCHECK(bookmark_model_);

  if (bookmarks.empty()) {
    return {};
  }

  std::vector<Category> categories;

  // Group by domain
  auto domain_groups = GroupByDomain(bookmarks);
  for (const auto& [domain, domain_bookmarks] : domain_groups) {
    if (domain_bookmarks.size() >= 3) {  // At least 3 bookmarks
      Category category;
      category.name = domain;
      category.description = u"Bookmarks from " + domain;
      category.bookmarks = domain_bookmarks;
      category.confidence = std::min(1.0f,
          static_cast<float>(domain_bookmarks.size()) / 10.0f);
      categories.push_back(category);
    }
  }

  // Group by URL pattern
  auto pattern_groups = GroupByPattern(bookmarks);
  for (const auto& [pattern, pattern_bookmarks] : pattern_groups) {
    if (pattern_bookmarks.size() >= 2) {  // At least 2 bookmarks
      Category category;
      category.name = pattern;
      category.description = u"Related " + pattern + u" resources";
      category.bookmarks = pattern_bookmarks;
      category.confidence = 0.8f;
      categories.push_back(category);
    }
  }

  DLOG(INFO) << "Suggested " << categories.size() << " categories for "
             << bookmarks.size() << " bookmarks";

  return categories;
}

std::map<std::u16string, std::vector<const bookmarks::BookmarkNode*>>
SmartCategorizer::GroupByDomain(
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks) {
  std::map<std::u16string, std::vector<const bookmarks::BookmarkNode*>>
      groups;

  for (const auto* bookmark : bookmarks) {
    DCHECK(bookmark);
    DCHECK(bookmark->is_url());

    std::u16string domain = ExtractDomain(bookmark->url());
    if (!domain.empty()) {
      groups[domain].push_back(bookmark);
    }
  }

  return groups;
}

std::map<std::u16string, std::vector<const bookmarks::BookmarkNode*>>
SmartCategorizer::GroupByPattern(
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks) {
  std::map<std::u16string, std::vector<const bookmarks::BookmarkNode*>>
      groups;

  for (const auto* bookmark : bookmarks) {
    DCHECK(bookmark);
    DCHECK(bookmark->is_url());

    std::u16string pattern = DetectURLPattern(bookmark->url());
    if (!pattern.empty()) {
      groups[pattern].push_back(bookmark);
    }
  }

  return groups;
}

std::map<std::u16string, std::vector<const bookmarks::BookmarkNode*>>
SmartCategorizer::GroupByTimePeriod(
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks) {
  std::map<std::u16string, std::vector<const bookmarks::BookmarkNode*>>
      groups;

  for (const auto* bookmark : bookmarks) {
    DCHECK(bookmark);

    std::u16string period = GetTimePeriod(bookmark->date_added());
    groups[period].push_back(bookmark);
  }

  return groups;
}

std::u16string SmartCategorizer::DetectURLPattern(const GURL& url) {
  if (!url.is_valid()) {
    return u"";
  }

  std::u16string url_str = base::UTF8ToUTF16(url.spec());
  std::u16string lower_url = base::ToLowerASCII(url_str);

  // Check against known patterns
  for (const auto& pattern : kURLPatterns) {
    if (lower_url.find(base::ToLowerASCII(pattern.pattern)) !=
        std::u16string::npos) {
      return pattern.category;
    }
  }

  return u"";
}
