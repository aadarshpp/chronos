public class ChronosClient {
    
    static {
        System.loadLibrary("chronoscore");
    }

    // 1. Initialize the C engine, returns a pointer to the C struct
    public native long initEngine();

    // 2. Insert data. Note: 'price' is now an 'int' (Fixed-Point)
    public native void insertCompressed(long ptr, long timestamp, int fixedPrice);

    // 3. Close the engine, flush buffer, close file
    public native void closeEngine(long ptr);
}