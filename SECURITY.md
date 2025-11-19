# Security Policy

## Supported Versions

Au-Zone Technologies provides security updates for the following versions of VideoStream Library:

| Version | Support Status | Notes |
|---------|----------------|-------|
| 1.x     | ✅ Full support | Active development, all security fixes |
| 0.9.x   | 🔒 Security fixes only | Critical vulnerabilities only, until 2026-06-01 |
| < 0.9   | ❌ End of life | No security updates - please upgrade |

**Recommendation**: Always use the latest 1.x release for production deployments.

---

## Reporting a Vulnerability

Au-Zone Technologies takes security seriously across the entire EdgeFirst ecosystem, including VideoStream Library. We appreciate responsible disclosure of security vulnerabilities.

### How to Report

**Primary Contact:**
- **Email**: support@au-zone.com
- **Subject Line**: "Security Vulnerability - VideoStream"

**For EdgeFirst Studio Users:**
- You can also report through the EdgeFirst Studio interface under Settings → Support → Security Report

### What to Include

Please provide as much information as possible to help us understand and address the vulnerability:

1. **Vulnerability Description**
   - Clear description of the security issue
   - Type of vulnerability (e.g., buffer overflow, privilege escalation, information disclosure)

2. **Affected Components**
   - Which part of VideoStream is affected (e.g., client IPC, DmaBuf handling, GStreamer plugin)
   - Affected versions

3. **Steps to Reproduce**
   - Detailed steps to reproduce the vulnerability
   - Proof-of-concept code or example (if available)
   - Environment details (platform, OS version, kernel version)

4. **Impact Assessment**
   - Potential impact (data loss, DoS, arbitrary code execution, etc.)
   - Attack vector (local, remote, requires authentication, etc.)
   - CVSS score (if you've calculated one)

5. **Suggested Fixes** (Optional)
   - If you have ideas on how to fix the issue, please share them
   - Patches are welcome but not required

### What to Expect

**Response Timeline:**

1. **Initial Acknowledgment**: Within 48 hours (2 business days)
   - Confirmation that we received your report
   - Preliminary assessment of severity

2. **Detailed Assessment**: Within 7 calendar days
   - Validation of the vulnerability
   - Severity classification (Critical, High, Medium, Low)
   - Estimated timeline for fix

3. **Fix Timeline** (based on severity):
   - **Critical**: 7 calendar days
     - Remote code execution, privilege escalation, data corruption
   - **High**: 30 calendar days
     - Local privilege escalation, information disclosure, DoS
   - **Medium**: Next minor release (typically 60-90 days)
     - Lesser impact vulnerabilities
   - **Low**: Next major release or as time permits
     - Minimal security impact

4. **Public Disclosure**:
   - Coordinated disclosure after fix is available
   - Security advisory published on GitHub
   - CVE assigned for significant vulnerabilities

---

## Responsible Disclosure

We ask that security researchers and users follow responsible disclosure practices:

### Please DO:
- ✅ Allow reasonable time for us to fix the vulnerability before public disclosure
- ✅ Provide detailed information to help us understand and fix the issue
- ✅ Test against the latest version before reporting
- ✅ Work with us on a coordinated disclosure timeline
- ✅ Report the issue privately to support@au-zone.com

### Please DO NOT:
- ❌ Publicly disclose the vulnerability before a fix is available
- ❌ Exploit the vulnerability beyond what's necessary to demonstrate it
- ❌ Access, modify, or delete data belonging to others
- ❌ Perform actions that could harm Au-Zone or EdgeFirst users
- ❌ Demand payment or compensation for vulnerability reports

---

## Recognition

We value the contributions of security researchers who help keep VideoStream and the EdgeFirst ecosystem secure.

### How We Recognize Contributors:

**With your permission, we will:**
- Credit you in the security advisory
- List you in release notes for the fixed version
- Include you in our annual security report
- Provide a letter of acknowledgment for your portfolio (if requested)

**Optional:**
- You may choose to remain anonymous
- We'll respect your preferred attribution (name, handle, organization)

---

## Security Update Process

### For Users

Security updates are distributed through:

1. **GitHub Security Advisories**
   - Published at: https://github.com/EdgeFirstAI/videostream/security/advisories
   - Subscribe to repository notifications to receive alerts

2. **EdgeFirst Studio Notifications** (for integrated deployments)
   - In-app notifications for VideoStream security updates
   - Automated update prompts for managed devices

3. **Security Mailing List** (Coming Soon)
   - Subscribe at: https://edgefirst.ai/security-updates
   - Receive email notifications of security releases

4. **Release Notes**
   - Security fixes clearly marked in [CHANGELOG.md](CHANGELOG.md)
   - GitHub Releases include security fix details

### Applying Updates

**For package installations:**
```bash
# Check current version
vsl-monitor --version

# Update via package manager (example for Debian/Ubuntu)
sudo apt-get update
sudo apt-get install --only-upgrade videostream

# Verify new version
vsl-monitor --version
```

**For source builds:**
```bash
cd videostream
git fetch origin
git checkout tags/v1.x.y  # Latest secure version
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install
```

**For EdgeFirst Studio deployments:**
- Follow the in-app update prompts
- Or manually deploy updated version via Studio dashboard

---

## Security Best Practices

### For Developers Using VideoStream

1. **Input Validation**
   - Always validate frame data before processing
   - Check frame dimensions and formats against expected values
   - Sanitize metadata from untrusted sources

2. **Resource Limits**
   - Set timeouts on frame waits to prevent hanging
   - Limit number of concurrent client connections
   - Monitor memory usage and set appropriate limits

3. **Permission Management**
   - Use restrictive permissions on IPC socket paths
   - Don't create VideoStream sockets in world-writable directories
   - Consider using abstract sockets (no filesystem path) for added security

4. **Error Handling**
   - Don't leak sensitive information in error messages
   - Log security-relevant events (connection attempts, errors)
   - Gracefully handle malformed IPC messages

5. **DmaBuf Security**
   - Understand that DmaBuf file descriptors grant memory access
   - Close DmaBuf FDs immediately after use
   - Don't share DmaBuf FDs with untrusted processes

**Example: Secure Client Initialization**
```c
#include <videostream.h>
#include <sys/stat.h>

int secure_client_init(const char *pool_path, VStreamClient **client)
{
    // Validate pool path is not in world-writable location
    if (strncmp(pool_path, "/tmp/", 5) == 0) {
        // Warn: /tmp is world-writable, prefer /run/user/<uid>/
        fprintf(stderr, "Warning: Using world-writable /tmp for IPC\n");
    }

    // Check if path exists and has safe permissions
    struct stat st;
    if (stat(pool_path, &st) == 0) {
        if (st.st_mode & S_IWOTH) {
            fprintf(stderr, "Error: Pool path is world-writable\n");
            return -EPERM;
        }
    }

    // Open client with timeout
    return vstream_client_open(pool_path, client);
}
```

### For System Administrators

1. **Least Privilege**
   - Run VideoStream processes with minimal required permissions
   - Use dedicated user accounts for production deployments
   - Don't run as root unless absolutely necessary

2. **Network Isolation**
   - VideoStream uses local UNIX sockets - no network exposure
   - If tunneling over network, use encrypted channels (SSH, VPN)
   - Firewall rules should block external access to VideoStream processes

3. **Monitoring**
   - Monitor system logs for unusual VideoStream activity
   - Track client connection/disconnection patterns
   - Alert on excessive frame drops or errors

4. **Updates**
   - Subscribe to security mailing list
   - Test updates in staging before production
   - Have rollback plan in case of issues

---

## Known Security Considerations

### Architecture-Related

1. **Local-Only IPC**
   - VideoStream is designed for same-machine IPC
   - No built-in authentication or encryption (not needed for UNIX sockets)
   - If you need remote access, use SSH tunneling or VPN

2. **DmaBuf Permissions**
   - DmaBuf file descriptors grant direct memory access
   - Once shared, recipient has full access to buffer contents
   - Trust model: all processes sharing VideoStream pool must be trusted

3. **Frame Data Integrity**
   - VideoStream provides transport, not authentication
   - Applications must validate frame data if from untrusted sources
   - No built-in frame signing or encryption

4. **Denial of Service**
   - Malicious client could lock frames and exhaust pool
   - Host can configure timeouts to prevent indefinite locks
   - Consider resource limits for production deployments

### Platform-Specific

1. **Linux Kernel Dependencies**
   - DmaBuf security relies on kernel enforcement
   - Ensure kernel is up-to-date with security patches
   - Kernel < 4.14 may have DmaBuf vulnerabilities

2. **GStreamer Security**
   - VideoStream depends on GStreamer libraries
   - Keep GStreamer updated (subscribe to GStreamer security list)
   - Vulnerabilities in GStreamer codecs could affect VideoStream pipelines

---

## Additional Security Services

For production deployments requiring enhanced security, Au-Zone Technologies offers:

### Enterprise Security Services

- **Security Audits**
  - Comprehensive code review by security experts
  - Penetration testing of VideoStream deployments
  - Compliance certification (if required for your industry)

- **Priority Security Patches**
  - Fast-track security fixes for enterprise customers
  - Private disclosure before public release
  - Dedicated support for security incidents

- **Custom Security Hardening**
  - Application-specific security enhancements
  - Integration with enterprise security infrastructure (HSM, SIEM, etc.)
  - Secure deployment architecture review

- **Incident Response**
  - 24/7 security incident support
  - Forensic analysis of security breaches
  - Remediation and recovery assistance

**Contact**: support@au-zone.com (Subject: "Enterprise Security Services")

---

## Security Vulnerability History

### Disclosed Vulnerabilities

_(No security vulnerabilities have been publicly disclosed as of 2025-11-14)_

As vulnerabilities are discovered and fixed, they will be listed here with:
- CVE identifier
- Affected versions
- Impact summary
- Fix version
- Credit to reporter

---

## Compliance and Certifications

VideoStream Library is designed to support deployments in security-sensitive environments:

- **Memory Safety**: C11 with defensive coding practices
- **Input Validation**: All external inputs validated
- **Minimal Attack Surface**: No network exposure, local IPC only
- **Audit Trail**: Optional logging of security-relevant events

**Certifications** (available for enterprise customers):
- Common Criteria evaluation (on request)
- FIPS 140-2 compliance assistance
- Industry-specific certifications (automotive, medical, etc.)

Contact support@au-zone.com for compliance and certification inquiries.

---

## Resources

### Security Documentation
- [OWASP Secure Coding Practices](https://owasp.org/www-project-secure-coding-practices-quick-reference-guide/)
- [CWE/SANS Top 25](https://cwe.mitre.org/top25/archive/2023/2023_top25_list.html)
- [Linux Kernel Security](https://www.kernel.org/doc/html/latest/admin-guide/security.html)

### EdgeFirst Security
- [EdgeFirst Studio Security](https://doc.edgefirst.ai/security/)
- [EdgeFirst Modules Security Guide](https://doc.edgefirst.ai/test/platforms/hardware/security/)

### External Security Lists
- [GStreamer Security](https://gstreamer.freedesktop.org/security/)
- [Linux Security Announcements](https://www.kernel.org/category/security.html)
- [NVD - National Vulnerability Database](https://nvd.nist.gov/)

---

## Contact

**For security vulnerabilities**: support@au-zone.com (Subject: "Security Vulnerability - VideoStream")

**For general security questions**: support@au-zone.com

**For enterprise security services**: support@au-zone.com (Subject: "Enterprise Security Services")

---

**We appreciate your help in keeping VideoStream and the EdgeFirst ecosystem secure!**

---

*Last Updated: 2025-11-14*
*Document Version: 1.0*
