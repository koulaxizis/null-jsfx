// The Hub's languages. The English text is the key; EL holds its Greek. tr("Remove {what}?",
// { what }) fills the {names}. In index.html, data-t marks an element whose text is a key, and
// data-t-placeholder / data-t-aria / data-t-title its placeholder, aria-label or title.
// Plugin names, taglines and guides come from the catalog and stay in English, like the site.
// hub/tests/ui_test.cjs checks that every key used has its Greek.
"use strict";

const EL = {
  // --- the page ----------------------------------------------------------------------------
  "Check for updates": "Έλεγχος για ενημερώσεις",
  "Look for a new release": "Αναζήτηση νέας έκδοσης",
  "Checking": "Έλεγχος",
  "Sections": "Ενότητες",
  "All plugins": "Όλα τα plugins",
  "Installed": "Εγκατεστημένα",
  "Updates": "Ενημερώσεις",
  "New in this release": "Νέα στην έκδοση",
  "Groups": "Κατηγορίες",
  "Extras": "Επιπλέον",
  "Release": "Έκδοση",
  "What's new": "Τι νέο υπάρχει",
  "Settings": "Ρυθμίσεις",
  "Support on Ko-fi": "Στήριξη στο Ko-fi",
  "Search plugins, e.g. eq, reverb, vocal": "Αναζήτηση plugins, π.χ. eq, reverb, φωνή",
  "Formats to install": "Μορφές για εγκατάσταση",
  "Install as": "Εγκατάσταση ως",
  "Offline": "Εκτός σύνδεσης",
  "Release {v}": "Έκδοση {v}",
  "{n} to update": "{n} για ενημέρωση",
  "up to date": "ενημερωμένα",
  "Update all ({n})": "Ενημέρωση όλων ({n})",
  "A new NULL JSFX Hub ({v}) is out.": "Βγήκε νέο NULL JSFX Hub ({v}).",
  "Update the Hub": "Ενημέρωση του Hub",
  "or download it": "ή κατέβασέ το",
  "NEW": "ΝΕΟ",
  "update available": "υπάρχει ενημέρωση",
  "kept at {v}": "κρατιέται στην {v}",
  "installed": "εγκατεστημένο",
  "not installed": "μη εγκατεστημένο",
  "Remove {name} (every installed format)": "Αφαίρεση του {name} (όλες οι εγκατεστημένες μορφές)",
  "Remove {name}": "Αφαίρεση του {name}",
  "Update": "Ενημέρωση",
  "Install": "Εγκατάσταση",
  "Install the {formats} version": "Εγκατάσταση της έκδοσης {formats}",
  " and ": " και ",
  "The Rack for any DAW, and the extras for REAPER. They follow your plugins: an FX chain is added once all its plugins are installed.":
    "Το Rack για κάθε DAW και τα επιπλέον για το REAPER. Ακολουθούν τα plugins σου: μια αλυσίδα FX μπαίνει μόλις εγκατασταθούν όλα τα plugins της.",
  "Plugins with a newer version in the latest release.": "Plugins με νεότερη εκδοχή στην τελευταία έκδοση.",
  "Plugins added in release {v}.": "Plugins που προστέθηκαν στην έκδοση {v}.",
  "Install {plugins} ({files})": "Εγκατάσταση: {plugins} ({files})",
  "Installs the formats picked on the left in every plugin shown that does not have them yet":
    "Εγκαθιστά τις μορφές που διάλεξες αριστερά σε κάθε plugin της λίστας που δεν τις έχει ακόμη",
  "Installed {what}": "Εγκαταστάθηκαν: {what}",
  "Remove {what}": "Αφαίρεση: {what}",
  "The Hub could not reach GitHub yet. Connect to the internet and click Check for updates.":
    "Το Hub δεν μπόρεσε ακόμη να συνδεθεί στο GitHub. Σύνδεσου στο internet και πάτα Έλεγχος για ενημερώσεις.",
  "No NULL JSFX plugins installed yet.": "Δεν υπάρχουν ακόμη εγκατεστημένα NULL JSFX plugins.",
  "Everything is up to date.": "Όλα είναι ενημερωμένα.",
  "No plugin matches that search.": "Κανένα plugin δεν ταιριάζει με την αναζήτηση.",
  "Pick at least one format.": "Διάλεξε τουλάχιστον μία μορφή.",

  // formats
  "REAPER, with REAPER's own sliders": "REAPER, με τα sliders του REAPER",
  "REAPER, with the NULL JSFX interface": "REAPER, με το περιβάλλον του NULL JSFX",
  "any DAW": "κάθε DAW",

  // counting
  "plugin": "plugin",
  "plugins": "plugins",
  "file": "αρχείο",
  "files": "αρχεία",
  "plugin file": "αρχείο plugin",
  "plugin files": "αρχεία plugins",
  "NULL JSFX plugin": "NULL JSFX plugin",
  "NULL JSFX plugins": "NULL JSFX plugins",

  // groups and extras (from the catalog)
  "One Slider": "Ένα Slider",
  "One control, instant result. Turn it up until it sounds right.": "Ένας έλεγχος, άμεσο αποτέλεσμα. Ανέβασέ τον μέχρι να ακούγεται σωστά.",
  "Essentials": "Βασικά",
  "The everyday mixing and mastering toolbox.": "Η καθημερινή εργαλειοθήκη για μίξη και mastering.",
  "Advanced": "Προχωρημένα",
  "Deeper sound design, creative effects and special processing.": "Βαθύτερος σχεδιασμός ήχου, δημιουργικά εφέ και ειδική επεξεργασία.",
  "FX chains": "Αλυσίδες FX",
  "Ready-made REAPER FX chains of NULL JSFX plugins. A chain is added when all its plugins are installed.":
    "Έτοιμες αλυσίδες FX του REAPER με NULL JSFX plugins. Μια αλυσίδα μπαίνει όταν εγκατασταθούν όλα τα plugins της.",
  "Factory presets": "Εργοστασιακά presets",
  "The factory presets in REAPER's preset menu of each JSFX and GFX version, added after your own presets. Turning this off takes out only the factory presets you did not change.":
    "Τα εργοστασιακά presets στο μενού presets του REAPER για κάθε εκδοχή JSFX και GFX, μετά από τα δικά σου. Αν το κλείσεις, φεύγουν μόνο τα εργοστασιακά presets που δεν άλλαξες.",
  "Tablet remote": "Χειριστήριο tablet",
  "Control REAPER's mixer and the NULL JSFX plugins from a tablet. Needs a few steps in REAPER after.":
    "Χειρισμός της μίξης του REAPER και των NULL JSFX plugins από tablet. Χρειάζεται μετά λίγα βήματα στο REAPER.",
  "Noto Sans font": "Γραμματοσειρά Noto Sans",
  "The font of the GFX versions, for their exact look.": "Η γραμματοσειρά των εκδοχών GFX, για την ακριβή τους εμφάνιση.",
  "update": "ενημέρωση",
  "on": "ενεργό",
  "off": "ανενεργό",
  "Turn off": "Απενεργοποίηση",
  "Turn on": "Ενεργοποίηση",
  "Setup steps in REAPER": "Βήματα ρύθμισης στο REAPER",
  "Finish with the setup steps in REAPER (button below).": "Ολοκλήρωσε με τα βήματα ρύθμισης στο REAPER (κουμπί παρακάτω).",

  // working
  "Starting": "Ξεκινά",
  "Downloading {what}": "Λήψη {what}",
  "Unpacking {what}": "Αποσυμπίεση {what}",
  "Checking {what}": "Έλεγχος {what}",
  "{done} of {total} MB": "{done} από {total} MB",
  "Windows asks for permission to write to the VST3/CLAP folders": "Τα Windows ζητούν άδεια για εγγραφή στους φακέλους VST3/CLAP",
  "Copying into place": "Αντιγραφή στη θέση τους",
  "Asking for permission": "Αίτημα άδειας",
  "Done": "Έγινε",
  "Installing the new NULL JSFX Hub": "Εγκατάσταση του νέου NULL JSFX Hub",
  "{n} updates for release {v}": "{n} ενημερώσεις για την έκδοση {v}",
  "Up to date: release {v}": "Ενημερωμένο: έκδοση {v}",
  "Showing what the Hub knew last time.": "Εμφανίζεται ό,τι ήξερε το Hub την προηγούμενη φορά.",
  "{name} installed": "Το {name} εγκαταστάθηκε",
  "{name} updated": "Το {name} ενημερώθηκε",
  "{name}: done": "{name}: έγινε",
  "Updated to release {v}": "Ενημερώθηκαν στην έκδοση {v}",

  // a running DAW
  "Close {names} first?": "Να κλείσεις πρώτα το {names};",
  "{names} is running.": "Το {names} είναι ανοιχτό.",
  "{names} are running.": "Τα {names} είναι ανοιχτά.",
  "Windows does not let a DAW's loaded VST3 and CLAP plugins be replaced or removed until it closes.":
    "Τα Windows δεν αφήνουν να αντικατασταθούν ή να αφαιρεθούν τα VST3 και CLAP plugins που έχει φορτώσει ένα DAW πριν κλείσει.",
  "A DAW picks up new VST3 and CLAP files only after a restart.": "Ένα DAW βλέπει τα νέα αρχεία VST3 και CLAP μόνο μετά από επανεκκίνηση.",
  "Save your projects, close {names}, then continue.": "Αποθήκευσε τα projects σου, κλείσε το {names} και συνέχισε.",
  "Some files are in use by another program, so they could not change yet. The Hub finishes this by itself the next time it starts. Close REAPER (for the Noto Sans font, sign out of Windows and back in), then open the Hub again.":
    "Κάποια αρχεία χρησιμοποιούνται από άλλο πρόγραμμα και δεν άλλαξαν ακόμη. Το Hub θα το ολοκληρώσει μόνο του την επόμενη φορά που θα ξεκινήσει. Κλείσε το REAPER (για τη γραμματοσειρά Noto Sans, αποσυνδέσου από τα Windows και συνδέσου ξανά) και άνοιξε πάλι το Hub.",
  "Continue": "Συνέχεια",

  // removing
  "Remove": "Αφαίρεση",
  "Remove {what}?": "Αφαίρεση: {what};",
  "Removes {files} ({formats}).": "Αφαιρεί {files} ({formats}).",
  "FX chains that use it go too, and so do its factory presets you did not change. Your own presets stay. Projects that use it will show it as missing.":
    "Φεύγουν και οι αλυσίδες FX που το χρησιμοποιούν, καθώς και τα εργοστασιακά presets του που δεν άλλαξες. Τα δικά σου presets μένουν. Τα projects που το χρησιμοποιούν θα το δείχνουν ως απόν.",
  "FX chains that use them go too, and so do their factory presets you did not change. Your own presets stay. Projects that use them will show them as missing.":
    "Φεύγουν και οι αλυσίδες FX που τα χρησιμοποιούν, καθώς και τα εργοστασιακά presets τους που δεν άλλαξες. Τα δικά σου presets μένουν. Τα projects που τα χρησιμοποιούν θα τα δείχνουν ως απόντα.",
  "Removed {what}": "Αφαιρέθηκε: {what}",
  "Nothing from NULL JSFX is installed.": "Δεν είναι εγκατεστημένο τίποτα από το NULL JSFX.",
  "Remove everything?": "Αφαίρεση όλων;",
  "Removes all {plugins} in every format ({files}) and the extras: FX chains, the tablet remote, the font and the factory presets you did not change. Your own presets and REAPER's settings stay. The Hub itself stays, to install them again later.":
    "Αφαιρεί όλα τα {plugins} σε κάθε μορφή ({files}) και τα επιπλέον: αλυσίδες FX, χειριστήριο tablet, γραμματοσειρά και τα εργοστασιακά presets που δεν άλλαξες. Τα δικά σου presets και οι ρυθμίσεις του REAPER μένουν. Το ίδιο το Hub μένει, για να τα εγκαταστήσεις ξανά αργότερα.",
  "Remove everything": "Αφαίρεση όλων",
  "Removed everything from NULL JSFX": "Αφαιρέθηκαν όλα του NULL JSFX",

  // the Hub itself, checking, the list
  "Downloading the new NULL JSFX Hub": "Λήψη του νέου NULL JSFX Hub",
  "The new Hub is downloaded. Open the file shown to install it.": "Το νέο Hub κατέβηκε. Άνοιξε το αρχείο που εμφανίζεται για να το εγκαταστήσεις.",
  "Starting the new NULL JSFX Hub": "Εκκίνηση του νέου NULL JSFX Hub",
  "You can still download it with the link.": "Μπορείς ακόμη να το κατεβάσεις από τον σύνδεσμο.",
  "Checking the installed files": "Έλεγχος των εγκατεστημένων αρχείων",
  "All {what} match release {v}.": "Όλα τα {what} ταιριάζουν με την έκδοση {v}.",
  "Nothing up to date to check yet.": "Δεν υπάρχει ακόμη κάτι ενημερωμένο για έλεγχο.",
  "Repair {what}?": "Επισκευή: {what};",
  "These differ from release {v} (damaged, changed or with files missing): {names}. Repair puts the release's files back.":
    "Αυτά διαφέρουν από την έκδοση {v} (χαλασμένα, αλλαγμένα ή με αρχεία που λείπουν): {names}. Η επισκευή επαναφέρει τα αρχεία της έκδοσης.",
  "Repair": "Επισκευή",
  "Repaired {what}": "Επισκευάστηκαν: {what}",
  "Saved: {file}": "Αποθηκεύτηκε: {file}",
  "That file is not a NULL JSFX list (saved with Save the list).": "Αυτό το αρχείο δεν είναι λίστα NULL JSFX (από την Αποθήκευση λίστας).",
  "Everything on that list is already installed.": "Όλα όσα έχει η λίστα είναι ήδη εγκατεστημένα.",
  "Install {what} from the list?": "Εγκατάσταση από τη λίστα: {what};",
  "{files} that are not here yet{extras}, from release {v}. What is installed here already stays as it is.":
    "{files} που δεν υπάρχουν ακόμη εδώ{extras}, από την έκδοση {v}. Ό,τι είναι ήδη εγκατεστημένο μένει όπως είναι.",
  ", and {names}": ", και {names}",
  "Installed {what} from the list": "Εγκαταστάθηκαν από τη λίστα: {what}",

  // What's new, Settings
  "No notes for this release.": "Δεν υπάρχουν σημειώσεις για αυτή την έκδοση.",
  "Language": "Γλώσσα",
  "Formats": "Μορφές",
  "What Install adds for a plugin (the same toggles as above the plugin list). You can still pick formats one by one in a plugin's page.":
    "Τι προσθέτει η Εγκατάσταση σε ένα plugin (οι ίδιοι διακόπτες με πάνω από τη λίστα). Μπορείς πάντα να διαλέξεις μορφές μία μία στη σελίδα κάθε plugin.",
  "Look for a new release once a day": "Αναζήτηση νέας έκδοσης μία φορά τη μέρα",
  "Show a notification when there are updates or new plugins (once per release)": "Ειδοποίηση όταν υπάρχουν ενημερώσεις ή νέα plugins (μία φορά ανά έκδοση)",
  "Start with the computer and stay in the tray": "Εκκίνηση με τον υπολογιστή και παραμονή στην περιοχή ειδοποιήσεων",
  "(closing the window keeps the Hub there; Quit from the tray icon)": "(το κλείσιμο του παραθύρου κρατά το Hub εκεί· Έξοδος από το εικονίδιό του)",
  "Folders": "Φάκελοι",
  "The Hub puts the plugins where the NULL JSFX installers do. Change a folder for a portable REAPER or your own plugin folders.":
    "Το Hub βάζει τα plugins εκεί που τα βάζουν και οι installers του NULL JSFX. Άλλαξε φάκελο για φορητό REAPER ή για δικούς σου φακέλους plugins.",
  "REAPER resource folder": "Φάκελος πόρων του REAPER",
  "(REAPER: Options > Show REAPER resource path)": "(REAPER: Options > Show REAPER resource path)",
  "VST3 folder": "Φάκελος VST3",
  "CLAP folder": "Φάκελος CLAP",
  "(plugins go into its NULL JSFX folder)": "(τα plugins μπαίνουν στον φάκελο NULL JSFX μέσα του)",
  "Use the usual folders": "Οι συνηθισμένοι φάκελοι",
  "Privacy": "Απόρρητο",
  "To look for updates the Hub downloads the release list file from GitHub, where the plugins are released. It sends nothing about you or your computer: no account, no id, no list of what you installed.":
    "Για να βρει ενημερώσεις, το Hub κατεβάζει το αρχείο της έκδοσης από το GitHub, όπου κυκλοφορούν τα plugins. Δεν στέλνει τίποτα για σένα ή τον υπολογιστή σου: ούτε λογαριασμό, ούτε αναγνωριστικό, ούτε λίστα με όσα εγκατέστησες.",
  "Save": "Αποθήκευση",
  "Saved.": "Αποθηκεύτηκε.",
  "Another computer": "Άλλος υπολογιστής",
  "Save the list of what is installed here (plugins, formats and extras) to a small file, then open it in the Hub on the other computer to install the same. It holds no folders and nothing about this computer.":
    "Αποθήκευσε τη λίστα με ό,τι είναι εγκατεστημένο εδώ (plugins, μορφές και επιπλέον) σε ένα μικρό αρχείο και άνοιξέ το στο Hub του άλλου υπολογιστή για να εγκατασταθούν τα ίδια. Δεν περιέχει φακέλους ούτε τίποτα για αυτόν τον υπολογιστή.",
  "Save the list": "Αποθήκευση λίστας",
  "Install from a list": "Εγκατάσταση από λίστα",
  "Check and repair": "Έλεγχος και επισκευή",
  "Compares the installed plugin files with the release, file by file, and puts back the ones that are damaged, changed or missing. It reads the release's file list from GitHub and downloads only what needs repair.":
    "Συγκρίνει τα εγκατεστημένα αρχεία με την έκδοση, ένα ένα, και επαναφέρει όσα είναι χαλασμένα, αλλαγμένα ή λείπουν. Διαβάζει τη λίστα αρχείων της έκδοσης από το GitHub και κατεβάζει μόνο ό,τι χρειάζεται επισκευή.",
  "Check installed files": "Έλεγχος εγκατεστημένων αρχείων",
  "Uninstall": "Απεγκατάσταση",
  "Takes every NULL JSFX plugin off this computer, in every format, with the FX chains, the tablet remote, the font and the factory presets you did not change.":
    "Αφαιρεί κάθε NULL JSFX plugin από τον υπολογιστή, σε κάθε μορφή, μαζί με τις αλυσίδες FX, το χειριστήριο tablet, τη γραμματοσειρά και τα εργοστασιακά presets που δεν άλλαξες.",
  "{what} installed now.": "Εγκατεστημένα τώρα: {what}.",
  "No plugins installed now.": "Δεν υπάρχουν εγκατεστημένα plugins τώρα.",
  "To remove the Hub too, uninstall it like any other app afterwards.": "Για να φύγει και το Hub, απεγκατάστησέ το μετά όπως κάθε εφαρμογή.",
  "About": "Σχετικά",
  "Free and open source, MIT License.": "Δωρεάν και ανοιχτού κώδικα, άδεια MIT.",
  "Its own files:": "Τα δικά του αρχεία:",
  "Source on GitHub": "Κώδικας στο GitHub",

  // plugin page
  "Close": "Κλείσιμο",
  "Apply": "Εφαρμογή",
  "install {n}": "εγκατάσταση {n}",
  "update {n}": "ενημέρωση {n}",
  "remove {n}": "αφαίρεση {n}",
  "Remove all formats": "Αφαίρεση όλων των μορφών",
  "Guide on nulljsfx.tech": "Οδηγός στο nulljsfx.tech",
  "Listen": "Άκου",
  "Play": "Αναπαραγωγή",
  "Stop": "Διακοπή",
  "Dry": "Χωρίς εφέ",
  "What to hear": "Τι ακούς",
  'Preset "{preset}", on a short mix': 'Preset "{preset}", σε μια σύντομη μίξη',
  "The demo needs the internet.": "Το demo χρειάζεται internet.",
  "Controls": "Χειριστήρια",
  "Good for": "Κατάλληλο για",
  "How it works": "Πώς λειτουργεί",
  "Update to {v}": "Ενημέρωση στην {v}",
  "Roll back to {v}": "Επιστροφή στην {v}",
  "{formats} kept at an earlier release": "{formats}: κρατιέται σε παλαιότερη έκδοση",
  "Install the {v} version of {formats} and keep it there": "Εγκατάσταση της {v} για {formats}, και παραμονή εκεί",
  "{name} is up to date again": "Το {name} είναι ξανά ενημερωμένο",
  "Roll {name} back to {v}?": "Επιστροφή του {name} στην {v};",
  "Installs {formats} of {name} from release {v}. It stays there, without update notices, until you click Update here. Projects keep their settings.":
    "Εγκαθιστά {formats} του {name} από την έκδοση {v}. Μένει εκεί, χωρίς ειδοποιήσεις ενημέρωσης, μέχρι να πατήσεις Ενημέρωση εδώ. Τα projects κρατούν τις ρυθμίσεις τους.",
  "Roll back": "Επιστροφή",
  "{name} is back at {v}": "Το {name} γύρισε στην {v}",
  "Cancel": "Άκυρο",
};

let LANG = "en";
/** texts asked for in Greek that have none (the UI test reads it) */
const TR_MISSING = new Set();

function tr(text, vars) {
  if (LANG === "el" && text && !EL[text]) TR_MISSING.add(text);
  const s = (LANG === "el" && EL[text]) || text;
  return vars ? s.replace(/\{(\w+)\}/g, (m, k) => (k in vars ? String(vars[k]) : m)) : s;
}

/** Sets the language and translates the marked parts of the page. */
function setLang(lang) {
  LANG = lang === "el" ? "el" : "en";
  document.documentElement.lang = LANG;
  for (const el of document.querySelectorAll("[data-t]")) {
    el.dataset.en ??= el.textContent.replace(/\s+/g, " ").trim();
    el.textContent = tr(el.dataset.en);
  }
  for (const [attr, name] of [["tPlaceholder", "placeholder"], ["tAria", "aria-label"], ["tTitle", "title"]]) {
    for (const el of document.querySelectorAll(`[data-${attr.replace(/[A-Z]/g, (c) => "-" + c.toLowerCase())}]`)) {
      const key = `en${attr}`;
      el.dataset[key] ??= el.getAttribute(name);
      el.setAttribute(name, tr(el.dataset[key]));
    }
  }
}
