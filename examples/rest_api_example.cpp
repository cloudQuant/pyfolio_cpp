#include <chrono>
#include <iostream>
#include <pyfolio/web/rest_api_server.h>
#include <thread>

using namespace pyfolio;
using namespace pyfolio::web;

int main() {
    // Configure REST API server
    ApiConfig config;
    config.host           = "0.0.0.0";  // Listen on all interfaces
    config.port           = 8080;
    config.base_path      = "/api/v1";
    config.enable_cors    = true;
    config.enable_logging = true;

    // Create and start server
    RestApiServer server(config);

    // Start server in a separate thread
    std::thread server_thread([&server]() { server.start(); });

    std::cout << "\n=== Pyfolio REST API Server ===\n";
    std::cout << "Server is running at http://localhost:8080\n";
    std::cout << "API documentation:\n";
    std::cout << "  - Health Check: GET /api/v1/health\n";
    std::cout << "  - Performance Analysis: POST /api/v1/analyze/performance\n";
    std::cout << "  - Portfolio Analysis: POST /api/v1/analyze/portfolio\n";
    std::cout << "  - Transaction Analysis: POST /api/v1/analyze/transactions\n";
    std::cout << "  - Calculate Sharpe: POST /api/v1/calculate/sharpe\n";
    std::cout << "  - Calculate Drawdown: POST /api/v1/calculate/drawdown\n";
    std::cout << "  - Calculate Volatility: POST /api/v1/calculate/volatility\n";
    std::cout << "  - Batch Analysis: POST /api/v1/analyze/batch\n";
    std::cout << "\nPress 'q' to quit...\n";

    // Example curl commands
    std::cout << "\n=== Example API Calls ===\n";
    std::cout << "Health check:\n";
    std::cout << "  curl http://localhost:8080/api/v1/health\n";

    std::cout << "\nPerformance analysis:\n";
    std::cout << "  curl -X POST http://localhost:8080/api/v1/analyze/performance \\\n";
    std::cout << "    -H \"Content-Type: application/json\" \\\n";
    std::cout << "    -d '{\n";
    std::cout << "      \"returns\": {\n";
    std::cout << "        \"data\": [\n";
    std::cout << "          {\"timestamp\": \"2024-01-01T00:00:00Z\", \"value\": 0.01},\n";
    std::cout << "          {\"timestamp\": \"2024-01-02T00:00:00Z\", \"value\": -0.005},\n";
    std::cout << "          {\"timestamp\": \"2024-01-03T00:00:00Z\", \"value\": 0.008}\n";
    std::cout << "        ]\n";
    std::cout << "      }\n";
    std::cout << "    }'\n";

    std::cout << "\nPortfolio analysis:\n";
    std::cout << "  curl -X POST http://localhost:8080/api/v1/analyze/portfolio \\\n";
    std::cout << "    -H \"Content-Type: application/json\" \\\n";
    std::cout << "    -d '{\n";
    std::cout << "      \"cash\": 10000,\n";
    std::cout << "      \"holdings\": [\n";
    std::cout << "        {\"symbol\": \"AAPL\", \"shares\": 100, \"price\": 150, \"last_price\": 155},\n";
    std::cout << "        {\"symbol\": \"GOOGL\", \"shares\": 50, \"price\": 2800, \"last_price\": 2850}\n";
    std::cout << "      ]\n";
    std::cout << "    }'\n";

    // Wait for user to quit
    char input;
    while (std::cin >> input) {
        if (input == 'q' || input == 'Q') {
            break;
        }
    }

    // Stop server
    std::cout << "\nStopping server...\n";
    server.stop();

    // Wait for server thread to finish
    if (server_thread.joinable()) {
        server_thread.join();
    }

    std::cout << "Server stopped.\n";

    return 0;
}