#include "service.hh"
#include "dispatcher.hh"
#include "common.hh"
#include "backup.hh"

#include <seastar/core/reactor.hh>
#include <seastar/core/pipe.hh>
#include <seastar/core/thread.hh>
#include <optional>

using namespace seastar;

namespace kv_store {
service::service(size_t max_cache_size_kb /*= 1024 * 1024*/)
    : _cache(max_cache_size_kb)
    , _backup(this_shard_id())
    , _stop(false) {
}

future<> service::run(pipe_reader<service_request>&& reader, pipe_writer<service_response>&& writer) {
    fmt::print("service: run on {}\n", this_shard_id());
    return do_until(
            [&] {
                return _stop;
            },
            [&] {
                return reader.read().then([&](std::optional<kv_store::service_request> request) { // TODO: should be in loop
                    if (!request) {
                        return make_ready_future<>();
                    }
                    return process(*request).then([&](service_response response) {
                        if (response._payload) {
                            fmt::print("service: writing response with payload: {}\n", response._payload->_key);
                        }
                        return writer.write(std::move(response));
                    });
                });
            });
}

seastar::future<service_response> service::process(const service_request& request) {
    switch (request._op) { // TODO: might be done better
    case operation::GET:
        fmt::print("service: processing GET request\n");
        return _cache.get(request._payload._key).then([](std::optional<kv_store::payload> payload) {
            if (!payload) {
                return make_ready_future<service_response>(false, std::nullopt, operation::GET);
            }
            return make_ready_future<service_response>(true, *payload, operation::GET);
        });
    case operation::INSERT:
        fmt::print("service: processing INSERT request\n");
        return _cache.insert(request._payload).then([&](bool cache_inserted) {
            if(cache_inserted)
            {
                return _backup.insert(request._payload).then([cache_inserted] (auto backup_inserted){
                    return make_ready_future<service_response>(backup_inserted, std::nullopt, operation::INSERT);
                });
            }
            return make_ready_future<service_response>(false, std::nullopt, operation::INSERT);
        });
    case operation::REMOVE:
        fmt::print("service: processing REMOVE request\n");
        return _cache.remove(request._payload._key).then([](bool success) {
            return make_ready_future<service_response>(success, std::nullopt, operation::REMOVE);
        });
    }
    return make_ready_future<service_response>(false, std::nullopt, operation::UNKNOWN);
}

future<> service::stop() {
    _stop = true; // TODO wait till stop or return future which will resolve onse stopped
    return make_ready_future<>();
}
} // namespace kv_store
