public class ChronosClient {
    
    static {
        System.loadLibrary("chronoscore");
    }

    public native void insertRaw(long timestamp, double price);
    
    // This class is just a wrapper.
}