#pragma once

#include <string_view>
#include <vector>

#include "character.h"

namespace mango {

// InBytes work on byte.
// InCharacter work on grapheme.
// Any others work on codepoint

std::vector<std::string_view> StrSplit(std::string_view str,
                                       char delimiter = kSpaceChar);

// NOTE: Bytes cmpare need two strings in same encoding and have the same
// normalization, or the result will not be that accurate.

bool StrPrefixInBytes(const std::string_view sub, const std::string_view str,
                      bool filter_same_size);

bool StrFuzzyMatchInBytes(const std::string_view sub,
                          const std::string_view str, bool filter_same_size);

}  // namespace mango