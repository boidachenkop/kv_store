#pragma once

#include <seastar/http/handlers.hh>
#include <seastar/http/routes.hh>


namespace kv_store {
class dispatcher;

seastar::future<std::unique_ptr<seastar::http::reply>> handle_post_item(
        kv_store::dispatcher* dispatcher, std::unique_ptr<seastar::http::request> req, std::unique_ptr<seastar::http::reply> rep);

seastar::future<std::unique_ptr<seastar::http::reply>> handle_get_item(
        kv_store::dispatcher* dispatcher, std::unique_ptr<seastar::http::request> req, std::unique_ptr<seastar::http::reply> rep);

seastar::future<std::unique_ptr<seastar::http::reply>> handle_delete_item(
        kv_store::dispatcher* dispatcher, std::unique_ptr<seastar::http::request> req, std::unique_ptr<seastar::http::reply> rep);

seastar::future<std::unique_ptr<seastar::http::reply>> handle_default(std::unique_ptr<seastar::http::reply> rep);

void set_routes(seastar::httpd::routes& routes, kv_store::dispatcher* dispatcher);
} // namespace kv_store
