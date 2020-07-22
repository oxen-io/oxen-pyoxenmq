#include "common.hpp"
#include "lokimq/bt_serialize.h"

namespace lokimq
{
  void
  BEncode_Init(py::module & mod)
  {
    auto & submod =  mod.def_submodule("bencode");
    submod.def("decode", [](py::bytes data) {


                         });
  }
}
