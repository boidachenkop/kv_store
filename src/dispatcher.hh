#pragma once
#include <seastar/core/reactor.hh>
#include <seastar/core/pipe.hh>
#include <string>
#include <list>

namespace kv_store {
class service_request;
class service_response;

class dispatcher {

public:
    seastar::future<service_response> dispatch(service_request&& service_req);

    std::shared_ptr<seastar::pipe<service_request>> get_request_pipe(seastar::shard_id shard);
    std::shared_ptr<seastar::pipe<service_response>> get_response_pipe(seastar::shard_id shard);

    void add(seastar::shard_id id);

private:
    std::unordered_map<seastar::shard_id, std::shared_ptr<seastar::pipe<service_request>>> _shards_requests;
    std::unordered_map<seastar::shard_id, std::shared_ptr<seastar::pipe<service_response>>> _shards_responses;
};

std::unique_ptr<dispatcher> create_dispatcher();

} // namespace kv_store
