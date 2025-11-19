// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_cleanup_wizard.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/bookmarks/bookmark_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "url/gurl.h"

namespace {

// Common words to exclude from tags
const char16_t* kStopWords[] = {
    u"the", u"and", u"for", u"with", u"from", u"this", u"that",
    u"have", u"has", u"are", u"was", u"were", u"been", u"being",
    u"about", u"into", u"through", u"during", u"before", u"after",
    u"above", u"below", u"between", u"under", u"again", u"further",
    u"then", u"once", u"here", u"there", u"when", u"where", u"why",
    u"how", u"all", u"each", u"other", u"some", u"such", u"only",
    u"own", u"same", u"than", u"too", u"very", u"can", u"will",
    u"just", u"should", u"now", u"www", u"http", u"https", u"com",
};

bool IsStopWord(std::u16string_view word) {
  std::u16string lower = base::ToLowerASCII(word);
  for (const auto* stop_word : kStopWords) {
    if (lower == stop_word) {
      return true;
    }
  }
  return false;
}

// Extract domain from URL
std::u16string ExtractDomain(const GURL& url) {
  if (!url.is_valid()) {
    return u"";
  }
  std::string host = url.host();

  // Remove www. prefix
  if (host.find("www.") == 0) {
    host = host.substr(4);
  }

  // Remove TLD (.com, .org, etc.)
  size_t last_dot = host.find_last_of('.');
  if (last_dot != std::string::npos && last_dot > 0) {
    host = host.substr(0, last_dot);
  }

  return base::UTF8ToUTF16(host);
}

}  // namespace

// ===== BulkTagSuggester Implementation =====

BulkTagSuggester::BulkTagSuggester(bookmarks::BookmarkModel* model,
                                   BookmarkManager* manager)
    : bookmark_model_(model), manager_(manager) {
  DCHECK(bookmark_model_);
  DCHECK(manager_);
}

BulkTagSuggester::~BulkTagSuggester() = default;

std::vector<BulkTagSuggester::TagSuggestion> BulkTagSuggester::SuggestTags(
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks) {
  DCHECK(bookmark_model_);
  DCHECK(manager_);

  std::vector<TagSuggestion> suggestions;

  for (const auto* bookmark : bookmarks) {
    DCHECK(bookmark);
    DCHECK(bookmark->is_url());

    // Check if already has tags
    const BookmarkMetadata* metadata = manager_->GetMetadata(bookmark);
    if (metadata && !metadata->tags.empty()) {
      continue;  // Skip bookmarks that already have tags
    }

    TagSuggestion suggestion;
    suggestion.bookmark = bookmark;

    // Extract tags from URL
    std::vector<std::u16string> url_tags =
        ExtractTagsFromURL(bookmark->url());

    // Extract tags from title
    std::vector<std::u16string> title_tags =
        ExtractTagsFromTitle(bookmark->GetTitle());

    // Combine and deduplicate
    std::set<std::u16string> all_tags;
    all_tags.insert(url_tags.begin(), url_tags.end());
    all_tags.insert(title_tags.begin(), title_tags.end());

    // Add folder location as a tag
    const bookmarks::BookmarkNode* parent = bookmark->parent();
    if (parent && !parent->is_permanent_node()) {
      all_tags.insert(parent->GetTitle());
    }

    suggestion.suggested_tags.assign(all_tags.begin(), all_tags.end());

    // Calculate confidence based on number of tags found
    if (suggestion.suggested_tags.size() >= 3) {
      suggestion.confidence = 0.9f;
    } else if (suggestion.suggested_tags.size() >= 2) {
      suggestion.confidence = 0.7f;
    } else if (suggestion.suggested_tags.size() >= 1) {
      suggestion.confidence = 0.5f;
    } else {
      suggestion.confidence = 0.0f;
    }

    if (!suggestion.suggested_tags.empty()) {
      suggestions.push_back(suggestion);
    }
  }

  DLOG(INFO) << "Generated " << suggestions.size() << " tag suggestions";

  return suggestions;
}

std::vector<std::u16string> BulkTagSuggester::ExtractTagsFromURL(
    const GURL& url) {
  std::vector<std::u16string> tags;

  if (!url.is_valid()) {
    return tags;
  }

  // Add domain as tag
  std::u16string domain = ExtractDomain(url);
  if (!domain.empty() && !IsStopWord(domain)) {
    tags.push_back(domain);
  }

  // Extract from path
  std::string path = url.path();
  std::vector<std::string> path_parts = base::SplitString(
      path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (const auto& part : path_parts) {
    std::u16string tag = base::UTF8ToUTF16(part);

    // Clean up and validate
    if (tag.length() >= 3 && tag.length() <= 20 && !IsStopWord(tag)) {
      // Remove file extensions
      size_t dot = tag.find_last_of('.');
      if (dot != std::u16string::npos) {
        tag = tag.substr(0, dot);
      }

      tags.push_back(tag);
    }
  }

  // Pattern detection
  std::u16string url_str = base::UTF8ToUTF16(url.spec());
  std::u16string lower_url = base::ToLowerASCII(url_str);

  if (lower_url.find(u"github") != std::u16string::npos) {
    tags.push_back(u"development");
  }
  if (lower_url.find(u"docs") != std::u16string::npos ||
      lower_url.find(u"documentation") != std::u16string::npos) {
    tags.push_back(u"documentation");
  }
  if (lower_url.find(u"tutorial") != std::u16string::npos) {
    tags.push_back(u"tutorial");
  }
  if (lower_url.find(u"blog") != std::u16string::npos) {
    tags.push_back(u"blog");
  }
  if (lower_url.find(u"api") != std::u16string::npos) {
    tags.push_back(u"api");
  }

  return tags;
}

std::vector<std::u16string> BulkTagSuggester::ExtractTagsFromTitle(
    std::u16string_view title) {
  std::vector<std::u16string> tags;

  if (title.empty()) {
    return tags;
  }

  // Split title into words
  std::vector<std::u16string> words = base::SplitString(
      title, u" -_|:;,.()", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::set<std::u16string> unique_tags;

  for (const auto& word : words) {
    std::u16string lower = base::ToLowerASCII(word);

    // Filter out stop words and short words
    if (lower.length() >= 3 && lower.length() <= 20 && !IsStopWord(lower)) {
      unique_tags.insert(lower);
    }
  }

  // Limit to top 5 most relevant words
  size_t count = 0;
  for (const auto& tag : unique_tags) {
    if (count >= 5) {
      break;
    }
    tags.push_back(tag);
    ++count;
  }

  return tags;
}

void BulkTagSuggester::ApplyTagSuggestions(
    const std::vector<TagSuggestion>& suggestions) {
  DCHECK(manager_);

  for (const auto& suggestion : suggestions) {
    // Only apply high-confidence suggestions automatically
    if (suggestion.confidence >= 0.7f) {
      for (const auto& tag : suggestion.suggested_tags) {
        manager_->AddTag(suggestion.bookmark, tag);
      }
    }
  }

  DLOG(INFO) << "Applied tag suggestions to " << suggestions.size()
             << " bookmarks";
}
