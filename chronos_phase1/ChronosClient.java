public class ChronosClient {
    
    // 1. Load the C library immediately when this class is used
    static {
        System.loadLibrary("chronoscore");
    }

    // 2. Declare the C function. 'native' means Java doesn't write the body.
    public native void insertRaw(long timestamp, double price);

    public static void main(String[] args) {
        ChronosClient client = new ChronosClient();

        long ts1 = 1690000000000L; // Aug 2023 in milliseconds
        double price1 = 150.25;

        long ts2 = 1690000000001L;
        double price2 = 150.50;

        System.out.println("Sending data to C...");
        client.insertRaw(ts1, price1);
        client.insertRaw(ts2, price2);
        System.out.println("Done. Check data.chronos!");
    }
}