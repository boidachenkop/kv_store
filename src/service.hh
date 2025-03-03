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
    service() = default;

    service& set_reader(std::shared_ptr<seastar::pipe<service_request>> reader);
    service& set_writer(std::shared_ptr<seastar::pipe<service_response>> writer);
    service& set_cache(std::shared_ptr<cache> cache);
    service& set_backuper(std::shared_ptr<backuper> backup);

    seastar::future<> run();
    seastar::future<> stop();

private:
    seastar::future<service_response> process(service_request&& request);

private:
    std::shared_ptr<cache> _cache;
    std::shared_ptr<backuper> _backup;

    std::shared_ptr<seastar::pipe<service_request>> _reader; 
    std::shared_ptr<seastar::pipe<service_response>> _writer;
    bool _stop;
    bool _stopped{false};
    seastar::condition_variable _cond;
};
} // namespace kv_store
