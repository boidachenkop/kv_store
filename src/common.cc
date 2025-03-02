#include "common.hh"

namespace kv_store {

service_request::service_request(payload p, operation o)
    : _payload(p)
    , _op(o) {
}

payload::payload() {
    _key.reserve(255);
}
payload::payload(const std::string& key, const std::string& value) {
    _key.reserve(255);
    _key = key; // TODO: avoid copy? mb use seastar buffer
    _value = value;
}
} // namespace kv_store
