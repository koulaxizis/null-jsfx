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
    fs::write(&plan, serde_json::to_vec(ops).unwrap()).map_err(|e| e.to_string())?;
    let exe = std::env::current_exe().map_err(|e| e.to_string())?;
    let q = |p: &Path| p.to_string_lossy().replace('\'', "''");
    let script = format!(
        "$p = Start-Process -FilePath '{}' -ArgumentList '--apply-ops','\"{}\"' -Verb RunAs -Wait -PassThru -WindowStyle Hidden; exit $p.ExitCode",
        q(&exe),
        q(&plan)
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

/// The elevated side of `run_elevated`: `<hub> --apply-ops <plan.json>`. Writes "ok" or the
/// errors next to the plan and returns the exit code.
pub fn main_apply_ops(plan: &Path) -> i32 {
    let result = plan.with_extension("result");
    let ops: Vec<Op> = match fs::read(plan).ok().and_then(|b| serde_json::from_slice(&b).ok()) {
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
