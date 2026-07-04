import java.io.BufferedReader;
import java.io.FileReader;

public class DataLoader {
    public static void main(String[] args) {
        if (args.length != 1) {
            System.out.println("Usage: java DataLoader <path_to_csv>");
            System.out.println("CSV Format expected: timestamp,price (Header row optional)");
            return;
        }

        String csvFile = args[0];
        ChronosClient client = new ChronosClient();

        System.out.println("Initializing engine...");
        long ptr = client.initEngine();

        long startTime = System.currentTimeMillis();
        int count = 0;

        try (BufferedReader br = new BufferedReader(new FileReader(csvFile))) {
            String line;
            boolean isHeader = true;

            while ((line = br.readLine()) != null) {
                // Skip the first line if it says "timestamp"
                if (isHeader && line.startsWith("timestamp")) {
                    isHeader = false;
                    continue;
                }

                String[] parts = line.split(",");
                if (parts.length == 2) {
                    try {
                        long ts = Long.parseLong(parts[0].trim());
                        double price = Double.parseDouble(parts[1].trim());
                        
                        // Convert to fixed point just like the HTTP server does
                        int fixedPrice = (int) Math.round(price * 100);
                        
                        client.insertCompressed(ptr, ts, fixedPrice);
                        count++;
                        
                        // Print progress every 10k rows
                        if (count % 10000 == 0) {
                            System.out.println("Ingested " + count + " records...");
                        }
                    } catch (NumberFormatException e) {
                        System.out.println("Skipping malformed line: " + line);
                    }
                }
            }
        } catch (Exception e) {
            System.err.println("Error reading file: " + e.getMessage());
        }

        System.out.println("Flushing to disk...");
        client.closeEngine(ptr);

        long endTime = System.currentTimeMillis();
        long duration = endTime - startTime;

        System.out.println("\n=============================================");
        System.out.println("           DATA LOADER RESULTS               ");
        System.out.println("=============================================");
        System.out.println("File Processed : " + csvFile);
        System.out.println("Rows Ingested  : " + count);
        System.out.println("Time Taken     : " + duration + " ms");
        System.out.println("Throughput     : " + (count / (duration / 1000.0)) + " rows/sec");
        System.out.println("=============================================");
    }
}