import com.sun.net.httpserver.HttpServer;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpExchange;

import java.io.IOException;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.util.concurrent.Executors;

public class ChronosServer {

    public static void main(String[] args) throws IOException {
        // 1. Create the server listening on port 8080
        HttpServer server = HttpServer.create(new InetSocketAddress(8080), 0);

        // 2. Attach routes (endpoints)
        server.createContext("/insert", new InsertHandler());
        server.createContext("/query", new QueryHandler());

        // 3. CRITICAL REQUIREMENT: Set the thread pool to 4
        server.setExecutor(Executors.newFixedThreadPool(4));

        // 4. Start the server
        System.out.println("Chronos Server started on port 8080...");
        server.start();
    }

    // --- Handler for /insert ---
    static class InsertHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            // We only accept GET requests for simplicity in this project
            if ("GET".equals(exchange.getRequestMethod())) {
                String query = exchange.getRequestURI().getQuery();
                
                // Extract variables from the URL string
                String timeStr = getParam(query, "time");
                String priceStr = getParam(query, "price");

                if (timeStr != null && priceStr != null) {
                    try {
                        // Parse strings into the exact types C expects
                        long timestamp = Long.parseLong(timeStr);
                        double price = Double.parseDouble(priceStr);

                        // Send to our Phase 1 C bridge
                        ChronosClient client = new ChronosClient();
                        client.insertRaw(timestamp, price);

                        // Send HTTP 200 OK response back to curl
                        sendResponse(exchange, 200, "OK: Data inserted\n");
                    } catch (NumberFormatException e) {
                        sendResponse(exchange, 400, "ERROR: Invalid number format\n");
                    }
                } else {
                    sendResponse(exchange, 400, "ERROR: Missing 'time' or 'price' parameters\n");
                }
            } else {
                sendResponse(exchange, 405, "Method Not Allowed\n");
            }
        }
    }

    // --- Handler for /query (Dummy for now) ---
    static class QueryHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            // We will wire this up in Phase 4. For now, just acknowledge it exists.
            sendResponse(exchange, 200, "Query endpoint reached. Implementation pending Phase 4.\n");
        }
    }

    // --- Helper Methods ---
    private static void sendResponse(HttpExchange exchange, int statusCode, String response) throws IOException {
        exchange.sendResponseHeaders(statusCode, response.getBytes().length);
        OutputStream os = exchange.getResponseBody();
        os.write(response.getBytes());
        os.close();
    }

    // Simple helper to parse "key=value" from a URL query string
    private static String getParam(String query, String key) {
        if (query == null) return null;
        String[] pairs = query.split("&");
        for (String pair : pairs) {
            String[] kv = pair.split("=");
            if (kv.length == 2 && kv[0].equals(key)) {
                return kv[1];
            }
        }
        return null;
    }
}