#pragma once
#include <optional>
#include <string>
#include <memory>

#include <seastar/core/future.hh>
#include <seastar/core/reactor.hh>

namespace kv_store {
class payload;

class data_storage {
public:
    virtual seastar::future<bool> insert(const payload& payload) = 0;
    virtual seastar::future<std::optional<payload>> get(const std::string& key) = 0;
    virtual seastar::future<bool> remove(const std::string& key) = 0;
};

} // namespace kv_store
