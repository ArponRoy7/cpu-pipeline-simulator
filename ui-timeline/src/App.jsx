import React, { useEffect, useMemo, useRef, useState } from "react";

/**
 * CPU Pipeline Simulator — Timeline Viewer (Dark / Color-graded)
 * - Load timeline CSV: "cycle,IF,ID,EX,MEM,WB"
 * - Single or Compare (A vs B)
 * - Dark UI with strong yet smooth color coding per opcode
 * - Stalls (STALL_*) are red, high-contrast
 * - Cards for Cycles, Retired, CPI, Total Stalls + hazard breakdown
 * - Simple bar compare for Total Stalls and CPI
 */

// ------------------------------ CSV Helpers ------------------------------
function parseCSVText(text) {
  const lines = text.trim().split(/\r?\n/);
  if (lines.length === 0) return { header: [], rows: [] };
  const header = lines[0].split(",").map((s) => s.trim());
  const rows = lines
    .slice(1)
    .filter(Boolean)
    .map((line) => line.split(",").map((s) => s.trim()));
  return { header, rows };
}

function isStallCell(val) {
  return val && (val.startsWith("STALL") || val === "STALL");
}

// --- Dark color map for cells ---
function stageClass(value) {
  const base =
    "px-3 py-1 border-b border-slate-700/60 text-center whitespace-nowrap text-slate-100";
  if (!value || value === "-") return base + " text-slate-500";

  if (isStallCell(value))
    // 🔴 stalls: bright on dark
    return (
      base +
      " bg-red-700/60 text-red-100 font-semibold ring-1 ring-red-500/40"
    );

  // Instruction colors
  if (value.startsWith("LOAD") || value.startsWith("STORE"))
    return base + " bg-blue-700/40 text-blue-50";
  if (value.startsWith("ADD") || value.startsWith("SUB"))
    return base + " bg-emerald-700/40 text-emerald-50";
  if (value.startsWith("BEQ") || value.startsWith("BNE"))
    return base + " bg-amber-700/40 text-amber-50";
  if (value.startsWith("HALT"))
    return base + " bg-violet-700/40 text-violet-50";
  if (value.startsWith("NOP"))
    return base + " bg-slate-700/40 text-slate-300 italic";

  // default
  return base + " bg-slate-700/40";
}

function deriveMetricsFromTimeline(tl) {
  if (!tl || tl.rows.length === 0)
    return {
      cycles: 0,
      retired: 0,
      cpi: 0,
      stalls: { total: 0, raw: 0, war: 0, waw: 0, ctrl: 0 },
    };
  const idxWB = tl.header.findIndex((h) => h.toUpperCase() === "WB");
  const cycles = tl.rows.length;
  let retired = 0;

  let stallRAW = 0,
    stallWAR = 0,
    stallWAW = 0,
    stallCTRL = 0,
    stallTotal = 0;

  for (const r of tl.rows) {
    if (idxWB >= 0) {
      const wb = r[idxWB];
      if (wb && wb !== "-" && !wb.startsWith("NOP") && !wb.startsWith("HALT"))
        retired++;
    }
    for (const cell of r) {
      if (isStallCell(cell)) {
        stallTotal++;
        if (cell.startsWith("STALL_RAW")) stallRAW++;
        else if (cell.startsWith("STALL_WAR")) stallWAR++;
        else if (cell.startsWith("STALL_WAW")) stallWAW++;
        else if (cell.startsWith("STALL_CTRL")) stallCTRL++;
      }
    }
  }

  const cpi = retired ? cycles / retired : 0;
  return {
    cycles,
    retired,
    cpi,
    stalls: { total: stallTotal, raw: stallRAW, war: stallWAR, waw: stallWAW, ctrl: stallCTRL },
  };
}

// ------------------------------ PNG export (lazy import) ------------------
let html2canvasPromise = null;
async function getHtml2Canvas() {
  if (!html2canvasPromise) {
    try {
      html2canvasPromise = import("html2canvas").then((m) => m.default || m);
    } catch {
      html2canvasPromise = Promise.resolve(null);
    }
  }
  return html2canvasPromise;
}

// ------------------------------ UI Bits ----------------------------------
function StatCard({ title, value, note }) {
  return (
    <div className="rounded-xl border border-slate-700 bg-slate-800/80 p-4 shadow-sm">
      <div className="text-xs uppercase tracking-wide text-slate-400">
        {title}
      </div>
      <div className="text-2xl font-semibold mt-1 text-slate-50">
        {value}
      </div>
      {note && <div className="text-xs mt-1 text-slate-400">{note}</div>}
    </div>
  );
}

function FilePicker({ label, onFile, onName }) {
  return (
    <label className="px-3 py-1.5 rounded-md bg-slate-800 border border-slate-600 hover:bg-slate-700 cursor-pointer text-sm text-slate-100 shadow-sm">
      {label}
      <input
        type="file"
        accept=".csv,.txt"
        className="hidden"
        onChange={(e) => {
          const f = e.target.files?.[0];
          if (!f) return;
          onName && onName(f.name);
          f.text().then((txt) => onFile(txt));
        }}
      />
    </label>
  );
}

function TimelineGrid({ title, fileName, timeline, onExportPNGRef }) {
  const containerRef = useRef(null);

  useEffect(() => {
    if (!onExportPNGRef) return;
    onExportPNGRef.current = async () => {
      const h2c = await getHtml2Canvas();
      if (!h2c) {
        alert("To enable PNG export, install: npm i html2canvas");
        return;
      }
      const el = containerRef.current;
      if (!el) return;
      const canvas = await h2c(el);
      const url = canvas.toDataURL("image/png");
      const a = document.createElement("a");
      a.href = url;
      a.download = `${(fileName || title || "timeline").replace(/\s+/g, "_")}.png`;
      a.click();
    };
  }, [onExportPNGRef, title, fileName]);

  const header = timeline?.header || [];
  const rows = timeline?.rows || [];
  const expectedCols = ["cycle", "IF", "ID", "EX", "MEM", "WB"];

  return (
    <div className="rounded-xl border border-slate-700 overflow-hidden bg-slate-900/80 shadow-sm">
      <div className="px-4 py-2 bg-slate-800/80 text-sm font-medium flex items-center justify-between">
        <span className="text-slate-100">{title}</span>
        {fileName && <span className="text-xs text-slate-400">File: {fileName}</span>}
      </div>
      <div ref={containerRef} className="overflow-auto max-h-[520px]">
        <table className="min-w-full text-sm">
          <thead className="sticky top-0 bg-slate-900/95 backdrop-blur">
            <tr>
              {(header.length ? header : expectedCols).map((h) => (
                <th
                  key={h}
                  className="px-3 py-2 border-b border-slate-700/60 text-left text-slate-300"
                >
                  {h}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {rows.map((r, i) => (
              <tr key={i} className={i % 2 ? "bg-slate-900/40" : "bg-slate-900/20"}>
                {r.map((cell, j) => (
                  <td
                    key={j}
                    className={
                      j === 0
                        ? "px-3 py-1 border-b border-slate-700/60 text-slate-200"
                        : stageClass(cell)
                    }
                  >
                    {cell}
                  </td>
                ))}
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}

function CompareBars({ a, b, titleA = "A", titleB = "B" }) {
  const max = Math.max(a, b, 1);
  const widthA = Math.round((a / max) * 100);
  const widthB = Math.round((b / max) * 100);
  return (
    <div className="rounded-xl border border-slate-700 bg-slate-800/80 p-4 shadow-sm">
      <div className="text-sm mb-2 text-slate-200">Comparison</div>
      <div className="space-y-3">
        <div>
          <div className="text-xs mb-1 text-slate-300">{titleA}: {a}</div>
          <div className="h-3 bg-slate-700 rounded">
            <div className="h-3 rounded bg-indigo-500" style={{ width: `${widthA}%` }} />
          </div>
        </div>
        <div>
          <div className="text-xs mb-1 text-slate-300">{titleB}: {b}</div>
          <div className="h-3 bg-slate-700 rounded">
            <div className="h-3 rounded bg-cyan-500" style={{ width: `${widthB}%` }} />
          </div>
        </div>
      </div>
    </div>
  );
}

function Panel({ title, timeline, setTimeline, fileName, setFileName }) {
  const [pngRef] = useState({ current: null });
  const derived = useMemo(() => deriveMetricsFromTimeline(timeline), [timeline]);

  return (
    <div className="flex flex-col gap-4">
      <div className="flex flex-wrap items-center gap-2">
        <FilePicker
          label="Load Timeline CSV"
          onName={setFileName}
          onFile={(txt) => setTimeline(parseCSVText(txt))}
        />
        <button
          className="px-3 py-1.5 rounded-md bg-slate-800 border border-slate-600 hover:bg-slate-700 text-slate-100 shadow-sm"
          onClick={() => pngRef.current && pngRef.current()}
        >
          Export PNG
        </button>
      </div>

      <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
        <StatCard title="Cycles" value={derived.cycles} />
        <StatCard title="Retired" value={derived.retired} />
        <StatCard title="CPI" value={derived.cpi ? derived.cpi.toFixed(2) : "-"} />
        <StatCard title="Total Stalls" value={derived.stalls.total} />
      </div>

      <TimelineGrid title={title} fileName={fileName} timeline={timeline} onExportPNGRef={pngRef} />

      {timeline && (
        <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
          <StatCard title="RAW stalls" value={derived.stalls.raw} />
          <StatCard title="WAR stalls" value={derived.stalls.war} />
          <StatCard title="WAW stalls" value={derived.stalls.waw} />
          <StatCard title="CTRL stalls" value={derived.stalls.ctrl} />
        </div>
      )}
    </div>
  );
}

export default function App() {
  const [mode, setMode] = useState("single"); // single | compare

  const [timelineA, setTimelineA] = useState(null);
  const [fileA, setFileA] = useState("");
  const [timelineB, setTimelineB] = useState(null);
  const [fileB, setFileB] = useState("");

  const derivedA = useMemo(() => deriveMetricsFromTimeline(timelineA), [timelineA]);
  const derivedB = useMemo(() => deriveMetricsFromTimeline(timelineB), [timelineB]);

  // Enforce dark look (no 'dark' class relying — we style explicitly)
  useEffect(() => {
    document.documentElement.classList.remove("dark");
  }, []);

  return (
    <div className="min-h-screen bg-gradient-to-b from-slate-950 via-slate-900 to-slate-800 text-slate-100 p-6">
      <div className="max-w-7xl mx-auto flex flex-col gap-6">
        {/* Header */}
        <div className="flex items-center justify-between">
          <div className="text-2xl font-semibold text-slate-50">
            CPU Pipeline Simulator — Timeline
          </div>
          <div className="flex items-center gap-2">
            <span className="text-sm text-slate-300">Mode</span>
            <select
              className="border border-slate-600 rounded-md px-2 py-1 bg-slate-800 text-slate-100 shadow-sm"
              value={mode}
              onChange={(e) => setMode(e.target.value)}
            >
              <option value="single">Single</option>
              <option value="compare">Compare (A vs B)</option>
            </select>
          </div>
        </div>

        {/* Body */}
        {mode === "single" ? (
          <Panel
            title="Timeline"
            timeline={timelineA}
            setTimeline={setTimelineA}
            fileName={fileA}
            setFileName={setFileA}
          />
        ) : (
          <>
            <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
              <Panel
                title="Timeline A"
                timeline={timelineA}
                setTimeline={setTimelineA}
                fileName={fileA}
                setFileName={setFileA}
              />
              <Panel
                title="Timeline B"
                timeline={timelineB}
                setTimeline={setTimelineB}
                fileName={fileB}
                setFileName={setFileB}
              />
            </div>

            {(timelineA || timelineB) && (
              <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
                <CompareBars
                  a={derivedA.stalls?.total || 0}
                  b={derivedB.stalls?.total || 0}
                  titleA="A: Total Stalls"
                  titleB="B: Total Stalls"
                />
                <CompareBars
                  a={Number(derivedA.cpi || 0)}
                  b={Number(derivedB.cpi || 0)}
                  titleA={`A: CPI ${derivedA.cpi ? derivedA.cpi.toFixed(2) : "-"}`}
                  titleB={`B: CPI ${derivedB.cpi ? derivedB.cpi.toFixed(2) : "-"}`}
                />
              </div>
            )}
          </>
        )}

        <div className="text-xs text-slate-400">
          Load CSVs generated by your C++ simulator. Cells labeled
          <code> STALL_* </code> are highlighted in red.
        </div>
      </div>
    </div>
  );
}
