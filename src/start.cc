#include "request_handlers.hh"
#include "service.hh"
#include "dispatcher.hh"
#include "data_store.hh"

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
        stop_signal signal;
        uint16_t port = 1270;
        auto cache_size = 1024 * 1024;

        httpd::http_server_control http_server;
        auto dispatcher = std::make_unique<kv_store::dispatcher>();
        std::unordered_map<seastar::shard_id, std::shared_ptr<kv_store::cache>> caches;
        std::unordered_map<seastar::shard_id, std::shared_ptr<kv_store::backuper>> backupers;
        seastar::sharded<kv_store::service> service;

        seastar::smp::invoke_on_others([&caches, &backupers, d = dispatcher.get(), cache_size] {
            fmt::print("start: preparing shard: {}\n", this_shard_id());
            caches.emplace(this_shard_id(), std::make_shared<kv_store::cache>(cache_size));
            backupers.emplace(this_shard_id(), std::make_shared<kv_store::backuper>(this_shard_id()));
            d->add(this_shard_id());
        }).get();
        fmt::print("start: done shard preparation\n");

        (void)service.start().then([&, d = dispatcher.get()] {
            return service.invoke_on_others([&, d](kv_store::service& service) {
                fmt::print("start: Invoking service on cpu: {}\n", this_shard_id());

                // setup service
                service.set_reader(d->get_request_pipe(this_shard_id()))
                        .set_writer(d->get_response_pipe(this_shard_id()))
                        .set_cache(caches[this_shard_id()])
                        .set_backuper(backupers[this_shard_id()]);

                return service.run();
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

        fmt::print("start: Server listening on port: {}\n", port);
        (void)http_server.listen(ipv4_addr(port)).handle_exception([](auto ep) {
            fmt::print("start: Can't start server {}\n", ep);
            return make_exception_future<>(ep);
        });

        signal.wait()
                .then([&] {
                    return http_server.stop();
                })
                .then([&] {
                    for(auto [shard, backuper] : backupers) // wrong
                    {
                        backuper.reset();
                    }
                    return backupers.clear();
                })
                .then([&] {
                    dispatcher.reset();
                })
                .then([&] {
                    // return service.stop();
                    return make_ready_future<>();
                })
                .get();
    });
}
