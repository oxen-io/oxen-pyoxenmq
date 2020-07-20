
pybind11_add_module(pylokimq MODULE
  lokimq.cpp
  module.cpp
)

target_link_libraries(pylokimq PUBLIC lokimq::lokimq)


