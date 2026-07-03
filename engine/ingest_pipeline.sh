#!/bin/bash
# Reads JSON from a file or command and streams it into Chronos

# We use 'jq' to parse the JSON. (If you don't have jq: sudo apt install jq)
# This reads the JSON array and outputs "ts price" line by line
cat $1 | jq -r '.[] | "\(.ts) \(.price)"' | while read -r ts price; do
    # Fire the API call in the background (&) for maximum speed
    curl -s "http://localhost:8080/insert?time=${ts}&price=${price}" > /dev/null &
    
    # Tiny sleep to prevent DOS-ing your local machine's port limit
    sleep 0.001
done

# Wait for all background curl processes to finish
wait
echo "Pipeline Ingestion Complete."