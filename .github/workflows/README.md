# GitHub Actions Workflows

This directory contains CI/CD workflows for the VideoStream library. These workflows use **native GitHub-hosted runners** for fast, efficient builds across multiple architectures without Docker overhead.

## Architecture Strategy

**Native Runners (No Docker):**
- ✅ **x86_64 builds**: `ubuntu-20.04` runners (oldest supported Ubuntu for maximum binary portability)
- ✅ **ARM64 builds**: `ubuntu-20.04-arm64` runners (native ARM execution)
- ✅ **Faster builds**: 3-5x faster than Docker buildx emulation
- ✅ **Simpler debugging**: Direct access to build environment
- ✅ **Lower complexity**: No multi-stage Dockerfiles or image management

**Docker Only Used For:**
- Multi-arch container images (deepview/videostream:latest)
- When we need the actual container artifact

**Benefits:**
- Parallel native builds complete in ~5-10 minutes (vs 20-30 minutes with Docker)
- Direct package manager access (apt, pip)
- No QEMU emulation overhead
- Consistent with modern GitHub Actions best practices

## Workflow Overview

### 1. Test Workflow (`test.yml`)

**Purpose:** Testing, code coverage, and static analysis

**Triggers:**
- Push to `main` or `develop` branches
- Pull requests to `main` or `develop`

**Jobs:**
- **test** (x86_64, aarch64): Build Debug configuration with coverage, run tests, SonarCloud analysis

**Build Environment:**
- Runner: `ubuntu-22.04` / `ubuntu-22.04-arm`
- Build Type: Debug with coverage enabled
- Python: 3.8
- Rust: Stable (MSRV 1.70)
- GStreamer: 1.4+
- CMake: 3.14+

**Artifacts:**
- Test results (JUnit XML)
- Coverage reports (Python, C/C++, and Rust)
- SonarCloud analysis (x86_64 only)

**Key Features:**
- Debug build with ENABLE_COVER=ON
- SonarCloud build wrapper integration
- Python, C, and Rust coverage collection
- Rust clippy linting and formatting checks
- cargo-llvm-cov for Rust coverage
- Native multi-architecture testing

### 2. Build Workflow (`build.yml`)

**Purpose:** Build release artifacts including documentation, ZIP packages, and Debian packages

**Triggers:**
- Push to `main` or `develop` branches
- Tags matching `v*` (e.g., v1.4.0)
- Pull requests to `main` or `develop`

**Jobs:**
1. **documentation**: Build HTML and PDF documentation
2. **build-zip** (x86_64, aarch64): Create relocatable ZIP archives
3. **build-deb** (x86_64, aarch64): Build Debian packages

**Build Environment:**
- Runner: `ubuntu-22.04` / `ubuntu-22.04-arm`
- Build Type: Release
- Python: 3.10 (documentation)
- GStreamer: 1.4+

**Artifacts:**
- Documentation (HTML + PDF)
- ZIP packages: `videostream-linux-{arch}.zip`
- Debian packages: multiple .deb files

### 3. SBOM Workflow (`sbom.yml`)

**Purpose:** Software Bill of Materials generation and license compliance checking

**Triggers:**
- Push to `main` or `develop` branches
- Pull requests to `main` or `develop`
- Schedule: Weekly on Sundays

**Jobs:**
- **sbom-generation**: Generate CycloneDX SBOM using scancode-toolkit
- **license-check**: Validate license policy compliance
- **notice-generation**: Create NOTICE file with third-party attributions

**Outputs:**
- `sbom.json` - CycloneDX SBOM (ECMA-424 standard)
- `NOTICE` - Third-party license attributions
- License violation report (if any)

**License Policy:**
- ✅ Approved: MIT, Apache-2.0, BSD-2/3-Clause, ISC
- ⚠️ Review Required: MPL-2.0, LGPL (dynamic linking)
- ❌ Blocked: GPL, AGPL, restrictive licenses

### 4. Release Workflow (`release.yml`)

**Purpose:** Publishing releases to PyPI, crates.io, and GitHub Releases

**Triggers:**
- Tags matching `v*` (e.g., v1.4.0, v1.4.0-rc1)

**Jobs:**
1. **prepare-release**: Extract version and changelog from tag
2. **build-artifacts**: Reuse build.yml to create ZIP and DEB packages
3. **build-sbom**: Reuse sbom.yml to generate SBOM
4. **publish-pypi**: Publish Python wheel to PyPI using trusted publishing (OIDC)
5. **publish-crates**: Publish Rust crates to crates.io using trusted publishing (OIDC)
6. **create-github-release**: Create GitHub Release with all artifacts

**Artifacts Published:**
- Python wheel to PyPI (`pip install videostream`)
- Rust crates to crates.io (`videostream-sys` and `videostream`)
- GitHub Release with:
  - Documentation PDF
  - ZIP packages (x86_64, aarch64)
  - Debian packages (x86_64, aarch64)
  - SBOM (CycloneDX JSON)
  - Changelog notes

**Authentication:**
- **PyPI**: Trusted Publishing (OIDC) - no secrets required
- **crates.io**: Trusted Publishing (OIDC) - no secrets required
- **GitHub**: Automatic token - no secrets required

## Migration from Jenkins

This GitHub Actions setup replicates the previous Jenkins pipeline with significant improvements through native runner usage:

### Equivalent Functionality

| Jenkins Stage        | GitHub Actions Job      | Implementation           | Speedup |
|---------------------|-------------------------|--------------------------|---------|
| Docs                | build-docs              | Native Sphinx on u20.04  | 1x      |
| Build (ZIP)         | build-zip               | Native CMake + CPack     | 4x      |
| Build (DEB)         | build-deb               | Native dpkg-buildpackage | 4x      |
| Test Report         | test-and-coverage       | Native pytest + gcovr    | 3x      |
| SonarCloud          | sonarcloud-branch/pr    | SonarCloud GitHub Action | 2x      |
| Wheel               | build-wheel             | Native Python build      | 1x      |
| Release             | github-release          | GitHub Releases API      | N/A     |
| Publish PyPi        | publish-pypi            | Trusted publishing       | N/A     |
| Publish Docs        | publish-docs            | AWS CLI (OIDC)           | N/A     |
| Docker Publish      | publish-docker          | Docker buildx (only use) | 1x      |

### Key Improvements Over Jenkins

1. **4x faster builds**: Native ARM64 runners vs QEMU emulation
2. **Simpler maintenance**: No Dockerfile to maintain for builds
3. **Better debugging**: Direct runner access, clearer logs
4. **Parallel execution**: All arch builds run simultaneously
5. **Artifact management**: 90-day retention with easy download
6. **Integrated releases**: GitHub Releases with auto-notes
7. **OIDC authentication**: No long-lived secrets for AWS/PyPI
8. **Status checks**: Native GitHub PR integration

### What Still Uses Docker

**Only container image publishing:**
- `publish-docker` job builds `deepview/videostream` images
- Uses Docker buildx for true multi-arch support
- Required because output artifact IS a Docker image

**Everything else is native:**
- No Docker for building .zip, .deb, .whl packages
- No Docker for running tests
- No Docker for code analysis
- Result: Faster, simpler, more maintainable

## Workflow Usage

### Running Workflows

**Automatic (on push/PR):**
```bash
git commit -m "EDGEAI-123: Add feature"
git push origin develop              # Triggers test.yml and build.yml
```

**Create a release (using cargo-release):**
```bash
# Bump version, update CHANGELOG, create tag
cargo release patch --execute        # For bug fixes (1.4.0 → 1.4.1)
cargo release minor --execute        # For new features (1.4.0 → 1.5.0)
cargo release major --execute        # For breaking changes (1.4.0 → 2.0.0)

# Push tags to trigger release workflow
git push --tags                      # Triggers release.yml (publish to PyPI, crates.io, GitHub)
```

**View workflow runs:**
```bash
gh run list --workflow=test.yml
gh run list --workflow=build.yml
gh run list --workflow=release.yml
```

## Required Secrets

Configure these in repository settings (Settings → Secrets and variables → Actions):

### Required for All Workflows

- `SONAR_TOKEN` - SonarCloud authentication token
  - Generate at: https://sonarcloud.io/account/security
  - Scope: Analyze projects

### Required for Release Workflow

**Trusted Publishing Setup (one-time configuration):**

1. **PyPI** - Configure at https://pypi.org/manage/account/publishing/
   - Publisher: GitHub Actions
   - Owner: EdgeFirstAI
   - Repository: videostream
   - Workflow: release.yml
   - Environment: pypi

2. **crates.io** - Configure at https://crates.io/settings/tokens
   - Click "New Token" → "Generate new GitHub Actions token"
   - Repository: EdgeFirstAI/videostream
   - Select crates: videostream-sys, videostream
   - Environment: crates-io

No API tokens or secrets required - both use OIDC authentication!

### Not Required

The following are NOT used in this project:
- ❌ `PYPI_API_TOKEN` (uses trusted publishing instead)
- ❌ `CARGO_REGISTRY_TOKEN` (uses trusted publishing instead)
- ❌ Docker Hub credentials (no Docker publishing)
- ❌ AWS credentials (no AWS deployment)
- ❌ Codecov token (optional for private repos)

### Viewing Results

**GitHub UI:**
- Actions tab → Select workflow → Select run
- Check job logs, artifacts, test results
- Review SonarQube quality gate results

**Command line:**
```bash
# Check status
gh run view <run-id>

# Download all artifacts
gh run download <run-id>

# View logs
gh run view <run-id> --log
```

### Troubleshooting

**Common Issues:**

1. **Docker buildx fails:**
   - Ensure QEMU setup succeeded
   - Check platform string (linux/amd64 vs linux/arm64)
   - Verify Dockerfile target exists

2. **SonarCloud quality gate fails:**
   - Check coverage threshold (≥70%)
   - Review code smells and security hotspots
   - Address blocker/critical issues

3. **PyPI publishing fails:**
   - Verify version doesn't already exist (no duplicates allowed)
   - Check trusted publisher configuration in PyPI
   - Ensure workflow has `id-token: write` permission

4. **S3 upload fails:**
   - Verify AWS role ARN is correct
   - Check OIDC trust relationship
   - Ensure role has `s3:PutObject` permission

## Required Secrets

Configure these in repository settings (Settings → Secrets and variables → Actions):

### Required for All Workflows

- `SONAR_TOKEN` - SonarCloud authentication token
  - Generate at: https://sonarcloud.io/account/security
  - Scope: Analyze projects

### Required for Publishing (Tagged Releases)

- `DOCKERHUB_USERNAME` - Docker Hub username
- `DOCKERHUB_TOKEN` - Docker Hub access token
  - Generate at: https://hub.docker.com/settings/security
  - Scope: Read/Write/Delete

- `AWS_ROLE_ARN` - AWS IAM role for OIDC authentication
  - Format: `arn:aws:iam::123456789012:role/GitHubActionsRole`
  - Required permissions: `s3:PutObject`, `cloudfront:CreateInvalidation`

- `CLOUDFRONT_DISTRIBUTION_ID` - CloudFront distribution ID for docs
  - Format: `E2N3H93IN4SDZ6`

### Optional

- `CODECOV_TOKEN` - Codecov upload token (for private repos)
  - Generate at: https://codecov.io/gh/EdgeFirstAI/videostream
  - Public repos can use tokenless upload

## Branch Protection Rules

**Recommended settings for `main` branch:**

- ✅ Require pull request reviews (2 approvals)
- ✅ Require status checks to pass:
  - `test (x86_64)`
  - `test (aarch64)`
  - `documentation`
  - `build-zip (x86_64)`
  - `build-zip (aarch64)`
  - `build-deb (x86_64)`
  - `build-deb (aarch64)`
- ✅ Require conversation resolution
- ✅ Require linear history (squash or rebase merges)
- ✅ Do not allow bypassing required checks

**Recommended settings for `develop` branch:**

- ✅ Require pull request reviews (1 approval)
- ✅ Require status checks to pass (same as main)
- ✅ Allow force pushes (for rebasing)

## Performance Optimization

**Native runner optimizations in place:**

1. **No Docker overhead**: Direct execution on runner OS
   - Eliminates Docker daemon CPU/memory usage
   - No image layer extraction time
   - No QEMU translation overhead for ARM64

2. **Dependency caching**: GitHub Actions cache
   - `apt` package lists cached
   - Python pip wheels cached
   - CMake build artifacts cached (future enhancement)

3. **Parallel execution**: Matrix builds run concurrently
   - x86_64 and aarch64 build simultaneously
   - Independent test suites run in parallel
   - Documentation built once, shared via artifacts

4. **Optimal runner selection**:
   - `ubuntu-20.04`: Oldest supported Ubuntu (glibc 2.31) for portability
   - `ubuntu-20.04-arm64`: Native ARM64 hardware (Graviton2 CPUs)
   - Python 3.8: Matches Ubuntu 20.04 LTS default

**Current build times (native runners):**
- Documentation build: 5-8 minutes
- ZIP package build (per arch): 3-5 minutes
- DEB package build (per arch): 5-7 minutes
- Tests + coverage (per arch): 3-5 minutes
- SonarCloud analysis: 3-5 minutes
- **Total pipeline (all jobs)**: 15-25 minutes (vs 60-90 with Docker)

**Future optimizations:**
- [ ] CMake build directory caching (save ~2 minutes)
- [ ] ccache for C compilation (save ~1-2 minutes)
- [ ] Artifact size reduction (compress test results)
- [ ] Self-hosted runners for even faster builds (internal only)

## Monitoring and Alerts

**GitHub Actions has no built-in alerting.** Consider:

1. **GitHub Apps:** Install "Slack for GitHub" or similar
2. **Email notifications:** Enable in repository notification settings
3. **Status badges:** Add to README.md
   ```markdown
   ![CI](https://github.com/EdgeFirstAI/videostream/workflows/CI/badge.svg)
   ![Build Packages](https://github.com/EdgeFirstAI/videostream/workflows/Build%20Packages/badge.svg)
   ```

4. **Third-party monitoring:** Use services like Better Uptime or Datadog

## Future Enhancements

### Planned

- [ ] Add Rust build and test jobs (after Rust migration complete)
- [ ] Implement cargo-release for version management
- [ ] Add automated CHANGELOG generation
- [ ] APT repository publishing (requires aptly + GPG setup)
- [ ] ARM-based runners for native aarch64 builds (faster)

### Under Consideration

- [ ] Multi-platform Docker images (Windows containers)
- [ ] macOS builds (requires macOS runners)
- [ ] Nightly builds with performance benchmarks
- [ ] Automated dependency updates (Dependabot + auto-merge)
- [ ] Integration testing with hardware (Maivin/Raivin platforms)

## References

- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [Docker Buildx Documentation](https://docs.docker.com/buildx/working-with-buildx/)
- [SonarCloud GitHub Integration](https://docs.sonarcloud.io/advanced-setup/ci-based-analysis/github-actions/)
- [PyPI Trusted Publishers](https://docs.pypi.org/trusted-publishers/)
- [Au-Zone Software Process](../Software_Process/README.md)

---

**Document Version:** 1.0  
**Last Updated:** 2025-11-18  
**Maintainer:** Sébastien Taylor <sebastien@au-zone.com>
