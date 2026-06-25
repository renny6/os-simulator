# **DOCUMENT 04 — UI/UX DESIGN BRIEF**

# **How the TUI looks and feels**

---

## **Aesthetic Direction**

Terminal-native. Looks like it belongs in a Linux system monitor, not a web app. Inspired by: htop, btop, tmux. Dense with information but visually organised. Every pixel of terminal space carries data. No decorative elements. Dark terminal background assumed (black or very dark grey). Never set background explicitly — inherit the user's terminal theme.

## **Colour Palette (ANSI terminal colours via FTXUI)**

Primary accent : Bright Cyan (ftxui::Color::Cyan1) — headers, borders, labels Page Hit : Bright Green (ftxui::Color::Green1) — hit flash, GRANTED labels Page Fault : Bright Red (ftxui::Color::Red1) — fault flash, DENIED labels Running process : Bright Green (ftxui::Color::Green1) — RUNNING state text Blocked process : Bright Yellow (ftxui::Color::Yellow1) — BLOCKED state text Ready process : White (ftxui::Color::White) — READY state text Terminated : Dark Grey (ftxui::Color::GrayDark) — TERMINATED state text Panel borders : Cyan (ftxui::Color::Cyan) — all panel box borders Warning/alert : Bright Yellow (ftxui::Color::Yellow1) — starvation counter high Priority boost : Bright Magenta (ftxui::Color::Magenta1) — \[^\] indicator

## **Typography (terminal has no font control — use these conventions)**

Headings : ALL CAPS \+ space separation e.g. ─── PHYSICAL MEMORY GRID ─── Labels : Title Case e.g. Hit Rate: Values : plain text or colour coded Monospace : all text is monospace by nature of terminal — no special handling Bold : use ftxui::bold() for currently running process PID and panel titles

## **Component Style**

All panels : bordered box using ftxui::border() Panel titles : centered at top of border using ftxui::text() with bold \+ cyan Gauges : ftxui::gauge() for hit rate and CPU utilisation Memory grid : ftxui::gridbox() with fixed-width cells Tables : ftxui::flexbox or manual column alignment with fixed widths Separators : ftxui::separator() between major layout regions Scrolling : PCB table scrolls if more than 8 processes (ftxui::vscroll\_indicator)

## **Layout Constraints**

Minimum terminal width : 120 columns Minimum terminal height : 36 rows At startup, query ftxui::Terminal::Size() and exit with clear error if below minimum. On resize events (FTXUI fires automatically), recalculate memory grid boxes per row: boxes\_per\_row \= floor(available\_right\_column\_width / 6\) (each box is "\[ N \]" \= 5 chars \+ 1 gap)

## **Animation Rules**

Page hit/fault flash : 3 render frames. Counter-based, not timer-based. Priority boost arrow \[^\]: shows for duration of the boost. Removed immediately on revert. Starvation counter high : PCB row border changes to yellow when starvation\_counter \> 10\. Belady's Anomaly label : appears in memory grid panel footer when detected. Text: "Belady's Anomaly: FIFO fault count increased with more frames — expected for this reference string."

## **Reference Inspiration**

btop++ : panel layout, colour discipline, dense-but-readable information htop : process table style, colour coding of process states tmux : border conventions, header/footer positioning

