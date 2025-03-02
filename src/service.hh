#pragma once
#include "cache.hh"
#include "backup.hh"

#include <seastar/core/future.hh>
#include <seastar/core/pipe.hh>

namespace kv_store {
class service_request;
class service_response;
class dispatcher;

class service {
public:
    service(size_t max_cache_size_kb = 1024 * 1024);
    seastar::future<> run(seastar::pipe_reader<service_request>&& reader, seastar::pipe_writer<service_response>&& writer);
    seastar::future<> stop();

private:
    seastar::future<service_response> process(const service_request& request);
    cache _cache;
    backuper _backup;
    bool _stop;
};
} // namespace kv_store
