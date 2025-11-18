// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/advanced_bookmark_features.h"

#include <algorithm>
#include <sstream>
#include <utility>

#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "url/gurl.h"

namespace {

// HTML templates for bookmark export (Netscape format)
constexpr char kHTMLHeader[] = R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
<!-- This is an automatically generated file.
     It will be read and overwritten.
     DO NOT EDIT! -->
<META HTTP-EQUIV="Content-Type" CONTENT="text/html; charset=UTF-8">
<TITLE>Bookmarks</TITLE>
<H1>Bookmarks</H1>
<DL><p>
)";

constexpr char kHTMLFooter[] = "</DL><p>\n";

// Helper: Escape HTML special characters
std::string EscapeHTML(std::string_view text) {
  std::string result;
  result.reserve(text.size());

  for (char c : text) {
    switch (c) {
      case '&':
        result.append("&amp;");
        break;
      case '<':
        result.append("&lt;");
        break;
      case '>':
        result.append("&gt;");
        break;
      case '"':
        result.append("&quot;");
        break;
      case '\'':
        result.append("&#39;");
        break;
      default:
        result.push_back(c);
    }
  }

  return result;
}

// Helper: Format timestamp for HTML export
std::string FormatTimestamp(base::Time time) {
  return base::NumberToString(time.ToTimeT());
}

}  // namespace

AdvancedBookmarkFeatures::AdvancedBookmarkFeatures(
    bookmarks::BookmarkModel* model,
    BookmarkManager* manager)
    : bookmark_model_(model), bookmark_manager_(manager) {
  DCHECK(model);
  DCHECK(manager);
}

AdvancedBookmarkFeatures::~AdvancedBookmarkFeatures() = default;

// ===== Smart Folders =====

int64_t AdvancedBookmarkFeatures::CreateSmartFolder(
    std::u16string_view name,
    const BookmarkFilter& criteria) {
  SmartFolder folder;
  folder.id = next_smart_folder_id_++;
  folder.name = std::u16string(name);
  folder.criteria = criteria;
  folder.auto_update = true;
  folder.created = base::Time::Now();
  folder.last_updated = base::Time::Now();

  // Initial refresh
  folder.cached_results = bookmark_manager_->SearchBookmarks(criteria);

  smart_folders_[folder.id] = std::move(folder);

  return folder.id;
}

void AdvancedBookmarkFeatures::UpdateSmartFolder(
    int64_t id,
    const BookmarkFilter& criteria) {
  auto it = smart_folders_.find(id);
  if (it == smart_folders_.end()) {
    return;
  }

  it->second.criteria = criteria;
  RefreshSmartFolder(id);
}

void AdvancedBookmarkFeatures::DeleteSmartFolder(int64_t id) {
  smart_folders_.erase(id);
}

std::vector<SmartFolder> AdvancedBookmarkFeatures::GetAllSmartFolders() const {
  std::vector<SmartFolder> result;
  result.reserve(smart_folders_.size());

  for (const auto& [id, folder] : smart_folders_) {
    result.push_back(folder);
  }

  return result;
}

std::vector<const bookmarks::BookmarkNode*>
AdvancedBookmarkFeatures::GetSmartFolderContents(int64_t id) {
  auto it = smart_folders_.find(id);
  if (it == smart_folders_.end()) {
    return {};
  }

  // Refresh if auto-update is enabled
  if (it->second.auto_update) {
    RefreshSmartFolder(id);
  }

  return it->second.cached_results;
}

void AdvancedBookmarkFeatures::RefreshSmartFolder(int64_t id) {
  auto it = smart_folders_.find(id);
  if (it == smart_folders_.end()) {
    return;
  }

  it->second.cached_results =
      bookmark_manager_->SearchBookmarks(it->second.criteria);
  it->second.last_updated = base::Time::Now();
}

void AdvancedBookmarkFeatures::RefreshAllSmartFolders() {
  for (auto& [id, folder] : smart_folders_) {
    if (folder.auto_update) {
      folder.cached_results =
          bookmark_manager_->SearchBookmarks(folder.criteria);
      folder.last_updated = base::Time::Now();
    }
  }
}

// ===== Collections =====

int64_t AdvancedBookmarkFeatures::CreateCollection(std::u16string_view name) {
  Collection collection;
  collection.id = next_collection_id_++;
  collection.name = std::u16string(name);
  collection.created = base::Time::Now();
  collection.last_modified = base::Time::Now();

  collections_[collection.id] = std::move(collection);

  return collection.id;
}

void AdvancedBookmarkFeatures::DeleteCollection(int64_t id) {
  collections_.erase(id);
}

void AdvancedBookmarkFeatures::AddToCollection(
    int64_t collection_id,
    const bookmarks::BookmarkNode* bookmark) {
  if (!bookmark) {
    return;
  }

  auto it = collections_.find(collection_id);
  if (it == collections_.end()) {
    return;
  }

  it->second.manual_bookmark_ids.insert(bookmark->id());
  it->second.last_modified = base::Time::Now();
}

void AdvancedBookmarkFeatures::RemoveFromCollection(
    int64_t collection_id,
    const bookmarks::BookmarkNode* bookmark) {
  if (!bookmark) {
    return;
  }

  auto it = collections_.find(collection_id);
  if (it == collections_.end()) {
    return;
  }

  it->second.manual_bookmark_ids.erase(bookmark->id());
  it->second.last_modified = base::Time::Now();
}

void AdvancedBookmarkFeatures::SetCollectionAutoRule(
    int64_t collection_id,
    const BookmarkFilter& rule) {
  auto it = collections_.find(collection_id);
  if (it == collections_.end()) {
    return;
  }

  it->second.auto_rule = rule;
  it->second.last_modified = base::Time::Now();
}

void AdvancedBookmarkFeatures::ClearCollectionAutoRule(int64_t collection_id) {
  auto it = collections_.find(collection_id);
  if (it == collections_.end()) {
    return;
  }

  it->second.auto_rule.reset();
  it->second.last_modified = base::Time::Now();
}

std::vector<Collection> AdvancedBookmarkFeatures::GetAllCollections() const {
  std::vector<Collection> result;
  result.reserve(collections_.size());

  for (const auto& [id, collection] : collections_) {
    result.push_back(collection);
  }

  return result;
}

std::vector<const bookmarks::BookmarkNode*>
AdvancedBookmarkFeatures::GetCollectionContents(int64_t id) {
  auto it = collections_.find(id);
  if (it == collections_.end()) {
    return {};
  }

  base::flat_set<const bookmarks::BookmarkNode*> result_set;

  // Add manual bookmarks
  for (int64_t bookmark_id : it->second.manual_bookmark_ids) {
    const auto* node =
        bookmarks::GetBookmarkNodeByID(bookmark_model_, bookmark_id);
    if (node) {
      result_set.insert(node);
    }
  }

  // Add auto-matched bookmarks
  if (it->second.auto_rule.has_value()) {
    auto auto_matches =
        bookmark_manager_->SearchBookmarks(*it->second.auto_rule);
    result_set.insert(auto_matches.begin(), auto_matches.end());
  }

  return std::vector<const bookmarks::BookmarkNode*>(result_set.begin(),
                                                     result_set.end());
}

void AdvancedBookmarkFeatures::RefreshCollection(int64_t id) {
  // Collections auto-refresh when accessed via GetCollectionContents
  // This is a no-op but kept for API consistency
}

// ===== Health Analysis =====

BookmarkHealth AdvancedBookmarkFeatures::AnalyzeHealth() {
  BookmarkHealth health;
  health.analyzed_at = base::Time::Now();

  // Collect all bookmarks
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;
  CollectAllBookmarks(bookmark_model_->bookmark_bar_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->other_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->mobile_node(), all_bookmarks);

  health.total_bookmarks = all_bookmarks.size();

  // Analyze each bookmark
  for (const auto* bookmark : all_bookmarks) {
    // Check if untagged
    auto tags = bookmark_manager_->GetTagsForBookmark(bookmark);
    if (tags.empty()) {
      health.untagged++;
    }

    // Check if never visited
    const auto* metadata = bookmark_manager_->GetBookmarkMetadata(bookmark);
    if (metadata && metadata->access_count == 0) {
      health.never_visited++;
    }

    // Check if high value (rating >= 4 or access_count >= 10)
    if (metadata) {
      int rating = bookmark_manager_->GetBookmarkRating(bookmark);
      if (rating >= 4 || metadata->access_count >= 10) {
        health.high_value++;
      }

      // Check if well organized (has tags and description)
      if (!tags.empty() && !metadata->description.empty()) {
        health.well_organized++;
      }
    }

    // Check if archived
    if (bookmark_manager_->IsBookmarkArchived(bookmark)) {
      health.archived++;
    }
  }

  // Get broken links count from cache
  health.broken_links = 0;
  for (const auto& [id, result] : validation_cache_) {
    if (result.status == LinkStatus::kBroken ||
        result.status == LinkStatus::kTimeout ||
        result.status == LinkStatus::kDNSError) {
      health.broken_links++;
    }
  }

  // Get duplicates count
  auto duplicates = bookmark_manager_->FindDuplicateBookmarks();
  health.duplicates = 0;
  for (const auto& dup_set : duplicates) {
    if (dup_set.size() > 1) {
      health.duplicates += dup_set.size() - 1;  // Count extras only
    }
  }

  // Calculate health score
  health.health_score = CalculateHealthScore(health);

  // Generate recommendations
  if (health.untagged > 0) {
    health.recommendations.push_back(
        base::NumberToString16(health.untagged) +
        u" bookmarks need tags for better organization");
  }

  if (health.broken_links > 0) {
    health.recommendations.push_back(
        base::NumberToString16(health.broken_links) +
        u" bookmarks have broken links that need fixing");
  }

  if (health.duplicates > 0) {
    health.recommendations.push_back(
        base::NumberToString16(health.duplicates) +
        u" duplicate bookmarks should be merged or removed");
  }

  if (health.never_visited > health.total_bookmarks / 4) {
    health.recommendations.push_back(
        u"Consider archiving " +
        base::NumberToString16(health.never_visited) +
        u" bookmarks you haven't visited");
  }

  // Cache the results
  cached_health_ = health;

  return health;
}

std::optional<BookmarkHealth>
AdvancedBookmarkFeatures::GetLastHealthAnalysis() const {
  return cached_health_;
}

std::vector<const bookmarks::BookmarkNode*>
AdvancedBookmarkFeatures::FindBrokenLinks() const {
  std::vector<const bookmarks::BookmarkNode*> broken;

  for (const auto& [id, result] : validation_cache_) {
    if (result.status == LinkStatus::kBroken ||
        result.status == LinkStatus::kTimeout ||
        result.status == LinkStatus::kDNSError ||
        result.status == LinkStatus::kSSLError) {
      if (result.bookmark) {
        broken.push_back(result.bookmark);
      }
    }
  }

  return broken;
}

std::vector<const bookmarks::BookmarkNode*>
AdvancedBookmarkFeatures::FindUntaggedBookmarks() const {
  std::vector<const bookmarks::BookmarkNode*> untagged;
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;

  CollectAllBookmarks(bookmark_model_->bookmark_bar_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->other_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->mobile_node(), all_bookmarks);

  for (const auto* bookmark : all_bookmarks) {
    auto tags = bookmark_manager_->GetTagsForBookmark(bookmark);
    if (tags.empty()) {
      untagged.push_back(bookmark);
    }
  }

  return untagged;
}

std::vector<const bookmarks::BookmarkNode*>
AdvancedBookmarkFeatures::FindNeverVisitedBookmarks() const {
  std::vector<const bookmarks::BookmarkNode*> never_visited;
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;

  CollectAllBookmarks(bookmark_model_->bookmark_bar_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->other_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->mobile_node(), all_bookmarks);

  for (const auto* bookmark : all_bookmarks) {
    const auto* metadata = bookmark_manager_->GetBookmarkMetadata(bookmark);
    if (metadata && metadata->access_count == 0) {
      never_visited.push_back(bookmark);
    }
  }

  return never_visited;
}

std::vector<std::u16string>
AdvancedBookmarkFeatures::GetHealthRecommendations() {
  if (!cached_health_.has_value()) {
    AnalyzeHealth();
  }

  return cached_health_->recommendations;
}

// ===== Link Validation =====

void AdvancedBookmarkFeatures::ValidateAllLinks(ValidationCallback callback) {
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;
  CollectAllBookmarks(bookmark_model_->bookmark_bar_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->other_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->mobile_node(), all_bookmarks);

  std::vector<LinkValidationResult> results;
  results.reserve(all_bookmarks.size());

  // Synchronous validation (in production, this would be async)
  for (const auto* bookmark : all_bookmarks) {
    auto result = ValidateLinkSync(bookmark);
    results.push_back(result);

    // Cache the result
    validation_cache_[bookmark->id()] = result;
  }

  std::move(callback).Run(std::move(results));
}

void AdvancedBookmarkFeatures::ValidateLink(
    const bookmarks::BookmarkNode* bookmark,
    base::OnceCallback<void(LinkValidationResult)> callback) {
  if (!bookmark) {
    std::move(callback).Run(LinkValidationResult{});
    return;
  }

  // Check cache first
  auto it = validation_cache_.find(bookmark->id());
  if (it != validation_cache_.end()) {
    // Return cached result if recent (< 1 day old)
    if (base::Time::Now() - it->second.checked_at < base::Days(1)) {
      std::move(callback).Run(it->second);
      return;
    }
  }

  // Validate synchronously (in production, this would be async HTTP request)
  auto result = ValidateLinkSync(bookmark);
  validation_cache_[bookmark->id()] = result;

  std::move(callback).Run(result);
}

std::vector<LinkValidationResult>
AdvancedBookmarkFeatures::GetCachedValidationResults() const {
  std::vector<LinkValidationResult> results;
  results.reserve(validation_cache_.size());

  for (const auto& [id, result] : validation_cache_) {
    results.push_back(result);
  }

  return results;
}

void AdvancedBookmarkFeatures::ClearValidationCache() {
  validation_cache_.clear();
}

// ===== Related Bookmarks =====

std::vector<RelatedBookmark> AdvancedBookmarkFeatures::FindRelatedBookmarks(
    const bookmarks::BookmarkNode* bookmark,
    size_t max_results) {
  if (!bookmark || !bookmark->is_url()) {
    return {};
  }

  std::vector<RelatedBookmark> related;
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;

  CollectAllBookmarks(bookmark_model_->bookmark_bar_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->other_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->mobile_node(), all_bookmarks);

  for (const auto* other : all_bookmarks) {
    if (other == bookmark || !other->is_url()) {
      continue;
    }

    RelatedBookmark rel;
    rel.bookmark = other;
    rel.similarity_score = 0.0f;

    // Check same domain
    if (GetDomain(bookmark->url()) == GetDomain(other->url())) {
      rel.relation_type = RelationType::kSameDomain;
      rel.similarity_score = 0.8f;
      related.push_back(rel);
      continue;
    }

    // Check similar tags
    float tag_similarity = CalculateTagSimilarity(bookmark, other);
    if (tag_similarity > 0.3f) {
      rel.relation_type = RelationType::kSimilarTags;
      rel.similarity_score = tag_similarity;

      // Get shared tags
      auto tags1 = bookmark_manager_->GetTagsForBookmark(bookmark);
      auto tags2 = bookmark_manager_->GetTagsForBookmark(other);
      for (const auto& tag : tags1) {
        if (std::find(tags2.begin(), tags2.end(), tag) != tags2.end()) {
          rel.shared_tags.push_back(tag);
        }
      }

      related.push_back(rel);
    }
  }

  // Sort by similarity score (descending)
  std::sort(related.begin(), related.end(),
           [](const RelatedBookmark& a, const RelatedBookmark& b) {
             return a.similarity_score > b.similarity_score;
           });

  // Limit results
  if (related.size() > max_results) {
    related.resize(max_results);
  }

  return related;
}

std::vector<const bookmarks::BookmarkNode*>
AdvancedBookmarkFeatures::FindSameDomainBookmarks(
    const bookmarks::BookmarkNode* bookmark) {
  if (!bookmark || !bookmark->is_url()) {
    return {};
  }

  std::string domain = GetDomain(bookmark->url());
  std::vector<const bookmarks::BookmarkNode*> same_domain;
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;

  CollectAllBookmarks(bookmark_model_->bookmark_bar_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->other_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->mobile_node(), all_bookmarks);

  for (const auto* other : all_bookmarks) {
    if (other != bookmark && other->is_url() &&
        GetDomain(other->url()) == domain) {
      same_domain.push_back(other);
    }
  }

  return same_domain;
}

std::vector<const bookmarks::BookmarkNode*>
AdvancedBookmarkFeatures::FindSimilarTaggedBookmarks(
    const bookmarks::BookmarkNode* bookmark,
    size_t min_shared_tags) {
  if (!bookmark) {
    return {};
  }

  auto target_tags = bookmark_manager_->GetTagsForBookmark(bookmark);
  if (target_tags.empty()) {
    return {};
  }

  std::vector<const bookmarks::BookmarkNode*> similar;
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;

  CollectAllBookmarks(bookmark_model_->bookmark_bar_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->other_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->mobile_node(), all_bookmarks);

  for (const auto* other : all_bookmarks) {
    if (other == bookmark) {
      continue;
    }

    auto other_tags = bookmark_manager_->GetTagsForBookmark(other);
    if (other_tags.empty()) {
      continue;
    }

    // Count shared tags
    size_t shared_count = 0;
    for (const auto& tag : target_tags) {
      if (std::find(other_tags.begin(), other_tags.end(), tag) !=
          other_tags.end()) {
        shared_count++;
      }
    }

    if (shared_count >= min_shared_tags) {
      similar.push_back(other);
    }
  }

  return similar;
}

// ===== HTML Import/Export =====

std::string AdvancedBookmarkFeatures::ExportToHTML() const {
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;
  CollectAllBookmarks(bookmark_model_->bookmark_bar_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->other_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->mobile_node(), all_bookmarks);

  return ExportBookmarksToHTML(all_bookmarks);
}

std::string AdvancedBookmarkFeatures::ExportBookmarksToHTML(
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks) const {
  std::ostringstream html;

  html << kHTMLHeader;

  // Group bookmarks by folder for better organization
  base::flat_map<const bookmarks::BookmarkNode*, std::vector<const bookmarks::BookmarkNode*>>
      bookmarks_by_folder;

  for (const auto* bookmark : bookmarks) {
    if (bookmark->is_url() && bookmark->parent()) {
      bookmarks_by_folder[bookmark->parent()].push_back(bookmark);
    }
  }

  // Export bookmarks organized by folder
  for (const auto& [folder, folder_bookmarks] : bookmarks_by_folder) {
    // Folder header
    html << "    <DT><H3 ADD_DATE=\""
         << FormatTimestamp(folder->date_added()) << "\">"
         << EscapeHTML(base::UTF16ToUTF8(folder->GetTitle()))
         << "</H3>\n";
    html << "    <DL><p>\n";

    // Bookmarks in this folder
    for (const auto* bookmark : folder_bookmarks) {
      const auto* metadata = bookmark_manager_->GetBookmarkMetadata(bookmark);

      html << "        <DT><A HREF=\""
           << EscapeHTML(bookmark->url().spec())
           << "\" ADD_DATE=\""
           << FormatTimestamp(bookmark->date_added())
           << "\"";

      // Add metadata as custom attributes
      if (metadata) {
        if (!metadata->tags.empty()) {
          std::string tags_str;
          for (size_t i = 0; i < metadata->tags.size(); ++i) {
            if (i > 0) tags_str += ",";
            tags_str += base::UTF16ToUTF8(metadata->tags[i]);
          }
          html << " TAGS=\"" << EscapeHTML(tags_str) << "\"";
        }

        if (metadata->rating > 0) {
          html << " RATING=\"" << metadata->rating << "\"";
        }

        if (metadata->is_favorite) {
          html << " FAVORITE=\"1\"";
        }
      }

      html << ">"
           << EscapeHTML(base::UTF16ToUTF8(bookmark->GetTitle()))
           << "</A>\n";

      // Add description as DD element
      if (metadata && !metadata->description.empty()) {
        html << "        <DD>"
             << EscapeHTML(base::UTF16ToUTF8(metadata->description))
             << "\n";
      }
    }

    html << "    </DL><p>\n";
  }

  html << kHTMLFooter;

  return html.str();
}

size_t AdvancedBookmarkFeatures::ImportFromHTML(std::string_view html_data) {
  // Basic HTML parsing (in production, use a proper HTML parser)
  // This is a simplified implementation for demonstration

  size_t imported = 0;

  // Find all <A HREF=...> tags
  size_t pos = 0;
  while ((pos = html_data.find("<A HREF=\"", pos)) != std::string_view::npos) {
    pos += 9;  // Skip "<A HREF=\""

    // Extract URL
    size_t url_end = html_data.find("\"", pos);
    if (url_end == std::string_view::npos) break;

    std::string url(html_data.substr(pos, url_end - pos));
    pos = url_end + 1;

    // Extract title
    size_t title_start = html_data.find(">", pos);
    if (title_start == std::string_view::npos) break;
    title_start++;

    size_t title_end = html_data.find("</A>", title_start);
    if (title_end == std::string_view::npos) break;

    std::string title(html_data.substr(title_start, title_end - title_start));

    // Create bookmark
    const bookmarks::BookmarkNode* new_bookmark =
        bookmark_model_->AddURL(bookmark_model_->bookmark_bar_node(),
                               bookmark_model_->bookmark_bar_node()->children().size(),
                               base::UTF8ToUTF16(title),
                               GURL(url));

    if (new_bookmark) {
      bookmark_manager_->OnBookmarkAdded(new_bookmark);
      imported++;
    }

    pos = title_end;
  }

  return imported;
}

// ===== Quick Access =====

std::vector<const bookmarks::BookmarkNode*>
AdvancedBookmarkFeatures::GetMostUsedBookmarks(size_t max_count) {
  auto all_bookmarks = bookmark_manager_->GetSortedBookmarks(
      BookmarkSortOrder::kMostVisited, false);

  if (all_bookmarks.size() > max_count) {
    all_bookmarks.resize(max_count);
  }

  return all_bookmarks;
}

std::vector<const bookmarks::BookmarkNode*>
AdvancedBookmarkFeatures::GetHighValueBookmarks(size_t max_count) {
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;
  CollectAllBookmarks(bookmark_model_->bookmark_bar_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->other_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->mobile_node(), all_bookmarks);

  // Filter and score
  std::vector<std::pair<const bookmarks::BookmarkNode*, int>> scored;

  for (const auto* bookmark : all_bookmarks) {
    const auto* metadata = bookmark_manager_->GetBookmarkMetadata(bookmark);
    if (!metadata) {
      continue;
    }

    int score = 0;

    // Rating contributes heavily (0-50 points)
    score += bookmark_manager_->GetBookmarkRating(bookmark) * 10;

    // Access count (0-30 points)
    score += std::min(metadata->access_count * 3, 30);

    // Favorite bonus (20 points)
    if (bookmark_manager_->IsBookmarkFavorite(bookmark)) {
      score += 20;
    }

    if (score > 0) {
      scored.emplace_back(bookmark, score);
    }
  }

  // Sort by score (descending)
  std::sort(scored.begin(), scored.end(),
           [](const auto& a, const auto& b) {
             return a.second > b.second;
           });

  // Extract bookmarks
  std::vector<const bookmarks::BookmarkNode*> result;
  result.reserve(std::min(max_count, scored.size()));

  for (size_t i = 0; i < std::min(max_count, scored.size()); ++i) {
    result.push_back(scored[i].first);
  }

  return result;
}

std::vector<const bookmarks::BookmarkNode*>
AdvancedBookmarkFeatures::GetBookmarksNeedingAttention(size_t max_count) {
  std::vector<const bookmarks::BookmarkNode*> all_bookmarks;
  CollectAllBookmarks(bookmark_model_->bookmark_bar_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->other_node(), all_bookmarks);
  CollectAllBookmarks(bookmark_model_->mobile_node(), all_bookmarks);

  std::vector<std::pair<const bookmarks::BookmarkNode*, int>> scored;

  for (const auto* bookmark : all_bookmarks) {
    const auto* metadata = bookmark_manager_->GetBookmarkMetadata(bookmark);
    if (!metadata) {
      continue;
    }

    int attention_score = 0;

    // Never visited (30 points)
    if (metadata->access_count == 0) {
      attention_score += 30;
    }

    // No tags (20 points)
    if (bookmark_manager_->GetTagsForBookmark(bookmark).empty()) {
      attention_score += 20;
    }

    // No description (10 points)
    if (metadata->description.empty()) {
      attention_score += 10;
    }

    // Low rating (15 points)
    if (bookmark_manager_->GetBookmarkRating(bookmark) <= 2) {
      attention_score += 15;
    }

    // Old and unused (25 points)
    if (metadata->access_count == 0 &&
        (base::Time::Now() - metadata->date_added) > base::Days(90)) {
      attention_score += 25;
    }

    if (attention_score > 0) {
      scored.emplace_back(bookmark, attention_score);
    }
  }

  // Sort by attention score (descending)
  std::sort(scored.begin(), scored.end(),
           [](const auto& a, const auto& b) {
             return a.second > b.second;
           });

  // Extract bookmarks
  std::vector<const bookmarks::BookmarkNode*> result;
  result.reserve(std::min(max_count, scored.size()));

  for (size_t i = 0; i < std::min(max_count, scored.size()); ++i) {
    result.push_back(scored[i].first);
  }

  return result;
}

// ===== Private Helpers =====

void AdvancedBookmarkFeatures::CollectAllBookmarks(
    const bookmarks::BookmarkNode* node,
    std::vector<const bookmarks::BookmarkNode*>& bookmarks) const {
  if (!node) {
    return;
  }

  if (node->is_url()) {
    bookmarks.push_back(node);
  }

  for (const auto& child : node->children()) {
    CollectAllBookmarks(child.get(), bookmarks);
  }
}

int AdvancedBookmarkFeatures::CalculateHealthScore(
    const BookmarkHealth& health) const {
  if (health.total_bookmarks == 0) {
    return 100;
  }

  int score = 100;

  // Penalize broken links (up to -30 points)
  float broken_ratio =
      static_cast<float>(health.broken_links) / health.total_bookmarks;
  score -= static_cast<int>(broken_ratio * 30);

  // Penalize duplicates (up to -20 points)
  float dup_ratio =
      static_cast<float>(health.duplicates) / health.total_bookmarks;
  score -= static_cast<int>(dup_ratio * 20);

  // Penalize untagged (up to -20 points)
  float untagged_ratio =
      static_cast<float>(health.untagged) / health.total_bookmarks;
  score -= static_cast<int>(untagged_ratio * 20);

  // Penalize never visited (up to -15 points)
  float never_visited_ratio =
      static_cast<float>(health.never_visited) / health.total_bookmarks;
  score -= static_cast<int>(never_visited_ratio * 15);

  // Bonus for high value bookmarks (up to +15 points)
  float high_value_ratio =
      static_cast<float>(health.high_value) / health.total_bookmarks;
  score += static_cast<int>(high_value_ratio * 15);

  // Clamp to 0-100
  return std::clamp(score, 0, 100);
}

std::string AdvancedBookmarkFeatures::GetDomain(const GURL& url) const {
  if (!url.is_valid()) {
    return "";
  }

  return url.host();
}

float AdvancedBookmarkFeatures::CalculateTagSimilarity(
    const bookmarks::BookmarkNode* a,
    const bookmarks::BookmarkNode* b) const {
  auto tags_a = bookmark_manager_->GetTagsForBookmark(a);
  auto tags_b = bookmark_manager_->GetTagsForBookmark(b);

  if (tags_a.empty() || tags_b.empty()) {
    return 0.0f;
  }

  // Count shared tags
  size_t shared = 0;
  for (const auto& tag : tags_a) {
    if (std::find(tags_b.begin(), tags_b.end(), tag) != tags_b.end()) {
      shared++;
    }
  }

  // Jaccard similarity: intersection / union
  size_t union_size = tags_a.size() + tags_b.size() - shared;
  return static_cast<float>(shared) / union_size;
}

LinkValidationResult AdvancedBookmarkFeatures::ValidateLinkSync(
    const bookmarks::BookmarkNode* bookmark) {
  LinkValidationResult result;
  result.bookmark = bookmark;
  result.checked_at = base::Time::Now();

  if (!bookmark || !bookmark->is_url()) {
    result.status = LinkStatus::kUnknown;
    return result;
  }

  const GURL& url = bookmark->url();

  // Basic validation (in production, do actual HTTP request)
  if (!url.is_valid()) {
    result.status = LinkStatus::kBroken;
    result.error_message = "Invalid URL";
    return result;
  }

  if (!url.SchemeIsHTTPOrHTTPS()) {
    result.status = LinkStatus::kUnknown;
    result.error_message = "Non-HTTP(S) scheme";
    return result;
  }

  // Assume valid for now (in production, make HTTP HEAD request)
  result.status = LinkStatus::kValid;
  result.http_code = 200;

  return result;
}
