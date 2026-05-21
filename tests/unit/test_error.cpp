#include "mqtt/core/Error.hpp"

#include <gtest/gtest.h>

namespace mqtt::test {

TEST(ResultTest, OkVoid) {
    auto r = Result<void>::ok();
    EXPECT_TRUE(r.hasValue());
    EXPECT_TRUE(static_cast<bool>(r));
}

TEST(ResultTest, ErrVoid) {
    auto r = Result<void>::err(MqttException{ErrorCode::NotConnected, "not connected"});
    EXPECT_FALSE(r.hasValue());
    EXPECT_FALSE(static_cast<bool>(r));
    EXPECT_EQ(r.error().code(), ErrorCode::NotConnected);
}

TEST(ResultTest, OkInt) {
    auto r = Result<int>::ok(42);
    EXPECT_TRUE(r.hasValue());
    EXPECT_EQ(r.value(), 42);
}

TEST(ResultTest, ErrInt) {
    auto r = Result<int>::err(PublishException{ErrorCode::PublishFailed, "oops"});
    EXPECT_FALSE(r.hasValue());
    EXPECT_EQ(r.error().code(), ErrorCode::PublishFailed);
    EXPECT_EQ(r.error().message(), "oops");
}

TEST(ExceptionHierarchy, ConnectionIsBase) {
    ConnectionException ex{ErrorCode::ConnectionFailed, "fail"};
    EXPECT_EQ(ex.code(), ErrorCode::ConnectionFailed);
    EXPECT_THROW({ throw ex; }, MqttException);
}

} // namespace mqtt::test
