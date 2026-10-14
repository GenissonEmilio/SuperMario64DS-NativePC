"""Active locks from the tangos.dev claims service.

Schedulers skip a target someone else holds. The service is tangos.json
`data.claimsApi`, scoped with `data.projectId`.

Best-effort: offline, or with the service down, this returns no holds and the
batch still builds. Set CLAIMS_NO_API=1 to skip the network.

API locks are half-open [start, end) ranges on a module.

    import claims_md
    held = claims_md.held_targets()
    if claims_md.is_held(held, name, addr, module=mod):
        ...
"""
import json
import os
import pathlib
import urllib.request

REPO = pathlib.Path(__file__).resolve().parent.parent
DESCRIPTOR = REPO / "tangos.json"
API_TIMEOUT_S = 5


def _api_ranges():
    """Active locks as (module, start, end) tuples. Any failure returns []."""
    if os.environ.get("CLAIMS_NO_API"):
        return []
    try:
        data = json.loads(DESCRIPTOR.read_text(encoding="utf-8")).get("data", {})
        api = data.get("claimsApi")
        if not api:
            return []
        project = data.get("projectId")
        url = api + (("&" if "?" in api else "?") + "project=" + project if project else "")
        with urllib.request.urlopen(url, timeout=API_TIMEOUT_S) as r:
            body = json.loads(r.read().decode("utf-8"))
        ranges = []
        for c in body.get("claims", []):
            try:
                ranges.append((str(c["module"]).lower(), int(str(c["start"]), 0), int(str(c["end"]), 0)))
            except (KeyError, TypeError, ValueError):
                continue
        return ranges
    except Exception:
        return []


def held_targets():
    """Live holds from the claims service.

    Returns {"names": set[str], "addrs": set[int], "ranges": list[(module, start, end)],
    "rows": int}. names and addrs stay empty; ranges are the service's locks.
    """
    ranges = _api_ranges()
    return {"names": set(), "addrs": set(), "ranges": ranges, "rows": len(ranges)}


def is_held(held, name=None, addr=None, module=None):
    """True if this target is claimed by someone.

    `module` scopes the range check (overlays reuse addresses, so a bare address
    can collide across modules). Without it, a range in ANY module matches: a
    scheduler skips a target rather than duplicate work.
    """
    a = None
    if addr is not None:
        try:
            a = addr if isinstance(addr, int) else int(str(addr), 0)
        except (TypeError, ValueError):
            a = None
    if a is not None:
        if a in held["addrs"]:
            return True
        mod = str(module).lower() if module else None
        for r_module, start, end in held.get("ranges", ()):
            if mod is not None and r_module != mod:
                continue
            if start <= a < end:
                return True
    if name and name in held["names"]:
        return True
    return False


if __name__ == "__main__":
    h = held_targets()
    print(f"claims: {h['rows']} held ({len(h['ranges'])} API locks)")
    for mod, start, end in sorted(h["ranges"])[:20]:
        print(f"   {mod} 0x{start:08x}-0x{end:08x}")
