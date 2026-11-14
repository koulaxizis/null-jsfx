//! Copying into and removing from the plugin folders. On Windows the VST3 and CLAP folders are
//! under Program Files, so the steps that need it run once more with administrator rights (the
//! Hub starts itself with --apply-ops; see `run_elevated`).

use serde::{Deserialize, Serialize};
use std::fs;
use std::io;
use std::path::{Path, PathBuf};

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(tag = "op", rename_all = "lowercase")]
pub enum Op {
    /// replace `to` (file or folder) with a copy of `from`
    Copy { from: PathBuf, to: PathBuf },
    /// delete `path` (file or folder) and then its parent folders that are left empty, up to `keep`
    Remove { path: PathBuf, keep: Option<PathBuf> },
}

pub struct Failure {
    pub op: Op,
    pub denied: bool,
    /// the file is open in another program (Windows: REAPER with the plugin loaded, or the
    /// system holding a font), so it can be done once that lets go of it
    pub busy: bool,
    pub error: String,
}

/// Runs the steps in order and returns the ones that failed.
pub fn apply(ops: &[Op]) -> Vec<Failure> {
    let mut out = Vec::new();
    for op in ops {
        if let Err(e) = apply_one(op) {
            out.push(Failure { op: op.clone(), denied: e.kind() == io::ErrorKind::PermissionDenied, busy: in_use(&e), error: e.to_string() });
        }
    }
    out
}

/// Windows' "being used by another process" (sharing or lock violation).
fn in_use(e: &io::Error) -> bool {
    cfg!(windows) && matches!(e.raw_os_error(), Some(32 | 33))
}

fn apply_one(op: &Op) -> io::Result<()> {
    match op {
        Op::Copy { from, to } => {
            if same_file(from, to) {
                return Ok(()); // already there: an update that didn't change it, or a locked font
            }
            if let Some(dir) = to.parent() {
                fs::create_dir_all(dir)?;
            }
            remove_any(to)?;
            copy_any(from, to)
        }
        Op::Remove { path, keep } => {
            remove_any(path)?;
            if let Some(keep) = keep {
                let mut dir = path.parent();
                while let Some(d) = dir {
                    if d == keep.as_path() || !d.starts_with(keep) || fs::remove_dir(d).is_err() {
                        break;
                    }
                    dir = d.parent();
                }
            }
            Ok(())
        }
    }
}

fn remove_any(path: &Path) -> io::Result<()> {
    match fs::symlink_metadata(path) {
        Ok(m) if m.is_dir() => fs::remove_dir_all(path),
        Ok(_) => fs::remove_file(path),
        Err(e) if e.kind() == io::ErrorKind::NotFound => Ok(()),
        Err(e) => Err(e),
    }
}

/// Both are files with the same bytes.
fn same_file(a: &Path, b: &Path) -> bool {
    match (fs::metadata(a), fs::metadata(b)) {
        (Ok(x), Ok(y)) if x.is_file() && y.is_file() && x.len() == y.len() => {
            matches!((fs::read(a), fs::read(b)), (Ok(p), Ok(q)) if p == q)
        }
        _ => false,
    }
}

fn copy_any(from: &Path, to: &Path) -> io::Result<()> {
    if from.is_dir() {
        fs::create_dir_all(to)?;
        for entry in fs::read_dir(from)? {
            let entry = entry?;
            copy_any(&entry.path(), &to.join(entry.file_name()))?;
        }
        Ok(())
    } else {
        fs::copy(from, to).map(|_| ())
    }
}

/// Runs `ops` in a second copy of this program with administrator rights (Windows: one UAC
/// prompt) and waits for it. The elevated copy only runs `main_apply_ops` and exits.
#[cfg(windows)]
pub fn run_elevated(ops: &[Op], work: &Path) -> Result<(), String> {
    use std::process::Command;
    fs::create_dir_all(work).map_err(|e| e.to_string())?;
    let plan = work.join("elevated-ops.json");
    let result = work.join("elevated-ops.result");
    let _ = fs::remove_file(&result);
    let bytes = serde_json::to_vec(ops).unwrap();
    fs::write(&plan, &bytes).map_err(|e| e.to_string())?;
    let exe = std::env::current_exe().map_err(|e| e.to_string())?;
    let q = |p: &Path| p.to_string_lossy().replace('\'', "''");
    // the elevated copy runs the plan only if it still has this checksum
    let script = format!(
        "$p = Start-Process -FilePath '{}' -ArgumentList '--apply-ops','\"{}\"','{}' -Verb RunAs -Wait -PassThru -WindowStyle Hidden; exit $p.ExitCode",
        q(&exe),
        q(&plan),
        plan_sum(&bytes)
    );
    use std::os::windows::process::CommandExt;
    let status = Command::new("powershell")
        .args(["-NoProfile", "-NonInteractive", "-Command", &script])
        .creation_flags(0x0800_0000) // CREATE_NO_WINDOW
        .status()
        .map_err(|e| e.to_string())?;
    let report = fs::read_to_string(&result).unwrap_or_default();
    let _ = fs::remove_file(&plan);
    let _ = fs::remove_file(&result);
    if status.success() && report.trim() == "ok" {
        Ok(())
    } else if report.trim().is_empty() {
        Err("Windows needs your permission to write to the VST3/CLAP folders in Program Files, and it was not given. \
             Try again and click Yes, or pick other VST3/CLAP folders in Settings."
            .into())
    } else {
        Err(report)
    }
}

#[cfg(not(windows))]
pub fn run_elevated(_ops: &[Op], _work: &Path) -> Result<(), String> {
    Err("No permission to write there. Pick a folder you own in Settings.".into())
}

/// The SHA-256 of a plan, in hex.
pub fn plan_sum(bytes: &[u8]) -> String {
    use sha2::{Digest, Sha256};
    Sha256::digest(bytes).iter().map(|b| format!("{b:02x}")).collect()
}

/// A plan the elevated copy may run: every path absolute and without "..", and every copy taken
/// from the folder the plan sits in (the Hub's staging folder).
pub fn plan_ok(ops: &[Op], staging: &Path) -> bool {
    let plain = |p: &Path| p.is_absolute() && !p.components().any(|c| matches!(c, std::path::Component::ParentDir));
    ops.iter().all(|op| match op {
        Op::Copy { from, to } => plain(from) && plain(to) && from.starts_with(staging),
        Op::Remove { path, keep } => plain(path) && keep.as_deref().map_or(true, plain),
    })
}

/// The elevated side of `run_elevated`: `<hub> --apply-ops <plan.json> <sha256>`. Runs the plan
/// only if it still has the checksum the Hub gave, writes "ok" or the errors next to the plan
/// and returns the exit code.
pub fn main_apply_ops(plan: &Path, sum: &str) -> i32 {
    let result = plan.with_extension("result");
    let staging = plan.parent().unwrap_or(Path::new(""));
    let ops: Vec<Op> = match fs::read(plan)
        .ok()
        .filter(|b| plan_sum(b) == sum.to_ascii_lowercase())
        .and_then(|b| serde_json::from_slice(&b).ok())
        .filter(|ops: &Vec<Op>| plan_ok(ops, staging))
    {
        Some(ops) => ops,
        None => {
            let _ = fs::write(&result, "could not read the list of steps");
            return 2;
        }
    };
    let failed = apply(&ops);
    if failed.is_empty() {
        let _ = fs::write(&result, "ok");
        0
    } else {
        let text: Vec<String> = failed.iter().map(|f| f.error.clone()).collect();
        let _ = fs::write(&result, text.join("\n"));
        1
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_elevated_copy_runs_only_the_plan_it_was_given() {
        let dir = std::env::temp_dir().join(format!("null-hub-plan-{}", std::process::id()));
        let _ = fs::remove_dir_all(&dir);
        fs::create_dir_all(dir.join("in")).unwrap();
        fs::write(dir.join("in").join("a.txt"), b"a").unwrap();
        let ops = vec![Op::Copy { from: dir.join("in").join("a.txt"), to: dir.join("out").join("a.txt") }];
        let plan = dir.join("elevated-ops.json");
        let bytes = serde_json::to_vec(&ops).unwrap();
        fs::write(&plan, &bytes).unwrap();
        // changed after the Hub wrote it: refused, nothing copied
        assert_eq!(main_apply_ops(&plan, &plan_sum(b"something else")), 2);
        assert!(!dir.join("out").exists());
        assert_eq!(main_apply_ops(&plan, &plan_sum(&bytes)), 0);
        assert_eq!(fs::read(dir.join("out").join("a.txt")).unwrap(), b"a");
        // a copy from outside the staging folder, or a path with "..", is refused
        let outside = vec![Op::Copy { from: std::env::temp_dir().join("x"), to: dir.join("y") }];
        assert!(!plan_ok(&outside, &dir));
        let up = vec![Op::Remove { path: dir.join("..").join("z"), keep: None }];
        assert!(!plan_ok(&up, &dir));
        assert!(plan_ok(&ops, &dir));
        let _ = fs::remove_dir_all(&dir);
    }

    #[test]
    fn copy_replaces_bundle_and_remove_prunes_empty_folders() {
        let t = tempfile::tempdir().unwrap();
        let src = t.path().join("src/x.vst3/Contents");
        fs::create_dir_all(&src).unwrap();
        fs::write(src.join("a"), "new").unwrap();
        let dest = t.path().join("VST3/NULL JSFX/x.vst3");
        fs::create_dir_all(dest.join("Contents")).unwrap();
        fs::write(dest.join("Contents/old"), "old").unwrap();
        assert!(apply(&[Op::Copy { from: t.path().join("src/x.vst3"), to: dest.clone() }]).is_empty());
        assert_eq!(fs::read_to_string(dest.join("Contents/a")).unwrap(), "new");
        assert!(!dest.join("Contents/old").exists(), "old bundle contents must go");
        let keep = t.path().join("VST3");
        assert!(apply(&[Op::Remove { path: dest.clone(), keep: Some(keep.clone()) }]).is_empty());
        assert!(!t.path().join("VST3/NULL JSFX").exists());
        assert!(keep.exists());
    }

    #[test]
    fn copy_of_the_same_bytes_leaves_the_file_alone() {
        let t = tempfile::tempdir().unwrap();
        let (a, b) = (t.path().join("a.ttf"), t.path().join("b.ttf"));
        fs::write(&a, "font").unwrap();
        fs::write(&b, "font").unwrap();
        let before = fs::metadata(&b).unwrap().modified().unwrap();
        std::thread::sleep(std::time::Duration::from_millis(20));
        assert!(apply(&[Op::Copy { from: a.clone(), to: b.clone() }]).is_empty());
        assert_eq!(fs::metadata(&b).unwrap().modified().unwrap(), before, "not rewritten");
    }

    /// A file another program holds open: the step fails as busy, not as an error to show raw.
    #[cfg(windows)]
    #[test]
    fn a_file_in_use_is_busy() {
        use std::os::windows::fs::OpenOptionsExt;
        let t = tempfile::tempdir().unwrap();
        let f = t.path().join("NotoSans-Regular.ttf");
        fs::write(&f, "font").unwrap();
        let held = fs::OpenOptions::new().read(true).share_mode(1).open(&f).unwrap(); // FILE_SHARE_READ
        let failed = apply(&[Op::Remove { path: f.clone(), keep: None }]);
        assert!(failed.len() == 1 && failed[0].busy, "{:?}", failed.first().map(|x| &x.error));
        drop(held);
        assert!(apply(&[Op::Remove { path: f.clone(), keep: None }]).is_empty());
    }
}
