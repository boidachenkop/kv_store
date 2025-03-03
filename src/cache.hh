#pragma once
#include "common.hh"
#include "data_store.hh"

#include <seastar/core/pipe.hh>
#include <seastar/core/future.hh>
#include <list>
#include <unordered_map>
#include <string>


namespace kv_store {

class cache : public data_storage{
  using ListIterator = std::list<std::string>::const_iterator;
public:
    cache(size_t max_size);
    seastar::future<bool> insert(const payload& payload) override;
    seastar::future<std::optional<payload>> get(const std::string& key) override;
    seastar::future<bool> remove(const std::string& key) override;

private:
    void updateKey(const std::string& key, const std::optional<std::string>& value = std::nullopt);

    size_t _capacity;
    std::list<std::string> _list; // for order tracking, TODO: change to ordered set?
    // key - (list iter - value)
    std::unordered_map<std::string, std::pair<ListIterator, std::string>> _storage;
};
} // namespace kv_store
