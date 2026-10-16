#pragma once

#include <string>

/// Personal Thai word lists on the SD card (this fork; see lib/Thai/README.md).
///
/// When a book opens, these plain UTF-8 files (one word per line, '#' comments)
/// are merged and handed to the Thai line breaker as extra dictionary words:
///   /thai-words.txt                 everything you read
///   <book folder>/thai-words.txt    a series (epub.lan sends one folder per series)
///   <book path without ext>.words.txt   one book
/// Typical content: character and place names, which the built-in dictionary
/// would otherwise split at odd places. Limits: ThaiBrk::kMaxUserWords words,
/// 32 KB per file.
namespace ThaiUserDictionary {

/// Loads the lists that apply to `bookPath` (replacing any previous ones).
/// Returns the number of words now active.
size_t loadForBook(const std::string& bookPath);

/// Drops the active list and frees its memory.
void unload();

/// Book caches hold line breaks computed with the word list of the time. If the
/// active list differs from the one recorded in `cacheDir`, remove `layoutPath`
/// (a directory or a file of cached layout) and record the new list.
void invalidateStaleLayout(const std::string& cacheDir, const std::string& layoutPath);

}  // namespace ThaiUserDictionary
