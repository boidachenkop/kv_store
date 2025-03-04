#include "dispatcher.hh"
#include "common.hh"

using namespace seastar;

namespace kv_store {

future<service_response> dispatcher::dispatch(service_request&& service_req) {
    auto n = _shards_responses.size();
    auto key = service_req._payload._key;

    uint16_t hash = 0;
    hash |= (key.empty() ? 0 : static_cast<uint8_t>(key[0]));
    if (key.size() > 1) {
        hash = (hash << 8) | static_cast<uint8_t>(key[1]);
    }

    auto shard_id = (hash % n) + 1;
    fmt::print("dispatcher: dispatching key [{}] to shard {}\n", key, shard_id);
    return _shards_requests.at(shard_id)->writer.write(std::move(service_req)).then([&, shard_id] {
        return _shards_responses.at(shard_id)->reader.read().then([](std::optional<kv_store::service_response> response) {
            if (response) {
                return make_ready_future<service_response>(*response);
            }
            return make_ready_future<service_response>(false, std::nullopt, std::nullopt, operation::UNKNOWN);
        });
    });
}

future<std::map<std::string, std::string>> dispatcher::to_all(service_request service_req) {
    std::map<std::string, std::string> result;
    for(auto [shard, request_pipe] : _shards_requests)
    {
        co_await request_pipe->writer.write(std::move(service_request(service_req)));
    } 
    for(auto [shard, response_pipe] : _shards_responses )
    {
        if(auto response = co_await response_pipe->reader.read())
        {
            if(auto& sorted_pairs = response->_sorted)
            {
                result.insert(sorted_pairs->begin(), sorted_pairs->end());
            }
        }
    }
    co_return result;
}

std::shared_ptr<seastar::pipe<service_request>> dispatcher::get_request_pipe(shard_id shard) {
    return _shards_requests.at(shard);
}

std::shared_ptr<seastar::pipe<service_response>> dispatcher::get_response_pipe(shard_id shard) {
    return _shards_responses.at(shard);
}

void dispatcher::add(seastar::shard_id id) {
    if (_shards_requests.find(id) != _shards_requests.end()) {
        return;
    }
    _shards_requests.emplace(id, std::make_shared<seastar::pipe<service_request>>(10)); // TODO: deside if 10 is ok
    _shards_responses.emplace(id, std::make_shared<seastar::pipe<service_response>>(10));
}

std::unique_ptr<dispatcher> create_dispatcher()
{
    return std::make_unique<dispatcher>();
}


} // namespace kv_store
