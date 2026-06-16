from fastapi import FastAPI, Request
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
import struct
import os
import json
import pandas as pd
import uvicorn

app = FastAPI()

# --- CONFIG ---
# FILE_PATH = "data/market_data.bin"
CATALOG_FILE = "data/catalog.json"
RECORD_SIZE = 32
FORMAT = '<QiiiiII'

# --- HELPER: LOAD DATA (Cached in Memory) ---
# In a real app, you'd use Redis. Here we just keep it in RAM.
df_cache = None

def load_catalog():
    with open(CATALOG_FILE, "r") as f:
        return json.load(f)

def get_matching_partitions(start, end):
    catalog = load_catalog()

    matching = []

    for partition in catalog:
        if partition["end"] >= start and partition["start"] <= end:
            matching.append(partition)

    return matching

def read_partition(file_path):
    records = []

    with open(file_path, "rb") as f:
        while True:
            data = f.read(RECORD_SIZE)

            if not data:
                break

            ts, o, h, l, c, v, crc = struct.unpack(FORMAT, data)

            records.append({
                "x": ts * 1000,
                "o": o / 10000.0,
                "h": h / 10000.0,
                "l": l / 10000.0,
                "c": c / 10000.0
            })

    return records

def get_data(start=None, end=None):
    global df_cache
    # if df_cache is None:
    #     records = []
    #     with open(FILE_PATH, "rb") as f:
    #         while True:
    #             data = f.read(RECORD_SIZE)
    #             if not data: break
    #             ts, o, h, l, c, v, crc = struct.unpack(FORMAT, data)
    #             # Chart.js needs milliseconds
    #             records.append({
    #                 "x": ts * 1000, 
    #                 "o": o / 10000.0, 
    #                 "h": h / 10000.0, 
    #                 "l": l / 10000.0, 
    #                 "c": c / 10000.0
    #             })
    #     df_cache = records
    # return df_cache
    if start is None and end is None and df_cache is not None:
        return df_cache
    records = []

    # partitions = load_catalog()
    if start is not None and end is not None:
        partitions = get_matching_partitions(start, end)
        print("MATCHING PARTITIONS:", partitions)
    else:
        partitions = load_catalog()

    for partition in partitions:
        partition_records = read_partition(partition["file"])
        records.extend(partition_records)

    if start is None and end is None:
        df_cache = records

    return records

# --- ROUTE 1: SERVE THE HTML ---
@app.get("/", response_class=HTMLResponse)
async def read_root():
    with open("template/index.html", "r") as f:
        return f.read()

# --- ROUTE 2: API ENDPOINT (Returns JSON) ---
@app.get("/api/data")
# async def get_api_data():
#     return get_data()
async def get_api_data(start: int = None, end: int = None):
    print("API RANGE:", start, end) 
    return get_data(start, end)

if __name__ == "__main__":
    uvicorn.run("main:app", host="127.0.0.1", port=8000, reload=True)