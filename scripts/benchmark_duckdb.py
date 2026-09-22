import duckdb
import time
import argparse
import sys

def run_benchmark(csv_path: str):
    print(f"===============================================================")
    print(f"  DuckDB Baseline Benchmark")
    print(f"===============================================================")
    print(f"[CONFIG] Target Dataset: {csv_path}")
    
    # Initialize DuckDB in-memory
    con = duckdb.connect(database=':memory:')
    
    # Phase 1: Ingestion
    print(f"---------------------------------------------------------------")
    print(f"[STAGE 1 & 2] Ingesting CSV into DuckDB Columnar Memory...")
    start_time = time.perf_counter()
    con.execute(f"CREATE TABLE sales AS SELECT * FROM read_csv_auto('{csv_path}')")
    ingest_time = (time.perf_counter() - start_time) * 1000.0
    
    row_count = con.execute("SELECT COUNT(*) FROM sales").fetchone()[0]
    print(f"  -> Ingestion complete: {row_count:,} rows")
    print(f"  -> Ingestion Latency: {ingest_time:.2f} ms ({(row_count / (ingest_time / 1000.0)):,.0f} rows/sec)")
    print(f"---------------------------------------------------------------")
    
    # Pre-warm queries? No, just run them cold to match our C++ engine which runs once
    
    # Q1: Full Table Scan & COUNT(*)
    start_time = time.perf_counter()
    res1 = con.execute("SELECT COUNT(*) FROM sales").fetchall()
    q1_time = (time.perf_counter() - start_time) * 1000.0
    print(f"[DUCKDB] Q1: Full Table Scan & COUNT(*)")
    print(f"  -> Latency: {q1_time:.2f} ms")
    
    # Q2: Filter (Quantity > 50 AND CategoryID == 10)
    start_time = time.perf_counter()
    res2 = con.execute("SELECT * FROM sales WHERE Quantity > 50 AND CategoryID = 10").fetchall()
    q2_time = (time.perf_counter() - start_time) * 1000.0
    print(f"[DUCKDB] Q2: Filter (Quantity > 50 AND CategoryID == 10)")
    print(f"  -> Latency: {q2_time:.2f} ms")
    print(f"  -> Rows Matched: {len(res2)}")
    
    # Q3: Filter (Quantity > 50) + Aggregation
    start_time = time.perf_counter()
    res3 = con.execute("SELECT COUNT(*), SUM(Price), AVG(Price), MIN(Price), MAX(Price) FROM sales WHERE Quantity > 50").fetchall()
    q3_time = (time.perf_counter() - start_time) * 1000.0
    print(f"[DUCKDB] Q3: Filter (Quantity > 50) + Aggregation")
    print(f"  -> Latency: {q3_time:.2f} ms")
    
    # Q4: Hash GROUP BY CategoryID
    start_time = time.perf_counter()
    res4 = con.execute("SELECT CategoryID, COUNT(*), SUM(Quantity), SUM(Price), MIN(Price), MAX(Price) FROM sales GROUP BY CategoryID").fetchall()
    q4_time = (time.perf_counter() - start_time) * 1000.0
    print(f"[DUCKDB] Q4: Hash GROUP BY CategoryID")
    print(f"  -> Latency: {q4_time:.2f} ms")
    print(f"  -> Rows Matched: {len(res4)}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", type=str, default="data/scale/sales_10m.csv")
    args = parser.parse_args()
    run_benchmark(args.data)
