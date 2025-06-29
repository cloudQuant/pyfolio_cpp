#include <chrono>
#include <httplib.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

/**
 * @brief Simple REST API example for pyfolio
 *
 * This demonstrates basic REST API functionality using httplib and JSON.
 * This is a minimal working example that can be extended with actual
 * pyfolio functionality as the dependency issues are resolved.
 */

int main() {
    httplib::Server server;

    // Enable CORS
    server.set_post_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    });

    server.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 200;
    });

    // Health check endpoint
    server.Get("/api/v1/health", [](const httplib::Request&, httplib::Response& res) {
        json response = {
            {  "success",true                         },
            {   "status", "healthy"},
            {  "version",   "1.0.0"},
            {"timestamp",
             std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
             .count()              }
        };
        res.set_content(response.dump(), "application/json");
    });

    // Simple calculation endpoint (basic math example)
    server.Post("/api/v1/calculate/simple", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);

            if (!request_data.contains("values") || !request_data["values"].is_array()) {
                json error_response = {
                    {"success",                               false},
                    {  "error", "Missing or invalid 'values' array"}
                };
                res.status = 400;
                res.set_content(error_response.dump(), "application/json");
                return;
            }

            std::vector<double> values = request_data["values"];

            if (values.empty()) {
                json error_response = {
                    {"success",                          false},
                    {  "error", "Values array cannot be empty"}
                };
                res.status = 400;
                res.set_content(error_response.dump(), "application/json");
                return;
            }

            // Calculate basic statistics
            double sum     = 0.0;
            double min_val = values[0];
            double max_val = values[0];

            for (double val : values) {
                sum += val;
                if (val < min_val)
                    min_val = val;
                if (val > max_val)
                    max_val = val;
            }

            double mean = sum / values.size();

            // Calculate variance
            double variance = 0.0;
            for (double val : values) {
                variance += (val - mean) * (val - mean);
            }
            variance /= values.size();
            double std_dev = std::sqrt(variance);

            json response = {
                {"success",true                          },
                {   "data",
                 {{"count", values.size()},
                 {"sum", sum},
                 {"mean", mean},
                 {"min", min_val},
                 {"max", max_val},
                 {"variance", variance},
                 {"std_deviation", std_dev}}}
            };

            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            json error_response = {
                {"success",                                          false},
                {  "error", std::string("Calculation failed: ") + e.what()}
            };
            res.status = 500;
            res.set_content(error_response.dump(), "application/json");
        }
    });

    // Echo endpoint for testing
    server.Post("/api/v1/echo", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);
            json response     = {
                {  "success",true                             },
                {     "echo", request_data},
                {"timestamp",
                 std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                 .count()                 }
            };
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            json error_response = {
                {"success",                                          false},
                {  "error", std::string("JSON parsing error: ") + e.what()}
            };
            res.status = 400;
            res.set_content(error_response.dump(), "application/json");
        }
    });

    // Basic portfolio value calculation (simplified)
    server.Post("/api/v1/portfolio/value", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);

            if (!request_data.contains("holdings") || !request_data["holdings"].is_array()) {
                json error_response = {
                    {"success",                                 false},
                    {  "error", "Missing or invalid 'holdings' array"}
                };
                res.status = 400;
                res.set_content(error_response.dump(), "application/json");
                return;
            }

            double total_value = 0.0;
            double cash        = request_data.value("cash", 0.0);

            for (const auto& holding : request_data["holdings"]) {
                double shares = holding.value("shares", 0.0);
                double price  = holding.value("price", 0.0);
                total_value += shares * price;
            }

            total_value += cash;

            json response = {
                {"success",          true                          },
                {   "data",
                 {{"total_value", total_value},
                 {"cash", cash},
                 {"securities_value", total_value - cash},
                 {"num_holdings", request_data["holdings"].size()}}}
            };

            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            json error_response = {
                {"success",                                                    false},
                {  "error", std::string("Portfolio calculation failed: ") + e.what()}
            };
            res.status = 500;
            res.set_content(error_response.dump(), "application/json");
        }
    });

    std::cout << "=== Simple Pyfolio REST API Server ===\n";
    std::cout << "Starting server on http://localhost:8080\n";
    std::cout << "Available endpoints:\n";
    std::cout << "  - GET  /api/v1/health\n";
    std::cout << "  - POST /api/v1/calculate/simple\n";
    std::cout << "  - POST /api/v1/echo\n";
    std::cout << "  - POST /api/v1/portfolio/value\n";
    std::cout << "\nExample usage:\n";
    std::cout << "curl http://localhost:8080/api/v1/health\n";
    std::cout << "curl -X POST http://localhost:8080/api/v1/calculate/simple \\\n";
    std::cout << "  -H \"Content-Type: application/json\" \\\n";
    std::cout << "  -d '{\"values\": [1.0, 2.0, 3.0, 4.0, 5.0]}'\n";
    std::cout << "\nPress Ctrl+C to stop the server\n";

    // Start server
    server.listen("0.0.0.0", 8080);

    return 0;
}