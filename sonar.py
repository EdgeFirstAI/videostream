#!/usr/bin/env python3
"""
SonarCloud Issues Fetcher - Optimized for GitHub Copilot Integration

This script fetches current SonarCloud analysis results in a format optimized
for GitHub Copilot to understand and help fix identified issues.

Features:
- Fetches fresh analysis results with staleness detection
- Provides rich metadata (file paths, line numbers, descriptions, remediation)
- Filters issues by status, severity, type
- Formats output for easy Copilot interpretation
- Supports both branch and pull request analysis
"""

import argparse
import json
import os
import sys
from datetime import datetime, timedelta, timezone
from typing import Any, Dict, List, Optional
from urllib.parse import urlencode

import requests


class SonarCloudClient:
    """Client for interacting with SonarCloud API."""

    def __init__(self, host_url: str, token: str, organization: str):
        self.host_url = host_url.rstrip("/")
        self.organization = organization
        self.headers = {"Authorization": f"Bearer {token}"}

    def _get_paginated(
        self, endpoint: str, params: Dict[str, Any], data_key: str
    ) -> List[Dict[str, Any]]:
        """Fetch all pages of results from a paginated endpoint."""
        all_items = []
        page = 1
        page_size = 500

        while True:
            paginated_params = {**params, "p": page, "ps": page_size}
            url = f"{self.host_url}{endpoint}?{urlencode(paginated_params)}"

            response = requests.get(url, headers=self.headers, timeout=30)
            response.raise_for_status()
            data = response.json()

            items = data.get(data_key, [])
            all_items.extend(items)

            # Check if we've retrieved all items
            total = data.get("total", 0)
            if len(all_items) >= total or len(items) == 0:
                break

            page += 1

        return all_items

    def get_project_status(self, project_key: str) -> Dict[str, Any]:
        """Get the overall project quality gate status."""
        url = f"{self.host_url}/api/qualitygates/project_status"
        params = {"projectKey": project_key}

        response = requests.get(url, headers=self.headers, params=params, timeout=30)
        response.raise_for_status()
        return response.json()

    def get_project_measures(
        self, project_key: str, branch: Optional[str] = None
    ) -> Dict[str, Any]:
        """Get project metrics/measures."""
        url = f"{self.host_url}/api/measures/component"
        params = {
            "component": project_key,
            "metricKeys": (
                "bugs,vulnerabilities,code_smells,security_hotspots,"
                "coverage,duplicated_lines_density"
            ),
        }
        if branch:
            params["branch"] = branch

        response = requests.get(url, headers=self.headers, params=params, timeout=30)
        response.raise_for_status()
        return response.json()

    def get_analysis_date(
        self, project_key: str, branch: Optional[str] = None
    ) -> Optional[datetime]:
        """Get the date of the last analysis."""
        url = f"{self.host_url}/api/project_analyses/search"
        params = {"project": project_key, "ps": 1}
        if branch:
            params["branch"] = branch

        try:
            response = requests.get(
                url, headers=self.headers, params=params, timeout=30
            )
            response.raise_for_status()
            data = response.json()

            analyses = data.get("analyses", [])
            if analyses:
                # Parse ISO 8601 datetime
                date_str = analyses[0].get("date")
                if date_str:
                    return datetime.fromisoformat(date_str.replace("Z", "+00:00"))
        except Exception as e:
            print(f"Warning: Could not fetch analysis date: {e}", file=sys.stderr)

        return None

    def get_issues(
        self,
        project_key: str,
        branch: Optional[str] = None,
        pull_request: Optional[str] = None,
        resolved: bool = False,
        severities: Optional[List[str]] = None,
        types: Optional[List[str]] = None,
    ) -> List[Dict[str, Any]]:
        """Fetch issues from SonarCloud."""
        params: Dict[str, Any] = {
            "componentKeys": project_key,
            "organization": self.organization,
            "additionalFields": "_all",
            "resolved": str(resolved).lower(),
        }

        if pull_request:
            params["pullRequest"] = pull_request
        elif branch:
            params["branch"] = branch

        if severities:
            params["severities"] = ",".join(severities)

        if types:
            params["types"] = ",".join(types)

        return self._get_paginated("/api/issues/search", params, "issues")

    def get_hotspots(
        self,
        project_key: str,
        branch: Optional[str] = None,
        pull_request: Optional[str] = None,
        status: Optional[str] = None,
    ) -> List[Dict[str, Any]]:
        """Fetch security hotspots from SonarCloud."""
        params: Dict[str, Any] = {
            "projectKey": project_key,
        }

        if pull_request:
            params["pullRequest"] = pull_request
        elif branch:
            params["branch"] = branch

        if status:
            params["status"] = status

        return self._get_paginated("/api/hotspots/search", params, "hotspots")

    def get_rules(self, rule_keys: List[str]) -> List[Dict[str, Any]]:
        """Fetch rule details for given rule keys."""
        if not rule_keys:
            return []

        rules = []
        for key in rule_keys:
            try:
                response = requests.get(
                    f"{self.host_url}/api/rules/show",
                    params={"key": key, "organization": self.organization},
                    headers=self.headers,
                    timeout=30,
                )
                response.raise_for_status()
                data = response.json()
                if "rule" in data:
                    rules.append(data["rule"])
            except requests.exceptions.HTTPError as e:
                # Skip rules that don't exist or can't be fetched
                print(
                    f"⚠️  Warning: Could not fetch rule {key}: {e}",
                    file=sys.stderr,
                )
                continue

        return rules


class CopilotFormatter:
    """Format SonarCloud data for optimal Copilot consumption."""

    @staticmethod
    def format_issue(
        issue: Dict[str, Any],
        rule_map: Dict[str, Dict[str, Any]],
        component_map: Dict[str, Dict[str, Any]],
    ) -> Dict[str, Any]:
        """Format a single issue for Copilot."""
        component_key = issue.get("component", "")
        component = component_map.get(component_key, {})
        file_path = component.get("path", component_key)

        # Remove project key prefix from path if present
        if ":" in file_path:
            file_path = file_path.split(":", 1)[1]

        rule_key = issue.get("rule", "")
        rule = rule_map.get(rule_key, {})

        # Extract text range for precise location
        text_range = issue.get("textRange", {})
        start_line = text_range.get("startLine", issue.get("line"))
        end_line = text_range.get("endLine", start_line)
        start_offset = text_range.get("startOffset", 0)
        end_offset = text_range.get("endOffset", 0)

        return {
            "file": file_path,
            "line": start_line,
            "endLine": end_line,
            "column": start_offset,
            "endColumn": end_offset,
            "severity": issue.get("severity", "UNKNOWN"),
            "type": issue.get("type", "UNKNOWN"),
            "rule": rule_key,
            "ruleName": rule.get("name", rule_key),
            "message": issue.get("message", ""),
            "status": issue.get("status", "OPEN"),
            "effort": issue.get("effort", ""),
            "debt": issue.get("debt", ""),
            "tags": issue.get("tags", []),
            "creationDate": issue.get("creationDate", ""),
            "updateDate": issue.get("updateDate", ""),
            # Additional context for Copilot
            "context": {
                "ruleDescription": rule.get("htmlDesc", ""),
                "language": rule.get("lang", ""),
                "issueKey": issue.get("key", ""),
            },
        }

    @staticmethod
    def format_hotspot(
        hotspot: Dict[str, Any],
        rule_map: Dict[str, Dict[str, Any]],
        component_map: Dict[str, Dict[str, Any]],
    ) -> Dict[str, Any]:
        """Format a security hotspot for Copilot."""
        component_key = hotspot.get("component", "")
        component = component_map.get(component_key, {})
        file_path = component.get("path", component_key)

        if ":" in file_path:
            file_path = file_path.split(":", 1)[1]

        rule_key = hotspot.get("ruleKey", "")
        rule = rule_map.get(rule_key, {})

        text_range = hotspot.get("textRange", {})
        start_line = text_range.get("startLine", hotspot.get("line"))
        end_line = text_range.get("endLine", start_line)

        return {
            "file": file_path,
            "line": start_line,
            "endLine": end_line,
            "severity": "SECURITY_HOTSPOT",
            "type": "SECURITY_HOTSPOT",
            "rule": rule_key,
            "ruleName": rule.get("name", rule_key),
            "message": hotspot.get("message", ""),
            "status": hotspot.get("status", "TO_REVIEW"),
            "vulnerabilityProbability": hotspot.get("vulnerabilityProbability", ""),
            "securityCategory": hotspot.get("securityCategory", ""),
            "creationDate": hotspot.get("creationDate", ""),
            "updateDate": hotspot.get("updateDate", ""),
            "context": {
                "ruleDescription": rule.get("htmlDesc", ""),
                "language": rule.get("lang", ""),
                "hotspotKey": hotspot.get("key", ""),
            },
        }

    @staticmethod
    def create_summary(
        issues: List[Dict[str, Any]],
        hotspots: List[Dict[str, Any]],
        analysis_date: Optional[datetime],
        project_status: Optional[Dict[str, Any]],
    ) -> Dict[str, Any]:
        """Create a summary section for the report."""
        # Count by severity
        severity_counts = {}
        for issue in issues:
            severity = issue.get("severity", "UNKNOWN")
            severity_counts[severity] = severity_counts.get(severity, 0) + 1

        # Count by type
        type_counts = {}
        for issue in issues:
            issue_type = issue.get("type", "UNKNOWN")
            type_counts[issue_type] = type_counts.get(issue_type, 0) + 1

        summary = {
            "totalIssues": len(issues),
            "totalHotspots": len(hotspots),
            "bySeverity": severity_counts,
            "byType": type_counts,
            "analysisDate": (analysis_date.isoformat() if analysis_date else None),
            "isStale": False,
        }

        # Check if analysis is stale (older than 24 hours)
        if analysis_date:
            age = datetime.now(timezone.utc) - analysis_date
            summary["isStale"] = age > timedelta(hours=24)
            summary["ageHours"] = age.total_seconds() / 3600

        # Add quality gate status if available
        if project_status:
            qg_status = project_status.get("projectStatus", {})
            summary["qualityGateStatus"] = qg_status.get("status", "UNKNOWN")

        return summary


def create_argument_parser() -> argparse.ArgumentParser:
    """Create and configure the argument parser."""
    parser = argparse.ArgumentParser(
        description="Fetch SonarCloud issues optimized for GitHub Copilot",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Environment Variables:
  SONAR_HOST_URL      SonarCloud host URL (default: https://sonarcloud.io)
  SONAR_TOKEN         SonarCloud API token (required)
  SONAR_ORG           SonarCloud organization (required)
  SONAR_PROJECT       SonarCloud project key (required)

Examples:
  # Fetch open issues for main branch
  %(prog)s --branch main --output sonar-issues.json

  # Fetch all issues including resolved ones
  %(prog)s --branch main --include-resolved

  # Fetch only critical bugs and vulnerabilities
  %(prog)s --branch main --severity CRITICAL --type BUG,VULNERABILITY

  # Fetch issues for a pull request
  %(prog)s --pull-request 123 --output pr-issues.json
        """,
    )

    parser.add_argument(
        "--host-url",
        default=os.environ.get("SONAR_HOST_URL", "https://sonarcloud.io"),
        help="SonarCloud host URL",
    )
    parser.add_argument(
        "--token",
        default=os.environ.get("SONAR_TOKEN"),
        help="SonarCloud API token",
        required=not os.environ.get("SONAR_TOKEN"),
    )
    parser.add_argument(
        "--organization",
        default=os.environ.get("SONAR_ORG"),
        help="SonarCloud organization",
        required=not os.environ.get("SONAR_ORG"),
    )
    parser.add_argument(
        "--project",
        default=os.environ.get("SONAR_PROJECT"),
        help="SonarCloud project key",
        required=not os.environ.get("SONAR_PROJECT"),
    )
    parser.add_argument("--branch", help="Branch name to analyze")
    parser.add_argument("--pull-request", help="Pull request ID to analyze")
    parser.add_argument(
        "--output",
        "-o",
        default=os.environ.get("REPORT_PATH", "sonar-issues.json"),
        help="Output file path",
    )
    parser.add_argument(
        "--include-resolved",
        action="store_true",
        help="Include resolved issues in the output",
    )
    parser.add_argument(
        "--severity",
        help=(
            "Filter by severity (comma-separated): BLOCKER,CRITICAL,MAJOR,MINOR,INFO"
        ),
    )
    parser.add_argument(
        "--type",
        help=("Filter by type (comma-separated): BUG,VULNERABILITY,CODE_SMELL"),
    )
    parser.add_argument(
        "--hotspot-status",
        choices=["TO_REVIEW", "REVIEWED", "SAFE", "FIXED"],
        help="Filter hotspots by status",
    )
    parser.add_argument(
        "--max-age-hours",
        type=int,
        default=24,
        help="Maximum age of analysis in hours before warning (default: 24)",
    )
    parser.add_argument(
        "--verbose", "-v", action="store_true", help="Enable verbose output"
    )
    parser.add_argument(
        "--format",
        choices=["copilot", "sarif", "json"],
        default="copilot",
        help="Output format (default: copilot)",
    )

    return parser


def print_connection_info(args: argparse.Namespace) -> None:
    """Print connection information if verbose mode is enabled."""
    if not args.verbose:
        return

    print(f"Connecting to: {args.host_url}", file=sys.stderr)
    print(f"Organization: {args.organization}", file=sys.stderr)
    print(f"Project: {args.project}", file=sys.stderr)
    if args.branch:
        print(f"Branch: {args.branch}", file=sys.stderr)
    if args.pull_request:
        print(f"Pull Request: {args.pull_request}", file=sys.stderr)


def check_analysis_freshness(
    client: SonarCloudClient,
    project_key: str,
    branch: Optional[str],
    max_age_hours: int,
    verbose: bool,
) -> Optional[datetime]:
    """Check and report on analysis freshness."""
    analysis_date = client.get_analysis_date(project_key, branch)

    if not analysis_date:
        if verbose:
            print("Could not determine analysis date", file=sys.stderr)
        return None

    age = datetime.now(timezone.utc) - analysis_date
    age_hours = age.total_seconds() / 3600

    if verbose:
        print(
            f"Last analysis: {analysis_date.isoformat()} ({age_hours:.1f} hours ago)",
            file=sys.stderr,
        )

    if age_hours > max_age_hours:
        print(
            f"⚠️  WARNING: Analysis is {age_hours:.1f} hours old "
            f"(threshold: {max_age_hours}h)",
            file=sys.stderr,
        )
        print(
            "    Results may be stale. Consider triggering a new analysis.",
            file=sys.stderr,
        )

    return analysis_date


def fetch_project_status(
    client: SonarCloudClient, project_key: str, verbose: bool
) -> Optional[Dict[str, Any]]:
    """Fetch and optionally print project quality gate status."""
    try:
        project_status = client.get_project_status(project_key)
        if verbose and project_status:
            qg = project_status.get("projectStatus", {})
            print(
                f"Quality Gate: {qg.get('status', 'UNKNOWN')}",
                file=sys.stderr,
            )
        return project_status
    except Exception as e:
        if verbose:
            print(
                f"Could not fetch quality gate status: {e}",
                file=sys.stderr,
            )
        return None


def fetch_issues_and_hotspots(
    client: SonarCloudClient, args: argparse.Namespace
) -> tuple[List[Dict[str, Any]], List[Dict[str, Any]]]:
    """Fetch issues and hotspots from SonarCloud."""
    # Parse filters
    severities = (
        [s.strip() for s in args.severity.split(",")] if args.severity else None
    )
    types = [t.strip() for t in args.type.split(",")] if args.type else None

    # Fetch issues
    if args.verbose:
        print("Fetching issues...", file=sys.stderr)

    issues = client.get_issues(
        args.project,
        branch=args.branch,
        pull_request=args.pull_request,
        resolved=args.include_resolved,
        severities=severities,
        types=types,
    )

    if args.verbose:
        print(f"Found {len(issues)} issues", file=sys.stderr)

    # Fetch hotspots
    if args.verbose:
        print("Fetching security hotspots...", file=sys.stderr)

    hotspots = client.get_hotspots(
        args.project,
        branch=args.branch,
        pull_request=args.pull_request,
        status=args.hotspot_status,
    )

    if args.verbose:
        print(f"Found {len(hotspots)} hotspots", file=sys.stderr)

    return issues, hotspots


def fetch_rule_definitions(
    client: SonarCloudClient,
    issues: List[Dict[str, Any]],
    hotspots: List[Dict[str, Any]],
    verbose: bool,
) -> Dict[str, Dict[str, Any]]:
    """Fetch rule definitions for all issues and hotspots."""
    issue_rule_keys: List[str] = [
        str(issue.get("rule")) for issue in issues if issue.get("rule") is not None
    ]
    hotspot_rule_keys: List[str] = [
        str(hotspot.get("ruleKey"))
        for hotspot in hotspots
        if hotspot.get("ruleKey") is not None
    ]
    all_rule_keys: List[str] = list(set(issue_rule_keys + hotspot_rule_keys))

    if verbose:
        print(
            f"Fetching {len(all_rule_keys)} rule definitions...",
            file=sys.stderr,
        )

    rules = client.get_rules(all_rule_keys)
    return {rule["key"]: rule for rule in rules}


def build_component_map(issues: List[Dict[str, Any]]) -> Dict[str, Dict[str, Any]]:
    """Build a map of components from issues."""
    component_map = {}
    for issue in issues:
        component_key = issue.get("component", "")
        if component_key and component_key not in component_map:
            component_map[component_key] = {
                "key": component_key,
                "path": component_key,
            }
    return component_map


def format_output(
    args: argparse.Namespace,
    issues: List[Dict[str, Any]],
    hotspots: List[Dict[str, Any]],
    rules: Dict[str, Dict[str, Any]],
    component_map: Dict[str, Dict[str, Any]],
    analysis_date: Optional[datetime],
    project_status: Optional[Dict[str, Any]],
) -> tuple[Dict[str, Any], Optional[Dict[str, Any]]]:
    """Format the output based on the selected format."""
    summary = None

    if args.format == "copilot":
        formatter = CopilotFormatter()

        formatted_issues = [
            formatter.format_issue(issue, rules, component_map) for issue in issues
        ]
        formatted_hotspots = [
            formatter.format_hotspot(hotspot, rules, component_map)
            for hotspot in hotspots
        ]

        summary = formatter.create_summary(
            formatted_issues,
            formatted_hotspots,
            analysis_date,
            project_status,
        )

        output = {
            "version": "1.0",
            "generatedAt": datetime.now(timezone.utc).isoformat(),
            "project": {
                "key": args.project,
                "organization": args.organization,
                "branch": args.branch,
                "pullRequest": args.pull_request,
            },
            "summary": summary,
            "issues": formatted_issues,
            "hotspots": formatted_hotspots,
        }

    elif args.format == "json":
        output = {
            "issues": issues,
            "hotspots": hotspots,
            "rules": list(rules.values()),
        }

    else:
        print("SARIF format not yet implemented", file=sys.stderr)
        sys.exit(1)

    return output, summary


def print_summary(summary: Dict[str, Any]) -> None:
    """Print a summary of the results to stderr."""
    print("\n📊 Summary:", file=sys.stderr)
    print(f"  Total Issues: {summary['totalIssues']}", file=sys.stderr)
    print(
        f"  Security Hotspots: {summary['totalHotspots']}",
        file=sys.stderr,
    )
    if summary["bySeverity"]:
        print("  By Severity:", file=sys.stderr)
        for sev, count in sorted(summary["bySeverity"].items()):
            print(f"    {sev}: {count}", file=sys.stderr)


def main():
    """Main entry point for the script."""
    parser = create_argument_parser()
    args = parser.parse_args()

    # Validate that either branch or pull-request is specified
    if not args.branch and not args.pull_request:
        parser.error("Either --branch or --pull-request must be specified")

    try:
        # Initialize client
        client = SonarCloudClient(args.host_url, args.token, args.organization)

        print_connection_info(args)

        # Check analysis freshness
        analysis_date = check_analysis_freshness(
            client, args.project, args.branch, args.max_age_hours, args.verbose
        )

        # Fetch project status
        project_status = fetch_project_status(client, args.project, args.verbose)

        # Fetch issues and hotspots
        issues, hotspots = fetch_issues_and_hotspots(client, args)

        # Fetch rule definitions
        rule_map = fetch_rule_definitions(client, issues, hotspots, args.verbose)

        # Build component map
        component_map = build_component_map(issues)

        # Format output
        output, summary = format_output(
            args,
            issues,
            hotspots,
            rule_map,
            component_map,
            analysis_date,
            project_status,
        )

        # Write output
        with open(args.output, "w") as f:
            json.dump(output, f, indent=2)

        if args.verbose:
            print(f"✅ Report written to: {args.output}", file=sys.stderr)

        # Print summary
        if args.format == "copilot" and summary is not None:
            print_summary(summary)

    except requests.exceptions.RequestException as e:
        print(f"❌ API Error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"❌ Error: {e}", file=sys.stderr)
        if args.verbose:
            import traceback

            traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
