//! Which DAWs are running. Windows can't replace or delete a VST3/CLAP file that a running DAW
//! has loaded, and the others only pick up the new files after a restart, so the Hub asks the
//! user to close them first.

/// The DAW this process name belongs to (lower case, without ".exe" and the folder).
pub fn host_name(process: &str) -> Option<&'static str> {
    let p = process.trim().to_ascii_lowercase();
    let p = p.rsplit(['/', '\\']).next().unwrap_or(&p);
    let p = p.strip_suffix(".exe").unwrap_or(p);
    const EXACT: &[(&str, &str)] = &[
        ("reaper", "REAPER"),
        ("live", "Ableton Live"), // macOS
        ("fl", "FL Studio"),
        ("fl64", "FL Studio"),
        ("carla", "Carla"),
        ("reason", "Reason"),
        ("renoise", "Renoise"),
        ("audacity", "Audacity"),
        ("zrythm", "Zrythm"),
        ("qtractor", "Qtractor"),
    ];
    const PREFIX: &[(&str, &str)] = &[
        ("ableton live", "Ableton Live"),
        ("fl studio", "FL Studio"),
        ("bitwig", "Bitwig Studio"),
        ("studio one", "Studio One"),
        ("cubase", "Cubase"),
        ("nuendo", "Nuendo"),
        ("ardour", "Ardour"),
        ("mixbus", "Mixbus"),
        ("waveform", "Waveform"),
        ("digital performer", "Digital Performer"),
        ("adobe audition", "Audition"),
    ];
    EXACT.iter().find(|(n, _)| *n == p).or_else(|| PREFIX.iter().find(|(n, _)| p.starts_with(n))).map(|(_, name)| *name)
}

/// The names of the DAWs running now (each once, in a stable order).
pub fn running() -> Vec<&'static str> {
    let mut out: Vec<&'static str> = processes().iter().filter_map(|p| host_name(p)).collect();
    out.sort_unstable();
    out.dedup();
    out
}

#[cfg(windows)]
fn processes() -> Vec<String> {
    use std::os::windows::process::CommandExt;
    let Ok(out) = std::process::Command::new("tasklist").args(["/fo", "csv", "/nh"]).creation_flags(0x0800_0000).output() else {
        return Vec::new();
    };
    // "reaper.exe","1234","Console","1","250,000 K"
    String::from_utf8_lossy(&out.stdout).lines().filter_map(|l| l.strip_prefix('"')?.split('"').next().map(str::to_string)).collect()
}

#[cfg(not(windows))]
fn processes() -> Vec<String> {
    let Ok(out) = std::process::Command::new("ps").args(["-A", "-o", "comm="]).output() else {
        return Vec::new();
    };
    String::from_utf8_lossy(&out.stdout).lines().map(str::to_string).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn knows_the_usual_daws_by_their_process_names() {
        assert_eq!(host_name("reaper.exe"), Some("REAPER"));
        assert_eq!(host_name("/Applications/REAPER.app/Contents/MacOS/REAPER"), Some("REAPER"));
        assert_eq!(host_name("Ableton Live 12 Suite.exe"), Some("Ableton Live"));
        assert_eq!(host_name("/Applications/Ableton Live 12 Suite.app/Contents/MacOS/Live"), Some("Ableton Live"));
        assert_eq!(host_name("FL64.exe"), Some("FL Studio"));
        assert_eq!(host_name("bitwig-studio"), Some("Bitwig Studio"));
        assert_eq!(host_name("Cubase14.exe"), Some("Cubase"));
        assert_eq!(host_name("Studio One.exe"), Some("Studio One"));
        assert_eq!(host_name("explorer.exe"), None);
        assert_eq!(host_name("liveness"), None);
        assert_eq!(host_name("flatpak"), None);
    }
}
