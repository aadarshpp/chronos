import com.sun.net.httpserver.HttpServer;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpExchange;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.util.concurrent.Executors;

public class ChronosServer {
    
    // MUST be static so the HTTP handler inner-classes can see them
    static ChronosClient client = new ChronosClient();
    static long enginePointer; 

    public static void main(String[] args) throws IOException {
        // ASSIGN DIRECTLY TO THE STATIC VARIABLE!
        enginePointer = client.initEngine();
        System.out.println("C Engine initialized. Pointer: " + enginePointer);

        HttpServer server = HttpServer.create(new InetSocketAddress(8080), 0);
        server.createContext("/insert", new InsertHandler());
        server.createContext("/close", new CloseHandler());
        server.createContext("/query", new QueryHandler());
        server.createContext("/stats", new StatsHandler());
        server.createContext("/", new RootHandler());
        
        server.setExecutor(Executors.newFixedThreadPool(4));
        server.start();
        System.out.println("Chronos Server started on port 8080...");

        try {
            Thread.currentThread().join();
        } catch (InterruptedException e) {
            // Shutdown
        }
    }

    static class InsertHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            String query = exchange.getRequestURI().getQuery();
            String[] params = query.split("&");
            long ts = 0;
            double price = 0.0;

            for (String param : params) {
                String[] pair = param.split("=");
                if (pair[0].equals("time")) ts = Long.parseLong(pair[1]);
                if (pair[0].equals("price")) price = Double.parseDouble(pair[1]);
            }

            int fixedPrice = (int)(price * 100);
            client.insertCompressed(enginePointer, ts, fixedPrice); // Uses STATIC variable
            
            sendResponse(exchange, 200, "OK: Compressed Insert");
        }
    }

    static class CloseHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            client.closeEngine(enginePointer); // Uses STATIC variable
            sendResponse(exchange, 200, "OK: Engine Closed. File saved.");
        }
    }

    static class QueryHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            String query = exchange.getRequestURI().getQuery();
            String[] params = query.split("&");
            long startTs = 0;
            long endTs = Long.MAX_VALUE;

            for (String param : params) {
                String[] pair = param.split("=");
                if (pair[0].equals("start")) startTs = Long.parseLong(pair[1]);
                if (pair[0].equals("end")) endTs = Long.parseLong(pair[1]);
            }

            String result = client.queryData(startTs, endTs); 
            sendResponse(exchange, 200, result);
        }
    }

    static class StatsHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            String result = client.getIndexStats(enginePointer); 
            sendResponse(exchange, 200, result);
        }
    }

    static class RootHandler implements HttpHandler {
        @Override
        public void handle(HttpExchange exchange) throws IOException {
            InputStream is = ChronosServer.class.getResourceAsStream("index.html");
            if (is == null) {
                sendResponse(exchange, 404, "404 Not Found");
                return;
            }
            byte[] htmlBytes = is.readAllBytes();
            exchange.sendResponseHeaders(200, htmlBytes.length);
            OutputStream os = exchange.getResponseBody();
            os.write(htmlBytes);
            os.close();
        }
    }

    static void sendResponse(HttpExchange exchange, int statusCode, String responseText) throws IOException {
        exchange.sendResponseHeaders(statusCode, responseText.getBytes().length);
        OutputStream os = exchange.getResponseBody();
        os.write(responseText.getBytes());
        os.close();
    }
}