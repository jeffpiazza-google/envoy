#include <vector>

#include "common/buffer/buffer_impl.h"
#include "common/common/assert.h"

#include "extensions/filters/network/common/redis/codec_impl.h"

#include "test/extensions/filters/network/common/redis/mocks.h"
#include "test/test_common/printers.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"

using testing::InSequence;

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace Common {
namespace Redis {

class RedisRespValueTest : public testing::Test {
public:
  void makeBulkStringArray(RespValue& value, const std::vector<std::string>& strings) {
    std::vector<RespValue> values(strings.size());
    for (uint64_t i = 0; i < strings.size(); i++) {
      values[i].type(RespType::BulkString);
      values[i].asString() = strings[i];
    }

    value.type(RespType::Array);
    value.asArray().swap(values);
  }

  void makeArray(RespValue& value, const std::vector<RespValue> items) {
    value.type(RespType::Array);
    value.asArray().insert(value.asArray().end(), items.begin(), items.end());
  }
};

TEST_F(RedisRespValueTest, EqualityTestingAndCopyingTest) {
  InSequence s;

  RespValue value1, value2, value3;

  makeBulkStringArray(value1, {"get", "foo", "bar", "now"});
  makeBulkStringArray(value2, {"get", "foo", "bar", "now"});
  makeBulkStringArray(value3, {"get", "foo", "bar", "later"});

  EXPECT_TRUE(value1 == value2);
  EXPECT_FALSE(value1 == value3);

  RespValue value4, value5;
  value4.type(RespType::Array);
  value4.asArray() = {value1, value2};
  value5.type(RespType::Array);
  value5.asArray() = {value1, value3};

  EXPECT_FALSE(value4 == value5);
  EXPECT_TRUE(value4 == value4);
  EXPECT_TRUE(value5 == value5);

  RespValue bulkstring_value, simplestring_value, error_value, integer_value, null_value;
  bulkstring_value.type(RespType::BulkString);
  simplestring_value.type(RespType::SimpleString);
  error_value.type(RespType::Error);
  integer_value.type(RespType::Integer);
  integer_value.asInteger() = 123;

  EXPECT_NE(bulkstring_value, simplestring_value);
  EXPECT_NE(bulkstring_value, error_value);
  EXPECT_NE(bulkstring_value, integer_value);
  EXPECT_NE(bulkstring_value, null_value);

  RespValue value6, value7, value8;
  makeArray(value6,
            {bulkstring_value, simplestring_value, error_value, integer_value, null_value, value1});
  makeArray(value7,
            {bulkstring_value, simplestring_value, error_value, integer_value, null_value, value2});
  makeArray(value8,
            {bulkstring_value, simplestring_value, error_value, integer_value, null_value, value3});

  // This may look weird, but it is a way to actually do self-assignment without generating compiler
  // warnings. Self-assignment should succeed without changing the RespValue, and therefore no
  // expectations should change.
  RespValue* value6_ptr = &value6;
  value6 = *value6_ptr;
  EXPECT_EQ(value6, value7);
  EXPECT_NE(value6, value8);
  EXPECT_NE(value7, value8);
  EXPECT_EQ(value6.asArray()[5].asArray()[3].asString(), "now");
  EXPECT_EQ(value7.asArray()[5].asArray()[3].asString(), "now");
  EXPECT_EQ(value8.asArray()[5].asArray()[3].asString(), "later");

  value8 = value1;
  EXPECT_EQ(value8.type(), RespType::Array);
  EXPECT_EQ(value8.asArray().size(), value1.asArray().size());
  EXPECT_EQ(value8.asArray().size(), 4);
  for (unsigned int i = 0; i < value8.asArray().size(); i++) {
    EXPECT_EQ(value8.asArray()[i].type(), RespType::BulkString);
    EXPECT_EQ(value8.asArray()[i].asString(), value1.asArray()[i].asString());
  }
  value7 = value1;
  EXPECT_EQ(value7, value8);
  value7 = value3;
  EXPECT_NE(value7, value8);

  value8 = bulkstring_value;
  EXPECT_EQ(value8.type(), RespType::BulkString);
  value8 = simplestring_value;
  EXPECT_EQ(value8.type(), RespType::SimpleString);
  value8 = error_value;
  EXPECT_EQ(value8.type(), RespType::Error);
  value8 = integer_value;
  EXPECT_EQ(value8.type(), RespType::Integer);
  value8 = null_value;
  EXPECT_EQ(value8.type(), RespType::Null);
}

class RedisEncoderDecoderImplTest : public testing::Test, public DecoderCallbacks {
public:
  RedisEncoderDecoderImplTest() : decoder_(*this) {}

  // RedisProxy::DecoderCallbacks
  void onRespValue(RespValuePtr&& value) override {
    decoded_values_.emplace_back(std::move(value));
  }

  EncoderImpl encoder_;
  DecoderImpl decoder_;
  Buffer::OwnedImpl buffer_;
  std::vector<RespValuePtr> decoded_values_;
};

TEST_F(RedisEncoderDecoderImplTest, Null) {
  RespValue value;
  EXPECT_EQ("null", value.toString());
  encoder_.encode(value, buffer_);
  EXPECT_EQ("$-1\r\n", buffer_.toString());
  decoder_.decode(buffer_);
  EXPECT_EQ(value, *decoded_values_[0]);
  EXPECT_EQ(0UL, buffer_.length());
}

TEST_F(RedisEncoderDecoderImplTest, Error) {
  RespValue value;
  value.type(RespType::Error);
  value.asString() = "error";
  EXPECT_EQ("\"error\"", value.toString());
  encoder_.encode(value, buffer_);
  EXPECT_EQ("-error\r\n", buffer_.toString());
  decoder_.decode(buffer_);
  EXPECT_EQ(value, *decoded_values_[0]);
  EXPECT_EQ(0UL, buffer_.length());
}

TEST_F(RedisEncoderDecoderImplTest, SimpleString) {
  RespValue value;
  value.type(RespType::SimpleString);
  value.asString() = "simple string";
  EXPECT_EQ("\"simple string\"", value.toString());
  encoder_.encode(value, buffer_);
  EXPECT_EQ("+simple string\r\n", buffer_.toString());
  decoder_.decode(buffer_);
  EXPECT_EQ(value, *decoded_values_[0]);
  EXPECT_EQ(0UL, buffer_.length());
}

TEST_F(RedisEncoderDecoderImplTest, BulkString) {
  RespValue value;
  value.type(RespType::BulkString);
  value.asString() = "bulk string";
  EXPECT_EQ("\"bulk string\"", value.toString());
  encoder_.encode(value, buffer_);
  EXPECT_EQ("$11\r\nbulk string\r\n", buffer_.toString());
  decoder_.decode(buffer_);
  EXPECT_EQ(value, *decoded_values_[0]);
  EXPECT_EQ(0UL, buffer_.length());
}

TEST_F(RedisEncoderDecoderImplTest, Integer) {
  RespValue value;
  value.type(RespType::Integer);
  value.asInteger() = std::numeric_limits<int64_t>::max();
  EXPECT_EQ("9223372036854775807", value.toString());
  encoder_.encode(value, buffer_);
  EXPECT_EQ(":9223372036854775807\r\n", buffer_.toString());
  decoder_.decode(buffer_);
  EXPECT_EQ(value, *decoded_values_[0]);
  EXPECT_EQ(0UL, buffer_.length());
}

TEST_F(RedisEncoderDecoderImplTest, NegativeIntegerSmall) {
  RespValue value;
  value.type(RespType::Integer);
  value.asInteger() = -1;
  encoder_.encode(value, buffer_);
  EXPECT_EQ(":-1\r\n", buffer_.toString());
  decoder_.decode(buffer_);
  EXPECT_EQ(value, *decoded_values_[0]);
  EXPECT_EQ(0UL, buffer_.length());
}

TEST_F(RedisEncoderDecoderImplTest, NegativeIntegerLarge) {
  RespValue value;
  value.type(RespType::Integer);
  value.asInteger() = std::numeric_limits<int64_t>::min();
  encoder_.encode(value, buffer_);
  EXPECT_EQ(":-9223372036854775808\r\n", buffer_.toString());
  decoder_.decode(buffer_);
  EXPECT_EQ(value, *decoded_values_[0]);
  EXPECT_EQ(0UL, buffer_.length());
}

TEST_F(RedisEncoderDecoderImplTest, EmptyArray) {
  RespValue value;
  value.type(RespType::Array);
  EXPECT_EQ("[]", value.toString());
  encoder_.encode(value, buffer_);
  EXPECT_EQ("*0\r\n", buffer_.toString());
  decoder_.decode(buffer_);
  EXPECT_EQ(value, *decoded_values_[0]);
  EXPECT_EQ(0UL, buffer_.length());
}

TEST_F(RedisEncoderDecoderImplTest, Array) {
  std::vector<RespValue> values(2);
  values[0].type(RespType::BulkString);
  values[0].asString() = "hello";
  values[1].type(RespType::Integer);
  values[1].asInteger() = -5;

  RespValue value;
  value.type(RespType::Array);
  value.asArray().swap(values);
  EXPECT_EQ("[\"hello\", -5]", value.toString());
  encoder_.encode(value, buffer_);
  EXPECT_EQ("*2\r\n$5\r\nhello\r\n:-5\r\n", buffer_.toString());
  decoder_.decode(buffer_);
  EXPECT_EQ(value, *decoded_values_[0]);
  EXPECT_EQ(0UL, buffer_.length());
}

TEST_F(RedisEncoderDecoderImplTest, NestedArray) {
  std::vector<RespValue> nested_values(3);
  nested_values[0].type(RespType::BulkString);
  nested_values[0].asString() = "hello";
  nested_values[1].type(RespType::Integer);
  nested_values[1].asInteger() = 0;

  std::vector<RespValue> values(2);
  values[0].type(RespType::Array);
  values[0].asArray().swap(nested_values);
  values[1].type(RespType::BulkString);
  values[1].asString() = "world";

  RespValue value;
  value.type(RespType::Array);
  value.asArray().swap(values);
  encoder_.encode(value, buffer_);
  EXPECT_EQ("*2\r\n*3\r\n$5\r\nhello\r\n:0\r\n$-1\r\n$5\r\nworld\r\n", buffer_.toString());

  // To test partial decode we will feed the buffer in 1 char at a time.
  for (char c : buffer_.toString()) {
    Buffer::OwnedImpl temp_buffer(&c, 1);
    decoder_.decode(temp_buffer);
    EXPECT_EQ(0UL, temp_buffer.length());
  }

  EXPECT_EQ(value, *decoded_values_[0]);
}

TEST_F(RedisEncoderDecoderImplTest, NullArray) {
  buffer_.add("*-1\r\n");
  decoder_.decode(buffer_);
  EXPECT_EQ(RespType::Null, decoded_values_[0]->type());
}

TEST_F(RedisEncoderDecoderImplTest, InvalidType) {
  buffer_.add("^");
  EXPECT_THROW(decoder_.decode(buffer_), ProtocolError);
}

TEST_F(RedisEncoderDecoderImplTest, InvalidInteger) {
  buffer_.add(":-a");
  EXPECT_THROW(decoder_.decode(buffer_), ProtocolError);
}

TEST_F(RedisEncoderDecoderImplTest, InvalidIntegerExpectLF) {
  buffer_.add(":-123\ra");
  EXPECT_THROW(decoder_.decode(buffer_), ProtocolError);
}

TEST_F(RedisEncoderDecoderImplTest, InvalidBulkStringExpectCR) {
  buffer_.add("$1\r\nab");
  EXPECT_THROW(decoder_.decode(buffer_), ProtocolError);
}

TEST_F(RedisEncoderDecoderImplTest, InvalidBulkStringExpectLF) {
  buffer_.add("$1\r\na\ra");
  EXPECT_THROW(decoder_.decode(buffer_), ProtocolError);
}

} // namespace Redis
} // namespace Common
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
