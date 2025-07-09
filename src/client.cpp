#include "client.hpp"
#include "httplib.h"
#include "linux_metrics.hpp"
#include <string>
#include <thread>

int main(int argc, char *argv[]) {
    using namespace std::chrono_literals;

    std::string server_url = (argc > 1) ? argv[1] : "127.0.0.1";
    bool verbose = (argc == 3 && std::string(argv[2]) == "1");

    httplib::Client clt(server_url, 8080);
    clt.set_connection_timeout(5);

    std::string client_id = "client-" + std::to_string(rand() % 1000);
    bool running = true;

    while (running) {
        httplib::Headers headers = {
            {"X-Client-ID", client_id},  // Регистр исправлен на "X-Client-ID"
            {"Host", "localhost"}        // Добавлен обязательный заголовок
        };

        auto metrics = linux_metrics::get_system_metrics(verbose);
        auto res = clt.Post("/api/metrics", headers, metrics.dump(), "application/json");

        if (!res) {
            std::cerr << "Connection error\n";
            break;
        }

        if (res->body == "SHUTDOWN") {
            running = false;
        }

        std::this_thread::sleep_for(2s);
    }

    return 0;
}
