# CI/CD Pipeline Documentation

## Overview

PyFolio C++ includes comprehensive CI/CD pipelines for automated building, testing, and deployment across multiple platforms and environments. This document describes the complete automation infrastructure.

## Pipeline Components

### 1. GitHub Actions (Primary CI/CD)

Located in `.github/workflows/`, provides multi-platform CI/CD with the following workflows:

#### Main CI Pipeline (`ci.yml`)
- **Triggers**: Push to main/develop, Pull requests
- **Platforms**: Ubuntu, macOS, Windows
- **Compilers**: GCC, Clang, MSVC
- **Features**:
  - Matrix builds across platforms and compilers
  - Unit and integration testing
  - Code coverage reporting
  - Static analysis (cppcheck, clang-tidy)
  - Security scanning (CodeQL)
  - Performance benchmarking
  - Docker image building
  - Documentation generation

#### Release Pipeline (`release.yml`)
- **Triggers**: Tagged releases (v*.*.*)
- **Features**:
  - Multi-platform artifact building
  - Docker image publishing
  - GitHub release creation
  - Documentation deployment
  - Python package publishing
  - Kubernetes deployment manifests

#### Maintenance Pipeline (`maintenance.yml`)
- **Schedule**: Weekly on Sundays
- **Features**:
  - Dependency updates
  - Security vulnerability scanning
  - Performance regression testing
  - Automated code cleanup
  - Cache cleanup

#### Security Analysis (`codeql.yml`)
- **Triggers**: Push, PR, Weekly schedule
- **Features**:
  - Advanced security analysis
  - Vulnerability detection
  - Code quality assessment

### 2. GitLab CI/CD (`.gitlab-ci.yml`)

Comprehensive pipeline for GitLab environments:

#### Stages:
1. **Prepare**: Dependency installation and caching
2. **Build**: Multi-platform builds (Linux, macOS, Windows)
3. **Test**: Unit, integration, performance, memory tests
4. **Analysis**: Coverage, static analysis, security scanning
5. **Package**: Docker images and release packages
6. **Deploy**: Staging and production deployment
7. **Cleanup**: Resource cleanup and maintenance

#### Features:
- Parallel execution across multiple runners
- Advanced caching strategies
- Multi-environment deployment
- Performance regression testing
- Comprehensive reporting

### 3. Jenkins Pipeline (`Jenkinsfile`)

Enterprise-grade pipeline for Jenkins environments:

#### Pipeline Structure:
- **Declarative pipeline** with matrix builds
- **Multi-agent execution** across different environments
- **Parallel stages** for efficiency
- **Quality gates** and approval processes

#### Key Features:
- Blue-green deployment support
- Advanced notification system
- Integration with enterprise tools
- Compliance and audit reporting

### 4. Deployment Scripts

#### Automated Deployment (`scripts/deploy.sh`)
Comprehensive deployment automation supporting:

```bash
# Build and deploy to staging
./scripts/deploy.sh -e staging build test push deploy-k8s

# Production deployment
./scripts/deploy.sh -e production -v v1.2.3 deploy-k8s

# Rollback deployment
./scripts/deploy.sh -e production rollback

# Health check
./scripts/deploy.sh -e production health-check
```

**Supported Commands**:
- `build`: Build Docker images
- `test`: Run tests in containers
- `push`: Push images to registry
- `deploy-k8s`: Deploy to Kubernetes
- `deploy-docker`: Deploy using Docker Compose
- `rollback`: Rollback deployment
- `cleanup`: Clean up old deployments
- `health-check`: Check deployment health

#### Performance Testing (`scripts/performance_test.sh`)
Automated performance regression testing:

```bash
# Run performance tests
./scripts/performance_test.sh run

# Save baseline
./scripts/performance_test.sh baseline

# Compare with baseline
./scripts/performance_test.sh compare

# Generate report
./scripts/performance_test.sh report
```

## Configuration Management

### Environment Variables

#### Required Variables:
```bash
# Docker Registry
DOCKER_REGISTRY=ghcr.io/pyfolio-cpp

# Kubernetes
KUBE_NAMESPACE=pyfolio-production
KUBECONFIG=/path/to/kubeconfig

# Security
CODECOV_TOKEN=your-codecov-token
SONAR_TOKEN=your-sonar-token
```

#### Optional Variables:
```bash
# Build Configuration
CMAKE_BUILD_TYPE=Release
BUILD_PARALLEL_JOBS=4
ENABLE_COVERAGE=ON

# Testing
RUN_PERFORMANCE_TESTS=true
PERFORMANCE_THRESHOLD=5

# Deployment
DEPLOY_ENVIRONMENT=staging
FORCE_DEPLOYMENT=false
```

### Secrets Management

Secrets are managed through each platform's secure storage:

#### GitHub Secrets:
- `DOCKER_PASSWORD`: Docker registry password
- `KUBECONFIG`: Kubernetes configuration
- `CODECOV_TOKEN`: Code coverage token
- `SLACK_WEBHOOK`: Notification webhook

#### GitLab Variables:
- `CI_REGISTRY_PASSWORD`: GitLab registry password
- `KUBE_CONTEXT_STAGING`: Staging Kubernetes context
- `KUBE_CONTEXT_PRODUCTION`: Production Kubernetes context

#### Jenkins Credentials:
- `docker-registry-credentials`: Docker registry credentials
- `kubeconfig-staging`: Staging Kubernetes config
- `kubeconfig-production`: Production Kubernetes config

## Build Matrix

### Platform Support:
- **Linux**: Ubuntu 20.04, 22.04, CentOS 8
- **macOS**: Intel and Apple Silicon
- **Windows**: Windows Server 2019, 2022

### Compiler Support:
- **GCC**: 9, 10, 11, 12
- **Clang**: 10, 11, 12, 13, 14
- **MSVC**: 2019, 2022

### Build Types:
- **Release**: Optimized production builds
- **Debug**: Debug builds with symbols
- **RelWithDebInfo**: Release with debug info
- **MinSizeRel**: Size-optimized builds

## Testing Strategy

### Test Categories:

#### Unit Tests:
- Fast, isolated component tests
- Run on every commit
- Target: >95% code coverage

#### Integration Tests:
- End-to-end workflow testing
- Multi-component interaction
- Database and external service integration

#### Performance Tests:
- Benchmark critical algorithms
- Regression detection
- Scalability validation

#### Memory Tests:
- Memory leak detection (Valgrind)
- Memory usage profiling
- Resource cleanup validation

#### Security Tests:
- Static analysis (CodeQL, SonarQube)
- Dependency vulnerability scanning
- Container security scanning

## Deployment Strategies

### Environments:

#### Development:
- **Trigger**: Feature branch pushes
- **Testing**: Basic unit tests
- **Deployment**: Local/dev environment only

#### Staging:
- **Trigger**: Develop branch merges
- **Testing**: Full test suite
- **Deployment**: Staging Kubernetes cluster
- **Purpose**: Pre-production validation

#### Production:
- **Trigger**: Main branch merges/tags
- **Testing**: Comprehensive testing + manual approval
- **Deployment**: Production Kubernetes cluster
- **Strategy**: Blue-green or rolling updates

### Kubernetes Deployment:

The pipeline supports deployment to Kubernetes using:
- **Kustomize**: Environment-specific overlays
- **Helm**: Package management and templating
- **kubectl**: Direct manifest application

#### Deployment Process:
1. Build and test application
2. Create Docker image
3. Push to container registry
4. Update Kubernetes manifests
5. Apply to cluster
6. Verify deployment health
7. Run smoke tests

### Rollback Strategy:
- Automated rollback on health check failure
- Manual rollback command available
- Database migration rollback procedures
- Quick rollback to previous known-good version

## Monitoring and Alerting

### Build Monitoring:
- Real-time build status notifications
- Performance trend tracking
- Failure rate monitoring
- Resource usage tracking

### Deployment Monitoring:
- Application health checks
- Performance metrics
- Error rate monitoring
- Resource utilization

### Notification Channels:
- **Slack**: Real-time build/deployment status
- **Email**: Detailed failure reports
- **Dashboard**: Visual status overview
- **Webhooks**: Integration with external tools

## Quality Gates

### Mandatory Checks:
- [ ] All tests pass
- [ ] Code coverage ≥ 80%
- [ ] No critical security vulnerabilities
- [ ] Performance regression < 5%
- [ ] Documentation updated
- [ ] Static analysis passes

### Optional Checks:
- [ ] Manual code review approval
- [ ] Performance benchmarks
- [ ] Security scan approval
- [ ] Deployment approval

## Troubleshooting

### Common Issues:

#### Build Failures:
1. Check compiler compatibility
2. Verify dependency versions
3. Review CMake configuration
4. Check for missing headers

#### Test Failures:
1. Review test logs
2. Check for flaky tests
3. Verify test data integrity
4. Review environment setup

#### Deployment Issues:
1. Check Kubernetes cluster status
2. Verify Docker image availability
3. Review resource quotas
4. Check network connectivity

#### Performance Regressions:
1. Compare with baseline metrics
2. Profile critical code paths
3. Review recent changes
4. Check resource allocation

### Debug Commands:

```bash
# View pipeline logs
kubectl logs -f deployment/pyfolio-cpp -n pyfolio-staging

# Check deployment status
kubectl get pods -n pyfolio-staging

# Run debug container
docker run -it pyfolio-cpp:debug /bin/bash

# Performance analysis
./scripts/performance_test.sh profile
```

## Maintenance

### Regular Tasks:
- **Weekly**: Dependency updates
- **Monthly**: Security vulnerability assessment
- **Quarterly**: Performance baseline updates
- **Annually**: Infrastructure review and updates

### Automated Maintenance:
- Dependabot updates for dependencies
- Automated security patching
- Cache cleanup and optimization
- Log rotation and archival

## Best Practices

### Pipeline Design:
1. **Fail Fast**: Run quick tests first
2. **Parallel Execution**: Maximize concurrency
3. **Resource Efficiency**: Optimize build times
4. **Reproducible Builds**: Ensure consistency

### Security:
1. **Secret Management**: Use secure credential storage
2. **Image Scanning**: Scan all container images
3. **Access Control**: Limit deployment permissions
4. **Audit Logging**: Track all changes

### Performance:
1. **Caching**: Cache dependencies and build artifacts
2. **Incremental Builds**: Only rebuild changed components
3. **Resource Allocation**: Right-size compute resources
4. **Monitoring**: Track pipeline performance metrics

This comprehensive CI/CD infrastructure ensures reliable, secure, and efficient development and deployment of PyFolio C++.