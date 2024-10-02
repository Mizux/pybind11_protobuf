// Copyright (c) 2021 The Pybind Development Team. All rights reserved.
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/functional/function_ref.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "google/protobuf/dynamic_message.h"
#include "pybind11_protobuf/tests/test.pb.h"
#include "pybind11_protobuf/wrapped_proto_caster.h"

namespace py = ::pybind11;

namespace {

using ::pybind11::test::IntMessage;
using ::pybind11::test::TestMessage;
using ::pybind11_protobuf::WithWrappedProtos;
using ::pybind11_protobuf::WrappedProto;
using ::pybind11_protobuf::WrappedProtoKind;

const TestMessage& GetStatic() {
  static TestMessage test_message = [] {
    TestMessage msg;
    msg.set_int_value(123);
    return msg;
  }();

  return test_message;
}

bool CheckMessage(const ::google::protobuf::Message* message, int32_t value) {
  if (!message) return false;
  auto* f = message->GetDescriptor()->FindFieldByName("value");
  if (!f) f = message->GetDescriptor()->FindFieldByName("int_value");
  if (!f) return false;
  return message->GetReflection()->GetInt32(*message, f) == value;
}

bool CheckIntMessage(const IntMessage* message, int32_t value) {
  return CheckMessage(message, value);
}

class A {
 public:
  A(const IntMessage& message) : value_(message.value()) {}
  int64_t value() { return value_; }

 private:
  int64_t value_;
};

PYBIND11_MODULE(wrapped_proto_module, m) {
  pybind11_protobuf::ImportWrappedProtoCasters();

  m.def("get_test_message", WithWrappedProtos(GetStatic));

  m.def("make_int_message", WithWrappedProtos([](int value) -> IntMessage {
          IntMessage msg;
          msg.set_value(value);
          return msg;
        }),
        py::arg("value") = 123);

  m.def("fn_overload",
        [](WrappedProto<IntMessage, WrappedProtoKind::kConst> proto) {
          return 1;
        });
  m.def("fn_overload", [](const IntMessage& proto) { return 2; });

  m.def("check_int", WithWrappedProtos(&CheckIntMessage), py::arg("message"),
        py::arg("value"));

  // Check calls.
  m.def("check", WithWrappedProtos(&CheckMessage), py::arg("message"),
        py::arg("value"));

  m.def("check_cref",
        WithWrappedProtos([](const TestMessage& msg, int32_t value) {
          return CheckMessage(&msg, value);
        }),
        py::arg("proto"), py::arg("value"));
  m.def("check_cptr",
        WithWrappedProtos([](const TestMessage* msg, int32_t value) {
          return CheckMessage(msg, value);
        }),
        py::arg("proto"), py::arg("value"));
  m.def("check_val", WithWrappedProtos([](TestMessage msg, int32_t value) {
          return CheckMessage(&msg, value);
        }),
        py::arg("proto"), py::arg("value"));
  m.def("check_rval", WithWrappedProtos([](TestMessage&& msg, int32_t value) {
          return CheckMessage(&msg, value);
        }),
        py::arg("proto"), py::arg("value"));

  // WithWrappedProto does not auto-wrap mutable protos, but constructing a
  // wrapper manually will still work. Note, however, that the proto will be
  // copied.
  m.def(
      "check_mutable",
      [](WrappedProto<TestMessage, WrappedProtoKind::kMutable> msg,
         int32_t value) {
        return CheckMessage(static_cast<TestMessage*>(msg), value);
      },
      py::arg("proto"), py::arg("value"));

  // Use WithWrappedProto to define an A.__init__ method
  py::class_<A>(m, "A")
      .def(py::init(WithWrappedProtos(
          [](const IntMessage& message) { return A(message); })))
      .def("value", &A::value);

  // And wrap std::vector<Proto>
  m.def("check_int_message_list",
        WithWrappedProtos([](const std::vector<IntMessage>& v, int32_t value) {
          int i = 0;
          for (const auto& x : v) {
            i += CheckMessage(&x, value);
          }
          return i;
        }),
        py::arg("protos"), py::arg("value"));
  m.def("take_int_message_list",
        WithWrappedProtos([](std::vector<IntMessage> v, int32_t value) {
          int i = 0;
          for (const auto& x : v) {
            i += CheckMessage(&x, value);
          }
          return i;
        }),
        py::arg("protos"), py::arg("value"));

  m.def("make_int_message_list", WithWrappedProtos([](int value) {
          std::vector<IntMessage> result;
          for (int i = 0; i < 3; i++) {
            result.emplace_back();
            result.back().set_value(value);
          }
          return result;
        }),
        py::arg("value") = 123);
}

/// Below here are compile tests for fast_cpp_proto_casters
int GetInt() { return 0; }
static TestMessage kMessage;
const TestMessage& GetConstReference() { return kMessage; }
const TestMessage* GetConstPtr() { return &kMessage; }
TestMessage GetValue() { return TestMessage(); }
// Note that this should never actually run.
TestMessage&& GetRValue() { return std::move(kMessage); }
absl::StatusOr<TestMessage> GetStatusOr() { return TestMessage(); }
absl::optional<TestMessage> GetOptional() { return TestMessage(); }
std::vector<TestMessage> GetVector() { return {}; }

void PassInt(int) {}
void PassConstReference(const TestMessage&) {}
void PassConstPtr(const TestMessage*) {}
void PassValue(TestMessage) {}
void PassRValue(TestMessage&&) {}
void PassOptional(absl::optional<TestMessage>) {}
void PassVector(std::vector<TestMessage>) {}

struct Struct {
  TestMessage MemberFn() { return kMessage; }
  TestMessage ConstMemberFn() const { return kMessage; }
};

void test_static_asserts() {
  using pybind11::test::IntMessage;
  using pybind11::test::TestMessage;
  using pybind11_protobuf::WithWrappedProtos;
  using pybind11_protobuf::WrappedProto;
  using pybind11_protobuf::impl::WrapHelper;

  static_assert(std::is_same<WrappedProto<IntMessage, WrappedProtoKind::kConst>,
                             WrapHelper<const IntMessage&>::type>::value,
                "");

  static_assert(std::is_same<WrappedProto<IntMessage, WrappedProtoKind::kConst>,
                             WrapHelper<const IntMessage*>::type>::value,
                "");

  static_assert(std::is_same<WrappedProto<IntMessage, WrappedProtoKind::kValue>,
                             WrapHelper<IntMessage>::type>::value,
                "");

  static_assert(std::is_same<WrappedProto<IntMessage, WrappedProtoKind::kValue>,
                             WrapHelper<IntMessage&&>::type>::value,
                "");

  // These function refs ensure that the generated wrappers have the expected
  // type signatures.
  // Return types
  absl::FunctionRef<int()>(WithWrappedProtos(GetInt));
  absl::FunctionRef<const TestMessage&()>(WithWrappedProtos(GetConstReference));
  absl::FunctionRef<const TestMessage*()>(WithWrappedProtos(GetConstPtr));
  absl::FunctionRef<TestMessage()>(WithWrappedProtos(GetValue));
  absl::FunctionRef<TestMessage && ()>(WithWrappedProtos(GetRValue));
  absl::FunctionRef<TestMessage(Struct&)>(WithWrappedProtos(&Struct::MemberFn));
  absl::FunctionRef<TestMessage(const Struct&)>(
      WithWrappedProtos(&Struct::ConstMemberFn));
  absl::FunctionRef<absl::StatusOr<TestMessage>()>(
      WithWrappedProtos(GetStatusOr));
  absl::FunctionRef<absl::optional<TestMessage>()>(
      WithWrappedProtos(GetOptional));
  absl::FunctionRef<std::vector<TestMessage>()>(WithWrappedProtos(GetVector));

  // Passing types
  absl::FunctionRef<void(int)>(WithWrappedProtos(PassInt));
  absl::FunctionRef<void(const TestMessage&)>(
      WithWrappedProtos(PassConstReference));
  absl::FunctionRef<void(const TestMessage*)>(WithWrappedProtos(PassConstPtr));
  absl::FunctionRef<void(TestMessage)>(WithWrappedProtos(PassValue));
  absl::FunctionRef<void(TestMessage &&)>(WithWrappedProtos(PassRValue));
  absl::FunctionRef<void(absl::optional<TestMessage>)>(
      WithWrappedProtos(PassOptional));
  absl::FunctionRef<void(std::vector<TestMessage>)>(
      WithWrappedProtos(PassVector));
}

#if defined(WRAPPED_PROTO_CASTER_NONCOMPILE_TEST)
// This code could be added as a non-compile test.
//
// It exercises the WithWrappedProtos(...) codepaths when called with mutable
// protos, and is expected to fail with a static_assert.
//
TestMessage& GetReference();
TestMessage* GetPtr();
void PassPtr(TestMessage*);
void PassReference(TestMessage&);

void test_wrapping_disabled() {
  absl::FunctionRef<TestMessage&()>(WithWrappedProtos(GetReference));
  absl::FunctionRef<TestMessage*()>(WithWrappedProtos(GetPtr));
  absl::FunctionRef<void(TestMessage*)>(WithWrappedProtos(PassPtr));
  absl::FunctionRef<void(TestMessage&)>(WithWrappedProtos(PassReference));
}
#endif  // WRAPPED_PROTO_CASTER_NONCOMPILE_TEST

}  // namespace
