/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fstream>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "utils/sentencepiece/double_array_trie.h"

namespace libtextclassifier3 {
namespace {

std::string GetTestConfigPath() {
  return "";
}

TEST(DoubleArrayTest, Lookup) {
  // Test trie that contains pieces "hell", "hello", "o", "there".
  std::ifstream test_config_stream(GetTestConfigPath());
  std::string config((std::istreambuf_iterator<char>(test_config_stream)),
                     (std::istreambuf_iterator<char>()));
  DoubleArrayTrie trie(reinterpret_cast<const TrieNode*>(config.data()),
                       config.size() / sizeof(TrieNode));

  {
    std::vector<TrieMatch> matches;
    EXPECT_TRUE(trie.FindAllPrefixMatches("hello there", &matches));
    EXPECT_EQ(matches.size(), 2);
    EXPECT_EQ(matches[0].id, 0 /*hell*/);
    EXPECT_EQ(matches[0].match_length, 4 /*hell*/);
    EXPECT_EQ(matches[1].id, 1 /*hello*/);
    EXPECT_EQ(matches[1].match_length, 5 /*hello*/);
  }

  {
    std::vector<TrieMatch> matches;
    EXPECT_TRUE(trie.FindAllPrefixMatches("he", &matches));
    EXPECT_THAT(matches, testing::IsEmpty());
  }

  {
    std::vector<TrieMatch> matches;
    EXPECT_TRUE(trie.FindAllPrefixMatches("abcd", &matches));
    EXPECT_THAT(matches, testing::IsEmpty());
  }

  {
    std::vector<TrieMatch> matches;
    EXPECT_TRUE(trie.FindAllPrefixMatches("", &matches));
    EXPECT_THAT(matches, testing::IsEmpty());
  }

  {
    std::vector<TrieMatch> matches;
    EXPECT_TRUE(trie.FindAllPrefixMatches("hi there", &matches));
    EXPECT_THAT(matches, testing::IsEmpty());
  }

  {
    std::vector<TrieMatch> matches;
    EXPECT_TRUE(trie.FindAllPrefixMatches(StringPiece("\0", 1), &matches));
    EXPECT_THAT(matches, testing::IsEmpty());
  }

  {
    std::vector<TrieMatch> matches;
    EXPECT_TRUE(
        trie.FindAllPrefixMatches(StringPiece("\xff, \xfe", 2), &matches));
    EXPECT_THAT(matches, testing::IsEmpty());
  }

  {
    TrieMatch match;
    EXPECT_TRUE(trie.LongestPrefixMatch("hella there", &match));
    EXPECT_EQ(match.id, 0 /*hell*/);
  }

  {
    TrieMatch match;
    EXPECT_TRUE(trie.LongestPrefixMatch("hello there", &match));
    EXPECT_EQ(match.id, 1 /*hello*/);
  }

  {
    TrieMatch match;
    EXPECT_TRUE(trie.LongestPrefixMatch("abcd", &match));
    EXPECT_EQ(match.id, -1);
  }

  {
    TrieMatch match;
    EXPECT_TRUE(trie.LongestPrefixMatch("", &match));
    EXPECT_EQ(match.id, -1);
  }
}

}  // namespace
}  // namespace libtextclassifier3
