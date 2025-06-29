#!/bin/bash

# PyFolio C++ Deployment Script
# Automated deployment script for various environments

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
VERSION=${VERSION:-$(cat "$PROJECT_ROOT/VERSION" 2>/dev/null || echo "dev")}
ENVIRONMENT=${ENVIRONMENT:-"staging"}
DOCKER_REGISTRY=${DOCKER_REGISTRY:-"ghcr.io/pyfolio-cpp"}
KUBE_NAMESPACE=${KUBE_NAMESPACE:-"pyfolio-$ENVIRONMENT"}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $*"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*"
}

# Help function
show_help() {
    cat << EOF
PyFolio C++ Deployment Script

Usage: $0 [OPTIONS] COMMAND

Commands:
    build           Build Docker image
    push            Push image to registry
    docker          Deploy with Docker Compose
    k8s             Deploy to Kubernetes
    all             Build, push, and deploy
    clean           Clean up resources
    status          Check deployment status
    logs            Show application logs
    shell           Open shell in running container

Options:
    -e, --env ENV       Environment (production|staging|development) [default: production]
    -n, --namespace NS  Kubernetes namespace [default: pyfolio]
    -r, --registry REG  Docker registry [default: pyfolio]
    -t, --tag TAG       Image tag [default: latest]
    -f, --force         Force rebuild and deployment
    -v, --verbose       Verbose output
    -h, --help          Show this help

Examples:
    $0 build
    $0 -e staging k8s
    $0 -t v1.2.0 all
    $0 -n pyfolio-dev -e development docker

EOF
}

# Parse arguments
FORCE=false
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -e|--env)
            ENVIRONMENT="$2"
            shift 2
            ;;
        -n|--namespace)
            NAMESPACE="$2"
            shift 2
            ;;
        -r|--registry)
            DOCKER_REGISTRY="$2"
            shift 2
            ;;
        -t|--tag)
            IMAGE_TAG="$2"
            shift 2
            ;;
        -f|--force)
            FORCE=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            COMMAND="$1"
            shift
            ;;
    esac
done

# Set verbose mode
if [[ "$VERBOSE" == "true" ]]; then
    set -x
fi

# Validate environment
validate_environment() {
    case "$ENVIRONMENT" in
        production|staging|development)
            log_info "Environment: $ENVIRONMENT"
            ;;
        *)
            log_error "Invalid environment: $ENVIRONMENT"
            log_error "Supported environments: production, staging, development"
            exit 1
            ;;
    esac
}

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    # Check Docker
    if ! command -v docker &> /dev/null; then
        log_error "Docker is not installed"
        exit 1
    fi
    
    # Check Docker Compose
    if ! command -v docker-compose &> /dev/null; then
        log_warn "Docker Compose not found, trying docker compose..."
        if ! docker compose version &> /dev/null; then
            log_error "Docker Compose is not available"
            exit 1
        fi
        DOCKER_COMPOSE="docker compose"
    else
        DOCKER_COMPOSE="docker-compose"
    fi
    
    # Check kubectl for Kubernetes deployments
    if [[ "$COMMAND" == "k8s" || "$COMMAND" == "all" ]]; then
        if ! command -v kubectl &> /dev/null; then
            log_error "kubectl is not installed"
            exit 1
        fi
        
        # Check kustomize
        if ! command -v kustomize &> /dev/null; then
            log_warn "kustomize not found, using kubectl apply"
        fi
    fi
    
    log_success "Prerequisites check passed"
}

# Build Docker image
build_image() {
    log_info "Building Docker image..."
    
    cd "$PROJECT_DIR"
    
    local build_args=""
    if [[ "$FORCE" == "true" ]]; then
        build_args="--no-cache"
    fi
    
    # Build multi-stage image
    docker build $build_args \
        -t "${DOCKER_REGISTRY}/${IMAGE_NAME}:${IMAGE_TAG}" \
        -t "${DOCKER_REGISTRY}/${IMAGE_NAME}:latest" \
        --target runtime \
        .
    
    # Build development image if needed
    if [[ "$ENVIRONMENT" == "development" ]]; then
        docker build $build_args \
            -t "${DOCKER_REGISTRY}/${IMAGE_NAME}:${IMAGE_TAG}-dev" \
            -t "${DOCKER_REGISTRY}/${IMAGE_NAME}:latest-dev" \
            --target development \
            .
    fi
    
    log_success "Docker image built successfully"
}

# Push image to registry
push_image() {
    log_info "Pushing Docker image to registry..."
    
    # Push runtime image
    docker push "${DOCKER_REGISTRY}/${IMAGE_NAME}:${IMAGE_TAG}"
    docker push "${DOCKER_REGISTRY}/${IMAGE_NAME}:latest"
    
    # Push development image if exists
    if [[ "$ENVIRONMENT" == "development" ]]; then
        docker push "${DOCKER_REGISTRY}/${IMAGE_NAME}:${IMAGE_TAG}-dev"
        docker push "${DOCKER_REGISTRY}/${IMAGE_NAME}:latest-dev"
    fi
    
    log_success "Docker image pushed successfully"
}

# Deploy with Docker Compose
deploy_docker() {
    log_info "Deploying with Docker Compose..."
    
    cd "$PROJECT_DIR"
    
    # Set environment variables
    export PYFOLIO_IMAGE="${DOCKER_REGISTRY}/${IMAGE_NAME}:${IMAGE_TAG}"
    export PYFOLIO_ENVIRONMENT="$ENVIRONMENT"
    
    # Choose compose file based on environment
    local compose_file="docker-compose.yml"
    local compose_override=""
    
    case "$ENVIRONMENT" in
        production)
            compose_override="-f docker-compose.prod.yml"
            ;;
        staging)
            compose_override="-f docker-compose.staging.yml"
            ;;
        development)
            compose_override="-f docker-compose.dev.yml"
            ;;
    esac
    
    # Deploy services
    if [[ "$FORCE" == "true" ]]; then
        $DOCKER_COMPOSE -f "$compose_file" $compose_override down --remove-orphans
    fi
    
    $DOCKER_COMPOSE -f "$compose_file" $compose_override up -d
    
    # Wait for services to be ready
    log_info "Waiting for services to be ready..."
    sleep 30
    
    # Health check
    if check_docker_health; then
        log_success "Docker deployment completed successfully"
    else
        log_error "Docker deployment health check failed"
        exit 1
    fi
}

# Deploy to Kubernetes
deploy_kubernetes() {
    log_info "Deploying to Kubernetes..."
    
    cd "$PROJECT_DIR/k8s"
    
    # Create namespace if it doesn't exist
    kubectl create namespace "$NAMESPACE" --dry-run=client -o yaml | kubectl apply -f -
    
    # Update image in kustomization
    if command -v kustomize &> /dev/null; then
        # Use kustomize
        kustomize edit set image "pyfolio=${DOCKER_REGISTRY}/${IMAGE_NAME}:${IMAGE_TAG}"
        kustomize build . | kubectl apply -f -
    else
        # Use kubectl with manifests
        log_warn "Using kubectl apply instead of kustomize"
        
        # Apply in order
        kubectl apply -f namespace.yaml
        kubectl apply -f rbac.yaml
        kubectl apply -f configmap.yaml
        kubectl apply -f secrets.yaml
        kubectl apply -f pvc.yaml
        kubectl apply -f deployment.yaml
        kubectl apply -f service.yaml
        kubectl apply -f ingress.yaml
        kubectl apply -f hpa.yaml
        kubectl apply -f monitoring.yaml
    fi
    
    # Wait for deployment to be ready
    log_info "Waiting for deployment to be ready..."
    kubectl wait --for=condition=available --timeout=300s deployment/pyfolio-app -n "$NAMESPACE"
    
    # Check pod status
    kubectl get pods -n "$NAMESPACE" -l app.kubernetes.io/name=pyfolio
    
    log_success "Kubernetes deployment completed successfully"
}

# Check Docker health
check_docker_health() {
    log_info "Checking Docker deployment health..."
    
    # Check if containers are running
    local containers_running=true
    for service in pyfolio-app pyfolio-redis pyfolio-postgres; do
        if ! docker ps | grep -q "$service"; then
            log_error "Container $service is not running"
            containers_running=false
        fi
    done
    
    if [[ "$containers_running" == "false" ]]; then
        return 1
    fi
    
    # Check API health
    local max_attempts=30
    local attempt=0
    
    while [[ $attempt -lt $max_attempts ]]; do
        if curl -s http://localhost:8000/health > /dev/null 2>&1; then
            log_success "API health check passed"
            return 0
        fi
        
        ((attempt++))
        sleep 2
    done
    
    log_error "API health check failed after $max_attempts attempts"
    return 1
}

# Check Kubernetes health
check_kubernetes_health() {
    log_info "Checking Kubernetes deployment health..."
    
    # Check deployment status
    local deployment_ready
    deployment_ready=$(kubectl get deployment pyfolio-app -n "$NAMESPACE" -o jsonpath='{.status.readyReplicas}' 2>/dev/null || echo "0")
    
    if [[ "$deployment_ready" -gt 0 ]]; then
        log_success "Deployment is healthy ($deployment_ready replicas ready)"
        return 0
    else
        log_error "Deployment is not ready"
        return 1
    fi
}

# Clean up resources
cleanup() {
    log_info "Cleaning up resources..."
    
    case "$1" in
        docker)
            cd "$PROJECT_DIR"
            $DOCKER_COMPOSE down --remove-orphans --volumes
            docker system prune -f
            ;;
        k8s)
            kubectl delete namespace "$NAMESPACE" --ignore-not-found=true
            ;;
        all)
            cleanup docker
            cleanup k8s
            ;;
        *)
            log_error "Invalid cleanup target: $1"
            exit 1
            ;;
    esac
    
    log_success "Cleanup completed"
}

# Show deployment status
show_status() {
    log_info "Checking deployment status..."
    
    # Docker status
    log_info "Docker services:"
    docker ps --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}" | grep pyfolio || echo "No Docker services running"
    
    # Kubernetes status
    if kubectl get namespace "$NAMESPACE" &> /dev/null; then
        log_info "Kubernetes resources in namespace $NAMESPACE:"
        kubectl get all -n "$NAMESPACE"
    else
        echo "Namespace $NAMESPACE does not exist"
    fi
}

# Show logs
show_logs() {
    local service="${1:-pyfolio-app}"
    
    log_info "Showing logs for $service..."
    
    # Try Docker first
    if docker ps | grep -q "$service"; then
        docker logs -f "$service"
    # Then try Kubernetes
    elif kubectl get pods -n "$NAMESPACE" -l app.kubernetes.io/component=app &> /dev/null; then
        kubectl logs -f -l app.kubernetes.io/component=app -n "$NAMESPACE"
    else
        log_error "Service $service not found"
        exit 1
    fi
}

# Open shell in container
open_shell() {
    log_info "Opening shell in container..."
    
    # Try Docker first
    if docker ps | grep -q "pyfolio-app"; then
        docker exec -it pyfolio-app /bin/bash
    # Then try Kubernetes
    elif kubectl get pods -n "$NAMESPACE" -l app.kubernetes.io/component=app &> /dev/null; then
        local pod
        pod=$(kubectl get pods -n "$NAMESPACE" -l app.kubernetes.io/component=app -o jsonpath='{.items[0].metadata.name}')
        kubectl exec -it "$pod" -n "$NAMESPACE" -- /bin/bash
    else
        log_error "No running containers found"
        exit 1
    fi
}

# Main execution
main() {
    log_info "PyFolio C++ Deployment Script"
    log_info "Environment: $ENVIRONMENT"
    log_info "Namespace: $NAMESPACE"
    log_info "Image: ${DOCKER_REGISTRY}/${IMAGE_NAME}:${IMAGE_TAG}"
    
    validate_environment
    check_prerequisites
    
    case "${COMMAND:-}" in
        build)
            build_image
            ;;
        push)
            push_image
            ;;
        docker)
            deploy_docker
            ;;
        k8s)
            deploy_kubernetes
            ;;
        all)
            build_image
            push_image
            if [[ "$ENVIRONMENT" == "development" ]]; then
                deploy_docker
            else
                deploy_kubernetes
            fi
            ;;
        clean)
            cleanup all
            ;;
        status)
            show_status
            ;;
        logs)
            show_logs
            ;;
        shell)
            open_shell
            ;;
        *)
            log_error "Unknown command: ${COMMAND:-}"
            show_help
            exit 1
            ;;
    esac
}

# Run main function
main "$@"