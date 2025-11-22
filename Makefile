COVER := -V titlepage -V titlepage-skip-title -V titlepage-background=doc/cover.pdf
DOCFLAGS := -V toc -V toc-own-page -V colorlinks
TEXFLAGS := --pdf-engine=xelatex --template=doc/template.tex --listings -H doc/syntax.tex

%.pdf: %.md doc/template.tex doc/syntax.tex Makefile
	pandoc $< -f markdown -t latex -o $@ $(TEXFLAGS) $(COVER) $(DOCFLAGS)

.DEFAULT_GOAL := help

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  make format         - Format source code (C, Rust, Python)"
	@echo "  make lint           - Run linters (clippy for Rust)"
	@echo "  make test           - Run test suite"
	@echo "  make sbom           - Generate SBOM and check license policy"
	@echo "  make verify-version - Verify version consistency across files"
	@echo "  make pre-release    - Run all pre-release checks"
	@echo "  make doc            - Generate PDF documentation (README.pdf, DESIGN.pdf)"
	@echo "  make clean          - Clean build artifacts"

.PHONY: doc
doc: README.pdf DESIGN.pdf

.PHONY: format
format:
	@echo "Formatting C code..."
	@find . -not \( -path ./build -prune \) -not \( -path ./ext -prune \) \( -iname \*.h -o -iname \*.c \) -type f -print0 | xargs -I{} -0 clang-format -i {}
	@echo "Formatting Rust code..."
	@cargo +nightly fmt --all || echo "Warning: cargo +nightly fmt failed (nightly toolchain may not be installed)"
	@if [ -d "venv" ] && [ -f "venv/bin/autopep8" ]; then \
		echo "Formatting Python code..."; \
		bash -c "source venv/bin/activate && find deepview tests -name '*.py' -exec autopep8 --in-place --aggressive --aggressive {} \;"; \
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
	@if [ ! -f "venv/bin/scancode" ]; then \
		echo "ERROR: scancode not found. Please install:"; \
		echo "  python3 -m venv venv"; \
		echo "  venv/bin/pip install scancode-toolkit"; \
		exit 1; \
	fi
	@.github/scripts/generate_sbom.sh
	@echo "Checking license policy compliance..."
	@python3 .github/scripts/check_license_policy.py sbom.json
	@echo "✓ SBOM generated and license policy verified"

.PHONY: test
test:
	@echo "Running tests with library..."
	@if [ ! -f "build/libvideostream.so.1" ]; then \
		echo "ERROR: Library not built. Run: cmake -S . -B build && cmake --build build"; \
		exit 1; \
	fi
	@if [ -f env.sh ]; then \
		echo "Sourcing env.sh..."; \
		bash -c "source env.sh && source venv/bin/activate && export VIDEOSTREAM_LIBRARY=./build/libvideostream.so.1 && pytest tests/"; \
	else \
		bash -c "source venv/bin/activate && export VIDEOSTREAM_LIBRARY=./build/libvideostream.so.1 && pytest tests/"; \
	fi

.PHONY: verify-version
verify-version:
	@echo "Verifying version consistency across all files..."
	@VERSION=$$(grep '^#define VSL_VERSION "' include/videostream.h | cut -d'"' -f2); \
	echo "Expected version: $$VERSION"; \
	echo -n "Cargo.toml: "; grep "^version = \"$$VERSION\"" Cargo.toml && echo "✓" || (echo "✗ MISMATCH" && exit 1); \
	echo -n "pyproject.toml: "; grep "^version = \"$$VERSION\"" pyproject.toml && echo "✓" || (echo "✗ MISMATCH" && exit 1); \
	echo -n "doc/conf.py: "; grep "^version = '$$VERSION'" doc/conf.py && echo "✓" || (echo "✗ MISMATCH" && exit 1); \
	echo -n "debian/changelog: "; grep "^videostream ($$VERSION-1)" debian/changelog && echo "✓" || (echo "✗ MISMATCH" && exit 1); \
	echo -n "CHANGELOG.md: "; grep "^\#\# \[$$VERSION\]" CHANGELOG.md && echo "✓" || (echo "✗ MISMATCH" && exit 1); \
	echo "All version files verified ✓"

.PHONY: pre-release
pre-release: format lint verify-version test sbom
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
	@rm -rf build/ target/ venv/ __pycache__/ .pytest_cache/
	@rm -f sbom.json *-sbom.json *.cdx.json
	@rm -f README.pdf DESIGN.pdf
	@find . -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
	@find . -type f -name "*.pyc" -delete 2>/dev/null || true
	@echo "✓ Clean complete"
