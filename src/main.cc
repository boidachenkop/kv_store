#include <seastar/core/app-template.hh>

using namespace seastar;

extern future<> start();

int main(int argc, char** argv)
{
    app_template app;
    try {
        app.run(argc, argv, start);
    } catch (...) {
        std::cerr << "Couldn't start application: " << std::current_exception() << "\n";
        return 1;
    }
    return 0;
}