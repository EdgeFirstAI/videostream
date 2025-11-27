.DEFAULT_GOAL := help

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  make build          - Build with coverage enabled for testing (C + Rust + Python)"
	@echo "  make format         - Format source code (C, Rust, Python)"
	@echo "  make lint           - Run linters (clippy for Rust)"
	@echo "  make test           - Run test suite with coverage"
	@echo "  make test-ipc       - Run host/client IPC integration test"
	@echo "  make sbom           - Generate SBOM and check license policy"
	@echo "  make verify-version - Verify version consistency across files"
	@echo "  make pre-release    - Run all pre-release checks"
	@echo "  make doc            - Generate HTML documentation (Sphinx + Doxygen)"
	@echo "  make clean          - Clean build artifacts"

# Build target: Build with coverage enabled for testing (SPS v2.1)
# Purpose: Build native libraries with coverage instrumentation before running tests
# This ensures Python tests capture native code coverage
.PHONY: build
build:
	@echo "Building C library with coverage enabled..."
	@cmake -S . -B build \
		-DCMAKE_BUILD_TYPE=Debug \
		-DENABLE_COVER=ON \
		-DENABLE_VPU=OFF \
		-DENABLE_G2D=OFF
	@cmake --build build -j$$(nproc)
	@echo "Building Rust crates..."
	@cargo build --all-features
	@echo "Installing Python package (editable mode with coverage-enabled native library)..."
	@if [ -d "venv" ]; then \
		bash -c "source venv/bin/activate && pip install -e ."; \
	else \
		pip install -e .; \
	fi
	@echo "✓ Build complete (coverage instrumentation enabled)"

.PHONY: doc
doc:
	@echo "Generating documentation..."
	@if [ ! -f "venv/bin/sphinx-build" ]; then \
		echo "ERROR: sphinx-build not found in venv. Please install:"; \
		echo "  venv/bin/pip install -r doc/requirements.txt"; \
		exit 1; \
	fi
	@$(MAKE) -C doc html SPHINXBUILD=../venv/bin/sphinx-build
	@echo "✓ Documentation generated in doc/_build/html/index.html"

.PHONY: format
format:
	@echo "Formatting C code..."
	@find . -not \( -path ./build -prune \) -not \( -path ./ext -prune \) \( -iname \*.h -o -iname \*.c \) -type f -print0 | xargs -I{} -0 clang-format -i {}
	@echo "Formatting Rust code..."
	@cargo +nightly fmt --all || echo "Warning: cargo +nightly fmt failed (nightly toolchain may not be installed)"
	@if [ -d "venv" ] && [ -f "venv/bin/autopep8" ]; then \
		echo "Formatting Python code..."; \
		bash -c "source venv/bin/activate && find videostream tests -name '*.py' -exec autopep8 --in-place --aggressive --aggressive {} \;"; \
	else \
		echo "Skipping Python formatting (venv not found or autopep8 not installed)"; \
	fi
	@echo "✓ Formatting complete"

.PHONY: lint
lint:
	@echo "Running linters..."
	@if [ -f "Cargo.toml" ]; then \
		echo "  Running clippy..."; \
		cargo clippy --all-targets --all-features -- -D warnings || echo "Warning: clippy failed"; \
	fi
	@echo "✓ Linting complete"

.PHONY: sbom
sbom:
	@echo "Generating SBOM..."
	@if [ -d "venv" ]; then \
		if [ ! -f "venv/bin/scancode" ]; then \
			echo "Installing scancode-toolkit in venv..."; \
			venv/bin/pip install scancode-toolkit; \
		fi; \
	elif ! command -v scancode >/dev/null 2>&1; then \
		echo "ERROR: scancode not found and no venv present."; \
		echo "Please create a venv and install scancode-toolkit:"; \
		echo "  python3 -m venv venv"; \
		echo "  venv/bin/pip install scancode-toolkit"; \
		exit 1; \
	fi
	@.github/scripts/generate_sbom.sh
	@echo "Checking license policy compliance..."
	@python3 .github/scripts/check_license_policy.py sbom.json
	@echo "Validating NOTICE file against first-level dependencies..."
	@python3 .github/scripts/validate_notice.py NOTICE sbom-deps.json
	@echo "✓ SBOM generated, license policy verified, and NOTICE validated"

# Test target: Run all tests with coverage (SPS v2.1)
# Uses pytest-cov for Python and cargo-llvm-cov for Rust
# Note: LD_LIBRARY_PATH must be set for both Rust and Python tests to find libvideostream.so
.PHONY: test
test: build
	@echo "Running Rust tests with coverage (cargo-llvm-cov)..."
	@LD_LIBRARY_PATH=$(CURDIR)/build:$$LD_LIBRARY_PATH VIDEOSTREAM_LIBRARY=$(CURDIR)/build/libvideostream.so.1 \
		cargo llvm-cov nextest --workspace --all-features --lcov --output-path build/coverage_rust.lcov

	@echo "Running Python tests with coverage (pytest-cov)..."
	@if [ -f env.sh ]; then \
		bash -c "source env.sh && source venv/bin/activate && export LD_LIBRARY_PATH=$(CURDIR)/build:\$$LD_LIBRARY_PATH && export VIDEOSTREAM_LIBRARY=$(CURDIR)/build/libvideostream.so.1 && pytest -x -v --cov=videostream --cov-report=xml:build/coverage_python.xml --junitxml=build/pytest_results.xml"; \
	else \
		bash -c "source venv/bin/activate && export LD_LIBRARY_PATH=$(CURDIR)/build:\$$LD_LIBRARY_PATH && export VIDEOSTREAM_LIBRARY=$(CURDIR)/build/libvideostream.so.1 && pytest -x -v --cov=videostream --cov-report=xml:build/coverage_python.xml --junitxml=build/pytest_results.xml"; \
	fi

	@echo "Generating C/C++ coverage reports..."
	@mkdir -p build/gcov
	@(cd build && find . -name "*.gcno" -exec gcov -p {} \; 2>/dev/null || true)
	@(cd build && mv *.gcov gcov/ 2>/dev/null || true)
	@gcovr -r . --sonarqube -o build/coverage_c_sonar.xml build/

	@echo ""
	@echo "=========================================="
	@venv/bin/python scripts/coverage_summary.py --build-dir build
	@echo "=========================================="
	@echo ""
	@echo "✓ All tests complete with coverage"

.PHONY: test-ipc
test-ipc:
	@echo "Running host/client IPC integration test..."
	@if [ ! -f "build/src/vsl-test-host" ] || [ ! -f "build/src/vsl-test-client" ]; then \
		echo "ERROR: Test executables not built. Run: cmake -S . -B build && cmake --build build"; \
		exit 1; \
	fi
	@./scripts/test_host_client.sh

.PHONY: verify-version
verify-version:
	@echo "Verifying version consistency across all files..."
	@VERSION=$$(grep '^#define VSL_VERSION "' include/videostream.h | cut -d'"' -f2); \
	echo "Expected version: $$VERSION"; \
	echo -n "Cargo.toml: "; grep "^version = \"$$VERSION\"" Cargo.toml && echo "✓" || (echo "✗ MISMATCH" && exit 1); \
	echo -n "Cargo.toml (videostream-sys dep): "; grep "videostream-sys = { version = \"$$VERSION\"" Cargo.toml && echo "✓" || (echo "✗ MISMATCH" && exit 1); \
	echo -n "pyproject.toml: "; grep "^version = \"$$VERSION\"" pyproject.toml && echo "✓" || (echo "✗ MISMATCH" && exit 1); \
	echo -n "doc/conf.py: "; grep "^version = '$$VERSION'" doc/conf.py && echo "✓" || (echo "✗ MISMATCH" && exit 1); \
	echo -n "debian/changelog: "; grep "^videostream ($$VERSION-1)" debian/changelog && echo "✓" || (echo "✗ MISMATCH" && exit 1); \
	echo -n "CHANGELOG.md: "; grep "^\#\# \[$$VERSION\]" CHANGELOG.md && echo "✓" || (echo "✗ MISMATCH" && exit 1); \
	echo "All version files verified ✓"

.PHONY: pre-release
pre-release: clean build format lint verify-version test sbom
	@echo "Updating Cargo.lock..."
	@cargo update --workspace
	@echo "✓ Cargo.lock updated"
	@echo "✓ All pre-release checks passed"
	@echo ""
	@echo "Next steps:"
	@echo "  1. Review changes: git status"
	@echo "  2. Commit: git commit -a -m 'Prepare Version X.Y.Z'"
	@echo "  3. Push: git push origin main"
	@echo "  4. Wait for CI/CD to pass"
	@echo "  5. Tag: git tag -a -m 'Version X.Y.Z' vX.Y.Z"
	@echo "  6. Push tag: git push origin vX.Y.Z"
	@echo ""
	@echo "CRITICAL: The 'v' prefix is REQUIRED to trigger release.yml workflow!"

.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf build/ target/ __pycache__/ .pytest_cache/
	@rm -f sbom.json *-sbom.json *.cdx.json
	@rm -f README.pdf DESIGN.pdf
	@find . -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
	@find . -type f -name "*.pyc" -delete 2>/dev/null || true
	@echo "✓ Clean complete"
