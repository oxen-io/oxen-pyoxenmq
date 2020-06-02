
pybind11_add_module(pylokimq MODULE
  module.cpp
)

target_link_libraries(pylokimq PUBLIC lokimq::lokimq)


