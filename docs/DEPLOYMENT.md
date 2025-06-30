# PyFolio C++ Deployment Guide

This guide provides comprehensive instructions for deploying PyFolio C++ in production environments using Docker and Kubernetes.

## Table of Contents

1. [Quick Start](#quick-start)
2. [Docker Deployment](#docker-deployment)
3. [Kubernetes Deployment](#kubernetes-deployment)
4. [Configuration](#configuration)
5. [Monitoring](#monitoring)
6. [Security](#security)
7. [Scaling](#scaling)
8. [Troubleshooting](#troubleshooting)

## Quick Start

### Prerequisites

- Docker 20.10+
- Docker Compose 2.0+
- Kubernetes 1.25+ (for K8s deployment)
- kubectl and kustomize
- 4GB+ RAM, 2+ CPU cores
- 50GB+ disk space

### Build and Deploy

```bash
# Clone repository
git clone https://github.com/pyfolio/pyfolio-cpp.git
cd pyfolio-cpp

# Make deployment script executable
chmod +x scripts/deploy.sh

# Build and deploy with Docker Compose (development)
./scripts/deploy.sh -e development docker

# Build and deploy to Kubernetes (production)
./scripts/deploy.sh -e production k8s

# Build, push, and deploy everything
./scripts/deploy.sh all
```

## Docker Deployment

### Single Container

```bash
# Build image
docker build -t pyfolio:latest .

# Run container
docker run -d \
  --name pyfolio-app \
  -p 8000:8000 \
  -v pyfolio-data:/app/data \
  -v pyfolio-logs:/app/logs \
  -e PYFOLIO_MODE=production \
  pyfolio:latest
```

### Docker Compose

The provided `docker-compose.yml` includes:
- PyFolio C++ application
- Redis cache
- PostgreSQL database
- Nginx reverse proxy
- Prometheus monitoring
- Grafana dashboard

```bash
# Start all services
docker-compose up -d

# View logs
docker-compose logs -f pyfolio-app

# Scale application
docker-compose up -d --scale pyfolio-app=3

# Stop services
docker-compose down
```

### Environment-Specific Deployments

```bash
# Development with hot reload
docker-compose -f docker-compose.yml -f docker-compose.dev.yml up -d

# Staging environment
docker-compose -f docker-compose.yml -f docker-compose.staging.yml up -d

# Production with security hardening
docker-compose -f docker-compose.yml -f docker-compose.prod.yml up -d
```

## Kubernetes Deployment

### Architecture

The Kubernetes deployment includes:
- **Application**: Highly available PyFolio C++ pods
- **Storage**: Persistent volumes for data and logs
- **Networking**: Services, ingress, and load balancing
- **Security**: RBAC, network policies, pod security
- **Monitoring**: Prometheus, Grafana, alerting
- **Autoscaling**: HPA and VPA for automatic scaling

### Deployment Steps

1. **Prepare Cluster**
   ```bash
   # Create namespace
   kubectl create namespace pyfolio
   
   # Set context
   kubectl config set-context --current --namespace=pyfolio
   ```

2. **Deploy with Kustomize**
   ```bash
   cd k8s
   
   # Review configuration
   kustomize build .
   
   # Deploy to cluster
   kustomize build . | kubectl apply -f -
   ```

3. **Verify Deployment**
   ```bash
   # Check pod status
   kubectl get pods -l app.kubernetes.io/name=pyfolio
   
   # Check services
   kubectl get services
   
   # Check ingress
   kubectl get ingress
   ```

### Manual Deployment

If kustomize is not available:

```bash
cd k8s

# Apply manifests in order
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
```

## Configuration

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `PYFOLIO_MODE` | Deployment mode | `production` |
| `PYFOLIO_SERVICE` | Service type | `api` |
| `API_HOST` | API bind address | `0.0.0.0` |
| `API_PORT` | API port | `8000` |
| `API_WORKERS` | Worker processes | `4` |
| `POSTGRES_HOST` | Database host | `pyfolio-postgres` |
| `POSTGRES_PORT` | Database port | `5432` |
| `REDIS_HOST` | Cache host | `pyfolio-redis` |
| `REDIS_PORT` | Cache port | `6379` |

### Configuration Files

1. **Application Config** (`/app/config/pyfolio.json`)
   ```json
   {
     "debug": false,
     "log_level": "INFO",
     "max_threads": 8,
     "cache_size": "2GB",
     "features": {
       "enable_gpu": false,
       "enable_mpi": true,
       "enable_streaming": true
     }
   }
   ```

2. **Database Config**
   - Connection pooling
   - SSL/TLS encryption
   - Backup configuration

3. **Security Config**
   - API key authentication
   - Rate limiting
   - CORS policies

### Secrets Management

```bash
# Create secrets manually
kubectl create secret generic pyfolio-secrets \
  --from-literal=POSTGRES_PASSWORD=secure_password \
  --from-literal=REDIS_PASSWORD=redis_password \
  --from-literal=JWT_SECRET=jwt_secret_key

# Or use external secret management
# - HashiCorp Vault
# - AWS Secrets Manager
# - Azure Key Vault
# - GCP Secret Manager
```

## Monitoring

### Health Checks

The application provides multiple health check endpoints:

- `/health` - Basic health status
- `/health/detailed` - Comprehensive system status
- `/metrics` - Prometheus metrics

### Prometheus Metrics

Key metrics collected:
- Request rate and latency
- Error rates
- Resource utilization
- Business metrics (portfolio performance)

### Grafana Dashboards

Pre-configured dashboards include:
- System overview
- API performance
- Resource utilization
- Financial analytics metrics

### Alerting

Alerts are configured for:
- High error rates
- Slow response times
- Resource exhaustion
- Service unavailability

## Security

### Container Security

- Non-root user execution
- Read-only root filesystem
- Minimal base image (Ubuntu 24.04)
- Security scanning with Trivy

### Network Security

- Network policies for traffic control
- TLS/SSL encryption
- Private container networks
- Firewall rules

### Authentication & Authorization

- API key authentication
- JWT tokens for sessions
- RBAC for Kubernetes resources
- Service account tokens

### Secrets

- Encrypted at rest
- Rotation policies
- Minimal privilege access
- External secret management

## Scaling

### Horizontal Pod Autoscaler (HPA)

Automatically scales based on:
- CPU utilization (70%)
- Memory utilization (80%)
- Custom metrics (request rate)

```yaml
minReplicas: 3
maxReplicas: 20
```

### Vertical Pod Autoscaler (VPA)

Automatically adjusts resource requests/limits:
- CPU: 100m - 4 cores
- Memory: 512Mi - 8Gi

### Manual Scaling

```bash
# Scale deployment
kubectl scale deployment pyfolio-app --replicas=10

# Scale with Docker Compose
docker-compose up -d --scale pyfolio-app=5
```

### Performance Optimization

1. **Resource Allocation**
   - CPU: 2+ cores recommended
   - Memory: 4GB+ for large datasets
   - Storage: SSD for databases

2. **Caching Strategy**
   - Redis for API responses
   - Application-level caching
   - CDN for static assets

3. **Database Optimization**
   - Connection pooling
   - Read replicas
   - Query optimization

## Troubleshooting

### Common Issues

1. **Container Won't Start**
   ```bash
   # Check logs
   kubectl logs -l app.kubernetes.io/name=pyfolio
   
   # Check events
   kubectl get events --sort-by=.metadata.creationTimestamp
   ```

2. **Database Connection Issues**
   ```bash
   # Test connectivity
   kubectl exec -it deployment/pyfolio-app -- nc -zv pyfolio-postgres 5432
   
   # Check database logs
   kubectl logs deployment/pyfolio-postgres
   ```

3. **High Memory Usage**
   ```bash
   # Monitor resources
   kubectl top pods
   
   # Adjust limits
   kubectl patch deployment pyfolio-app -p '{"spec":{"template":{"spec":{"containers":[{"name":"pyfolio-app","resources":{"limits":{"memory":"8Gi"}}}]}}}}'
   ```

### Debugging Commands

```bash
# Get shell in container
kubectl exec -it deployment/pyfolio-app -- /bin/bash

# Port forward for local access
kubectl port-forward service/pyfolio-app 8000:8000

# View resource usage
kubectl describe pod <pod-name>

# Check deployment status
kubectl rollout status deployment/pyfolio-app
```

### Performance Debugging

1. **Application Profiling**
   ```bash
   # Enable profiling
   kubectl set env deployment/pyfolio-app PYFOLIO_ENABLE_PROFILING=1
   
   # Access profiling endpoint
   curl http://localhost:8000/debug/pprof/
   ```

2. **Database Performance**
   ```bash
   # Check slow queries
   kubectl exec -it deployment/pyfolio-postgres -- psql -U pyfolio -c "SELECT * FROM pg_stat_activity WHERE state = 'active';"
   ```

3. **Network Issues**
   ```bash
   # Test service discovery
   kubectl exec -it deployment/pyfolio-app -- nslookup pyfolio-postgres
   
   # Check network policies
   kubectl get networkpolicies
   ```

### Log Analysis

```bash
# Structured logging with JSON
kubectl logs deployment/pyfolio-app | jq '.level="ERROR"'

# Follow logs from all pods
kubectl logs -f -l app.kubernetes.io/name=pyfolio --all-containers=true

# Search for specific patterns
kubectl logs deployment/pyfolio-app | grep -i "error\|exception\|fail"
```

## Maintenance

### Updates and Rollbacks

```bash
# Update image
kubectl set image deployment/pyfolio-app pyfolio-app=pyfolio:v2.0.0

# Check rollout status
kubectl rollout status deployment/pyfolio-app

# Rollback if needed
kubectl rollout undo deployment/pyfolio-app
```

### Backup and Restore

1. **Database Backup**
   ```bash
   kubectl exec deployment/pyfolio-postgres -- pg_dump -U pyfolio pyfolio > backup.sql
   ```

2. **Persistent Volume Backup**
   ```bash
   # Use velero or similar backup solution
   velero backup create pyfolio-backup --include-namespaces pyfolio
   ```

### Security Updates

- Regular image updates
- Security patch deployment
- Vulnerability scanning
- Compliance monitoring

For more detailed information, see the individual configuration files and the official Kubernetes documentation.