#include "common.hpp"
#include <chrono>
#include <exception>
#include <lokimq/lokimq.h>
#include <lokimq/address.h>
#include <pybind11/chrono.h>
#include <future>
#include <memory>

namespace lokimq
{
  template <typename... Options>
  std::future<std::vector<std::string>> LokiMQ_start_request(
      LokiMQ& lmq,
      ConnectionID conn,
      std::string name,
      std::vector<py::bytes> byte_args,
      Options&&... opts)
  {
    std::vector<std::string> args;
    args.reserve(byte_args.size());
    for (auto& b : byte_args)
      args.push_back(b);

    auto result = std::make_shared<std::promise<std::vector<std::string>>>();
    auto fut = result->get_future();
    lmq.request(conn, std::move(name),
        [result=std::move(result)](bool success, std::vector<std::string> value)
        {
          if (success)
            result->set_value(std::move(value));
          else
          {
            std::string err;
            for (auto& m : value) {
              if (!err.empty()) err += ", ";
              err += m;
            }
            result->set_exception(std::make_exception_ptr(std::runtime_error{"Request failed: " + err}));
          }
        },
        lokimq::send_option::data_parts(args.begin(), args.end()),
        std::forward<Options>(opts)...
    );
    return fut;
  }

  // Binds a stl future.  `Conv` is a lambda that converts the future's .get() value into something
  // Python-y (it can be the value directly, if the value is convertible to Python already).
  template <typename F, typename Conv>
  void bind_future(py::module& m, std::string class_name, Conv conv)
  {
    py::class_<F>(m, class_name.c_str())
      .def("get", [conv=std::move(conv)](F& f) { return conv(f.get()); },
          "Gets the result (or raises an exception if the result set an exception); must only be called once")
      .def("valid", [](F& f) { return f.valid(); },
          "Returns true if the result is available")
      .def("wait", &F::wait,
          "Waits indefinitely for the result to become available")
      .def("wait_for", &F::template wait_for<double, std::ratio<1>>,
          "Waits up to the given timedelta for the result to become available")
      .def("wait_for", [](F& f, double seconds) { return f.wait_for(std::chrono::duration<double>{seconds}); },
          "Waits up to the given number of seconds for the result to become available")
      .def("wait_until", &F::template wait_until<std::chrono::system_clock, std::chrono::system_clock::duration>,
          "Wait until the given datetime for the result to become available")
      ;
  }

  static std::mutex log_mutex;

  void
  LokiMQ_Init(py::module & mod)
  {
    using namespace pybind11::literals;
    py::class_<ConnectionID>(mod, "ConnectionID");
    py::class_<address>(mod, "Address")
      .def(py::init<std::string>());
    py::class_<TaggedThreadID>(mod, "TaggedThreadID");
    py::enum_<LogLevel>(mod, "LogLevel")
      .value("fatal", LogLevel::fatal).value("error", LogLevel::error).value("warn", LogLevel::warn)
      .value("info", LogLevel::info).value("debug", LogLevel::debug).value("trace", LogLevel::trace);

    py::enum_<std::future_status>(mod, "future_status")
      .value("deferred", std::future_status::deferred)
      .value("ready", std::future_status::ready)
      .value("timeout", std::future_status::timeout);
    bind_future<std::future<std::vector<std::string>>>(mod, "ResultFuture",
            [](std::vector<std::string> bytes) {
              py::list l;
              for (const auto& v : bytes)
                l.append(py::bytes(v));
              return l;
            });

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
      .def_readwrite("handshake_time", &LokiMQ::HANDSHAKE_TIME)
      .def_readwrite("pubkey_base_routing_id", &LokiMQ::PUBKEY_BASED_ROUTING_ID)
      .def_readwrite("max_message_size", &LokiMQ::MAX_MSG_SIZE)
      .def_readwrite("max_sockets", &LokiMQ::MAX_SOCKETS)
      .def_readwrite("reconnect_interval", &LokiMQ::RECONNECT_INTERVAL)
      .def_readwrite("close_longer", &LokiMQ::CLOSE_LINGER)
      .def_readwrite("connection_check_interval", &LokiMQ::CONN_CHECK_INTERVAL)
      .def_readwrite("connection_heartbeat", &LokiMQ::CONN_HEARTBEAT)
      .def_readwrite("connection_heartbeat_timeout", &LokiMQ::CONN_HEARTBEAT_TIMEOUT)
      .def_readwrite("startup_umask", &LokiMQ::STARTUP_UMASK)
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
      .def("add_request_command_ex",
           [](LokiMQ &self,
              const std::string & category,
              const std::string & name,
              std::function<std::string(std::vector<std::string_view>, std::string_view, ConnectionID)> handler)
           {
             self.add_request_command(category, name,
               [handler](Message & msg) {
                 std::string result;
                 {
                   py::gil_scoped_acquire gil;
                   try
                   {
                     result = handler(msg.data, msg.remote, msg.conn);
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
              std::string remote) -> ConnectionID
           {
             std::promise<ConnectionID> promise;
             self.connect_remote(
               remote,
               [&promise](ConnectionID id) { promise.set_value(std::move(id)); },
               [&promise](auto, std::string_view reason) {
                 promise.set_exception(std::make_exception_ptr(
                       std::runtime_error{"Connection failed: " + std::string{reason}}));
               });
             return promise.get_future().get();
           })
      .def("request",
           [](LokiMQ & self,
              ConnectionID conn,
              std::string name,
              std::vector<py::bytes> args,
              std::optional<double> timeout) -> py::list
           {
             py::list l;
             for (auto& s : LokiMQ_start_request(self, conn, std::move(name), std::move(args),
                     lokimq::send_option::request_timeout{timeout ? std::chrono::milliseconds(long(*timeout * 1000)) : DEFAULT_REQUEST_TIMEOUT}
                     ).get())
               l.append(py::bytes(s));
             return l;
           },
           "conn"_a, "name"_a, "args"_a = std::vector<py::bytes>{}, "timeout"_a = py::none{})
      .def("request_future",
           [](LokiMQ & self,
              ConnectionID conn,
              std::string name,
              std::vector<py::bytes> args,
              std::optional<double> timeout) -> std::future<std::vector<std::string>>
           {
             return LokiMQ_start_request(self, conn, std::move(name), std::move(args),
                     lokimq::send_option::request_timeout{timeout ? std::chrono::milliseconds(long(*timeout * 1000)) : DEFAULT_REQUEST_TIMEOUT}
                     );
           },
           "conn"_a, "name"_a, "args"_a = std::vector<py::bytes>{}, "timeout"_a = py::none{})
      ;
  }
}
