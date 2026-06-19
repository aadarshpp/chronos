public class ChronosClient {
    static { System.loadLibrary("chronoscore"); }

    public native long initEngine();
    public native void insertCompressed(long ptr, long timestamp, int fixedPrice);
    public native void closeEngine(long ptr);
    public native String getIndexStats(long ptr);
    
    // The Query Method
    public native String queryData(long startTs, long endTs);
}