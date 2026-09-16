#!/usr/bin/env python3
"""
Synthetic Sales Dataset Generator for Parallel Query Execution Engine
Schema:
  1. TransactionID (int32_t, unique primary key, 1..N)
  2. CustomerID (int32_t, foreign key, 1..100000)
  3. Quantity (int32_t, purchased units, 1..100)
  4. Price (float, unit price, 5.0 to 1500.0)
  5. Discount (float, discount multiplier, 0.00 to 0.30)
  6. CategoryID (int32_t, product category, 1..50)
  7. StoreRegion (string, Geographic Region: North, South, East, West, Central)
"""

import argparse
import os
import random
import sys
import time

REGIONS = ["North", "South", "East", "West", "Central"]

def generate_sales_data(row_count: int, output_path: str, batch_size: int = 100_000):
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    
    print(f"[GENERATOR] Generating {row_count:,} rows into '{output_path}'...")
    start_time = time.perf_counter()
    
    random.seed(42)  # Deterministic seed for reproducible benchmarks
    
    rows_written = 0
    with open(output_path, "w", encoding="utf-8", newline="\n") as f:
        # Write CSV Header
        f.write("TransactionID,CustomerID,Quantity,Price,Discount,CategoryID,StoreRegion\n")
        
        while rows_written < row_count:
            current_batch = min(batch_size, row_count - rows_written)
            lines = []
            for i in range(current_batch):
                tx_id = rows_written + i + 1
                cust_id = random.randint(1, 100_000)
                qty = random.randint(1, 100)
                price = round(random.uniform(5.0, 1500.0), 2)
                discount = round(random.choice([0.0, 0.05, 0.10, 0.15, 0.20, 0.25, 0.30]), 2)
                cat_id = random.randint(1, 50)
                region = random.choice(REGIONS)
                lines.append(f"{tx_id},{cust_id},{qty},{price:.2f},{discount:.2f},{cat_id},{region}\n")
            
            f.writelines(lines)
            rows_written += current_batch
            
            elapsed = time.perf_counter() - start_time
            rate = rows_written / elapsed if elapsed > 0 else 0
            print(f"  Progress: {rows_written:,} / {row_count:,} rows ({rows_written / row_count * 100:.1f}%) "
                  f"- Rate: {rate:,.0f} rows/sec", end="\r")
            
    print()
    total_time = time.perf_counter() - start_time
    file_size_mb = os.path.getsize(output_path) / (1024 * 1024)
    print(f"[GENERATOR] Complete! Wrote {row_count:,} rows ({file_size_mb:.2f} MB) in {total_time:.2f}s "
          f"({row_count / total_time:,.0f} rows/sec).")

def main():
    parser = argparse.ArgumentParser(description="Generate synthetic sales CSV data for analytical queries.")
    parser.add_argument("--rows", type=int, default=250_000, help="Number of rows to generate (default: 250,000)")
    parser.add_argument("--out", type=str, default="data/sample/sales_250k.csv", help="Output file path")
    parser.add_argument("--batch-size", type=int, default=100_000, help="Batch write buffer size (default: 100,000)")
    args = parser.parse_args()
    
    generate_sales_data(args.rows, args.out, args.batch_size)

if __name__ == "__main__":
    main()
