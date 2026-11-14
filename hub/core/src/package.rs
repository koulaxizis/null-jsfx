//! Downloads from our GitHub releases (only from there) and unpacks parts of the release zips.

use std::collections::BTreeMap;
use std::fs::{self, File};
use std::io::{self, Read, Write};
use std::path::{Path, PathBuf};

/// Where releases live. NULL_JSFX_HUB_RELEASES points it elsewhere (tests use a local server
/// with the same layout: <base>/latest/download/<file> and <base>/download/<tag>/<file>).
pub fn releases_base() -> String {
    std::env::var("NULL_JSFX_HUB_RELEASES")
        .unwrap_or_else(|_| "https://github.com/koulaxizis/null-jsfx/releases".into())
        .trim_end_matches('/')
        .to_string()
}

pub fn catalog_url() -> String {
    format!("{}/latest/download/NULL-JSFX-Catalog.json", releases_base())
}

pub fn asset_url(version: &str, file: &str) -> String {
    format!("{}/download/{}/{}", releases_base(), version, file.replace(' ', "%20"))
}

fn agent() -> ureq::Agent {
    ureq::Agent::config_builder()
        .timeout_connect(Some(std::time::Duration::from_secs(20)))
        .user_agent(concat!("NULL-JSFX-Hub/", env!("CARGO_PKG_VERSION")))
        // the computer's own certificates, so it works behind company proxies too
        .tls_config(ureq::tls::TlsConfig::builder().root_certs(ureq::tls::RootCerts::PlatformVerifier).build())
        .max_redirects(0)
        .http_status_as_error(false)
        .build()
        .into()
}

/// scheme://host[:port] of an address
fn origin(url: &str) -> &str {
    let start = url.find("://").map(|i| i + 3).unwrap_or(0);
    let end = url[start..].find(['/', '?', '#']).map(|j| start + j).unwrap_or(url.len());
    &url[..end]
}

fn host(url: &str) -> &str {
    let o = origin(url);
    let h = o.split_once("://").map(|(_, h)| h).unwrap_or(o);
    let h = h.rsplit_once('@').map(|(_, h)| h).unwrap_or(h);
    h.split(':').next().unwrap_or(h)
}

/// Where a download may go: GitHub's own hosts over HTTPS, or where the releases are (a test
/// server when NULL_JSFX_HUB_RELEASES says so).
pub fn allowed(url: &str) -> bool {
    if origin(url) == origin(&releases_base()) {
        return true;
    }
    let h = host(url).to_ascii_lowercase();
    url.starts_with("https://")
        && !origin(url).contains('@')
        && (h == "github.com" || h.ends_with(".github.com") || h == "githubusercontent.com" || h.ends_with(".githubusercontent.com"))
}

/// GET with redirects followed by hand, without reading the redirect pages (GitHub sends the
/// release files from another host, and some proxies garble those small pages).
fn get(url: &str, range: Option<(u64, u64)>) -> Result<ureq::http::Response<ureq::Body>, String> {
    let mut at = url.to_string();
    for _ in 0..8 {
        let mut req = agent().get(&at);
        if let Some((start, end)) = range {
            req = req.header("Range", &format!("bytes={}-{}", start, end.saturating_sub(1))).header("Accept-Encoding", "identity");
        }
        let res = req.call().map_err(|e| net_error(url, e))?;
        let status = res.status().as_u16();
        if (300..400).contains(&status) {
            let next = res.headers().get("location").and_then(|v| v.to_str().ok()).ok_or_else(|| format!("{url}: redirect without a target"))?;
            at = if next.starts_with("http://") || next.starts_with("https://") {
                next.to_string()
            } else if next.starts_with('/') && !next.starts_with("//") {
                format!("{}{}", origin(&at), next)
            } else {
                return Err(format!("{url}: redirect to an unexpected place"));
            };
            if !allowed(&at) {
                return Err(format!("{url}: redirect to {} refused (only GitHub over HTTPS)", host(&at)));
            }
            continue;
        }
        if status == 404 {
            return Err(format!("Not found: {url}"));
        }
        if status >= 400 {
            return Err(format!("{} answered {status} for {url}", url.split('/').nth(2).unwrap_or(url)));
        }
        return Ok(res);
    }
    Err(format!("{url}: too many redirects"))
}

/// GET a small text file (the catalog). Sends nothing about the user: no id, no list of plugins.
pub fn fetch_text(url: &str) -> Result<String, String> {
    let mut res = get(url, None)?;
    res.body_mut().with_config().limit(32 << 20).read_to_string().map_err(|e| net_error(url, e))
}

fn net_error(url: &str, e: ureq::Error) -> String {
    format!("Could not reach {}: {e}", url.split('/').nth(2).unwrap_or(url))
}

/// The SHA-256 of a file, lowercase hex.
pub fn sha256_file(path: &Path) -> Option<String> {
    use sha2::Digest;
    let mut file = File::open(path).ok()?;
    let mut h = sha2::Sha256::new();
    let mut buf = vec![0u8; 1 << 16];
    loop {
        match file.read(&mut buf) {
            Ok(0) => break,
            Ok(n) => h.update(&buf[..n]),
            Err(_) => return None,
        }
    }
    Some(hex(&h.finalize()))
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

/// Downloads `url` to `dest` unless it is already there, through a .part file so a broken
/// download is never taken for a whole one. `progress(done, total)` follows the bytes. With
/// `sha256` (the release's checksum of the file) a copy that does not match is never kept: a
/// cached one is fetched again, a new one is refused.
pub fn download(url: &str, dest: &Path, sha256: Option<&str>, progress: &mut dyn FnMut(u64, u64)) -> Result<PathBuf, String> {
    use sha2::Digest;
    if dest.is_file() {
        if sha256.is_none_or(|want| sha256_file(dest).as_deref() == Some(want)) {
            return Ok(dest.to_path_buf());
        }
        let _ = fs::remove_file(dest);
    }
    if let Some(dir) = dest.parent() {
        fs::create_dir_all(dir).map_err(|e| format!("{}: {e}", dir.display()))?;
    }
    let res = get(url, None)?;
    let total = res.headers().get("content-length").and_then(|v| v.to_str().ok()).and_then(|v| v.parse().ok()).unwrap_or(0);
    let mut body = res.into_body().into_with_config().limit(u64::MAX).reader();
    let part = dest.with_extension("part");
    let mut out = File::create(&part).map_err(|e| format!("{}: {e}", part.display()))?;
    let mut buf = vec![0u8; 1 << 16];
    let mut done = 0u64;
    let mut sum = sha2::Sha256::new();
    loop {
        let n = body.read(&mut buf).map_err(|e| format!("Download of {url} broke off: {e}"))?;
        if n == 0 {
            break;
        }
        sum.update(&buf[..n]);
        out.write_all(&buf[..n]).map_err(|e| format!("{}: {e}", part.display()))?;
        done += n as u64;
        progress(done, total);
    }
    drop(out);
    if total > 0 && done != total {
        let _ = fs::remove_file(&part);
        return Err(format!("Download of {url} was cut short ({done} of {total} bytes)"));
    }
    if let Some(want) = sha256 {
        if hex(&sum.finalize()) != want {
            let _ = fs::remove_file(&part);
            return Err(format!("The download of {} does not match the release's checksum; nothing was changed. Try again later.", dest.file_name().unwrap_or_default().to_string_lossy()));
        }
    }
    fs::rename(&part, dest).map_err(|e| format!("{}: {e}", dest.display()))?;
    Ok(dest.to_path_buf())
}

/// True for a path inside a package entry that stays below where it is unpacked: plain names
/// only, on every system (no "..", no "C:" or other drive, no leading slash, no empty parts).
pub fn safe_rel(rel: &str) -> bool {
    rel.is_empty()
        || rel.split('/').all(|c| !c.is_empty() && c != "." && c != ".." && !c.contains([':', '\\', '\0']))
}

/// A release zip, with entry names normalised: forward slashes and without the top folder
/// (NULL-JSFX-JSFX/..., NULL-JSFX-VST3-CLAP-Windows/...), whichever tool zipped it.
pub struct Package {
    zip: zip::ZipArchive<File>,
    names: BTreeMap<String, usize>,
}

impl Package {
    pub fn open(path: &Path) -> Result<Package, String> {
        let file = File::open(path).map_err(|e| format!("{}: {e}", path.display()))?;
        let zip = zip::ZipArchive::new(file).map_err(|e| format!("{} is damaged ({e}); it will be downloaded again", path.display()))?;
        let mut names = BTreeMap::new();
        for i in 0..zip.len() {
            let raw = zip.name_for_index(i).and_then(|r| r.ok()).map(|n| n.to_string()).unwrap_or_default().replace('\\', "/");
            if let Some((_, rest)) = raw.split_once('/') {
                if !rest.is_empty() {
                    names.insert(rest.trim_end_matches('/').to_string(), i);
                }
            }
        }
        Ok(Package { zip, names })
    }

    pub fn contains(&self, path: &str) -> bool {
        let prefix = format!("{path}/");
        self.names.contains_key(path) || self.names.range(prefix.clone()..).next().is_some_and(|(k, _)| k.starts_with(&prefix))
    }

    /// Unpacks `path` (a file, or a folder such as a .vst3 bundle) to `dest`. Returns false when
    /// the package has no such entry.
    pub fn extract(&mut self, path: &str, dest: &Path) -> Result<bool, String> {
        let prefix = format!("{path}/");
        let entries: Vec<(String, usize)> = self
            .names
            .iter()
            .filter(|(k, _)| *k == path || k.starts_with(&prefix))
            .map(|(k, i)| (k.clone(), *i))
            .collect();
        if entries.is_empty() {
            return Ok(false);
        }
        for (name, i) in entries {
            let rel = &name[path.len()..].trim_start_matches('/');
            if !safe_rel(rel) {
                return Err(format!("unsafe path in package: {name}"));
            }
            let target = if rel.is_empty() { dest.to_path_buf() } else { dest.join(rel) };
            let mut entry = self.zip.by_index(i).map_err(|e| e.to_string())?;
            if entry.is_dir() {
                fs::create_dir_all(&target).map_err(|e| format!("{}: {e}", target.display()))?;
                continue;
            }
            if let Some(dir) = target.parent() {
                fs::create_dir_all(dir).map_err(|e| format!("{}: {e}", dir.display()))?;
            }
            let mut out = File::create(&target).map_err(|e| format!("{}: {e}", target.display()))?;
            io::copy(&mut entry, &mut out).map_err(|e| format!("{}: {e}", target.display()))?;
            #[cfg(unix)]
            {
                use std::os::unix::fs::PermissionsExt;
                if let Some(mode) = entry.unix_mode() {
                    let _ = fs::set_permissions(&target, fs::Permissions::from_mode(mode & 0o777 | 0o600));
                }
            }
        }
        Ok(true)
    }

    /// The files of `path` (a file or a bundle folder): (path inside it, CRC-32, size).
    pub fn files(&mut self, path: &str) -> Vec<(String, u32, u64)> {
        let prefix = format!("{path}/");
        let list: Vec<(String, usize)> =
            self.names.iter().filter(|(k, _)| *k == path || k.starts_with(&prefix)).map(|(k, i)| (k.clone(), *i)).collect();
        list.into_iter()
            .filter_map(|(name, i)| {
                let e = self.zip.by_index_raw(i).ok()?;
                (!e.is_dir()).then(|| (name[path.len()..].trim_start_matches('/').to_string(), e.crc32(), e.size()))
            })
            .collect()
    }

    pub fn read_text(&mut self, path: &str) -> Option<String> {
        let i = *self.names.get(path)?;
        let mut s = String::new();
        self.zip.by_index(i).ok()?.read_to_string(&mut s).ok()?;
        Some(s)
    }

    /// The entries directly under `dir` (one level).
    pub fn list(&self, dir: &str) -> Vec<String> {
        let prefix = format!("{dir}/");
        let mut out: Vec<String> = self
            .names
            .keys()
            .filter_map(|k| k.strip_prefix(&prefix))
            .map(|rest| rest.split('/').next().unwrap_or(rest).to_string())
            .collect();
        out.dedup();
        out
    }
}

/// A release zip read over HTTP range requests: only the central directory and the entries
/// asked for are downloaded, so updating one VST3 does not fetch the whole ~250 MB package.
pub struct RemoteZip {
    url: String,
    pub size: u64,
    entries: BTreeMap<String, RemoteEntry>,
}

#[derive(Clone, Debug)]
struct RemoteEntry {
    method: u16,
    crc: u32,
    comp: u64,
    size: u64,
    offset: u64,
    mode: Option<u32>,
    dir: bool,
}

fn le16(b: &[u8], i: usize) -> u64 {
    u16::from_le_bytes([b[i], b[i + 1]]) as u64
}
fn le32(b: &[u8], i: usize) -> u64 {
    u32::from_le_bytes(b[i..i + 4].try_into().unwrap()) as u64
}
fn le64(b: &[u8], i: usize) -> u64 {
    u64::from_le_bytes(b[i..i + 8].try_into().unwrap())
}

/// GET bytes [start, end) of `url`; None when the server ignores ranges.
fn range(url: &str, start: u64, end: u64) -> Result<Option<(Vec<u8>, u64)>, String> {
    let res = get(url, Some((start, end)))?;
    if res.status() != 206 {
        return Ok(None);
    }
    let total = res
        .headers()
        .get("content-range")
        .and_then(|v| v.to_str().ok())
        .and_then(|v| v.rsplit('/').next())
        .and_then(|v| v.parse().ok())
        .unwrap_or(0);
    let mut body = Vec::new();
    res.into_body()
        .into_with_config()
        .limit(u64::MAX)
        .reader()
        .read_to_end(&mut body)
        .map_err(|e| format!("Download of {url} broke off: {e}"))?;
    if body.len() as u64 != end - start {
        return Err(format!("Download of {url} was cut short"));
    }
    Ok(Some((body, total)))
}

impl RemoteZip {
    /// Reads the zip's directory. Ok(None) when the server can't send parts of the file.
    pub fn open(url: &str) -> Result<Option<RemoteZip>, String> {
        let Some((_, size)) = range(url, 0, 1)? else { return Ok(None) };
        if size < 22 {
            return Ok(None);
        }
        let tail_start = size.saturating_sub(65_557);
        let (tail, _) = range(url, tail_start, size)?.ok_or("range requests stopped working")?;
        let eocd = (0..=tail.len() - 22)
            .rev()
            .find(|&i| tail[i..i + 4] == [0x50, 0x4b, 0x05, 0x06])
            .ok_or("not a zip file")?;
        let (mut cd_size, mut cd_off) = (le32(&tail, eocd + 12), le32(&tail, eocd + 16));
        if (cd_size == 0xFFFF_FFFF || cd_off == 0xFFFF_FFFF) && eocd >= 20 && tail[eocd - 20..eocd - 16] == [0x50, 0x4b, 0x06, 0x07] {
            let at = le64(&tail, eocd - 20 + 8);
            let (z64, _) = range(url, at, at + 56)?.ok_or("range requests stopped working")?;
            cd_size = le64(&z64, 40);
            cd_off = le64(&z64, 48);
        }
        // a directory that does not fit in the file (or is absurdly big) is a damaged package
        let cd_end = cd_off.checked_add(cd_size).filter(|&e| e <= size && cd_size <= 64 << 20).ok_or("damaged package (directory)")?;
        let (cd, _) = range(url, cd_off, cd_end)?.ok_or("range requests stopped working")?;
        let mut entries = BTreeMap::new();
        let mut i = 0usize;
        while i + 46 <= cd.len() && cd[i..i + 4] == [0x50, 0x4b, 0x01, 0x02] {
            let made_by = le16(&cd, i + 4) >> 8;
            let method = le16(&cd, i + 10) as u16;
            let crc = le32(&cd, i + 16) as u32;
            let mut comp = le32(&cd, i + 20);
            let mut usize_ = le32(&cd, i + 24);
            let (nlen, xlen, clen) = (le16(&cd, i + 28) as usize, le16(&cd, i + 30) as usize, le16(&cd, i + 32) as usize);
            if i + 46 + nlen + xlen + clen > cd.len() {
                return Err("damaged package (directory)".into());
            }
            let attrs = le32(&cd, i + 38);
            let mut offset = le32(&cd, i + 42);
            let name = String::from_utf8_lossy(&cd[i + 46..i + 46 + nlen]).replace('\\', "/");
            // zip64: the 0xFFFFFFFF fields come in the 0x0001 extra field, in this order
            let mut x = i + 46 + nlen;
            let xend = x + xlen;
            while x + 4 <= xend {
                let (id, len) = (le16(&cd, x), le16(&cd, x + 2) as usize);
                if x + 4 + len > xend {
                    break;
                }
                if id == 1 {
                    let mut p = x + 4;
                    for field in [&mut usize_, &mut comp, &mut offset] {
                        if *field == 0xFFFF_FFFF && p + 8 <= x + 4 + len {
                            *field = le64(&cd, p);
                            p += 8;
                        }
                    }
                }
                x += 4 + len;
            }
            if let Some((_, rest)) = name.split_once('/') {
                let dir = rest.ends_with('/') || rest.is_empty();
                let rest = rest.trim_end_matches('/');
                if !rest.is_empty() {
                    let mode = (made_by == 3).then_some((attrs >> 16) as u32).filter(|m| *m != 0);
                    entries.insert(rest.to_string(), RemoteEntry { method, crc, comp, size: usize_, offset, mode, dir });
                }
            }
            i += 46 + nlen + xlen + clen;
        }
        Ok(Some(RemoteZip { url: url.to_string(), size, entries }))
    }

    fn under<'a>(&'a self, path: &'a str) -> impl Iterator<Item = (&'a String, &'a RemoteEntry)> + 'a {
        let prefix = format!("{path}/");
        self.entries.iter().filter(move |(k, _)| *k == path || k.starts_with(&prefix))
    }

    pub fn contains(&self, path: &str) -> bool {
        self.under(path).next().is_some()
    }

    /// The compressed bytes `path` needs (what `extract` downloads, roughly).
    pub fn bytes_for(&self, path: &str) -> u64 {
        self.under(path).map(|(_, e)| e.comp + 30 + 256).sum()
    }

    /// Like `Package::files`, from the zip's directory alone.
    pub fn files(&self, path: &str) -> Vec<(String, u32, u64)> {
        self.under(path).filter(|(_, e)| !e.dir).map(|(k, e)| (k[path.len()..].trim_start_matches('/').to_string(), e.crc, e.size)).collect()
    }

    /// Like `Package::extract`, downloading only this file or folder.
    pub fn extract(&self, path: &str, dest: &Path) -> Result<bool, String> {
        let list: Vec<(String, RemoteEntry)> = self.under(path).map(|(k, e)| (k.clone(), e.clone())).collect();
        if list.is_empty() {
            return Ok(false);
        }
        let files: Vec<&(String, RemoteEntry)> = list.iter().filter(|(_, e)| !e.dir).collect();
        // the entries of one bundle sit together in the zip: fetch them in one go
        let start = files.iter().map(|(_, e)| e.offset).min().unwrap_or(0);
        let end = files.iter().map(|(_, e)| e.offset.saturating_add(30 + 1024).saturating_add(e.comp)).max().unwrap_or(0).min(self.size);
        let block = if files.is_empty() { Vec::new() } else { range(&self.url, start, end)?.ok_or("range requests stopped working")?.0 };
        for (name, e) in &list {
            let rel = name[path.len()..].trim_start_matches('/');
            if !safe_rel(rel) {
                return Err(format!("unsafe path in package: {name}"));
            }
            let target = if rel.is_empty() { dest.to_path_buf() } else { dest.join(rel) };
            if e.dir {
                fs::create_dir_all(&target).map_err(|err| format!("{}: {err}", target.display()))?;
                continue;
            }
            let at = (e.offset - start) as usize; // start is the smallest offset
            if block.len() < at + 30 || block[at..at + 4] != [0x50, 0x4b, 0x03, 0x04] {
                return Err(format!("{name}: damaged package"));
            }
            let data_at = at + 30 + le16(&block, at + 26) as usize + le16(&block, at + 28) as usize;
            let data = block.get(data_at..data_at.saturating_add(e.comp as usize)).ok_or_else(|| format!("{name}: damaged package"))?;
            let bytes = match e.method {
                0 => data.to_vec(),
                8 => {
                    let mut out = Vec::with_capacity((e.size as usize).min(64 << 20));
                    // never more than the directory says (a damaged entry cannot fill the memory)
                    flate2::read::DeflateDecoder::new(data).take(e.size.saturating_add(1)).read_to_end(&mut out).map_err(|err| format!("{name}: {err}"))?;
                    out
                }
                m => return Err(format!("{name}: unsupported compression {m}")),
            };
            let mut crc = flate2::Crc::new();
            crc.update(&bytes);
            if crc.sum() != e.crc || bytes.len() as u64 != e.size {
                return Err(format!("{name}: download damaged (checksum)"));
            }
            if let Some(dir) = target.parent() {
                fs::create_dir_all(dir).map_err(|err| format!("{}: {err}", dir.display()))?;
            }
            fs::write(&target, &bytes).map_err(|err| format!("{}: {err}", target.display()))?;
            #[cfg(unix)]
            if let Some(mode) = e.mode {
                use std::os::unix::fs::PermissionsExt;
                let _ = fs::set_permissions(&target, fs::Permissions::from_mode(mode & 0o777 | 0o600));
            }
        }
        Ok(true)
    }
}

/// A release package: the whole zip on disk, or parts of it over the network.
pub enum Source {
    Local(Package),
    Remote(RemoteZip),
}

impl Source {
    pub fn extract(&mut self, path: &str, dest: &Path) -> Result<bool, String> {
        match self {
            Source::Local(p) => p.extract(path, dest),
            Source::Remote(r) => r.extract(path, dest),
        }
    }

    /// The files of `path` with their CRC-32 and size, as the release has them.
    pub fn files(&mut self, path: &str) -> Vec<(String, u32, u64)> {
        match self {
            Source::Local(p) => p.files(path),
            Source::Remote(r) => r.files(path),
        }
    }
}

/// True when the file at `path` has this size and CRC-32.
pub fn file_matches(path: &Path, crc: u32, size: u64) -> bool {
    if fs::metadata(path).map(|m| !m.is_file() || m.len() != size).unwrap_or(true) {
        return false;
    }
    let Ok(mut file) = File::open(path) else { return false };
    let mut sum = flate2::Crc::new();
    let mut buf = vec![0u8; 1 << 16];
    loop {
        match file.read(&mut buf) {
            Ok(0) => break,
            Ok(n) => sum.update(&buf[..n]),
            Err(_) => return false,
        }
    }
    sum.sum() == crc
}

#[cfg(test)]
mod tests {
    use super::safe_rel;

    #[test]
    fn downloads_stay_on_github_over_https() {
        use super::allowed;
        assert!(allowed("https://github.com/koulaxizis/null-jsfx/releases/download/3.3/a.zip"));
        assert!(allowed("https://release-assets.githubusercontent.com/github-production-release-asset/1?x=y"));
        assert!(allowed("https://objects.githubusercontent.com/a"));
        for bad in [
            "http://github.com/a",
            "https://evil.example/a",
            "https://github.com.evil.example/a",
            "https://evilgithubusercontent.com/a",
            "https://github.com@evil.example/a",
            "ftp://github.com/a",
        ] {
            assert!(!allowed(bad), "{bad}");
        }
    }

    #[test]
    fn only_plain_names_inside_a_package() {
        for ok in ["", "a", "Contents/x64-win/null_jsfx_tilt.vst3", "a b/c.d"] {
            assert!(safe_rel(ok), "{ok}");
        }
        for bad in ["..", "a/../b", "/etc/passwd", "C:/Users/x/Startup/a.bat", "a/C:x", "a//b", "./a", "a\\b", "a/"] {
            assert!(!safe_rel(bad), "{bad}");
        }
    }
}
