import React, { useEffect, useMemo, useRef, useState } from "react";

// Optional: enable PNG export with `npm i html2canvas`
let html2canvas = undefined;
try {
  // eslint-disable-next-line @typescript-eslint/no-var-requires
  html2canvas = require("html2canvas");
} catch {}

/**
 * CPU Pipeline Timeline Viewer (CSV-backed)
 * - Load timeline CSV: "cycle,IF,ID,EX,MEM,WB"
 * - (Optional) Load results_run.csv: appended by your C++ main
 * - A/B compare, dark mode, PNG export
 */

// ------------------------------ CSV Helpers ------------------------------
function parseCSVText(text) {
  const lines = text.trim().split(/\r?\n/);                 // <-- fixed regex
  if (lines.length === 0) return { header: [], rows: [] };
  const header = lines[0].split(",").map((s) => s.trim());
  const rows = lines
    .slice(1)
    .filter(Boolean)
    .map((line) => line.split(",").map((s) => s.trim()));
  return { header, rows };
}

function stageClass(value) {
  const base =
    "px-3 py-1 border-b border-gray-100 dark:border-gray-800 text-center whitespace-nowrap";
  if (!value || value === "-") return base + " text-gray-400";
  let stageColor = "";
  if (value.startsWith("LOAD") || value.startsWith("STORE"))
    stageColor = "bg-blue-50 dark:bg-blue-900/30";
  else if (value.startsWith("ADD") || value.startsWith("SUB"))
    stageColor = "bg-green-50 dark:bg-green-900/30";
  else if (value.startsWith("BEQ") || value.startsWith("BNE"))
    stageColor = "bg-yellow-50 dark:bg-yellow-900/30";
  else if (value.startsWith("HALT"))
    stageColor = "bg-purple-50 dark:bg-purple-900/30";
  else if (value.startsWith("NOP"))
    stageColor = "bg-gray-50 dark:bg-gray-800/40 italic opacity-60";
  return `${base} ${stageColor}`;
}

function deriveMetricsFromTimeline(timeline) {
  if (!timeline || timeline.rows.length === 0)
    return { cycles: 0, retired: 0, cpi: 0 };
  const idxWB = timeline.header.findIndex((h) => h.toUpperCase() === "WB");
  const cycles = timeline.rows.length;
  let retired = 0;
  if (idxWB >= 0) {
    for (const r of timeline.rows) {
      const wb = r[idxWB];
      if (!wb || wb === "-") continue;
      if (wb.startsWith("NOP")) continue;
      if (wb.startsWith("HALT")) continue;
      retired++;
    }
  }
  const cpi = retired ? cycles / retired : 0;
  return { cycles, retired, cpi };
}

// ------------------------------ UI Bits ------------------------------
function Toggle({ checked, onChange, label }) {
  return (
    <label className="flex items-center gap-2 cursor-pointer select-none">
      <span className="text-sm text-gray-700 dark:text-gray-200">{label}</span>
      <span
        className={`w-10 h-6 rounded-full p-1 transition ${
          checked ? "bg-indigo-600" : "bg-gray-300"
        }`}
        onClick={() => onChange(!checked)}
      >
        <span
          className={`block w-4 h-4 bg-white rounded-full transition ${
            checked ? "translate-x-4" : "translate-x-0"
          }`}
        />
      </span>
    </label>
  );
}

function StatCard({ title, value, note }) {
  return (
    <div className="rounded-2xl shadow-sm border border-gray-200 dark:border-gray-700 p-4">
      <div className="text-xs uppercase tracking-wide text-gray-500 dark:text-gray-400">
        {title}
      </div>
      <div className="text-2xl font-semibold mt-1 text-gray-900 dark:text-gray-50">
        {value}
      </div>
      {note && (
        <div className="text-xs mt-1 text-gray-500 dark:text-gray-400">
          {note}
        </div>
      )}
    </div>
  );
}

function FilePicker({ label, onFile }) {
  return (
    <label className="px-3 py-1.5 rounded-md bg-gray-100 dark:bg-gray-800 cursor-pointer text-sm">
      {label}
      <input
        type="file"
        accept=".csv,.txt"
        className="hidden"
        onChange={(e) => {
          const f = e.target.files?.[0];
          if (!f) return;
          f.text().then((txt) => onFile(txt));
        }}
      />
    </label>
  );
}

function TimelineGrid({ title, timeline, onExportPNGRef }) {
  const containerRef = useRef(null);

  useEffect(() => {
    if (!onExportPNGRef) return;
    onExportPNGRef.current = async () => {
      if (!html2canvas)
        return alert(
          "Install html2canvas to use PNG export: npm i html2canvas"
        );
      const el = containerRef.current;
      if (!el) return;
      const canvas = await html2canvas(el);
      const url = canvas.toDataURL("image/png");
      const a = document.createElement("a");
      a.href = url;
      a.download = `${title || "timeline"}.png`;
      a.click();
    };
  }, [onExportPNGRef, title]);

  const header = timeline?.header || [];
  const rows = timeline?.rows || [];
  const expectedCols = ["cycle", "IF", "ID", "EX", "MEM", "WB"];

  return (
    <div className="rounded-2xl border border-gray-200 dark:border-gray-700 overflow-hidden">
      <div className="px-4 py-2 bg-gray-50 dark:bg-gray-800/60 text-sm font-medium">
        {title}
      </div>
      <div ref={containerRef} className="overflow-auto max-h-[520px]">
        <table className="min-w-full text-sm">
          <thead className="sticky top-0 bg-white dark:bg-gray-900">
            <tr>
              {(header.length ? header : expectedCols).map((h) => (
                <th
                  key={h}
                  className="px-3 py-2 border-b border-gray-200 dark:border-gray-700 text-left text-gray-600 dark:text-gray-300"
                >
                  {h}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {rows.map((r, i) => (
              <tr
                key={i}
                className="odd:bg-gray-50/40 dark:odd:bg-gray-800/40"
              >
                {r.map((cell, j) => (
                  <td
                    key={j}
                    className={
                      j === 0
                        ? "px-3 py-1 border-b border-gray-100 dark:border-gray-800 text-gray-700 dark:text-gray-200"
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

function SummaryPanel({ resultsCSVText, filter }) {
  const parsed = useMemo(
    () => (resultsCSVText ? parseCSVText(resultsCSVText) : null),
    [resultsCSVText]
  );
  if (!parsed)
    return (
      <div className="rounded-2xl border border-gray-200 dark:border-gray-700 p-4 text-sm text-gray-600 dark:text-gray-300">
        Load <code>data/results_run.csv</code> to show predictor accuracy,
        stalls, CPI, etc.
      </div>
    );
  const { header, rows } = parsed;
  const col = (name) => header.findIndex((h) => h.trim() === name);
  const idxPred = col("predictor"),
    idxFwd = col("forwarding"),
    idxTrace = col("trace");

  const filtered = rows.filter((r) => {
    if (!filter) return true;
    const okPred = filter.predictor ? r[idxPred] === filter.predictor : true;
    const okFwd = filter.forwarding ? r[idxFwd] === filter.forwarding : true;
    const okTr = filter.trace ? r[idxTrace]?.includes(filter.trace) : true;
    return okPred && okFwd && okTr;
  });

  return (
    <div className="rounded-2xl border border-gray-200 dark:border-gray-700 overflow-hidden">
      <div className="px-4 py-2 bg-gray-50 dark:bg-gray-800/60 text-sm font-medium">
        Run Summaries (results_run.csv)
      </div>
      <div className="overflow-auto max-h-[320px]">
        <table className="min-w-full text-sm">
          <thead className="sticky top-0 bg-white dark:bg-gray-900">
            <tr>
              {header.map((h) => (
                <th
                  key={h}
                  className="px-3 py-2 border-b border-gray-200 dark:border-gray-700 text-left text-gray-600 dark:text-gray-300"
                >
                  {h}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {filtered.map((r, i) => (
              <tr
                key={i}
                className="odd:bg-gray-50/40 dark:odd:bg-gray-800/40"
              >
                {r.map((cell, j) => (
                  <td
                    key={j}
                    className="px-3 py-1 border-b border-gray-100 dark:border-gray-800 text-gray-700 dark:text-gray-200"
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

function Panel({ title, onPickTimeline, onPickResults, timeline }) {
  const [pngRef] = useState({ current: null });
  const derived = useMemo(
    () => deriveMetricsFromTimeline(timeline),
    [timeline]
  );

  const onExportCSV = () => {
    if (!timeline) return;
    const raw = [
      timeline.header.join(","),
      ...timeline.rows.map((r) => r.join(",")),
    ].join("\n"); // <-- fixed newline
    const blob = new Blob([raw], { type: "text/csv" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = `${title || "timeline"}.csv`;
    a.click();
  };

  return (
    <div className="flex flex-col gap-4">
      <div className="flex flex-wrap items-center gap-2">
        <FilePicker
          label="Load Timeline CSV"
          onFile={(txt) => onPickTimeline(parseCSVText(txt))}
        />
        <FilePicker
          label="Load results_run.csv (optional)"
          onFile={(txt) => onPickResults(txt)}
        />
        <button
          className="px-3 py-1.5 rounded-md bg-gray-100 dark:bg-gray-800"
          onClick={onExportCSV}
        >
          Export CSV
        </button>
        <button
          className="px-3 py-1.5 rounded-md bg-gray-100 dark:bg-gray-800"
          onClick={() => pngRef.current && pngRef.current()}
        >
          Export PNG
        </button>
      </div>

      <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
        <StatCard title="Cycles" value={derived.cycles} />
        <StatCard title="Retired" value={derived.retired} />
        <StatCard
          title="CPI"
          value={derived.cpi ? derived.cpi.toFixed(2) : "-"}
        />
      </div>

      <TimelineGrid title={title} timeline={timeline} onExportPNGRef={pngRef} />
    </div>
  );
}

export default function App() {
  const [dark, setDark] = useState(false);
  const [mode, setMode] = useState("single"); // single | compare

  const [timelineA, setTimelineA] = useState(null);
  const [timelineB, setTimelineB] = useState(null);
  const [resultsText, setResultsText] = useState("");

  useEffect(() => {
    document.documentElement.classList.toggle("dark", dark);
  }, [dark]);

  return (
    <div className="min-h-screen bg-white dark:bg-gray-950 text-gray-900 dark:text-gray-50 p-4 md:p-6">
      <div className="max-w-7xl mx-auto flex flex-col gap-4">
        <div className="flex items-center justify-between">
          <div className="text-xl font-bold">
            🚀 CPU Pipeline Timeline Viewer (CSV from C++)
          </div>
          <div className="flex items-center gap-4">
            <Toggle checked={dark} onChange={setDark} label={dark ? "Dark" : "Light"} />
            <div className="flex items-center gap-2">
              <span className="text-sm">Mode</span>
              <select
                className="border rounded-md px-2 py-1 bg-white dark:bg-gray-900"
                value={mode}
                onChange={(e) => setMode(e.target.value)}
              >
                <option value="single">Single</option>
                <option value="compare">Compare (A vs B)</option>
              </select>
            </div>
          </div>
        </div>

        {mode === "single" ? (
          <Panel
            title="Timeline"
            onPickTimeline={setTimelineA}
            onPickResults={setResultsText}
            timeline={timelineA}
          />
        ) : (
          <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
            <Panel
              title="Timeline A"
              onPickTimeline={setTimelineA}
              onPickResults={setResultsText}
              timeline={timelineA}
            />
            <Panel
              title="Timeline B"
              onPickTimeline={setTimelineB}
              onPickResults={setResultsText}
              timeline={timelineB}
            />
          </div>
        )}

        <SummaryPanel resultsCSVText={resultsText} />

        <div className="text-xs text-gray-500 dark:text-gray-400 mt-6">
          Usage: Run your C++ simulator to generate CSVs (e.g.,
          <code> data/timeline_*.csv </code> and
          <code> data/results_run.csv</code>), then click “Load Timeline CSV”
          (and optionally “Load results_run.csv”). WB-derived retired count
          drives CPI here.
        </div>
      </div>
    </div>
  );
}
