#include "request_handlers.hh"
#include "dispatcher.hh"
#include "common.hh"

#include <seastar/core/future.hh>
#include <seastar/http/request.hh>
#include <seastar/http/reply.hh>
#include <seastar/http/url.hh>
#include <seastar/http/function_handlers.hh>
#include <seastar/core/thread.hh>
#include <seastar/core/reactor.hh>
#include <functional>
#include <memory>

using namespace seastar;

namespace kv_store {
future<std::unique_ptr<http::reply>> handle_service_communication(
        kv_store::dispatcher* dispatcher, kv_store::service_request service_request, std::unique_ptr<http::reply>&& rep) {
    if (service_request._op == operation::GET_ALL) {
            return dispatcher->to_all(std::move(service_request)).then([rep = std::move(rep)](auto response) mutable {
                fmt::print("handle_service_communication: All items size: {}\n", response.size());
                fmt::memory_buffer buffer;
                for (auto& [key, value] : response) {
                    fmt::format_to(std::back_inserter(buffer), "{} : {}\n", key, value);
                }
                rep->write_body("text/plain", fmt::to_string(buffer));
                return make_ready_future<std::unique_ptr<http::reply>>(std::move(rep));
            });
    }

    return dispatcher->dispatch(std::move(service_request)).then([rep = std::move(rep)](kv_store::service_response response) mutable {
        fmt::print("{}: service response: {}\n", this_shard_id(), response._status);
        if (!response._status) {
            rep->set_status(http::reply::status_type::internal_server_error);
        }
        if (response._payload) {
            rep->write_body("text/plain", fmt::format("{}\n{}", response._payload->_key, response._payload->_value));
        }
        return make_ready_future<std::unique_ptr<http::reply>>(std::move(rep));
    });
}

future<std::unique_ptr<http::reply>> handle_post_item(kv_store::dispatcher* dispatcher, std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) {
    fmt::print("handle_post_item called: {}\n", this_shard_id());
    std::string body = req->content; // TODO: use content_stream instead
    kv_store::payload payload;

    size_t pos = body.find('\n');
    if (pos == std::string::npos) {
        fmt::print(stderr, "handle_post_item: can't parse body\n");
        rep->set_status(seastar::http::reply::status_type::bad_request);
        return make_ready_future<std::unique_ptr<http::reply>>(std::move(rep));
    }

    payload._key = body.substr(0, pos);
    payload._value = body.substr(pos + 1);
    fmt::print("{}: handle_post_item: dispatching payload: [{}, {}]\n", this_shard_id(), payload._key, payload._value);
    return handle_service_communication(dispatcher, {payload, operation::INSERT}, std::move(rep));
}

future<std::unique_ptr<http::reply>> handle_get_item(kv_store::dispatcher* dispatcher, std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) {
    fmt::print("handle_get_item called\n");
    kv_store::payload payload;
    payload._key = req->get_query_param("item");
    fmt::print("handle_get_item query item: {}\n", payload._key);
    return handle_service_communication(dispatcher, {payload, operation::GET}, std::move(rep));
}

seastar::future<std::unique_ptr<seastar::http::reply>> handle_get_items(
        kv_store::dispatcher* dispatcher, std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) {
    fmt::print("handle_get_items called\n");
    return handle_service_communication(dispatcher, {kv_store::payload(), operation::GET_ALL}, std::move(rep));
}

future<std::unique_ptr<http::reply>> handle_delete_item(
        kv_store::dispatcher* dispatcher, std::unique_ptr<http::request> req, std::unique_ptr<http::reply> rep) {
    fmt::print("handle_delete_item called\n");
    kv_store::payload payload;
    payload._key = req->get_query_param("item");
    fmt::print("handle_delete_item query item: {}\n", payload._key);

    return handle_service_communication(dispatcher, {payload, operation::REMOVE}, std::move(rep));
}

future<std::unique_ptr<http::reply>> handle_default(std::unique_ptr<http::reply> rep) {
    fmt::print("handle_default called\n");
    rep->set_status(http::reply::status_type::forbidden);
    return make_ready_future<std::unique_ptr<http::reply>>(std::move(rep));
}

void set_routes(seastar::httpd::routes& routes, kv_store::dispatcher* dispatcher) {
    auto* handle_post = new seastar::httpd::function_handler(
            [dispatcher](std::unique_ptr<seastar::http::request> req, std::unique_ptr<seastar::http::reply> rep) {
                return kv_store::handle_post_item(dispatcher, std::move(req), std::move(rep));
            },
            "post_item");

    auto* handle_get = new seastar::httpd::function_handler(
            [dispatcher](std::unique_ptr<seastar::http::request> req, std::unique_ptr<seastar::http::reply> rep) {
                return kv_store::handle_get_item(dispatcher, std::move(req), std::move(rep));
            },
            "get_item");

    auto* handle_get_all = new seastar::httpd::function_handler(
            [dispatcher](std::unique_ptr<seastar::http::request> req, std::unique_ptr<seastar::http::reply> rep) {
                return kv_store::handle_get_items(dispatcher, std::move(req), std::move(rep));
            },
            "get_items");

    auto* handle_delete = new seastar::httpd::function_handler(
            [dispatcher](std::unique_ptr<seastar::http::request> req, std::unique_ptr<seastar::http::reply> rep) {
                return kv_store::handle_delete_item(dispatcher, std::move(req), std::move(rep));
            },
            "delete_item");

    auto* handle_default = new seastar::httpd::function_handler(
            [](std::unique_ptr<seastar::http::request> req, std::unique_ptr<seastar::http::reply> rep) {
                return kv_store::handle_default(std::move(rep));
            },
            "default");

    routes.add(seastar::httpd::POST, seastar::httpd::url("/item"), handle_post);
    routes.add(seastar::httpd::GET, seastar::httpd::url("/item"), handle_get);
    routes.add(seastar::httpd::GET, seastar::httpd::url("/all"), handle_get_all);
    routes.add(seastar::httpd::DELETE, seastar::httpd::url("/item"), handle_delete);
    routes.add_default_handler(handle_default);
}
} // namespace kv_store
