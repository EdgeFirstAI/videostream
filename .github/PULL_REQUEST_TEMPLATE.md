## Description

Brief description of changes made in this pull request.

## Type of Change

- [ ] Bug fix (non-breaking change which fixes an issue)
- [ ] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Documentation update
- [ ] Performance improvement
- [ ] Code refactoring
- [ ] Build/CI improvement

## Related Issues

Fixes #(issue number)
Relates to #(issue number)

## Motivation and Context

Why is this change required? What problem does it solve?

## How Has This Been Tested?

Describe the tests you ran to verify your changes:

- [ ] Test configuration 1 (platform, component, etc.)
- [ ] Test configuration 2
- [ ] Test configuration 3

**Test Platform:**
- Platform: (e.g., NXP i.MX8M Plus)
- Kernel: (output of `uname -r`)
- GStreamer: (if applicable)

## Code Quality Checklist

- [ ] Code follows project style guidelines (run `make format`)
- [ ] Self-reviewed code changes
- [ ] Added comments for complex logic
- [ ] No new compiler warnings
- [ ] Static analysis passes (run `make lint`)

## Testing Checklist

- [ ] Tests pass locally (`make test`)
- [ ] Code coverage maintained/improved (70% minimum, 80%+ for core modules)
- [ ] Added tests for new features
- [ ] Fixed broken tests
- [ ] Tested on target platform (embedded Linux)

## Documentation Checklist

- [ ] Updated API documentation (inline comments)
- [ ] Updated README.md (if user-facing changes)
- [ ] Updated ARCHITECTURE.md (if design changes)
- [ ] Updated CHANGELOG.md under `[Unreleased]`
- [ ] Added/updated code examples (if applicable)

## Version Files Checklist

If this PR changes the version (releases only):
- [ ] Updated `include/videostream.h` - `#define VSL_VERSION "X.Y.Z"`
- [ ] Updated `Cargo.toml` - `version = "X.Y.Z"`
- [ ] Updated `pyproject.toml` - `version = "X.Y.Z"`
- [ ] Updated `doc/conf.py` - `version = 'X.Y.Z'` (single quotes!)
- [ ] Updated `debian/changelog` - `videostream (X.Y.Z-1) stable`
- [ ] Updated `CHANGELOG.md` - Moved `[Unreleased]` to `[X.Y.Z]` with date
- [ ] Verified with `make verify-version`

## License and Legal

- [ ] All commits are signed off (DCO): `git commit -s`
- [ ] No new dependencies added, or all dependencies are license-compliant (Apache-2.0, MIT, BSD, LGPL dynamic only)
- [ ] Updated NOTICE file if adding third-party code
- [ ] SBOM generation passes (`make sbom`)

## Pre-Release Checklist (Releases Only)

- [ ] Ran `make pre-release` successfully
- [ ] All CI/CD checks pass
- [ ] Documentation builds successfully (`make doc`)
- [ ] Version consistent across all files

## Additional Notes

Any additional information for reviewers (implementation details, design decisions, performance impact, etc.):

---

## Developer Certificate of Origin (DCO)

By submitting this pull request, you certify that you have the right to submit it under the Apache-2.0 license and agree to the [Developer Certificate of Origin](https://developercertificate.org/).

**All commits must be signed off with `git commit -s` to indicate your agreement.**

To sign off commits retroactively:
```bash
git rebase --signoff HEAD~N  # where N is the number of commits
git push --force-with-lease
```
