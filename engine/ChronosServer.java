import com.sun.net.httpserver.HttpServer;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpExchange;
import java.io.IOException;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.util.concurrent.Executors;

public class ChronosServer {

    // Store the C struct pointer globally for the server's lifetime
    private static long enginePointer;

    public static void main(String[] args) throws IOException {
        ChronosClient client = new ChronosClient();
        enginePointer = client.initEngine();
        System.out.println("C Engine initialized. Pointer: " + enginePointer);

        HttpServer server = HttpServer.create(new InetSocketAddress(8080), 0);
        server.createContext("/insert", new InsertHandler());
        server.createContext("/query", new QueryHandler());
        server.createContext("/close", new CloseHandler());
        server.createContext("/stats", new StatsHandler()); 
        server.setExecutor(Executors.newFixedThreadPool(4));
        
        System.out.println("Chronos Server started on port 8080...");
        server.start();
    }

    static class InsertHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if ("GET".equals(exchange.getRequestMethod())) {
                String query = exchange.getRequestURI().getQuery();
                String timeStr = getParam(query, "time");
                String priceStr = getParam(query, "price");

                if (timeStr != null && priceStr != null) {
                    try {
                        long timestamp = Long.parseLong(timeStr);
                        double rawPrice = Double.parseDouble(priceStr);
                        
                        // FIXED POINT CONVERSION
                        int fixedPrice = (int) Math.round(rawPrice * 100);

                        ChronosClient client = new ChronosClient();
                        client.insertCompressed(enginePointer, timestamp, fixedPrice);

                        sendResponse(exchange, 200, "OK: Compressed Insert\n");
                    } catch (Exception e) {
                        sendResponse(exchange, 500, "ERROR: " + e.getMessage() + "\n");
                    }
                } else {
                    sendResponse(exchange, 400, "ERROR: Missing params\n");
                }
            }
        }
    }

    static class CloseHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            ChronosClient client = new ChronosClient();
            client.closeEngine(enginePointer);
            sendResponse(exchange, 200, "OK: Engine Closed. File saved.\n");
            // In a real app, you'd exit(0) here. For testing, we just leave server running.
        }
    }

    static class StatsHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            ChronosClient client = new ChronosClient();
            String stats = client.getIndexStats(enginePointer);
            sendResponse(exchange, 200, stats + "\n");
        }
    }

    static class QueryHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            if ("GET".equals(exchange.getRequestMethod())) {
                String query = exchange.getRequestURI().getQuery();
                String startStr = getParam(query, "start");
                String endStr = getParam(query, "end");

                if (startStr != null && endStr != null) {
                    try {
                        long qStart = Long.parseLong(startStr);
                        long qEnd = Long.parseLong(endStr);
                        
                        ChronosClient client = new ChronosClient();
                        String jsonResult = client.queryData(qStart, qEnd);
                        
                        sendResponse(exchange, 200, jsonResult);
                    } catch (Exception e) {
                        sendResponse(exchange, 500, "ERROR: " + e.getMessage() + "\n");
                    }
                } else {
                    sendResponse(exchange, 400, "ERROR: Missing start/end params\n");
                }
            }
        }
    }

    private static void sendResponse(HttpExchange exchange, int statusCode, String response) throws IOException {
        exchange.sendResponseHeaders(statusCode, response.getBytes().length);
        OutputStream os = exchange.getResponseBody();
        os.write(response.getBytes());
        os.close();
    }

    private static String getParam(String query, String key) {
        if (query == null) return null;
        for (String pair : query.split("&")) {
            String[] kv = pair.split("=");
            if (kv.length == 2 && kv[0].equals(key)) return kv[1];
        }
        return null;
    }
}