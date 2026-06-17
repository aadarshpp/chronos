public class ChronosClient {
    
    static {
        System.loadLibrary("chronoscore");
    }

    // The exact same native method from Phase 1
    public native void insertRaw(long timestamp, double price);
    
    // This class is just a wrapper.
}