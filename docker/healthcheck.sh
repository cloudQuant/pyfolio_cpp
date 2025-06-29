#!/bin/bash

# PyFolio C++ Health Check Script
# Comprehensive health monitoring for container services

set -euo pipefail

# Configuration
HEALTH_CHECK_TIMEOUT="${HEALTH_CHECK_TIMEOUT:-10}"
PYFOLIO_DATA_DIR="${PYFOLIO_DATA_DIR:-/app/data}"
PYFOLIO_LOG_DIR="${PYFOLIO_LOG_DIR:-/app/logs}"
API_HOST="${API_HOST:-localhost}"
API_PORT="${API_PORT:-8000}"

# Exit codes
EXIT_SUCCESS=0
EXIT_FAILURE=1
EXIT_WARNING=2

# Logging
log_health() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] HEALTH: $*"
}

# Check API service health
check_api_health() {
    local endpoint="http://${API_HOST}:${API_PORT}/health"
    
    log_health "Checking API health at ${endpoint}"
    
    # Check if API is responding
    if ! curl -s --max-time "${HEALTH_CHECK_TIMEOUT}" "${endpoint}" > /dev/null 2>&1; then
        log_health "ERROR: API is not responding"
        return 1
    fi
    
    # Check if API returns healthy status
    local response
    response=$(curl -s --max-time "${HEALTH_CHECK_TIMEOUT}" "${endpoint}" || echo '{"status":"error"}')
    
    local status
    status=$(echo "$response" | python3 -c "import sys, json; print(json.load(sys.stdin).get('status', 'unknown'))" 2>/dev/null || echo "unknown")
    
    if [[ "$status" != "healthy" ]]; then
        log_health "ERROR: API status is ${status}"
        return 1
    fi
    
    log_health "API health check passed"
    return 0
}

# Check process health
check_processes() {
    log_health "Checking process health"
    
    local processes_ok=true
    
    # Check if API process is running
    if [[ -f "${PYFOLIO_DATA_DIR}/tmp/api.pid" ]]; then
        local api_pid
        api_pid=$(cat "${PYFOLIO_DATA_DIR}/tmp/api.pid")
        
        if ! kill -0 "$api_pid" 2>/dev/null; then
            log_health "ERROR: API process (PID: ${api_pid}) is not running"
            processes_ok=false
        else
            log_health "API process (PID: ${api_pid}) is running"
        fi
    fi
    
    # Check if monitoring process is running
    if [[ -f "${PYFOLIO_DATA_DIR}/tmp/monitor.pid" ]]; then
        local monitor_pid
        monitor_pid=$(cat "${PYFOLIO_DATA_DIR}/tmp/monitor.pid")
        
        if ! kill -0 "$monitor_pid" 2>/dev/null; then
            log_health "WARNING: Monitor process (PID: ${monitor_pid}) is not running"
            # Monitoring is not critical, so don't fail
        else
            log_health "Monitor process (PID: ${monitor_pid}) is running"
        fi
    fi
    
    if [[ "$processes_ok" == "true" ]]; then
        log_health "Process health check passed"
        return 0
    else
        log_health "Process health check failed"
        return 1
    fi
}

# Check system resources
check_resources() {
    log_health "Checking system resources"
    
    local warnings=0
    
    # Check memory usage
    if command -v free >/dev/null 2>&1; then
        local memory_usage
        memory_usage=$(free | awk 'NR==2{printf "%.1f", $3*100/($3+$7)}')
        
        if (( $(echo "$memory_usage > 90" | bc -l) )); then
            log_health "WARNING: High memory usage: ${memory_usage}%"
            ((warnings++))
        else
            log_health "Memory usage: ${memory_usage}%"
        fi
    fi
    
    # Check disk usage
    local disk_usage
    disk_usage=$(df "${PYFOLIO_DATA_DIR}" | awk 'NR==2{print $5}' | sed 's/%//')
    
    if [[ $disk_usage -gt 90 ]]; then
        log_health "WARNING: High disk usage: ${disk_usage}%"
        ((warnings++))
    else
        log_health "Disk usage: ${disk_usage}%"
    fi
    
    # Check CPU load (if available)
    if [[ -f /proc/loadavg ]]; then
        local load_avg
        load_avg=$(cut -d' ' -f1 /proc/loadavg)
        local cpu_cores
        cpu_cores=$(nproc)
        
        # Load average should typically be less than number of CPU cores
        if (( $(echo "$load_avg > $cpu_cores" | bc -l) )); then
            log_health "WARNING: High CPU load: ${load_avg} (cores: ${cpu_cores})"
            ((warnings++))
        else
            log_health "CPU load: ${load_avg} (cores: ${cpu_cores})"
        fi
    fi
    
    # Check log directory writable
    if [[ ! -w "${PYFOLIO_LOG_DIR}" ]]; then
        log_health "ERROR: Log directory not writable: ${PYFOLIO_LOG_DIR}"
        return 1
    fi
    
    # Check data directory writable
    if [[ ! -w "${PYFOLIO_DATA_DIR}" ]]; then
        log_health "ERROR: Data directory not writable: ${PYFOLIO_DATA_DIR}"
        return 1
    fi
    
    if [[ $warnings -gt 0 ]]; then
        log_health "Resource check completed with ${warnings} warnings"
        return 2  # Warning exit code
    else
        log_health "Resource health check passed"
        return 0
    fi
}

# Check file system health
check_filesystem() {
    log_health "Checking file system health"
    
    # Check required directories exist
    local required_dirs=(
        "${PYFOLIO_DATA_DIR}"
        "${PYFOLIO_LOG_DIR}"
        "${PYFOLIO_DATA_DIR}/tmp"
        "${PYFOLIO_DATA_DIR}/cache"
        "${PYFOLIO_LOG_DIR}/performance"
    )
    
    for dir in "${required_dirs[@]}"; do
        if [[ ! -d "$dir" ]]; then
            log_health "ERROR: Required directory missing: ${dir}"
            return 1
        fi
    done
    
    # Check log files are being written
    local log_files=(
        "${PYFOLIO_LOG_DIR}/startup.log"
        "${PYFOLIO_LOG_DIR}/performance/metrics.json"
    )
    
    for log_file in "${log_files[@]}"; do
        if [[ -f "$log_file" ]]; then
            # Check if file was modified in last 5 minutes
            local file_age
            file_age=$(( $(date +%s) - $(stat -c %Y "$log_file") ))
            
            if [[ $file_age -gt 300 ]]; then  # 5 minutes
                log_health "WARNING: Log file not recently updated: ${log_file} (${file_age}s ago)"
            fi
        fi
    done
    
    log_health "File system health check passed"
    return 0
}

# Comprehensive health check
run_health_check() {
    log_health "Starting comprehensive health check"
    
    local exit_code=0
    local warning_count=0
    
    # Run all health checks
    if ! check_filesystem; then
        log_health "File system check failed"
        exit_code=1
    fi
    
    if ! check_processes; then
        log_health "Process check failed"
        exit_code=1
    fi
    
    if ! check_api_health; then
        log_health "API check failed"
        exit_code=1
    fi
    
    local resource_result
    check_resources
    resource_result=$?
    
    if [[ $resource_result -eq 1 ]]; then
        log_health "Resource check failed"
        exit_code=1
    elif [[ $resource_result -eq 2 ]]; then
        ((warning_count++))
    fi
    
    # Final status
    if [[ $exit_code -eq 0 ]]; then
        if [[ $warning_count -gt 0 ]]; then
            log_health "Health check PASSED with ${warning_count} warnings"
            exit_code=2
        else
            log_health "Health check PASSED - all systems healthy"
            exit_code=0
        fi
    else
        log_health "Health check FAILED - system unhealthy"
        exit_code=1
    fi
    
    return $exit_code
}

# Detailed health report
generate_health_report() {
    local report_file="${PYFOLIO_LOG_DIR}/health_report.json"
    
    log_health "Generating detailed health report: ${report_file}"
    
    # Generate JSON health report
    cat > "$report_file" << EOF
{
    "timestamp": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
    "status": "unknown",
    "checks": {
        "api_health": {},
        "processes": {},
        "resources": {},
        "filesystem": {}
    },
    "system_info": {
        "hostname": "$(hostname)",
        "uptime": "$(uptime -p 2>/dev/null || echo 'unknown')",
        "kernel": "$(uname -r)",
        "architecture": "$(uname -m)"
    }
}
EOF
    
    log_health "Health report generated"
}

# Main function
main() {
    local command="${1:-check}"
    
    case "$command" in
        "check")
            run_health_check
            ;;
        "report")
            generate_health_report
            run_health_check
            ;;
        "api")
            check_api_health
            ;;
        "processes")
            check_processes
            ;;
        "resources")
            check_resources
            ;;
        "filesystem")
            check_filesystem
            ;;
        *)
            echo "Usage: $0 {check|report|api|processes|resources|filesystem}"
            exit 1
            ;;
    esac
}

# Make script executable and run
main "$@"