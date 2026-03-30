const REPO = "doralteres/jimmy";
const KOFI_URL = "https://ko-fi.com/doralteres";
const DOWNLOAD_URL = `https://github.com/${REPO}/releases/latest`;
const GITHUB_URL = `https://github.com/${REPO}`;

const features = [
  {
    title: "Auto-Scrolling Teleprompter",
    description:
      "Lyrics and chords scroll in real-time, perfectly synced to your DAW timeline during playback or recording.",
    icon: "🎤",
  },
  {
    title: "MIDI Chord Detection",
    description:
      "Route your chord track to Jimmy and it identifies chords automatically — no manual entry needed.",
    icon: "🎹",
  },
  {
    title: "Lyrics Editor & Sections",
    description:
      "Write lyrics, define verse/chorus/bridge sections, and map each line to specific bar positions.",
    icon: "📝",
  },
  {
    title: "Hebrew & RTL Support",
    description:
      "Automatically detects and right-aligns Hebrew text. Full Unicode support out of the box.",
    icon: "🌐",
  },
  {
    title: "Zero Latency",
    description:
      "Produces no audio and adds zero CPU load. It's purely a display tool — your mix stays untouched.",
    icon: "⚡",
  },
  {
    title: "Saved With Project",
    description:
      "All lyrics, chords, and sections persist inside your Cubase project file. Nothing to manage externally.",
    icon: "💾",
  },
];

function App() {
  return (
    <div className="min-h-screen font-sans">
      {/* Nav */}
      <nav className="fixed top-0 z-50 w-full border-b border-white/5 bg-neutral-950/80 backdrop-blur-xl">
        <div className="mx-auto flex max-w-6xl items-center justify-between px-6 py-4">
          <a href="#" className="flex items-center gap-2.5 text-lg font-bold">
            <img
              src={`${import.meta.env.BASE_URL}favicon.svg`}
              alt=""
              className="h-7 w-7"
            />
            Jimmy
          </a>
          <div className="flex items-center gap-4">
            <a
              href={GITHUB_URL}
              target="_blank"
              rel="noopener noreferrer"
              className="text-sm text-neutral-400 transition hover:text-white"
            >
              GitHub
            </a>
            <a
              href={DOWNLOAD_URL}
              className="rounded-lg bg-purple-glow px-4 py-2 text-sm font-semibold transition hover:brightness-110"
            >
              Download
            </a>
          </div>
        </div>
      </nav>

      {/* Hero */}
      <section className="relative overflow-hidden pt-32 pb-20 sm:pt-40 sm:pb-28">
        {/* Glow */}
        <div className="pointer-events-none absolute inset-0 overflow-hidden">
          <div className="absolute top-[-20%] left-1/2 h-[600px] w-[900px] -translate-x-1/2 rounded-full bg-purple-glow/10 blur-[120px]" />
        </div>

        <div className="relative mx-auto max-w-4xl px-6 text-center">
          <div className="mb-6 inline-block rounded-full border border-purple-glow/30 bg-purple-glow/10 px-4 py-1.5 text-sm font-medium text-purple-300">
            Free &amp; Open Source VST3 Plugin
          </div>
          <h1 className="text-4xl leading-tight font-extrabold tracking-tight sm:text-6xl sm:leading-tight">
            Your lyrics &amp; chords,
            <br />
            <span className="bg-gradient-to-r from-purple-400 to-purple-glow bg-clip-text text-transparent">
              right inside your DAW.
            </span>
          </h1>
          <p className="mx-auto mt-6 max-w-2xl text-lg leading-relaxed text-neutral-400 sm:text-xl">
            Jimmy is a live teleprompter that syncs lyrics and chords to your
            timeline. Never lose your place on stage or in the studio again.
          </p>
          <div className="mt-10 flex flex-col items-center justify-center gap-4 sm:flex-row">
            <a
              href={DOWNLOAD_URL}
              className="inline-flex items-center gap-2 rounded-xl bg-purple-glow px-8 py-3.5 text-base font-semibold shadow-lg shadow-purple-glow/25 transition hover:brightness-110"
            >
              <svg
                className="h-5 w-5"
                fill="none"
                stroke="currentColor"
                strokeWidth={2}
                viewBox="0 0 24 24"
              >
                <path
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  d="M4 16v2a2 2 0 002 2h12a2 2 0 002-2v-2M7 10l5 5m0 0l5-5m-5 5V3"
                />
              </svg>
              Download Latest Release
            </a>
            <a
              href={KOFI_URL}
              target="_blank"
              rel="noopener noreferrer"
              className="inline-flex items-center gap-2 rounded-xl border border-white/10 bg-white/5 px-8 py-3.5 text-base font-semibold transition hover:bg-white/10"
            >
              ☕ Buy Me a Coffee
            </a>
          </div>
        </div>
      </section>

      {/* Screenshots */}
      <section id="screenshots" className="py-16 sm:py-24">
        <div className="mx-auto max-w-6xl px-6">
          <h2 className="mb-4 text-center text-3xl font-bold sm:text-4xl">
            See it in action
          </h2>
          <p className="mx-auto mb-14 max-w-xl text-center text-neutral-400">
            Two modes built for real workflows — Edit mode for preparation, Live
            mode for performance.
          </p>
          <div className="grid gap-10 lg:grid-cols-2">
            <div>
              <div className="screenshot-shadow overflow-hidden rounded-xl border border-white/10">
                <img
                  src={`${import.meta.env.BASE_URL}jimmy_live_mode.png`}
                  alt="Jimmy live mode showing lyrics and chords synced to the Cubase timeline"
                  className="w-full"
                  loading="lazy"
                />
              </div>
              <p className="mt-4 text-center text-sm font-medium text-neutral-400">
                <span className="text-purple-400">Live Mode</span> — Lyrics and
                chords scroll automatically with playback
              </p>
            </div>
            <div>
              <div className="screenshot-shadow overflow-hidden rounded-xl border border-white/10">
                <img
                  src={`${import.meta.env.BASE_URL}jimmy_edit_mode.png`}
                  alt="Jimmy edit mode showing the lyrics editor and section manager"
                  className="w-full"
                  loading="lazy"
                />
              </div>
              <p className="mt-4 text-center text-sm font-medium text-neutral-400">
                <span className="text-purple-400">Edit Mode</span> — Write
                lyrics, define sections, map bars
              </p>
            </div>
          </div>
        </div>
      </section>

      {/* Features */}
      <section id="features" className="py-16 sm:py-24">
        <div className="mx-auto max-w-6xl px-6">
          <h2 className="mb-4 text-center text-3xl font-bold sm:text-4xl">
            Everything you need on stage
          </h2>
          <p className="mx-auto mb-14 max-w-xl text-center text-neutral-400">
            Built by a musician, for musicians. No bloat — just the essentials
            done right.
          </p>
          <div className="grid gap-6 sm:grid-cols-2 lg:grid-cols-3">
            {features.map((f) => (
              <div
                key={f.title}
                className="rounded-2xl border border-white/5 bg-white/[0.02] p-6 transition hover:border-purple-glow/20 hover:bg-white/[0.04]"
              >
                <div className="mb-3 text-3xl">{f.icon}</div>
                <h3 className="mb-1 text-lg font-semibold">{f.title}</h3>
                <p className="text-sm leading-relaxed text-neutral-400">
                  {f.description}
                </p>
              </div>
            ))}
          </div>
        </div>
      </section>

      {/* CTA */}
      <section className="py-16 sm:py-24">
        <div className="mx-auto max-w-3xl px-6 text-center">
          <h2 className="text-3xl font-bold sm:text-4xl">
            Ready to ditch the music stand?
          </h2>
          <p className="mx-auto mt-4 max-w-lg text-neutral-400">
            Download Jimmy for free and get a teleprompter inside your DAW in
            minutes. macOS (Apple&nbsp;Silicon) supported, Windows coming soon.
          </p>
          <div className="mt-10 flex flex-col items-center justify-center gap-4 sm:flex-row">
            <a
              href={DOWNLOAD_URL}
              className="inline-flex items-center gap-2 rounded-xl bg-purple-glow px-8 py-3.5 text-base font-semibold shadow-lg shadow-purple-glow/25 transition hover:brightness-110"
            >
              <svg
                className="h-5 w-5"
                fill="none"
                stroke="currentColor"
                strokeWidth={2}
                viewBox="0 0 24 24"
              >
                <path
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  d="M4 16v2a2 2 0 002 2h12a2 2 0 002-2v-2M7 10l5 5m0 0l5-5m-5 5V3"
                />
              </svg>
              Download Latest Release
            </a>
            <a
              href={KOFI_URL}
              target="_blank"
              rel="noopener noreferrer"
              className="inline-flex items-center gap-2 rounded-xl border border-white/10 bg-white/5 px-8 py-3.5 text-base font-semibold transition hover:bg-white/10"
            >
              ☕ Buy Me a Coffee
            </a>
          </div>
        </div>
      </section>

      {/* Footer */}
      <footer className="border-t border-white/5 py-8">
        <div className="mx-auto flex max-w-6xl flex-col items-center justify-between gap-4 px-6 text-sm text-neutral-500 sm:flex-row">
          <span>
            © {new Date().getFullYear()} Jimmy — Open source under MIT License
          </span>
          <div className="flex gap-6">
            <a
              href={GITHUB_URL}
              target="_blank"
              rel="noopener noreferrer"
              className="transition hover:text-white"
            >
              GitHub
            </a>
            <a
              href={KOFI_URL}
              target="_blank"
              rel="noopener noreferrer"
              className="transition hover:text-white"
            >
              Ko-fi
            </a>
          </div>
        </div>
      </footer>
    </div>
  );
}

export default App;
