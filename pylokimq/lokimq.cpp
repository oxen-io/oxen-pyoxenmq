#include "common.hpp"
#include <lokimq/lokimq.h>
#include <lokimq/address.h>
#include <future>
#include <memory>

namespace lokimq
{

  static std::mutex log_mutex;

  void
  LokiMQ_Init(py::module & mod)
  {
    py::class_<ConnectionID>(mod, "ConnectionID");
    py::class_<address>(mod, "Address")
      .def(py::init<std::string>());
    py::class_<TaggedThreadID>(mod, "TaggedThreadID");
    py::enum_<LogLevel>(mod, "LogLevel")
      .value("fatal", LogLevel::fatal).value("error", LogLevel::error).value("warn", LogLevel::warn)
      .value("info", LogLevel::info).value("debug", LogLevel::debug).value("trace", LogLevel::trace);

    py::class_<LokiMQ>(mod, "LokiMQ")
      .def(py::init<>())
      .def(py::init([](LogLevel level) {
        // Quick and dirty logger that logs to stderr.  It would be much nicer to take a python
        // function, but that deadlocks pretty much right away because of the crappiness of the gil.
        return std::make_unique<LokiMQ>([] (LogLevel lvl, const char* file, int line, std::string msg) mutable {
          std::lock_guard l{log_mutex};
          std::cerr << '[' << lvl << "][" << file << ':' << line << "]: " << msg << "\n";
        }, level);
      }))
      .def("start", &LokiMQ::start)
      .def("listen_plain",
           [](LokiMQ & self, std::string path) {
             self.listen_plain(path);
           })
      .def("listen_curve", &LokiMQ::listen_curve)
      .def("add_tagged_thread",
           [](LokiMQ & self, const std::string & name) {
             return self.add_tagged_thread(name);
           })
      .def("add_timer",
           [](LokiMQ & self, std::chrono::milliseconds interval, std::function<void(void)> callback) {
             self.add_timer(callback, interval);
           })
      .def("call_soon",
           [](LokiMQ & self, std::function<void(void)> job, std::optional<TaggedThreadID> thread)
           {
             self.job(std::move(job), std::move(thread));
           })
      .def("add_anonymous_category",
           [](LokiMQ & self, std::string name)
           {
             self.add_category(std::move(name), AuthLevel::none);
           })
      .def("add_request_command",
           [](LokiMQ &self,
              const std::string & category,
              const std::string & name,
              std::function<std::string(std::vector<std::string_view>)> handler)
           {
             self.add_request_command(category, name,
               [handler](Message & msg) {
                 std::string result;
                 {
                   py::gil_scoped_acquire gil;
                   try
                   {
                     result = handler(msg.data);
                   }
                   catch(std::exception & ex)
                   {
                     PyErr_SetString(PyExc_RuntimeError, ex.what());
                   }
                 }
                 msg.send_reply(result);
               });
           })
      .def("connect_remote",
           [](LokiMQ & self,
              std::string remote) -> std::optional<ConnectionID>
           {
             std::promise<std::optional<ConnectionID>> promise;
             self.connect_remote(
               remote,
               [&promise](ConnectionID id) { promise.set_value(std::move(id)); },
               [&promise](auto, auto) { promise.set_value(std::nullopt); });
             return promise.get_future().get();
           })
      .def("request",
           [](LokiMQ & self,
              ConnectionID conn,
              std::string name,
              std::vector<std::string> args) -> std::optional<std::vector<std::string>>
           {
             std::promise<std::optional<std::vector<std::string>>> result;
             self.request(conn, std::move(name),
                          [&result](bool success, std::vector<std::string> value)
                          {
                            if(not success)
                            {
                              result.set_value(std::nullopt);
                              return;
                            }
                            result.set_value(std::move(value));
                          }, lokimq::send_option::data_parts(args.begin(), args.end()));
             return result.get_future().get();
           });  
  }
}
