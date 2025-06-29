#!/bin/bash

# PyFolio C++ Production Entrypoint Script
# Handles graceful startup, configuration, and service management

set -euo pipefail

# Configuration
PYFOLIO_CONFIG_DIR="${PYFOLIO_CONFIG_DIR:-/app/config}"
PYFOLIO_DATA_DIR="${PYFOLIO_DATA_DIR:-/app/data}"
PYFOLIO_LOG_DIR="${PYFOLIO_LOG_DIR:-/app/logs}"
PYFOLIO_MODE="${PYFOLIO_MODE:-production}"
PYFOLIO_SERVICE="${PYFOLIO_SERVICE:-api}"

# Logging functions
log_info() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] INFO: $*" | tee -a "${PYFOLIO_LOG_DIR}/startup.log"
}

log_error() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] ERROR: $*" | tee -a "${PYFOLIO_LOG_DIR}/startup.log" >&2
}

log_warn() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] WARN: $*" | tee -a "${PYFOLIO_LOG_DIR}/startup.log"
}

# Cleanup function
cleanup() {
    log_info "Shutting down PyFolio C++ services..."
    # Kill any background processes
    jobs -p | xargs -r kill
    exit 0
}

# Set up signal handlers
trap cleanup SIGTERM SIGINT SIGQUIT

# Initialize directories
init_directories() {
    log_info "Initializing directory structure..."
    
    mkdir -p "${PYFOLIO_CONFIG_DIR}" "${PYFOLIO_DATA_DIR}" "${PYFOLIO_LOG_DIR}"
    mkdir -p "${PYFOLIO_DATA_DIR}/cache" "${PYFOLIO_DATA_DIR}/models" "${PYFOLIO_DATA_DIR}/tmp"
    mkdir -p "${PYFOLIO_LOG_DIR}/access" "${PYFOLIO_LOG_DIR}/error" "${PYFOLIO_LOG_DIR}/performance"
    
    # Set permissions
    chmod 755 "${PYFOLIO_CONFIG_DIR}" "${PYFOLIO_DATA_DIR}" "${PYFOLIO_LOG_DIR}"
    chmod 755 "${PYFOLIO_DATA_DIR}/cache" "${PYFOLIO_DATA_DIR}/models" "${PYFOLIO_DATA_DIR}/tmp"
    chmod 755 "${PYFOLIO_LOG_DIR}/access" "${PYFOLIO_LOG_DIR}/error" "${PYFOLIO_LOG_DIR}/performance"
}

# Load configuration
load_config() {
    log_info "Loading configuration..."
    
    local config_file="${PYFOLIO_CONFIG_DIR}/pyfolio.json"
    
    if [[ ! -f "$config_file" ]]; then
        log_warn "Configuration file not found, creating default configuration..."
        cat > "$config_file" << EOF
{
    "debug": false,
    "log_level": "INFO",
    "max_threads": 4,
    "cache_size": "1GB",
    "api": {
        "host": "0.0.0.0",
        "port": 8000,
        "workers": 4,
        "timeout": 30
    },
    "database": {
        "type": "sqlite",
        "path": "${PYFOLIO_DATA_DIR}/pyfolio.db"
    },
    "redis": {
        "host": "localhost",
        "port": 6379,
        "db": 0
    },
    "features": {
        "enable_gpu": false,
        "enable_mpi": false,
        "enable_streaming": true,
        "enable_caching": true
    },
    "security": {
        "api_key_required": false,
        "rate_limit": 1000,
        "cors_origins": ["*"]
    }
}
EOF
    fi
    
    # Export configuration as environment variables
    export PYFOLIO_CONFIG_FILE="$config_file"
}

# Check system requirements
check_requirements() {
    log_info "Checking system requirements..."
    
    # Check memory
    local available_memory=$(free -m | awk 'NR==2{print $7}')
    if [[ $available_memory -lt 512 ]]; then
        log_warn "Low available memory: ${available_memory}MB. Consider increasing container memory."
    fi
    
    # Check disk space
    local available_space=$(df "${PYFOLIO_DATA_DIR}" | awk 'NR==2{print $4}')
    if [[ $available_space -lt 1048576 ]]; then # 1GB in KB
        log_warn "Low disk space: ${available_space}KB available."
    fi
    
    # Check CPU cores
    local cpu_cores=$(nproc)
    log_info "Available CPU cores: ${cpu_cores}"
    
    # Verify binary exists
    if [[ ! -x "/app/bin/basic_example" ]]; then
        log_error "PyFolio C++ binaries not found. Build may have failed."
        return 1
    fi
}

# Start API service
start_api_service() {
    log_info "Starting PyFolio API service..."
    
    local host="${API_HOST:-0.0.0.0}"
    local port="${API_PORT:-8000}"
    local workers="${API_WORKERS:-4}"
    
    # Create a simple FastAPI service
    cat > "${PYFOLIO_DATA_DIR}/tmp/api_server.py" << 'EOF'
#!/usr/bin/env python3

import asyncio
import json
import os
import subprocess
import time
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional

import uvicorn
from fastapi import FastAPI, HTTPException, BackgroundTasks
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from pydantic import BaseModel

app = FastAPI(
    title="PyFolio C++ API",
    description="High-performance financial portfolio analytics",
    version="1.0.0"
)

# CORS middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

class AnalysisRequest(BaseModel):
    returns: List[float]
    positions: Optional[List[Dict]] = None
    start_date: Optional[str] = None
    end_date: Optional[str] = None

class HealthResponse(BaseModel):
    status: str
    timestamp: str
    version: str
    uptime_seconds: float

start_time = time.time()

@app.get("/", response_model=Dict[str, str])
async def root():
    return {
        "service": "PyFolio C++ API",
        "version": "1.0.0",
        "status": "running",
        "documentation": "/docs"
    }

@app.get("/health", response_model=HealthResponse)
async def health_check():
    return HealthResponse(
        status="healthy",
        timestamp=datetime.utcnow().isoformat(),
        version="1.0.0",
        uptime_seconds=time.time() - start_time
    )

@app.post("/analyze/performance")
async def analyze_performance(request: AnalysisRequest):
    """Analyze portfolio performance using PyFolio C++"""
    try:
        # Create temporary data file
        data_file = f"/app/data/tmp/analysis_{int(time.time())}.json"
        with open(data_file, 'w') as f:
            json.dump(request.dict(), f)
        
        # Run analysis (placeholder - would call actual C++ binary)
        result = {
            "sharpe_ratio": 1.23,
            "max_drawdown": -0.15,
            "annual_return": 0.12,
            "volatility": 0.18,
            "calmar_ratio": 0.8,
            "sortino_ratio": 1.45,
            "total_return": sum(request.returns) if request.returns else 0.0,
            "num_observations": len(request.returns) if request.returns else 0
        }
        
        # Cleanup
        if os.path.exists(data_file):
            os.remove(data_file)
        
        return JSONResponse(content=result)
        
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/metrics/system")
async def system_metrics():
    """Get system performance metrics"""
    try:
        # Get memory usage
        with open('/proc/meminfo', 'r') as f:
            meminfo = f.read()
        
        # Parse memory info
        mem_total = int([line for line in meminfo.split('\n') if 'MemTotal' in line][0].split()[1]) * 1024
        mem_available = int([line for line in meminfo.split('\n') if 'MemAvailable' in line][0].split()[1]) * 1024
        
        return {
            "memory": {
                "total_bytes": mem_total,
                "available_bytes": mem_available,
                "used_bytes": mem_total - mem_available,
                "usage_percent": ((mem_total - mem_available) / mem_total) * 100
            },
            "cpu_cores": os.cpu_count(),
            "timestamp": datetime.utcnow().isoformat()
        }
    except Exception as e:
        return {"error": str(e)}

if __name__ == "__main__":
    uvicorn.run(
        app,
        host=os.getenv("API_HOST", "0.0.0.0"),
        port=int(os.getenv("API_PORT", "8000")),
        workers=1,
        log_level="info"
    )
EOF

    # Start the API server
    python3 "${PYFOLIO_DATA_DIR}/tmp/api_server.py" &
    local api_pid=$!
    
    log_info "API service started (PID: ${api_pid}) on http://${host}:${port}"
    echo $api_pid > "${PYFOLIO_DATA_DIR}/tmp/api.pid"
}

# Start example service
start_example_service() {
    log_info "Starting PyFolio example service..."
    
    # Run basic example in background
    /app/bin/basic_example > "${PYFOLIO_LOG_DIR}/basic_example.log" 2>&1 &
    local example_pid=$!
    
    log_info "Example service started (PID: ${example_pid})"
    echo $example_pid > "${PYFOLIO_DATA_DIR}/tmp/example.pid"
}

# Start monitoring
start_monitoring() {
    log_info "Starting monitoring services..."
    
    # Simple resource monitoring script
    cat > "${PYFOLIO_DATA_DIR}/tmp/monitor.py" << 'EOF'
#!/usr/bin/env python3

import time
import json
import psutil
from datetime import datetime

def log_metrics():
    while True:
        try:
            metrics = {
                "timestamp": datetime.utcnow().isoformat(),
                "cpu_percent": psutil.cpu_percent(interval=1),
                "memory": dict(psutil.virtual_memory()._asdict()),
                "disk": dict(psutil.disk_usage('/app')._asdict()),
                "load_avg": psutil.getloadavg() if hasattr(psutil, 'getloadavg') else [0, 0, 0]
            }
            
            with open('/app/logs/performance/metrics.json', 'a') as f:
                f.write(json.dumps(metrics) + '\n')
                
        except Exception as e:
            print(f"Monitoring error: {e}")
        
        time.sleep(30)  # Log every 30 seconds

if __name__ == "__main__":
    log_metrics()
EOF

    python3 "${PYFOLIO_DATA_DIR}/tmp/monitor.py" > "${PYFOLIO_LOG_DIR}/monitor.log" 2>&1 &
    local monitor_pid=$!
    
    log_info "Monitoring started (PID: ${monitor_pid})"
    echo $monitor_pid > "${PYFOLIO_DATA_DIR}/tmp/monitor.pid"
}

# Wait for services
wait_for_services() {
    log_info "Waiting for services to start..."
    
    # Wait for API to be ready
    local max_attempts=30
    local attempt=0
    
    while [[ $attempt -lt $max_attempts ]]; do
        if curl -s "http://localhost:8000/health" > /dev/null 2>&1; then
            log_info "API service is ready"
            break
        fi
        
        ((attempt++))
        sleep 1
    done
    
    if [[ $attempt -eq $max_attempts ]]; then
        log_error "API service failed to start within ${max_attempts} seconds"
        return 1
    fi
}

# Main execution
main() {
    log_info "Starting PyFolio C++ services in ${PYFOLIO_MODE} mode..."
    
    # Initialize
    init_directories
    load_config
    check_requirements
    
    # Start services based on mode
    case "${PYFOLIO_SERVICE}" in
        "api")
            start_api_service
            start_monitoring
            wait_for_services
            ;;
        "example")
            start_example_service
            ;;
        "all")
            start_api_service
            start_example_service
            start_monitoring
            wait_for_services
            ;;
        *)
            log_error "Unknown service mode: ${PYFOLIO_SERVICE}"
            exit 1
            ;;
    esac
    
    log_info "All services started successfully"
    
    # Keep container running
    wait
}

# Execute main function
main "$@"