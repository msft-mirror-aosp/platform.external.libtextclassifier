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

#include "utils/lua-utils.h"

#include <string>

#include "utils/flatbuffers/flatbuffers.h"
#include "utils/flatbuffers/mutable.h"
#include "utils/lua_utils_tests_generated.h"
#include "utils/strings/stringpiece.h"
#include "utils/test-data-test-utils.h"
#include "utils/testing/test_data_generator.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

using testing::DoubleEq;
using testing::ElementsAre;
using testing::Eq;
using testing::FloatEq;

class LuaUtilsTest : public testing::Test, protected LuaEnvironment {
 protected:
  LuaUtilsTest()
      : schema_(GetTestFileContent("utils/lua_utils_tests.bfbs")),
        flatbuffer_builder_(schema_.get()) {
    EXPECT_THAT(RunProtected([this] {
                  LoadDefaultLibraries();
                  return LUA_OK;
                }),
                Eq(LUA_OK));
  }

  void RunScript(StringPiece script) {
    EXPECT_THAT(luaL_loadbuffer(state_, script.data(), script.size(),
                                /*name=*/nullptr),
                Eq(LUA_OK));
    EXPECT_THAT(
        lua_pcall(state_, /*nargs=*/0, /*num_results=*/1, /*errfunc=*/0),
        Eq(LUA_OK));
  }

  OwnedFlatbuffer<reflection::Schema, std::string> schema_;
  MutableFlatbufferBuilder flatbuffer_builder_;
  TestDataGenerator test_data_generator_;
};

template <typename T>
class TypedLuaUtilsTest : public LuaUtilsTest {};

using testing::Types;
using LuaTypes =
    ::testing::Types<int64, uint64, int32, uint32, int16, uint16, int8, uint8,
                     float, double, bool, std::string>;
TYPED_TEST_SUITE(TypedLuaUtilsTest, LuaTypes);

TYPED_TEST(TypedLuaUtilsTest, HandlesVectors) {
  std::vector<TypeParam> elements(5);
  std::generate_n(elements.begin(), 5, [&]() {
    return this->test_data_generator_.template generate<TypeParam>();
  });

  this->PushVector(elements);

  EXPECT_THAT(this->template ReadVector<TypeParam>(),
              testing::ContainerEq(elements));
}

TYPED_TEST(TypedLuaUtilsTest, HandlesVectorIterators) {
  std::vector<TypeParam> elements(5);
  std::generate_n(elements.begin(), 5, [&]() {
    return this->test_data_generator_.template generate<TypeParam>();
  });

  this->PushVectorIterator(&elements);

  EXPECT_THAT(this->template ReadVector<TypeParam>(),
              testing::ContainerEq(elements));
}

TEST_F(LuaUtilsTest, ReadsFlatbufferResults) {
  // Setup.
  RunScript(R"lua(
    return {
        byte_field = 1,
        ubyte_field = 2,
        int_field = 10,
        uint_field = 11,
        long_field = 20,
        ulong_field = 21,
        bool_field = true,
        float_field = 42.1,
        double_field = 12.4,
        string_field = "hello there",

        -- Nested field.
        nested_field = {
          float_field = 64,
          string_field = "hello nested",
        },

        -- Repeated fields.
        repeated_byte_field = {1, 2, 1},
        repeated_ubyte_field = {2, 4, 2},
        repeated_int_field = { 1, 2, 3},
        repeated_uint_field = { 2, 4, 6},
        repeated_long_field = { 4, 5, 6},
        repeated_ulong_field = { 8, 10, 12},
        repeated_bool_field = {true, false, true},
        repeated_float_field = { 1.23, 2.34, 3.45},
        repeated_double_field = { 1.11, 2.22, 3.33},
        repeated_string_field = { "a", "bold", "one" },
        repeated_nested_field = {
          { string_field = "a" },
          { string_field = "b" },
          { repeated_string_field = { "nested", "nested2" } },
        },
    }
  )lua");

  // Read the flatbuffer.
  std::unique_ptr<MutableFlatbuffer> buffer = flatbuffer_builder_.NewRoot();
  ReadFlatbuffer(/*index=*/-1, buffer.get());
  const std::string serialized_buffer = buffer->Serialize();

  std::unique_ptr<test::TestDataT> test_data =
      LoadAndVerifyMutableFlatbuffer<test::TestData>(buffer->Serialize());

  EXPECT_THAT(test_data->byte_field, 1);
  EXPECT_THAT(test_data->ubyte_field, 2);
  EXPECT_THAT(test_data->int_field, 10);
  EXPECT_THAT(test_data->uint_field, 11);
  EXPECT_THAT(test_data->long_field, 20);
  EXPECT_THAT(test_data->ulong_field, 21);
  EXPECT_THAT(test_data->bool_field, true);
  EXPECT_THAT(test_data->float_field, FloatEq(42.1));
  EXPECT_THAT(test_data->double_field, DoubleEq(12.4));
  EXPECT_THAT(test_data->string_field, "hello there");

  EXPECT_THAT(test_data->repeated_byte_field, ElementsAre(1, 2, 1));
  EXPECT_THAT(test_data->repeated_ubyte_field, ElementsAre(2, 4, 2));
  EXPECT_THAT(test_data->repeated_int_field, ElementsAre(1, 2, 3));
  EXPECT_THAT(test_data->repeated_uint_field, ElementsAre(2, 4, 6));
  EXPECT_THAT(test_data->repeated_long_field, ElementsAre(4, 5, 6));
  EXPECT_THAT(test_data->repeated_ulong_field, ElementsAre(8, 10, 12));
  EXPECT_THAT(test_data->repeated_bool_field, ElementsAre(true, false, true));
  EXPECT_THAT(test_data->repeated_float_field, ElementsAre(1.23, 2.34, 3.45));
  EXPECT_THAT(test_data->repeated_double_field, ElementsAre(1.11, 2.22, 3.33));

  // Nested fields.
  EXPECT_THAT(test_data->nested_field->float_field, FloatEq(64));
  EXPECT_THAT(test_data->nested_field->string_field, "hello nested");
  // Repeated nested fields.
  EXPECT_THAT(test_data->repeated_nested_field[0]->string_field, "a");
  EXPECT_THAT(test_data->repeated_nested_field[1]->string_field, "b");
  EXPECT_THAT(test_data->repeated_nested_field[2]->repeated_string_field,
              ElementsAre("nested", "nested2"));
}

TEST_F(LuaUtilsTest, HandlesSimpleFlatbufferFields) {
  // Create test flatbuffer.
  std::unique_ptr<MutableFlatbuffer> buffer = flatbuffer_builder_.NewRoot();
  buffer->Set("float_field", 42.f);
  const std::string serialized_buffer = buffer->Serialize();
  PushFlatbuffer(schema_.get(), flatbuffers::GetRoot<flatbuffers::Table>(
                                    serialized_buffer.data()));
  lua_setglobal(state_, "arg");

  // Setup.
  RunScript(R"lua(
    return arg.float_field
  )lua");

  EXPECT_THAT(Read<float>(), FloatEq(42));
}

TEST_F(LuaUtilsTest, HandlesRepeatedFlatbufferFields) {
  // Create test flatbuffer.
  std::unique_ptr<MutableFlatbuffer> buffer = flatbuffer_builder_.NewRoot();
  RepeatedField* repeated_field = buffer->Repeated("repeated_string_field");
  repeated_field->Add("this");
  repeated_field->Add("is");
  repeated_field->Add("a");
  repeated_field->Add("test");
  const std::string serialized_buffer = buffer->Serialize();
  PushFlatbuffer(schema_.get(), flatbuffers::GetRoot<flatbuffers::Table>(
                                    serialized_buffer.data()));
  lua_setglobal(state_, "arg");

  // Return flatbuffer repeated field as vector.
  RunScript(R"lua(
    return arg.repeated_string_field
  )lua");

  EXPECT_THAT(ReadVector<std::string>(),
              ElementsAre("this", "is", "a", "test"));
}

TEST_F(LuaUtilsTest, HandlesRepeatedNestedFlatbufferFields) {
  // Create test flatbuffer.
  std::unique_ptr<MutableFlatbuffer> buffer = flatbuffer_builder_.NewRoot();
  RepeatedField* repeated_field = buffer->Repeated("repeated_nested_field");
  repeated_field->Add()->Set("string_field", "hello");
  repeated_field->Add()->Set("string_field", "my");
  MutableFlatbuffer* nested = repeated_field->Add();
  nested->Set("string_field", "old");
  RepeatedField* nested_repeated = nested->Repeated("repeated_string_field");
  nested_repeated->Add("friend");
  nested_repeated->Add("how");
  nested_repeated->Add("are");
  repeated_field->Add()->Set("string_field", "you?");
  const std::string serialized_buffer = buffer->Serialize();
  PushFlatbuffer(schema_.get(), flatbuffers::GetRoot<flatbuffers::Table>(
                                    serialized_buffer.data()));
  lua_setglobal(state_, "arg");

  RunScript(R"lua(
    result = {}
    for _, nested in pairs(arg.repeated_nested_field) do
      result[#result + 1] = nested.string_field
      for _, nested_string in pairs(nested.repeated_string_field) do
        result[#result + 1] = nested_string
      end
    end
    return result
  )lua");

  EXPECT_THAT(
      ReadVector<std::string>(),
      ElementsAre("hello", "my", "old", "friend", "how", "are", "you?"));
}

TEST_F(LuaUtilsTest, CorrectlyReadsTwoFlatbuffersSimultaneously) {
  // The first flatbuffer.
  std::unique_ptr<MutableFlatbuffer> buffer = flatbuffer_builder_.NewRoot();
  buffer->Set("string_field", "first");
  const std::string serialized_buffer = buffer->Serialize();
  PushFlatbuffer(schema_.get(), flatbuffers::GetRoot<flatbuffers::Table>(
                                    serialized_buffer.data()));
  lua_setglobal(state_, "arg");
  // The second flatbuffer.
  std::unique_ptr<MutableFlatbuffer> buffer2 = flatbuffer_builder_.NewRoot();
  buffer2->Set("string_field", "second");
  const std::string serialized_buffer2 = buffer2->Serialize();
  PushFlatbuffer(schema_.get(), flatbuffers::GetRoot<flatbuffers::Table>(
                                    serialized_buffer2.data()));
  lua_setglobal(state_, "arg2");

  RunScript(R"lua(
    return {arg.string_field, arg2.string_field}
  )lua");

  EXPECT_THAT(ReadVector<std::string>(), ElementsAre("first", "second"));
}

}  // namespace
}  // namespace libtextclassifier3
