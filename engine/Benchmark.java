import java.io.File;

public class Benchmark {
    public static void main(String[] args) {
        int NUM_RECORDS = 100000;
        ChronosClient client = new ChronosClient();

        System.out.println("Initializing C Engine...");
        long ptr = client.initEngine();

        System.out.println("Inserting " + NUM_RECORDS + " simulated tick records...");
        long startTime = System.currentTimeMillis();

        long ts = 1690000000000L; // Aug 2023
        int price = 15025;        // $150.25 in fixed-point

        for (int i = 0; i < NUM_RECORDS; i++) {
            // Simulate realistic market jitter
            ts += (long)(Math.random() * 5) + 1;      // Time moves forward 1 to 5 ms
            price += (int)(Math.random() * 11) - 5;    // Price moves -5 to +5 ticks
            
            // Prevent negative prices for the simulation
            if (price < 10000) price = 10000; 

            client.insertCompressed(ptr, ts, price);
        }

        System.out.println("Flushing buffers and writing index footer...");
        client.closeEngine(ptr);

        long endTime = System.currentTimeMillis();
        long durationMs = endTime - startTime;

        // Calculate theoretical raw size (8 bytes long + 8 bytes double per record)
        long rawSizeBytes = (long) NUM_RECORDS * 16; 
        File file = new File("data.chronos");
        long compressedSizeBytes = file.length();
        
        // Calculate space saved
        double compressionRatio = 100.0 - ((double) compressedSizeBytes / rawSizeBytes * 100.0);

        // Output the "Resume Numbers"
        System.out.println("\n=============================================");
        System.out.println("           CHRONOS BENCHMARK RESULTS          ");
        System.out.println("=============================================");
        System.out.println("Total Records Processed : " + NUM_RECORDS);
        System.out.println("Total Time Taken        : " + durationMs + " ms");
        System.out.println("Throughput              : " + (NUM_RECORDS / (durationMs / 1000.0)) + " records/sec");
        System.out.println("---------------------------------------------");
        System.out.println("Raw Data Footprint      : " + (rawSizeBytes / 1024 / 1024) + " MB");
        System.out.println("Chronos File Size       : " + (compressedSizeBytes / 1024) + " KB");
        System.out.printf("Space Saved (Compression): %.2f%%\n", compressionRatio);
        System.out.println("=============================================");
    }
}