#pragma once

#include <string>

/// A tiny per-book note for the home screen (this fork's personal themes):
/// how far into the book you are and the chapter you were in, written by the
/// reader when you leave the book (or the device sleeps) into the book's cache
/// folder as "home-summary.txt". The home theme finds it next to the book's
/// cover thumbnail, so it never has to open the book to draw a progress line.
namespace HomeSummary {

void write(const std::string& cacheDir, int percent, const std::string& chapter);

/// False when there is no summary yet (book never left, or another firmware).
bool read(const std::string& cacheDir, int& percent, std::string& chapter);

/// The cache folder of a book, from its cover thumbnail path ("<cache>/thumb_...").
std::string cacheDirFromThumbPath(const std::string& thumbPath);

}  // namespace HomeSummary
