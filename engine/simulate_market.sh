#!/bin/bash
# Generates a realistic JSON array of 500 stock trades

echo "["
ts=1696158000000 # Sept 2023, 10:00 AM
price=15025      # $150.25

for i in $(seq 1 500); do
    # Simulate market jitter: time moves 1-10ms, price moves -3 to +3 cents
    ts=$((ts + RANDOM % 10 + 1))
    price=$((price + RANDOM % 7 - 3))
    if [ $price -lt 10000 ]; then price=10000; fi

    # Print standard JSON format
    printf '  {"ts": %d, "price": %.2f}' $ts $(echo "scale=2; $price / 100" | bc)
    
    # Add a comma unless it's the last element
    if [ $i -ne 500 ]; then
        echo ","
    else
        echo ""
    fi
done
echo "]"