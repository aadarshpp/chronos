import streamlit as st
import struct
import os
import pandas as pd
import mplfinance as mpf
import subprocess

# --- CONFIGURATION ---
FILE_PATH = "data/market_data.bin"
RECORD_SIZE = 32
FORMAT = '<QiiiiII' 

# --- PAGE SETUP ---
st.set_page_config(page_title="Metric-T", page_icon="📈", layout="wide")
# Suppress plotly warning if you have it, though we are removing plotly
st.set_option('deprecation.showPyplotGlobalUse', False)

st.title("Metric-T: High-Performance Financial Engine")
st.markdown("### Binary Storage (C) + Visualization (Python)")

# --- SIDEBAR CONTROLS ---
with st.sidebar:
    st.header("Controls")
    
    # We use a form to prevent the script from rerunning immediately while typing
    with st.form("fetch_form"):
        st.write("Click to fetch live data from API")
        submitted = st.form_submit_button("Refresh Data")
        
        if submitted:
            with st.spinner("Calling C Engine..."):
                result = subprocess.run(["./fetcher"], capture_output=True, text=True)
                if result.returncode == 0:
                    st.success("Data Updated Successfully!")
                    st.rerun() # Only rerun if we actually changed the data
                else:
                    st.error(f"C Engine Error: {result.stderr}")

    st.markdown("---")
    st.metric("Engine", "C (libcurl + cJSON)")
    st.metric("Storage", "Binary (32-byte Fixed)")

# --- DATA LOADER ---
@st.cache_data # CRITICAL: Prevents re-reading file on every interaction
def load_data():
    if not os.path.exists(FILE_PATH):
        return None
    
    records = []
    with open(FILE_PATH, "rb") as f:
        while True:
            data = f.read(RECORD_SIZE)
            if not data: break
            
            ts, open_p, high_p, low_p, close_p, vol, crc = struct.unpack(FORMAT, data)
            
            # Convert timestamp to datetime
            # mplfinance requires a Datetime Index
            dt = pd.to_datetime(ts, unit='s')
            
            records.append({
                "Date": dt,
                "Open": open_p / 10000.0,
                "High": high_p / 10000.0,
                "Low": low_p / 10000.0,
                "Close": close_p / 10000.0,
                "Volume": vol
            })
            
    df = pd.DataFrame(records)
    df.set_index("Date", inplace=True) # Required for mplfinance
    return df

df = load_data()

# --- MAIN DISPLAY ---
if df is not None:
    st.subheader(f"Market Data ({len(df)} Records)")
    
    # Metrics
    col1, col2, col3 = st.columns(3)
    col1.metric("Latest Price", f"${df['Close'].iloc[-1]:.2f}")
    col2.metric("Volume", f"{df['Volume'].sum():,}")
    col3.metric("File Size", f"{os.path.getsize(FILE_PATH)} bytes")

    # Plot using Matplotlib (Faster image generation)
    # style='yahoo' makes it look professional without custom CSS
    fig, axlist = mpf.plot(df, type='candle', style='yahoo', 
                           title="AAPL Intraday (1 Minute)", 
                           returnfig=True, 
                           figsize=(12, 6))
    
    st.pyplot(fig)
    
    # Raw Data Preview
    with st.expander("View Raw Data"):
        st.dataframe(df.tail(10))
else:
    st.warning("No data found. Click 'Refresh Data' to fetch.")