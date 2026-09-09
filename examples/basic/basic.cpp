#include <pybind11/pybind11.h>
#include "pybind11_protobuf/native_proto_caster.h"

int add(int i, int j) {
  return i + j;
}

PYBIND11_MODULE(basic, module) {
  pybind11_protobuf::ImportNativeProtoCasters();
  module.doc() = "A basic pybind11_protobuf extension";
  module.def("add", &add, "A function that adds two numbers");
}
