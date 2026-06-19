public class ChronosClient {
    
    static {
        System.loadLibrary("chronoscore");
    }

    public native long initEngine();
    public native void insertCompressed(long ptr, long timestamp, int fixedPrice);
    public native void closeEngine(long ptr);
    
    // Temporary helper for Sub-Phase 4A testing
    public native String getIndexStats(long ptr);
}