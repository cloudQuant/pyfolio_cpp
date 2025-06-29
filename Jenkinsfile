// Jenkins Pipeline for PyFolio C++
// Comprehensive CI/CD pipeline with multi-platform builds and deployment

pipeline {
    agent none
    
    options {
        buildDiscarder(logRotator(numToKeepStr: '10'))
        timeout(time: 2, unit: 'HOURS')
        timestamps()
        parallelsAlwaysFailFast()
    }
    
    parameters {
        choice(
            name: 'BUILD_TYPE',
            choices: ['Release', 'Debug', 'RelWithDebInfo'],
            description: 'CMake build type'
        )
        booleanParam(
            name: 'RUN_PERFORMANCE_TESTS',
            defaultValue: false,
            description: 'Run performance benchmarks'
        )
        booleanParam(
            name: 'DEPLOY_TO_STAGING',
            defaultValue: false,
            description: 'Deploy to staging environment'
        )
        string(
            name: 'DOCKER_TAG',
            defaultValue: 'latest',
            description: 'Docker image tag'
        )
    }
    
    environment {
        DOCKER_REGISTRY = 'your-registry.com'
        DOCKER_IMAGE = 'pyfolio-cpp'
        KUBECONFIG = credentials('kubeconfig')
        SONAR_TOKEN = credentials('sonar-token')
        CODECOV_TOKEN = credentials('codecov-token')
    }
    
    stages {
        stage('Checkout') {
            agent any
            steps {
                checkout scm
                script {
                    env.GIT_COMMIT_SHORT = sh(
                        script: "git rev-parse --short HEAD",
                        returnStdout: true
                    ).trim()
                    env.BUILD_NUMBER_FORMATTED = sprintf("%04d", BUILD_NUMBER.toInteger())
                }
            }
        }
        
        stage('Build Matrix') {
            parallel {
                stage('Linux GCC') {
                    agent {
                        docker {
                            image 'ubuntu:22.04'
                            args '--user root'
                        }
                    }
                    steps {
                        sh '''
                            apt-get update -qq
                            apt-get install -y build-essential cmake ninja-build pkg-config
                            apt-get install -y libeigen3-dev libopenmpi-dev libhdf5-dev
                            apt-get install -y python3 python3-pip git curl lcov
                        '''
                        sh '''
                            mkdir -p build
                            cd build
                            cmake -GNinja \
                                -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
                                -DBUILD_TESTS=ON \
                                -DBUILD_EXAMPLES=ON \
                                -DBUILD_BENCHMARKS=ON \
                                -DENABLE_COVERAGE=ON \
                                ..
                            ninja -j$(nproc)
                        '''
                    }
                    post {
                        always {
                            archiveArtifacts artifacts: 'build/**', fingerprint: true
                        }
                    }
                }
                
                stage('Linux Clang') {
                    agent {
                        docker {
                            image 'ubuntu:22.04'
                            args '--user root'
                        }
                    }
                    environment {
                        CC = 'clang'
                        CXX = 'clang++'
                    }
                    steps {
                        sh '''
                            apt-get update -qq
                            apt-get install -y clang cmake ninja-build pkg-config
                            apt-get install -y libeigen3-dev libopenmpi-dev libhdf5-dev
                        '''
                        sh '''
                            mkdir -p build
                            cd build
                            cmake -GNinja \
                                -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
                                -DBUILD_TESTS=ON \
                                ..
                            ninja -j$(nproc)
                        '''
                    }
                }
                
                stage('macOS') {
                    agent {
                        label 'macos'
                    }
                    steps {
                        sh '''
                            brew install cmake ninja eigen openmpi hdf5
                            mkdir -p build
                            cd build
                            cmake -GNinja \
                                -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
                                -DBUILD_TESTS=ON \
                                ..
                            ninja -j$(sysctl -n hw.ncpu)
                        '''
                    }
                }
                
                stage('Windows') {
                    agent {
                        label 'windows'
                    }
                    steps {
                        bat '''
                            mkdir build
                            cd build
                            cmake -G"Visual Studio 16 2019" ^
                                -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
                                -DBUILD_TESTS=ON ^
                                ..
                            cmake --build . --config %BUILD_TYPE% --parallel
                        '''
                    }
                }
            }
        }
        
        stage('Testing') {
            parallel {
                stage('Unit Tests') {
                    agent {
                        docker {
                            image 'ubuntu:22.04'
                            args '--user root'
                        }
                    }
                    steps {
                        unstash 'linux-build'
                        sh '''
                            cd build
                            ctest --output-on-failure --parallel $(nproc) -L "unit"
                        '''
                    }
                    post {
                        always {
                            publishTestResults testResultsPattern: 'build/test_results.xml'
                        }
                    }
                }
                
                stage('Integration Tests') {
                    agent {
                        docker {
                            image 'ubuntu:22.04'
                            args '--user root'
                        }
                    }
                    steps {
                        unstash 'linux-build'
                        sh '''
                            cd build
                            ctest --output-on-failure --parallel $(nproc) -L "integration"
                        '''
                    }
                }
                
                stage('Memory Tests') {
                    agent {
                        docker {
                            image 'ubuntu:22.04'
                            args '--user root'
                        }
                    }
                    steps {
                        sh '''
                            apt-get update -qq
                            apt-get install -y valgrind
                        '''
                        unstash 'linux-build'
                        sh '''
                            cd build
                            valgrind --leak-check=full --error-exitcode=1 \
                                ./tests/memory_tests
                        '''
                    }
                }
                
                stage('Performance Tests') {
                    when {
                        anyOf {
                            params.RUN_PERFORMANCE_TESTS
                            branch 'main'
                            branch 'develop'
                        }
                    }
                    agent {
                        docker {
                            image 'ubuntu:22.04'
                            args '--user root'
                        }
                    }
                    steps {
                        unstash 'linux-build'
                        sh '''
                            cd build
                            ./tests/performance_benchmarks \
                                --benchmark_format=json \
                                --benchmark_out=performance_results.json
                        '''
                        script {
                            if (fileExists('build/performance_baseline.json')) {
                                sh '''
                                    python3 scripts/compare_performance.py \
                                        build/performance_baseline.json \
                                        build/performance_results.json
                                '''
                            } else {
                                sh '''
                                    cp build/performance_results.json \
                                       build/performance_baseline.json
                                '''
                            }
                        }
                    }
                    post {
                        always {
                            archiveArtifacts artifacts: 'build/performance_*.json'
                        }
                    }
                }
            }
        }
        
        stage('Code Quality') {
            parallel {
                stage('Coverage') {
                    agent {
                        docker {
                            image 'ubuntu:22.04'
                            args '--user root'
                        }
                    }
                    steps {
                        sh '''
                            apt-get update -qq
                            apt-get install -y lcov
                        '''
                        unstash 'linux-build'
                        sh '''
                            cd build
                            make coverage
                            lcov --capture --directory . --output-file coverage.info
                            lcov --remove coverage.info '/usr/*' --output-file coverage.info
                            genhtml coverage.info --output-directory coverage_html
                        '''
                        sh '''
                            curl -Os https://uploader.codecov.io/latest/linux/codecov
                            chmod +x codecov
                            ./codecov -f build/coverage.info
                        '''
                    }
                    post {
                        always {
                            publishHTML([
                                allowMissing: false,
                                alwaysLinkToLastBuild: true,
                                keepAll: true,
                                reportDir: 'build/coverage_html',
                                reportFiles: 'index.html',
                                reportName: 'Coverage Report'
                            ])
                        }
                    }
                }
                
                stage('Static Analysis') {
                    agent {
                        docker {
                            image 'ubuntu:22.04'
                            args '--user root'
                        }
                    }
                    steps {
                        sh '''
                            apt-get update -qq
                            apt-get install -y cppcheck clang-tidy
                        '''
                        sh '''
                            cppcheck --xml --xml-version=2 \
                                --enable=all --suppress=missingIncludeSystem \
                                src/ 2> cppcheck_report.xml
                        '''
                        sh '''
                            find src/ -name "*.cpp" -exec clang-tidy {} -- -Iinclude \\; \
                                > clang_tidy_report.txt 2>&1 || true
                        '''
                    }
                    post {
                        always {
                            recordIssues(
                                enabledForFailure: true,
                                tools: [
                                    cppCheck(pattern: 'cppcheck_report.xml'),
                                    clangTidy(pattern: 'clang_tidy_report.txt')
                                ]
                            )
                        }
                    }
                }
                
                stage('SonarQube') {
                    agent {
                        docker {
                            image 'sonarsource/sonar-scanner-cli:latest'
                        }
                    }
                    steps {
                        withSonarQubeEnv('SonarQube') {
                            sh '''
                                sonar-scanner \
                                    -Dsonar.projectKey=pyfolio-cpp \
                                    -Dsonar.sources=src \
                                    -Dsonar.cfamily.build-wrapper-output=build-wrapper-output \
                                    -Dsonar.cfamily.gcov.reportsPath=build
                            '''
                        }
                    }
                }
            }
        }
        
        stage('Security Scanning') {
            parallel {
                stage('Dependency Check') {
                    agent any
                    steps {
                        dependencyCheck additionalArguments: '--scan src/ --format XML --format HTML', odcInstallation: 'Default'
                        dependencyCheckPublisher pattern: 'dependency-check-report.xml'
                    }
                }
                
                stage('Container Scan') {
                    agent any
                    steps {
                        script {
                            docker.image("${DOCKER_REGISTRY}/${DOCKER_IMAGE}:${DOCKER_TAG}").inside {
                                sh '''
                                    # Run container security scanning
                                    trivy image --format json --output trivy_report.json \
                                        ${DOCKER_REGISTRY}/${DOCKER_IMAGE}:${DOCKER_TAG}
                                '''
                            }
                        }
                    }
                }
            }
        }
        
        stage('Package') {
            parallel {
                stage('Docker Image') {
                    agent any
                    steps {
                        script {
                            def image = docker.build("${DOCKER_REGISTRY}/${DOCKER_IMAGE}:${DOCKER_TAG}")
                            docker.withRegistry("https://${DOCKER_REGISTRY}", 'docker-registry-credentials') {
                                image.push()
                                image.push('latest')
                            }
                        }
                    }
                }
                
                stage('Release Packages') {
                    when {
                        anyOf {
                            branch 'main'
                            tag pattern: 'v\\d+\\.\\d+\\.\\d+', comparator: 'REGEXP'
                        }
                    }
                    agent {
                        docker {
                            image 'ubuntu:22.04'
                            args '--user root'
                        }
                    }
                    steps {
                        unstash 'linux-build'
                        sh '''
                            cd build
                            make package
                            mkdir -p ../artifacts
                            cp *.tar.gz *.deb *.rpm ../artifacts/ 2>/dev/null || true
                        '''
                    }
                    post {
                        always {
                            archiveArtifacts artifacts: 'artifacts/*', fingerprint: true
                        }
                    }
                }
            }
        }
        
        stage('Deploy') {
            when {
                anyOf {
                    params.DEPLOY_TO_STAGING
                    branch 'main'
                }
            }
            parallel {
                stage('Staging') {
                    when {
                        anyOf {
                            params.DEPLOY_TO_STAGING
                            branch 'develop'
                        }
                    }
                    agent any
                    steps {
                        script {
                            kubernetesDeploy(
                                configs: 'k8s/**/*.yaml',
                                kubeconfigId: 'kubeconfig-staging',
                                secretNamespace: 'pyfolio-staging'
                            )
                        }
                    }
                }
                
                stage('Production') {
                    when {
                        branch 'main'
                    }
                    agent any
                    steps {
                        input message: 'Deploy to production?', ok: 'Deploy'
                        script {
                            kubernetesDeploy(
                                configs: 'k8s/**/*.yaml',
                                kubeconfigId: 'kubeconfig-production',
                                secretNamespace: 'pyfolio-production'
                            )
                        }
                    }
                }
            }
        }
        
        stage('Documentation') {
            when {
                branch 'main'
            }
            agent {
                docker {
                    image 'ubuntu:22.04'
                    args '--user root'
                }
            }
            steps {
                sh '''
                    apt-get update -qq
                    apt-get install -y doxygen graphviz
                '''
                unstash 'linux-build'
                sh '''
                    cd build
                    make docs
                '''
                publishHTML([
                    allowMissing: false,
                    alwaysLinkToLastBuild: true,
                    keepAll: true,
                    reportDir: 'build/docs/html',
                    reportFiles: 'index.html',
                    reportName: 'API Documentation'
                ])
            }
        }
    }
    
    post {
        always {
            node('master') {
                echo "Pipeline completed for ${env.BRANCH_NAME}"
                
                // Clean up workspace
                cleanWs()
            }
        }
        
        success {
            node('master') {
                // Send success notification
                emailext(
                    subject: "✅ PyFolio C++ Build Success - ${env.BRANCH_NAME} #${env.BUILD_NUMBER}",
                    body: """
                        Build successful for branch ${env.BRANCH_NAME}
                        
                        Build Number: ${env.BUILD_NUMBER}
                        Commit: ${env.GIT_COMMIT_SHORT}
                        Duration: ${currentBuild.durationString}
                        
                        View build: ${env.BUILD_URL}
                    """,
                    to: "${env.CHANGE_AUTHOR_EMAIL}",
                    recipientProviders: [developers(), requestor()]
                )
                
                // Slack notification
                slackSend(
                    channel: '#pyfolio-builds',
                    color: 'good',
                    message: "✅ PyFolio C++ build succeeded for ${env.BRANCH_NAME} (${env.BUILD_NUMBER})"
                )
            }
        }
        
        failure {
            node('master') {
                // Send failure notification
                emailext(
                    subject: "❌ PyFolio C++ Build Failed - ${env.BRANCH_NAME} #${env.BUILD_NUMBER}",
                    body: """
                        Build failed for branch ${env.BRANCH_NAME}
                        
                        Build Number: ${env.BUILD_NUMBER}
                        Commit: ${env.GIT_COMMIT_SHORT}
                        Duration: ${currentBuild.durationString}
                        
                        View build: ${env.BUILD_URL}
                        Console: ${env.BUILD_URL}console
                    """,
                    to: "${env.CHANGE_AUTHOR_EMAIL}",
                    recipientProviders: [developers(), requestor(), culprits()]
                )
                
                // Slack notification
                slackSend(
                    channel: '#pyfolio-builds',
                    color: 'danger',
                    message: "❌ PyFolio C++ build failed for ${env.BRANCH_NAME} (${env.BUILD_NUMBER}) - ${env.BUILD_URL}"
                )
            }
        }
        
        unstable {
            node('master') {
                slackSend(
                    channel: '#pyfolio-builds',
                    color: 'warning',
                    message: "⚠️ PyFolio C++ build unstable for ${env.BRANCH_NAME} (${env.BUILD_NUMBER})"
                )
            }
        }
    }
}