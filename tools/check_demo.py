#!/usr/bin/env python3
"""Check a deployed (or locally served) copy of the interactive demo.

Two checks run in headless Chrome through the DevTools protocol:

  1. index.html   - the page must finish a full simulation run and publish its
                    diagnostics (runSeq, request count, per-scheduler dispatch
                    counts).
  2. verify.html  - the JavaScript port must reproduce every reference scenario
                    produced by the C++ engine, within the tolerances documented
                    in verify.html.

Requirements: Google Chrome, and `pip install websocket-client`.

Usage:
    python3 tools/check_demo.py                                  # local: http://127.0.0.1:8000/docs/
    python3 tools/check_demo.py https://example.github.io/repo/   # deployed site
    python3 tools/check_demo.py http://127.0.0.1:8000/docs/ 300   # custom timeout (s)
"""

import json
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.parse
import urllib.request

import websocket

CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
PORT = 9334


def wait_for_target(timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with urllib.request.urlopen("http://127.0.0.1:%d/json/list" % PORT, timeout=2) as resp:
                targets = json.load(resp)
            for target in targets:
                if target.get("type") == "page" and target.get("webSocketDebuggerUrl"):
                    return target["webSocketDebuggerUrl"]
        except Exception:
            pass
        time.sleep(0.3)
    raise RuntimeError("no DevTools page target; is Chrome installed?")


class Session(object):
    """Minimal CDP session: navigate, poll a readiness expression, evaluate JS."""

    def __init__(self, timeout):
        self.timeout = timeout
        self.profile = tempfile.mkdtemp(prefix="mfts-check-")
        self.proc = subprocess.Popen(
            [
                CHROME,
                "--headless=new",
                "--disable-gpu",
                "--no-sandbox",
                "--no-first-run",
                "--no-default-browser-check",
                "--disable-extensions",
                "--user-data-dir=" + self.profile,
                "--remote-debugging-port=%d" % PORT,
                "--remote-allow-origins=*",
                "about:blank",
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        self.ws = websocket.create_connection(wait_for_target(30), timeout=60)
        self.msg_id = 0

    def call(self, method, params=None):
        self.msg_id += 1
        self.ws.send(json.dumps({"id": self.msg_id, "method": method, "params": params or {}}))
        while True:
            data = json.loads(self.ws.recv())
            if data.get("id") == self.msg_id:
                return data

    def evaluate(self, expression):
        response = self.call("Runtime.evaluate", {
            "expression": expression,
            "returnByValue": True,
            "awaitPromise": True,
        })
        result = response.get("result", {})
        if "exceptionDetails" in result:
            return "JS_ERROR: %s" % str(result["exceptionDetails"])[:200]
        return result.get("result", {}).get("value")

    def open(self, url, ready_expr):
        self.call("Page.enable")
        self.call("Runtime.enable")
        self.call("Page.navigate", {"url": url})
        deadline = time.time() + self.timeout
        while time.time() < deadline:
            try:
                value = self.evaluate(ready_expr)
                if value is True:
                    return True
            except Exception:
                pass
            time.sleep(0.5)
        return False

    def close(self):
        try:
            self.ws.close()
        except Exception:
            pass
        self.proc.terminate()
        try:
            self.proc.wait(timeout=10)
        except Exception:
            self.proc.kill()
        shutil.rmtree(self.profile, ignore_errors=True)


def check_demo_page(session, base):
    url = urllib.parse.urljoin(base, "index.html")
    ready = (
        "!!document.getElementById('diag') && (function(){"
        "try{return JSON.parse(document.getElementById('diag').textContent).runSeq >= 1}"
        "catch(e){return false}})()"
    )
    print("== index.html ==")
    if not session.open(url, ready):
        print("FAIL: %s did not finish a run" % url)
        return False
    diag = session.evaluate("document.getElementById('diag').textContent")
    try:
        data = json.loads(diag)
    except Exception:
        print("FAIL: diagnostics are not valid JSON: %s" % str(diag)[:200])
        return False
    ok = data.get("status") == "ready" and data.get("requests", 0) > 0 and len(data.get("schedulers", [])) == 5
    print("   url            %s" % url)
    print("   workload       %s (%s)" % (data.get("workload"), data.get("source")))
    print("   requests       %s" % data.get("requests"))
    print("   schedulers     %s" % ", ".join(data.get("schedulers", [])))
    print("   dispatches     %s" % data.get("dispatches"))
    print("   detail rows    %s" % data.get("detailRows"))
    print("   => %s" % ("OK" if ok else "FAIL: expected 5 schedulers and a non-empty run"))
    return ok


def check_parity_page(session, base):
    url = urllib.parse.urljoin(base, "verify.html")
    ready = "document.body.dataset.status === 'ready' || document.body.dataset.status === 'error'"
    print("== verify.html ==")
    if not session.open(url, ready):
        print("FAIL: %s did not finish the equivalence check" % url)
        return False
    status = session.evaluate("document.body.dataset.status")
    raw = session.evaluate(
        "(function(){var n=document.getElementById('parity-result');return n?n.innerText:'';})()"
    )
    try:
        report = json.loads(raw)
    except Exception:
        print("FAIL: parity report is not valid JSON (page status=%s): %s" % (status, str(raw)[:200]))
        return False
    ok = (
        status == "ready"
        and report.get("mismatches") == 0
        and report.get("scenarios")
        and all(item.get("ok") for item in report["scenarios"])
    )
    print("   url                %s" % url)
    print("   page status        %s" % status)
    print("   scenarios          %d (all ok: %s)" % (
        len(report.get("scenarios", [])),
        all(item.get("ok") for item in report.get("scenarios", [])),
    ))
    print("   compared rows      %s" % report.get("comparedRows"))
    print("   compared summaries %s" % report.get("comparedSummaries"))
    print("   compared scheduler runs %s" % report.get("comparedSchedulers"))
    print("   mismatches         %s" % report.get("mismatches"))
    print("   max abs diff       %s" % report.get("maxAbsDiff"))
    print("   => %s" % ("OK" if ok else "FAIL: the port does not reproduce the reference outputs"))
    return ok


def main():
    base = sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:8000/docs/"
    if not base.endswith("/"):
        base += "/"
    timeout = float(sys.argv[2]) if len(sys.argv) > 2 else 240.0

    session = Session(timeout)
    try:
        results = [check_demo_page(session, base), check_parity_page(session, base)]
    finally:
        session.close()

    print()
    print("SUMMARY: %d/%d checks passed" % (sum(1 for r in results if r), len(results)))
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
