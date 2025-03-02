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

    seastar::pipe_reader<service_request>&& get_request_reader(seastar::shard_id shard);
    seastar::pipe_writer<service_request>&& get_request_writer(seastar::shard_id shard);

    seastar::pipe_reader<service_response>&& get_response_reader(seastar::shard_id shard);
    seastar::pipe_writer<service_response>&& get_response_writer(seastar::shard_id shard);

    void add(seastar::shard_id id);

private:
    std::unordered_map<seastar::shard_id, seastar::pipe<service_request>> _shards_requests;
    std::unordered_map<seastar::shard_id, seastar::pipe<service_response>> _shards_responses;
};

} // namespace kv_store
