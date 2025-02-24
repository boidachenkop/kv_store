 #include <seastar/core/app-template.hh>
 #include <seastar/core/reactor.hh>
 #include <seastar/core/shared_ptr.hh>
 #include <seastar/core/iostream.hh>
 #include <seastar/core/fstream.hh>
 #include <seastar/core/thread.hh>
 #include <seastar/http/client.hh>
 #include <seastar/http/request.hh>
 #include <seastar/http/reply.hh>
 #include <seastar/net/inet_address.hh>
 #include <seastar/net/dns.hh>
 #include <seastar/net/tls.hh>
 
 using namespace seastar;
 namespace bpo = boost::program_options;
 
 struct printer {
     future<consumption_result<char>> operator() (temporary_buffer<char> buf) {
         if (buf.empty()) {
             return make_ready_future<consumption_result<char>>(stop_consuming(std::move(buf)));
         }
         fmt::print("{}", sstring(buf.get(), buf.size()));
         return make_ready_future<consumption_result<char>>(continue_consuming());
     }
 };
 
 int main(int ac, char** av) {
     app_template app;
     app.add_options()
             ("https", bpo::bool_switch(), "Use HTTPS on port 443 (if off -- use HTTP on port 80)")
             ("host", bpo::value<std::string>(), "Host to connect")
             ("path", bpo::value<std::string>(), "Path to query upon")
             ("method", bpo::value<std::string>()->default_value("GET"), "Method to use")
             ("file", bpo::value<std::string>(), "File to get body from (no body if missing)")
     ;
 
 
     return app.run(ac, av, [&] {
         auto&& config = app.configuration();
         auto host = config["host"].as<std::string>();
         auto path = config["path"].as<std::string>();
         auto method = config["method"].as<std::string>();
         auto body = config.count("file") == 0 ? std::string("") : config["file"].as<std::string>();
         auto https = config["https"].as<bool>();
 
         return seastar::async([=] {
             net::hostent e = net::dns::get_host_by_name(host, net::inet_address::family::INET).get();
             std::unique_ptr<http::experimental::client> cln;
             if (https) {
                 auto certs = ::make_shared<tls::certificate_credentials>();
                 certs->set_system_trust().get();
                 fmt::print("{} {}:443{}\n", method, e.addr_list.front(), path);
                 cln = std::make_unique<http::experimental::client>(socket_address(e.addr_list.front(), 443), std::move(certs), host);
             } else {
                 fmt::print("{} {}:80{}\n", method, e.addr_list.front(), path);
                 cln = std::make_unique<http::experimental::client>(socket_address(e.addr_list.front(), 80));
             }
             auto req = http::request::make(method, host, path);
             if (body != "") {
                 future<file> f = open_file_dma(body, open_flags::ro);
                 req.write_body("txt", [ f = std::move(f) ] (output_stream<char>&& out) mutable {
                     return seastar::async([f = std::move(f), out = std::move(out)] () mutable {
                         auto in = make_file_input_stream(f.get());
                         copy(in, out).get();
                         out.flush().get();
                         out.close().get();
                         in.close().get();
                     });
                 });
             }
             cln->make_request(std::move(req), [] (const http::reply& rep, input_stream<char>&& in) {
                 fmt::print("Reply status {}\n--------8<--------\n", rep._status);
                 return seastar::async([in = std::move(in)] () mutable {
                     in.consume(printer{}).get();
                     in.close().get();
                 });
             }).get();
 
             cln->close().get();
         }).handle_exception([](auto ep) {
             fmt::print("Error: {}", ep);
         });
     });
 }