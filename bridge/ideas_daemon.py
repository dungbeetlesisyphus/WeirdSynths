#!/usr/bin/env python3
"""
WeirdOS Ideas Generator Daemon
═══════════════════════════════════════════════════════════
Generates 10 new VCV Rack module ideas per day at a scheduled
time, learns from approval/rejection/ratings to refine future
ideas, and stores everything on external HDD.

Storage layout (IDEAS_STORAGE_PATH):
  /batches/          — one JSON file per daily batch
  /approved/         — approved ideas (symlinked into VCV roadmap)
  /rejected/         — rejected ideas (kept for learning)
  /pending/          — today's batch awaiting approval
  ratings_history.json
  preferences.json   — learned preference weights
  feed.json          — rolling 30-day approved feed (read by ideas.html)

Usage:
  python3 ideas_daemon.py [--now] [--dry-run] [--storage /path]

  --now        Generate immediately (don't wait for scheduled time)
  --dry-run    Show generated ideas without saving
  --storage    Override IDEAS_STORAGE_PATH from environment
═══════════════════════════════════════════════════════════
"""

import os
import sys
import json
import time
import math
import random
import hashlib
import logging
import argparse
import datetime
import threading
import http.client
import urllib.request
from pathlib import Path
from collections import defaultdict

# ── Logging ──────────────────────────────────────────────────────────────────
logging.basicConfig(
    level=logging.INFO,
    format="[ideas] %(asctime)s %(levelname)s %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("ideas")


# ═════════════════════════════════════════════════════════════════════════════
# CONFIGURATION
# ═════════════════════════════════════════════════════════════════════════════

DEFAULT_STORAGE = Path("/home/weird/ideas")
IDEAS_PER_DAY   = 10
DEFAULT_TIME    = "06:00"   # HH:MM local time

CATEGORIES = [
    "Oscillator", "Filter", "Envelope", "LFO", "Sequencer",
    "Effect", "Utility", "Mixer", "CV Source", "Clock",
    "Sampler", "Granular", "Reverb", "Delay", "Distortion",
    "Waveshaper", "Spectral", "Physical Model", "Rhythm", "Generative",
]

BODY_PARTS = [
    "eyes", "mouth", "nose", "eyebrows", "cheeks",
    "head-tilt", "jaw", "forehead", "hands", "full-body",
    "breath", "heartbeat", "gaze", "pupils", "lips",
]


# ═════════════════════════════════════════════════════════════════════════════
# PREFERENCE LEARNING ENGINE
# ═════════════════════════════════════════════════════════════════════════════

class PreferenceEngine:
    """
    Learns Millicent's module preferences from ratings and approvals.
    Maintains weighted preference vectors across:
      - category (Oscillator, Filter, …)
      - HP width (compact vs sprawling)
      - body part (eyes, mouth, …)
      - output count (utility vs dense)
      - concept style tags (rhythmic, tonal, textural, generative, …)
    """

    def __init__(self, prefs_path: Path):
        self.path = prefs_path
        self.data = self._load()

    def _load(self) -> dict:
        if self.path.exists():
            try:
                with open(self.path) as f:
                    return json.load(f)
            except Exception:
                pass
        return {
            "category_scores":  {},   # category → (total_score, count)
            "hp_scores":        {},   # "compact"/"medium"/"wide" → (score, count)
            "body_scores":      {},   # body_part → (score, count)
            "output_scores":    {},   # output_count_bucket → (score, count)
            "style_scores":     {},   # style tag → (score, count)
            "total_rated":      0,
            "total_approved":   0,
            "total_rejected":   0,
            "last_updated":     None,
        }

    def save(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with open(self.path, "w") as f:
            json.dump(self.data, f, indent=2)

    def _update_score(self, bucket: str, key: str, score: float):
        """Exponential moving average update (α=0.3)."""
        alpha = 0.3
        scores = self.data[bucket]
        if key not in scores:
            scores[key] = [score, 1]
        else:
            prev, count = scores[key]
            scores[key] = [alpha * score + (1 - alpha) * prev, count + 1]

    def record_rating(self, idea: dict, rating: float, critique: str = ""):
        """
        rating: 0.0–1.0 (maps to approve=1.0, 4-star=0.8, 3-star=0.6, etc.)
        """
        # Normalise rating to 0–1
        r = max(0.0, min(1.0, rating))

        self._update_score("category_scores", idea.get("category", "Unknown"), r)
        self._update_score("body_scores", idea.get("bodyPart", "unknown"), r)

        hp = idea.get("hp", 8)
        bucket = "compact" if hp <= 8 else ("wide" if hp >= 16 else "medium")
        self._update_score("hp_scores", bucket, r)

        out_count = len(idea.get("outputs", []))
        out_bucket = "few" if out_count <= 3 else ("many" if out_count >= 6 else "normal")
        self._update_score("output_scores", out_bucket, r)

        # Extract style tags from concept text
        concept = (idea.get("concept", "") + " " + idea.get("keyFeature", "")).lower()
        for tag in ["rhythmic", "tonal", "textural", "generative", "modulation",
                    "performance", "expressive", "abstract", "physical", "chaotic"]:
            if tag in concept:
                self._update_score("style_scores", tag, r)

        self.data["total_rated"] += 1
        self.data["last_updated"] = datetime.datetime.now().isoformat()
        self.save()

    def record_approved(self, idea: dict, critique: str = ""):
        self.record_rating(idea, 1.0, critique)
        self.data["total_approved"] += 1
        self.save()

    def record_rejected(self, idea: dict, reason: str = ""):
        self.record_rating(idea, 0.0, reason)
        self.data["total_rejected"] += 1
        self.save()

    def top_categories(self, n: int = 5) -> list:
        scores = self.data["category_scores"]
        ranked = sorted(scores.items(), key=lambda x: x[1][0], reverse=True)
        return [k for k, _ in ranked[:n]]

    def top_body_parts(self, n: int = 4) -> list:
        scores = self.data["body_scores"]
        ranked = sorted(scores.items(), key=lambda x: x[1][0], reverse=True)
        return [k for k, _ in ranked[:n]]

    def preferred_hp(self) -> str:
        scores = self.data["hp_scores"]
        if not scores:
            return "compact to medium"
        best = max(scores.items(), key=lambda x: x[1][0])
        return {"compact": "8 HP or less", "medium": "8–14 HP", "wide": "16+ HP"}.get(best[0], "medium")

    def preferred_styles(self, n: int = 3) -> list:
        scores = self.data["style_scores"]
        ranked = sorted(scores.items(), key=lambda x: x[1][0], reverse=True)
        return [k for k, _ in ranked[:n]]

    def preference_summary(self) -> str:
        """Build a natural language preference description for injection into LLM prompt."""
        parts = []
        top_cats = self.top_categories(3)
        if top_cats:
            parts.append(f"tends to favour {', '.join(top_cats)} modules")
        top_bp = self.top_body_parts(2)
        if top_bp:
            parts.append(f"responds well to ideas using {' and '.join(top_bp)}")
        hp = self.preferred_hp()
        if hp:
            parts.append(f"prefers {hp} width")
        styles = self.preferred_styles(2)
        if styles:
            parts.append(f"leans toward {' and '.join(styles)} concepts")
        n_rated = self.data["total_rated"]
        if n_rated < 5:
            return "No strong preferences learned yet (keep rating to improve!)"
        return "Millicent " + ", ".join(parts) + "."


# ═════════════════════════════════════════════════════════════════════════════
# IDEA GENERATOR
# ═════════════════════════════════════════════════════════════════════════════

class IdeaGenerator:
    """
    Calls local Qwen2.5-Coder (via llm_server.py HTTP API) or
    Claude API to generate a batch of module ideas as structured JSON.
    """

    def __init__(self, backend: str = "local", storage: Path = DEFAULT_STORAGE):
        self.backend = backend
        self.storage = storage
        self.llm_url  = os.environ.get("LLM_SERVER_URL", "http://127.0.0.1:8080")
        self.claude_key = self._load_api_key()

    def _load_api_key(self) -> str:
        """Load Claude API key from keyring or encrypted file."""
        # Try CLAUDE_API_KEY env var first (set by llm_server during hybrid mode)
        key = os.environ.get("CLAUDE_API_KEY", "")
        if key:
            return key

        # Try encrypted file
        key_file = Path("/etc/weirdos/.api_key.enc")
        if key_file.exists():
            machine_id = Path("/etc/machine-id").read_text().strip() if Path("/etc/machine-id").exists() else "weirdos-default"
            try:
                import subprocess
                result = subprocess.run(
                    ["openssl", "enc", "-aes-256-cbc", "-pbkdf2", "-d",
                     "-pass", f"pass:{machine_id}", "-in", str(key_file)],
                    capture_output=True, text=True, timeout=5
                )
                if result.returncode == 0:
                    return result.stdout.strip()
            except Exception:
                pass
        return ""

    def _build_prompt(self, prefs: PreferenceEngine, avoid_ids: list) -> str:
        """Construct the generation prompt with learned preferences injected."""
        pref_summary = prefs.preference_summary()

        # Load recently generated idea names to avoid duplicates
        recent_names = self._get_recent_names(30)
        avoid_str = ""
        if recent_names:
            avoid_str = f"\nDo NOT generate ideas named: {', '.join(recent_names[:20])}.\n"

        return f"""You are a master synthesizer module designer for VCV Rack Pro 2.
The plugin is called WeirdSynths — all modules use webcam face/body tracking as control input.
A Raspberry Pi 5 with Hailo-10H NPU does the tracking in real-time.

{pref_summary}

Generate exactly 10 original, innovative, diverse VCV Rack module ideas.
Each must use a different face/body tracking input (eyes, mouth, nose-tip, brow, jaw, head-tilt, breath, etc.).
Aim for variety across categories. Think weird, expressive, and musically useful.
{avoid_str}
Output ONLY valid JSON — an array of exactly 10 objects. Each object:
{{
  "name": "MODULE_NAME",
  "tagline": "one-line poetic description",
  "category": "<one of: {', '.join(CATEGORIES[:10])}...>",
  "hp": <integer 4-20>,
  "concept": "2-3 sentence description of what it does and why it's interesting",
  "keyFeature": "the single most unique/compelling thing about it",
  "inputs": ["port names"],
  "outputs": ["port names"],
  "params": ["knob/switch names"],
  "inspiration": "what inspired this — real instrument, phenomenon, or technique",
  "bodyPart": "<primary face/body input used>"
}}

Be inventive. Prioritise musical usefulness AND weirdness. Make Millicent excited.
Output JSON array only, no preamble, no explanation."""

    def _get_recent_names(self, days: int) -> list:
        """Collect idea names from the last N days to avoid repeats."""
        names = []
        batches_dir = self.storage / "batches"
        if not batches_dir.exists():
            return names
        cutoff = datetime.date.today() - datetime.timedelta(days=days)
        for f in sorted(batches_dir.glob("*.json"), reverse=True)[:days]:
            try:
                stem = f.stem  # "2026-02-23"
                batch_date = datetime.date.fromisoformat(stem)
                if batch_date < cutoff:
                    break
                with open(f) as fp:
                    batch = json.load(fp)
                names += [idea["name"] for idea in batch.get("ideas", [])]
            except Exception:
                pass
        return names

    def _call_local_llm(self, prompt: str) -> str:
        """Call local llm_server.py HTTP API (Qwen2.5-Coder on Hailo)."""
        payload = json.dumps({
            "prompt": prompt,
            "max_tokens": 3000,
            "temperature": 0.85,
            "stop": [],
        }).encode()
        req = urllib.request.Request(
            f"{self.llm_url}/generate",
            data=payload,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=90) as resp:
                result = json.loads(resp.read())
                return result.get("text", result.get("content", ""))
        except Exception as e:
            log.error(f"Local LLM call failed: {e}")
            return ""

    def _call_claude_api(self, prompt: str) -> str:
        """Call Claude API via anthropic SDK."""
        if not self.claude_key:
            log.error("No Claude API key available")
            return ""
        try:
            import anthropic
            client = anthropic.Anthropic(api_key=self.claude_key)
            message = client.messages.create(
                model="claude-opus-4-5-20251101",
                max_tokens=3000,
                messages=[{"role": "user", "content": prompt}]
            )
            return message.content[0].text
        except ImportError:
            log.error("anthropic SDK not installed: pip install anthropic")
            return ""
        except Exception as e:
            log.error(f"Claude API call failed: {e}")
            return ""

    def _parse_ideas(self, raw: str, date_str: str) -> list:
        """Parse LLM response JSON into validated idea objects."""
        # Extract JSON array from response (handle markdown code blocks)
        raw = raw.strip()
        if "```" in raw:
            raw = raw.split("```")[1]
            if raw.startswith("json"):
                raw = raw[4:]
        raw = raw.strip()

        # Find array bounds
        start = raw.find("[")
        end   = raw.rfind("]") + 1
        if start < 0 or end <= start:
            log.error("No JSON array found in LLM response")
            log.debug(f"Raw response: {raw[:500]}")
            return []

        try:
            ideas = json.loads(raw[start:end])
        except json.JSONDecodeError as e:
            log.error(f"JSON parse failed: {e}")
            return []

        # Validate and normalise each idea
        validated = []
        for i, idea in enumerate(ideas[:IDEAS_PER_DAY]):
            if not isinstance(idea, dict):
                continue
            if not idea.get("name") or not idea.get("concept"):
                continue

            # Generate stable ID from date + index
            date_slug = date_str.replace("-", "")
            validated.append({
                "id": f"{date_slug}-{i+1:02d}",
                "name": str(idea.get("name", f"IDEA-{i+1}")).upper()[:20],
                "tagline": str(idea.get("tagline", ""))[:100],
                "category": idea.get("category", "Utility"),
                "hp": max(4, min(24, int(idea.get("hp", 8)))),
                "concept": str(idea.get("concept", ""))[:500],
                "keyFeature": str(idea.get("keyFeature", ""))[:200],
                "inputs": idea.get("inputs", [])[:8],
                "outputs": idea.get("outputs", [])[:8],
                "params": idea.get("params", [])[:8],
                "inspiration": str(idea.get("inspiration", ""))[:200],
                "bodyPart": idea.get("bodyPart", "face"),
                "generated": date_str,
                "status": "pending",
                "rating": None,
                "critique": "",
                "approved_at": None,
            })
        return validated

    def generate(self, prefs: PreferenceEngine, dry_run: bool = False) -> list:
        """Generate a batch of IDEAS_PER_DAY ideas."""
        today = datetime.date.today().isoformat()
        log.info(f"Generating {IDEAS_PER_DAY} ideas for {today} (backend={self.backend})")

        prompt = self._build_prompt(prefs, [])

        # Choose backend
        raw = ""
        if self.backend in ("local", "hybrid"):
            raw = self._call_local_llm(prompt)
            if not raw and self.backend == "hybrid":
                log.info("Local LLM failed, falling back to Claude API")
                raw = self._call_claude_api(prompt)
        elif self.backend == "cloud":
            raw = self._call_claude_api(prompt)
        else:
            # Fallback: use procedural generation (no LLM required)
            log.warning("No AI backend — using procedural fallback")
            raw = self._procedural_fallback(today)

        if not raw:
            log.error("No response from any backend")
            return []

        ideas = self._parse_ideas(raw, today)
        log.info(f"Parsed {len(ideas)} valid ideas")

        if not dry_run:
            self._save_batch(ideas, today)
            self._write_pending(ideas)
            self._update_feed()

        return ideas

    def _procedural_fallback(self, date_str: str) -> str:
        """
        Seeded procedural idea generation — no LLM required.
        Combines templates to produce varied, always-different ideas.
        Useful if WeirdBox has no network and Hailo is busy.
        """
        random.seed(int(hashlib.md5(date_str.encode()).hexdigest(), 16) % (2**32))

        templates = [
            {"name": "FLUTTER", "tagline": "eyelid tremor becomes tremolo rate", "category": "LFO",
             "hp": 6, "bodyPart": "eyes", "concept": "Maps the micro-tremor frequency of eyelids to LFO rate. Perfectly still = slow throb. Tired eyes = fast flutter. The instability of human anatomy becomes a living modulation source.", "keyFeature": "Involuntary tremor as expressive vibrato", "inputs": ["RATE CV", "DEPTH CV"], "outputs": ["LFO", "TREMOR CV", "GATE"], "params": ["RATE SCALE", "DEPTH", "SHAPE"], "inspiration": "Vocal vibrato and involuntary muscle fasciculations"},
            {"name": "CHEEKBONE", "tagline": "smile width tunes the filter", "category": "Filter",
             "hp": 10, "bodyPart": "cheeks", "concept": "Tracks cheekbone elevation (smile vs neutral vs frown) as a -5 to +5V CV. High cheeks open a ladder filter; low cheeks engage a comb. Neutral position gives clean pass-through.", "keyFeature": "Facial expression controls timbre in real-time", "inputs": ["AUDIO", "MOD CV"], "outputs": ["LP", "HP", "COMB"], "params": ["CUTOFF", "RESONANCE", "CHROMA"], "inspiration": "The formant shift in the human voice when smiling"},
            {"name": "EXHALE", "tagline": "your breath is the envelope", "category": "Envelope",
             "hp": 8, "bodyPart": "breath", "concept": "Monitors breathing via mic amplitude envelope detection plus nostril landmark tracking. Inhale triggers attack; exhale shapes decay and release. Breathing rate controls overall envelope speed.", "keyFeature": "Breathing synchronises synthesis to your body rhythm", "inputs": ["GATE", "RATE CV"], "outputs": ["ENV", "BREATH CV", "PHASE"], "params": ["INHALE SCALE", "EXHALE SCALE", "SMOOTH"], "inspiration": "Bansuri flute dynamics and throat singing"},
        ]

        # Pad to IDEAS_PER_DAY with shuffled + varied versions
        pool = templates * 4
        random.shuffle(pool)
        selected = pool[:IDEAS_PER_DAY]

        # Vary names by appending random suffix to avoid duplicates
        suffixes = ["II", "X", "PLUS", "DEEP", "LITE", "DARK", "RAW", "ULTRA", "MICRO", "MEGA"]
        results = []
        used_names = set()
        for t in selected:
            name = t["name"]
            if name in used_names:
                name = name + " " + random.choice(suffixes)
            used_names.add(name)
            t = dict(t, name=name)
            results.append(t)

        return json.dumps(results)

    def _save_batch(self, ideas: list, date_str: str):
        """Persist batch to storage/batches/YYYY-MM-DD.json"""
        batches_dir = self.storage / "batches"
        batches_dir.mkdir(parents=True, exist_ok=True)
        batch_file = batches_dir / f"{date_str}.json"
        with open(batch_file, "w") as f:
            json.dump({"date": date_str, "ideas": ideas}, f, indent=2)
        log.info(f"Batch saved: {batch_file}")

    def _write_pending(self, ideas: list):
        """Write today's pending ideas to storage/pending/"""
        pending_dir = self.storage / "pending"
        pending_dir.mkdir(parents=True, exist_ok=True)
        # Clear old pending files
        for old in pending_dir.glob("*.json"):
            old.unlink()
        for idea in ideas:
            fname = pending_dir / f"{idea['id']}.json"
            with open(fname, "w") as f:
                json.dump(idea, f, indent=2)
        log.info(f"Written {len(ideas)} pending idea files")

    def _update_feed(self):
        """
        Regenerate the rolling feed (last 30 days of approved ideas) →
        storage/feed.json + symlink docs/ideas.json → storage/feed.json
        """
        approved_dir = self.storage / "approved"
        approved_dir.mkdir(parents=True, exist_ok=True)

        feed_ideas = []
        cutoff = datetime.date.today() - datetime.timedelta(days=30)
        for f in sorted(approved_dir.glob("*.json"), reverse=True):
            try:
                with open(f) as fp:
                    idea = json.load(fp)
                if idea.get("generated"):
                    idea_date = datetime.date.fromisoformat(idea["generated"])
                    if idea_date >= cutoff:
                        feed_ideas.append(idea)
            except Exception:
                pass

        feed = {
            "meta": {
                "plugin": "WeirdSynths",
                "description": "AI-generated VCV Rack module ideas — rate them to build the roadmap",
                "lastGenerated": datetime.date.today().isoformat(),
                "totalIdeas": len(feed_ideas),
            },
            "ideas": feed_ideas,
        }
        feed_file = self.storage / "feed.json"
        with open(feed_file, "w") as f:
            json.dump(feed, f, indent=2)
        log.info(f"Feed updated: {len(feed_ideas)} approved ideas")


# ═════════════════════════════════════════════════════════════════════════════
# APPROVAL API (HTTP server for WeirdStudio)
# ═════════════════════════════════════════════════════════════════════════════

class ApprovalServer:
    """
    Tiny HTTP server that WeirdStudio calls to approve/reject/rate ideas.
    Listens on localhost:9010 (configurable).

    Endpoints:
      GET  /pending         — list pending ideas
      GET  /preferences     — current learned preferences
      POST /approve/<id>    — approve idea (body: {"critique": "..."})
      POST /reject/<id>     — reject idea  (body: {"reason": "..."})
      POST /rate/<id>       — rate idea    (body: {"rating": 0-5, "critique": "..."})
      POST /request-changes/<id> — send back for revision (body: {"notes": "..."})
    """

    def __init__(self, storage: Path, prefs: PreferenceEngine,
                 generator: IdeaGenerator, port: int = 9010):
        self.storage  = storage
        self.prefs    = prefs
        self.gen      = generator
        self.port     = port

    def _move_idea(self, idea_id: str, status: str) -> dict | None:
        pending_file = self.storage / "pending" / f"{idea_id}.json"
        if not pending_file.exists():
            # Check batches
            for b in (self.storage / "batches").glob("*.json"):
                with open(b) as f:
                    batch = json.load(f)
                for idea in batch.get("ideas", []):
                    if idea["id"] == idea_id:
                        return idea
            return None
        with open(pending_file) as f:
            idea = json.load(f)
        return idea

    def _save_to(self, idea: dict, dest_dir: str):
        dest = self.storage / dest_dir
        dest.mkdir(parents=True, exist_ok=True)
        with open(dest / f"{idea['id']}.json", "w") as f:
            json.dump(idea, f, indent=2)
        # Remove from pending
        pending = self.storage / "pending" / f"{idea['id']}.json"
        if pending.exists():
            pending.unlink()

    def handle(self, method: str, path: str, body: bytes) -> tuple[int, dict]:
        """Dispatch request → (status_code, response_dict)"""
        parts = path.strip("/").split("/")

        # GET /pending
        if method == "GET" and parts[0] == "pending":
            pending = []
            for f in sorted((self.storage / "pending").glob("*.json")):
                with open(f) as fp:
                    pending.append(json.load(fp))
            return 200, {"pending": pending, "count": len(pending)}

        # GET /preferences
        if method == "GET" and parts[0] == "preferences":
            return 200, {
                "preferences": self.prefs.data,
                "summary": self.prefs.preference_summary(),
                "top_categories": self.prefs.top_categories(5),
                "top_body_parts": self.prefs.top_body_parts(4),
            }

        # POST /approve/<id>
        if method == "POST" and parts[0] == "approve" and len(parts) > 1:
            idea_id = parts[1]
            payload = json.loads(body or b"{}") if body else {}
            idea = self._move_idea(idea_id, "approved")
            if not idea:
                return 404, {"error": f"idea {idea_id} not found"}
            idea["status"]      = "approved"
            idea["critique"]    = payload.get("critique", "")
            idea["approved_at"] = datetime.datetime.now().isoformat()
            idea["rating"]      = 1.0
            self._save_to(idea, "approved")
            self.prefs.record_approved(idea, idea["critique"])
            self.gen._update_feed()
            log.info(f"✓ Approved: {idea['name']} ({idea_id})")
            return 200, {"ok": True, "idea": idea["name"]}

        # POST /reject/<id>
        if method == "POST" and parts[0] == "reject" and len(parts) > 1:
            idea_id = parts[1]
            payload = json.loads(body or b"{}") if body else {}
            idea = self._move_idea(idea_id, "rejected")
            if not idea:
                return 404, {"error": f"idea {idea_id} not found"}
            idea["status"]   = "rejected"
            idea["critique"] = payload.get("reason", "")
            idea["rating"]   = 0.0
            self._save_to(idea, "rejected")
            self.prefs.record_rejected(idea, idea["critique"])
            log.info(f"✗ Rejected: {idea['name']} ({idea_id})")
            return 200, {"ok": True, "idea": idea["name"]}

        # POST /rate/<id>
        if method == "POST" and parts[0] == "rate" and len(parts) > 1:
            idea_id = parts[1]
            payload = json.loads(body or b"{}") if body else {}
            stars   = float(payload.get("rating", 3))  # 1–5
            norm    = (stars - 1) / 4.0                # → 0–1
            critique = payload.get("critique", "")
            idea = self._move_idea(idea_id, "rated")
            if not idea:
                return 404, {"error": f"idea {idea_id} not found"}
            idea["status"]   = "approved" if norm >= 0.6 else "rejected"
            idea["rating"]   = norm
            idea["critique"] = critique
            if norm >= 0.6:
                idea["approved_at"] = datetime.datetime.now().isoformat()
                self._save_to(idea, "approved")
            else:
                self._save_to(idea, "rejected")
            self.prefs.record_rating(idea, norm, critique)
            self.gen._update_feed()
            log.info(f"★ Rated {stars:.0f}/5: {idea['name']}")
            return 200, {"ok": True, "idea": idea["name"], "status": idea["status"]}

        # POST /request-changes/<id>
        if method == "POST" and parts[0] == "request-changes" and len(parts) > 1:
            idea_id = parts[1]
            payload = json.loads(body or b"{}") if body else {}
            notes = payload.get("notes", "")
            idea = self._move_idea(idea_id, "needs-revision")
            if not idea:
                return 404, {"error": f"idea {idea_id} not found"}
            idea["status"]   = "needs-revision"
            idea["critique"] = notes
            self._save_to(idea, "needs-revision")
            # Partial negative signal — 0.3 score
            self.prefs.record_rating(idea, 0.3, notes)
            log.info(f"↩ Changes requested: {idea['name']} — {notes[:60]}")
            return 200, {"ok": True, "idea": idea["name"]}

        return 404, {"error": "not found"}

    def run(self):
        """Start HTTP server in a daemon thread."""
        import http.server
        import socketserver

        server = self
        port   = self.port

        class Handler(http.server.BaseHTTPRequestHandler):
            def do_GET(self):
                code, resp = server.handle("GET", self.path, b"")
                body = json.dumps(resp).encode()
                self.send_response(code)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(body)

            def do_POST(self):
                length = int(self.headers.get("Content-Length", 0))
                body   = self.rfile.read(length) if length else b""
                code, resp = server.handle("POST", self.path, body)
                out = json.dumps(resp).encode()
                self.send_response(code)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(out)

            def do_OPTIONS(self):
                self.send_response(200)
                self.send_header("Access-Control-Allow-Origin", "*")
                self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
                self.send_header("Access-Control-Allow-Headers", "Content-Type")
                self.end_headers()

            def log_message(self, fmt, *args):
                log.debug(f"[http] {fmt % args}")

        httpd = socketserver.TCPServer(("127.0.0.1", port), Handler)
        httpd.allow_reuse_address = True
        t = threading.Thread(target=httpd.serve_forever, daemon=True)
        t.start()
        log.info(f"Approval API listening on http://127.0.0.1:{port}")
        return httpd


# ═════════════════════════════════════════════════════════════════════════════
# SCHEDULER
# ═════════════════════════════════════════════════════════════════════════════

class Scheduler:
    """
    Fires generation at a configured daily time (HH:MM).
    Also fires on startup if today's batch hasn't been generated yet.
    """

    def __init__(self, generate_fn, time_str: str = DEFAULT_TIME, storage: Path = DEFAULT_STORAGE):
        self.generate  = generate_fn
        self.time_str  = time_str
        self.storage   = storage

    def _already_generated_today(self) -> bool:
        today = datetime.date.today().isoformat()
        batch_file = self.storage / "batches" / f"{today}.json"
        return batch_file.exists()

    def _seconds_until_next(self) -> float:
        h, m = map(int, self.time_str.split(":"))
        now = datetime.datetime.now()
        target = now.replace(hour=h, minute=m, second=0, microsecond=0)
        if target <= now:
            target += datetime.timedelta(days=1)
        return (target - now).total_seconds()

    def run(self, run_now: bool = False):
        # Generate immediately if today's batch is missing
        if run_now or not self._already_generated_today():
            log.info("Generating today's ideas now...")
            try:
                self.generate()
            except Exception as e:
                log.error(f"Generation failed: {e}", exc_info=True)

        # Main scheduler loop
        while True:
            wait = self._seconds_until_next()
            h = int(wait // 3600)
            m = int((wait % 3600) // 60)
            log.info(f"Next idea generation in {h}h {m}m (at {self.time_str})")
            time.sleep(wait)
            try:
                self.generate()
            except Exception as e:
                log.error(f"Scheduled generation failed: {e}", exc_info=True)
            time.sleep(60)  # avoid double-firing on DST edge


# ═════════════════════════════════════════════════════════════════════════════
# MAIN
# ═════════════════════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(description="WeirdOS Daily Ideas Generator")
    parser.add_argument("--now",      action="store_true", help="Generate immediately")
    parser.add_argument("--dry-run",  action="store_true", help="Print ideas, don't save")
    parser.add_argument("--storage",  default=None,         help="Override storage path")
    parser.add_argument("--port",     type=int, default=9010, help="Approval API port")
    parser.add_argument("--time",     default=None,         help="Generation time HH:MM")
    parser.add_argument("--no-api",   action="store_true",  help="Don't start approval HTTP server")
    args = parser.parse_args()

    # ── Resolve storage path ──────────────────────────────────────────────────
    storage_env  = os.environ.get("IDEAS_STORAGE_PATH", "")
    storage_path = Path(args.storage or storage_env or DEFAULT_STORAGE)
    storage_path.mkdir(parents=True, exist_ok=True)
    log.info(f"Storage: {storage_path}")

    # ── Load config from weirdos.conf ─────────────────────────────────────────
    ai_backend = os.environ.get("AI_BACKEND", "local")
    sched_time = args.time or os.environ.get("IDEAS_TIME", DEFAULT_TIME)

    # ── Initialise components ─────────────────────────────────────────────────
    prefs   = PreferenceEngine(storage_path / "preferences.json")
    gen     = IdeaGenerator(backend=ai_backend, storage=storage_path)
    api_srv = ApprovalServer(storage_path, prefs, gen, port=args.port)

    log.info(f"Preferences: {prefs.preference_summary()}")

    # ── Start approval API ────────────────────────────────────────────────────
    if not args.no_api and not args.dry_run:
        api_srv.run()

    # ── Dry-run mode ──────────────────────────────────────────────────────────
    if args.dry_run:
        ideas = gen.generate(prefs, dry_run=True)
        print(json.dumps(ideas, indent=2))
        return

    # ── Start scheduler ───────────────────────────────────────────────────────
    def generate():
        gen.generate(prefs)

    scheduler = Scheduler(generate, time_str=sched_time, storage=storage_path)
    scheduler.run(run_now=args.now)


if __name__ == "__main__":
    main()
