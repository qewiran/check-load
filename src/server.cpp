#include <atomic>
#include "httplib.h"
#include "linux_metrics.hpp"
#include <unordered_map>

int main(int argc, char *argv[]) {
  httplib::Server svr;
  std::unordered_map<std::string, std::atomic<bool>> clients;
  const std::string ADMIN_TOKEN = "secret123";

  svr.Get("/ping", [](const httplib::Request &req, httplib::Response &res) {
    res.set_content("pong", "text/plain");
  });

  svr.Get("/admin/clients",
          [&ADMIN_TOKEN, &clients](const httplib::Request &req,
                                   httplib::Response &res) {
            std::string token = req.get_header_value("X-admin-token");
            if (token != ADMIN_TOKEN) {
              res.status = 403;
              res.set_content("access denied", "text/plain");
              return;
            }
            nlohmann::json clients_json;

            for (const auto &[id, active] : clients) {
              clients_json[id] = active.load();
            }
            res.set_content(clients_json.dump(), "application/json");
          });

  svr.Post(
      "/admin/shutdown", [&ADMIN_TOKEN, &clients](const httplib::Request &req,
                                                  httplib::Response &res) {
        std::string token = req.get_header_value("X-admin-token");
      std::cout<<"shutdown\n";
        if (token != ADMIN_TOKEN) {
          res.status = 403; // Forbidden
          res.set_content("access denied", "text/plain");
          return;
        }

        std::string client_id = req.get_param_value("id");
        if (clients.find(client_id) == clients.end()) {
          res.status = 404; // Not found
          res.set_content("client not found", "text/plain");
          return;
        }

        clients[client_id].store(false);
        res.set_content("Shutdown to client with id" + client_id, "text/plain");
      });

  svr.Post("/api/metrics",
           [&clients](const httplib::Request &req, httplib::Response &res) {
             std::string client_id = req.get_header_value("X-client-ID");

             if (client_id.empty()) {
               res.status = 400; // Bad Request
               res.set_content("X-client-ID header is missing", "text/plain");
               return;
             }

             if (clients.find(client_id) == clients.end()) {
               clients[client_id] = true;
             }

             if (!clients[client_id]) {
               res.set_content("SHUTDOWN", "text/plain");
               clients.erase(client_id);
               return;
             }

             try {
               auto request_body = req.body;
               nlohmann::json json_data = nlohmann::json::parse(request_body);
               std::cout << "Parsed JSON: " << json_data.dump(2) << std::endl;
             } catch (std::exception &e) {
               std::cerr << "JSON parsing error: " << e.what() << std::endl;
             }
             res.set_content("OK", "text/plain");
           });
  svr.listen("0.0.0.0", 8080);
  return 0;
}
