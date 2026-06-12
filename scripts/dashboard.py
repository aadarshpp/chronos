import streamlit as st
import struct
import os
import pandas as pd
import plotly.graph_objects as go
import subprocess

# --- CONFIGURATION ---
FILE_PATH = "data/market_data.bin"
RECORD_SIZE = 32
FORMAT = '<QiiiiII' # Timestamp, Open, High, Low, Close, Volume, CRC

# --- PAGE SETUP ---
st.set_page_config(page_title="Metric-T", page_icon="📈", layout="wide")
st.title("Metric-T: High-Performance Financial Engine")
st.markdown("### Binary Storage (C) + Visualization (Python)")

# --- SIDEBAR CONTROLS ---
with st.sidebar:
    st.header("Controls")
    if st.button("Refresh Data (Fetch from API)"):
        with st.spinner("Calling C Engine..."):
            # Call the compiled C binary
            result = subprocess.run(["./fetcher"], capture_output=True, text=True)
            if result.returncode == 0:
                st.success("Data Updated Successfully!")
                st.rerun()
            else:
                st.error(f"Error: {result.stderr}")

    st.markdown("---")
    st.metric("Engine", "C (libcurl + cJSON)")
    st.metric("Storage", "Binary (32-byte Fixed)")

# --- DATA LOADER ---
@st.cache_data # Cache the data so we don't reload on every click
def load_data():
    if not os.path.exists(FILE_PATH):
        return None
    
    records = []
    with open(FILE_PATH, "rb") as f:
        while True:
            data = f.read(RECORD_SIZE)
            if not data: break
            
            # Unpack
            ts, open_p, high_p, low_p, close_p, vol, crc = struct.unpack(FORMAT, data)
            
            # Convert timestamp to datetime for plotting
            # Note: ts is Unix epoch
            dt = pd.to_datetime(ts, unit='s')
            
            records.append({
                "Date": dt,
                "Open": open_p / 10000.0,
                "High": high_p / 10000.0,
                "Low": low_p / 10000.0,
                "Close": close_p / 10000.0,
                "Volume": vol
            })
            
    return pd.DataFrame(records)

df = load_data()

# --- MAIN DISPLAY ---
if df is not None:
    st.subheader(f"Market Data ({len(df)} Records)")
    
    # Metrics
    col1, col2, col3 = st.columns(3)
    col1.metric("Latest Price", f"${df['Close'].iloc[-1]:.2f}")
    col2.metric("Volume", f"{df['Volume'].sum():,}")
    col3.metric("File Size", f"{os.path.getsize(FILE_PATH)} bytes")

    # Plotly Candlestick Chart
    fig = go.Figure(data=[go.Candlestick(
        x=df['Date'],
        open=df['Open'],
        high=df['High'],
        low=df['Low'],
        close=df['Close']
    )])

    fig.update_layout(
        title="AAPL Intraday (1 Minute)",
        xaxis_rangeslider_visible=False,
        height=600
    )
    
    st.plotly_chart(fig, use_container_width=True)
    
    # Raw Data Preview
    with st.expander("View Raw Data"):
        st.dataframe(df.tail(10))
else:
    st.warning("No data found. Click 'Refresh Data' in the sidebar to fetch from API.")