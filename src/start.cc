#include "request_handlers.hh"
#include "service.hh"
#include "dispatcher.hh"

#include <seastar/core/future.hh>
#include <seastar/http/httpd.hh>
#include <seastar/net/api.hh>
#include <seastar/http/function_handlers.hh>
#include <seastar/core/thread.hh>
#include <seastar/core/coroutine.hh>
#include <seastar/core/reactor.hh>
#include <seastar/http/routes.hh>
#include <seastar/http/transformers.hh>
#include <seastar/net/socket_defs.hh>
#include <seastar/core/signal.hh>
#include <fmt/printf.h>


using namespace seastar;

class stop_signal { // from some example found on internet
    bool _caught = false;
    seastar::condition_variable _cond;

private:
    void signaled() {
        if (_caught) {
            return;
        }
        _caught = true;
        _cond.broadcast();
    }

public:
    stop_signal() {
        seastar::handle_signal(SIGINT, [this] {
            signaled();
        });
        seastar::handle_signal(SIGTERM, [this] {
            signaled();
        });
    }
    ~stop_signal() {
        seastar::handle_signal(SIGINT, [] {});
        seastar::handle_signal(SIGTERM, [] {});
    }
    seastar::future<> wait() {
        return _cond.wait([this] {
            return _caught;
        });
    }
};

seastar::future<> start() {
    return seastar::async([]() {
        uint16_t port = 1270;
        stop_signal stop_signal;

        httpd::http_server_control http_server;
        auto dispatcher = std::make_unique<kv_store::dispatcher>();
        seastar::sharded<kv_store::service> service;

        seastar::smp::invoke_on_others([d = dispatcher.get()] {
            fmt::print("start: adding pipes for shard: {}\n", this_shard_id());
            d->add(this_shard_id());
        }).get();
        fmt::print("start: done setting up dispatcher\n");

        (void)service.start().then([&service, d = dispatcher.get()] {
            return service.invoke_on_others([d](kv_store::service& service) {
                fmt::print("start: Invoking service on cpu: {}\n", this_shard_id());
                return service.run(std::move(d->get_request_reader(this_shard_id())), std::move(d->get_response_writer(this_shard_id())));
            });
        });

        fmt::print("start: Starting server...\n");
        http_server.start().get();

        fmt::print("start: Setting routes for server\n");
        http_server
                .set_routes([&dispatcher](auto& routes) {
                    kv_store::set_routes(routes, dispatcher.get());
                })
                .get();

        (void)http_server.listen(ipv4_addr(port))
                .then([port] {
                    fmt::print("start: Server listening on port: {}\n", port);
                })
                .handle_exception([](auto ep) {
                    fmt::print("start: Can't start server {}\n", ep);
                    return make_exception_future<>(ep);
                });

        engine().at_exit([&http_server, &service]() {
            fmt::print("start: Stopping server.");
            return http_server.stop().then([&service]{
                return service.stop();
            });
        });

        stop_signal.wait().get(); // TODO: seastar::gate instead?
    });
}
