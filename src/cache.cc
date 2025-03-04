#include "cache.hh"
#include "common.hh"

#include <cmath>

using namespace seastar;

namespace kv_store {
cache::cache(size_t max_size)
    : _capacity{static_cast<size_t>(std::floor(max_size / sizeof(payload)))} {
    assert(_capacity != 0);
    fmt::print("cache: created. capacity: {}\n", _capacity);
}

future<bool> cache::insert(const payload& payload) {
    if (_list.size() == _capacity) {
        auto key = _list.front();
        _list.pop_front();
        _storage.erase(key);
    }
    bool status = false;
    if (_storage.contains(payload._key)) {
        updateKey(payload._key, payload._value);
        status = true;
    } else {
        _list.push_back(payload._key);
        status = _storage.emplace(_list.back(), std::pair<ListIterator, std::string>(std::prev(_list.end()), payload._value)).second;
    }
    return make_ready_future<bool>(status);
}

future<std::optional<payload>> cache::get(const std::string& key) {
    if (auto item = _storage.find(key); item != _storage.end()) {
        updateKey(key);
        return make_ready_future<std::optional<payload>>(payload(item->first, (item->second).second));
    }
    return make_ready_future<std::optional<payload>>(std::nullopt);
}

future<bool> cache::remove(const std::string& key) {
    if (auto item = _storage.find(key); item != _storage.end()) {
        auto listIt = item->second.first;
        _list.erase(listIt);
        _storage.erase(item);
        return make_ready_future<bool>(true);
    }
    return make_ready_future<bool>(false);
}

void cache::updateKey(const std::string& key, const std::optional<std::string>& value /*= std::nullopt*/) {
    if (_storage.contains(key)) {
        _list.erase(_storage[key].first);
        _list.push_back(key);
        _storage[key] = std::make_pair(std::prev(_list.end()), value.value_or(_storage[key].second));
    }
}
} // namespace kv_store
