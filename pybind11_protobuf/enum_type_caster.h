// IWYU pragma: always_keep // Appears to be the best we can do at the moment.
// What we really want (but does not work with Include Cleaner @ 2023-10-27):
// * If native_proto_caster.h is included: suggest removing enum_type_caster.h
// * If only enum_type_caster.h is included: always_keep

#ifndef PYBIND11_PROTOBUF_ENUM_TYPE_CASTER_H_
#define PYBIND11_PROTOBUF_ENUM_TYPE_CASTER_H_

#include <Python.h>
#include <pybind11/cast.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>

#include <string>
#include <type_traits>

#include "net/proto2/public/descriptor.h"
#include "net/proto2/public/generated_enum_reflection.h"
#include "net/proto2/public/generated_enum_util.h"

// pybind11 type_caster specialization which translates Proto::Enum types
// to/from ints. This will have ODR conflicts when users specify wrappers for
// enums using py::enum_<T>.
//
// ::google::protobuf::is_proto_enum and ::google::protobuf::GetEnumDescriptor are require
//
// NOTE: The protobuf compiler does not generate ::google::protobuf::is_proto_enum traits
// for enumerations of oneof fields.
//
// Example:
//  #include <pybind11/pybind11.h>
//  #include "pybind11_protobuf/proto_enum_casters.h"
//
//  MyMessage::EnumType GetMessageEnum() { ... }
//  PYBIND11_MODULE(my_module, m) {
//    m.def("get_message_enum", &GetMessageEnum);
//  }
//
// Users of enum_type_caster based extensions need dependencies on:
// deps = [ "@com_google_protobuf//:protobuf_python" ]
//

namespace pybind11_protobuf {

// Implementation details for pybind11 enum casting.
template <typename EnumType>
struct enum_type_caster {
 private:
  using T = std::underlying_type_t<EnumType>;
  using base_caster = pybind11::detail::make_caster<T>;

 public:
  static constexpr auto name = pybind11::detail::const_name<EnumType>();

  // cast converts from C++ -> Python
  static pybind11::handle cast(EnumType src,
                               pybind11::return_value_policy policy,
                               pybind11::handle p) {
    return base_caster::cast(static_cast<T>(src), policy, p);
  }

  // load converts from Python -> C++
  bool load(pybind11::handle src, bool convert) {
    base_caster base;
    if (base.load(src, convert)) {
      T v = static_cast<T>(base);
      // Behavior change 2023-07-19: Previously we only accept integers that
      // are valid values of the enum.
      value = static_cast<EnumType>(v);
      return true;
    }
    // When convert is true, then the enum could be resolved via
    // FindValueByName.
    return false;
  }

  explicit operator EnumType() { return value; }

  template <typename>
  using cast_op_type = EnumType;

 private:
  EnumType value;
};

}  // namespace pybind11_protobuf
namespace pybind11 {
namespace detail {

// ADL function to enable/disable specializations of proto enum type_caster<>
// provided by pybind11_protobuf. Defaults to enabled. To disable the
// pybind11_protobuf enum_type_caster for a specific enum type, define a
// constexpr function in the same namespace, like:
//
//  constexpr bool pybind11_protobuf_enable_enum_type_caster(tensorflow::DType*)
//  { return false; }
//
constexpr bool pybind11_protobuf_enable_enum_type_caster(...) { return true; }

#if defined(PYBIND11_HAS_NATIVE_ENUM)
template <typename EnumType>
struct type_caster_enum_type_enabled<
    EnumType, std::enable_if_t<(::google::protobuf::is_proto_enum<EnumType>::value &&
                                pybind11_protobuf_enable_enum_type_caster(
                                    static_cast<EnumType*>(nullptr)))>>
    : std::false_type {};
#endif

// Specialization of pybind11::detail::type_caster<T> for types satisfying
// ::google::protobuf::is_proto_enum.
template <typename EnumType>
struct type_caster<EnumType,
                   std::enable_if_t<(::google::protobuf::is_proto_enum<EnumType>::value &&
                                     pybind11_protobuf_enable_enum_type_caster(
                                         static_cast<EnumType*>(nullptr)))>>
    : public pybind11_protobuf::enum_type_caster<EnumType> {};

}  // namespace detail
}  // namespace pybind11

#endif  // PYBIND11_PROTOBUF_ENUM_TYPE_CASTER_H_
