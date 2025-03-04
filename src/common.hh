#pragma once

#include <string>
#include <optional>
#include <map>

namespace kv_store {

enum class operation { UNKNOWN, INSERT, GET, REMOVE, GET_ALL };

class payload {
public:
    payload();
    payload(const std::string& key, const std::string& value);

    std::string _key;
    std::string _value;
};

class service_request {
public:
    service_request(payload p, operation o);
    payload _payload;
    operation _op;
};

struct service_response {
  bool _status;
  std::optional<payload> _payload;
  std::optional<std::map<std::string, std::string>> _sorted;
  operation _op;
};

} // namespace kv_store
