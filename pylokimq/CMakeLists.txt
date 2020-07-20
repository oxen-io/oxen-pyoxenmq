
pybind11_add_module(pylokimq MODULE
  batch_job.cpp
  lokimq.cpp
  module.cpp
)

target_link_libraries(pylokimq PUBLIC lokimq::lokimq)


