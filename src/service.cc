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

service& service::set_reader(std::shared_ptr<seastar::pipe<service_request>> reader) {
    _reader = reader;
    return *this;
}

service& service::set_writer(std::shared_ptr<seastar::pipe<service_response>> writer) {
    _writer = writer;
    return *this;
}

service& service::set_cache(std::shared_ptr<cache> cache) {
    _cache = cache;
    return *this;
}

service& service::set_backuper(std::shared_ptr<backuper> backup) {
    _backup = backup;
    return *this;
}


future<> service::run() {
    fmt::print("service: run on {}\n", this_shard_id());
    return do_until(
            [&] {
                return _stop;
            },
            [&] {
                return _reader->reader.read().then(
                        [&](std::optional<kv_store::service_request> request) { // TODO read blocks stopping, figure out way around it
                            if (!request) {
                                return make_ready_future<>();
                            }
                            return process(std::move(*request)).then([&](service_response response) {
                                if (response._payload) {
                                    fmt::print("service: writing response with payload: {}\n", response._payload->_key);
                                }
                                return _writer->writer.write(std::move(response));
                            });
                        });
            })
            .then([this] {
                fmt::print("{}: service: loop end\n", this_shard_id());
                _stopped = true;
                _cond.broadcast();
            });
}

seastar::future<service_response> service::process(service_request&& request) {
    switch (request._op) {
    case operation::GET:
        fmt::print("service: processing GET request\n");
        return do_with(std::move(request), [&](service_request& request) {
            return _cache->get(request._payload._key).then([&](std::optional<kv_store::payload> payload) {
                if (!payload) {
                    return _backup->get(request._payload._key).then([](std::optional<kv_store::payload> payload) {
                        if (!payload) {
                            return make_ready_future<service_response>(false, std::nullopt, std::nullopt, operation::GET);
                        }
                        return make_ready_future<service_response>(true, *payload, std::nullopt, operation::GET);
                    });
                }
                return make_ready_future<service_response>(true, *payload, std::nullopt, operation::GET);
            });
        });
    case operation::INSERT:
        fmt::print("service: processing INSERT request\n");
        return do_with(std::move(request), [&](service_request& request) {
            return _cache->insert(request._payload).then([&](bool cache_inserted) {
                if (cache_inserted) {
                    return _backup->insert(request._payload).then([cache_inserted](auto backup_inserted) {
                        return make_ready_future<service_response>(backup_inserted, std::nullopt, std::nullopt, operation::INSERT);
                    });
                }
                return make_ready_future<service_response>(false, std::nullopt, std::nullopt, operation::INSERT);
            });
        });
    case operation::REMOVE:
        fmt::print("service: processing REMOVE request\n");
        return do_with(std::move(request), [&](service_request& request) {
            return _cache->remove(request._payload._key).then([&](bool success) {
                return _backup->remove(request._payload._key).then([](bool success) {
                    return make_ready_future<service_response>(success, std::nullopt, std::nullopt, operation::REMOVE);
                });
                return make_ready_future<service_response>(success, std::nullopt, std::nullopt, operation::REMOVE);
            });
        });
    case operation::GET_ALL:
        fmt::print("service: processing GET_ALL request\n");

        return do_with(std::move(request), [&](service_request& request) {
            return _backup->get_all().then([](auto&& all) {
                return make_ready_future<service_response>(true, std::nullopt, std::move(all), operation::GET_ALL);
            });
        });
    }
    return make_ready_future<service_response>(false, std::nullopt, std::nullopt, operation::UNKNOWN);
}

future<> service::stop() {
    fmt::print("{}: service: stopping...\n", this_shard_id());
    _stop = true;
    return _cond
            .wait([this] {
                return _stopped;
            })
            .then([] {
                fmt::print("{}: service: stopped\n", this_shard_id());
            });
}
} // namespace kv_store
