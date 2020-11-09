
pybind11_add_module(pylokimq MODULE
  bencode.cpp
  lokimq.cpp
  module.cpp
)

target_link_libraries(pylokimq PUBLIC PkgConfig::lokimq)


