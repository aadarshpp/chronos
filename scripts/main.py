from fastapi import FastAPI, Request
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
import struct
import os
import pandas as pd
import uvicorn

app = FastAPI()

# --- CONFIG ---
FILE_PATH = "data/market_data.bin"
RECORD_SIZE = 32
FORMAT = '<QiiiiII'

# --- HELPER: LOAD DATA (Cached in Memory) ---
# In a real app, you'd use Redis. Here we just keep it in RAM.
df_cache = None

def get_data():
    global df_cache
    if df_cache is None:
        records = []
        with open(FILE_PATH, "rb") as f:
            while True:
                data = f.read(RECORD_SIZE)
                if not data: break
                ts, o, h, l, c, v, crc = struct.unpack(FORMAT, data)
                # Chart.js needs milliseconds
                records.append({
                    "x": ts * 1000, 
                    "o": o / 10000.0, 
                    "h": h / 10000.0, 
                    "l": l / 10000.0, 
                    "c": c / 10000.0
                })
        df_cache = records
    return df_cache

# --- ROUTE 1: SERVE THE HTML ---
@app.get("/", response_class=HTMLResponse)
async def read_root():
    with open("template/index.html", "r") as f:
        return f.read()

# --- ROUTE 2: API ENDPOINT (Returns JSON) ---
@app.get("/api/data")
async def get_api_data():
    return get_data()

if __name__ == "__main__":
    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=True)