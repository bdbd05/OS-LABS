# OS Lab 2 - Antivirus

## 1. Overview and Folder Hierarchy
This project contains a simple antivirus script that watches a folder for bad files, and a restore script to handle files that were quarantined by the antivirus.

**Folder structure:**
.
├── antivirusd.sh          # The main script that monitors the folder
├── restore.sh             # The interactive menu to manage caught files
├── Makefile               # Shortcuts to run the scripts
├── tester/                # The folder being watched by the antivirus
├── malicious_dir/         # The quarantine folder where viruses are sent
├── directory-info.last    # (Generated) The baseline snapshot of the folder
├── directory-info.new     # (Generated) The latest snapshot for comparison
└── README.md              # This instruction file

## 2. Prerequisites
- Ubuntu Linux with `bash` (which is pre-installed).
- You can optionally install `make` to run the Makefile commands instead of typing the full commands.
- To install it, run: `sudo apt update && sudo apt install make`

## 3. How to Run

**Step 1: Setup**
Open your terminal and navigate to the project folder:
`cd "lab 2"`
Give the scripts permission to run:
`chmod +x antivirusd.sh restore.sh`

**Step 2: Start the Antivirus**
Run `make antivirus` (or `./antivirusd.sh tester malicious_dir 3`).
It will run in the foreground. 

**Step 3: Test It**
Open a second terminal tab and create a fake virus in the watched folder:
`touch tester/fake.exe`
You will see the first terminal log that the file was deleted and moved.
*Important: Stop the antivirus by pressing Ctrl+C in the first terminal before moving to the next step. The antivirus and restore tool must not run at the same time.*

**Step 4: Restore Menu**
Run `make restore` (or `./restore.sh tester malicious_dir`).
The menu will appear. Follow the numbers on the screen to restore or permanently delete the file.

## 4. Flagged Lists Location
The flagged extensions (.exe, .bat, .vbs, .scr, .ps1) and the flagged keywords (virus, trojan, malware, worm, ransomware) are hardcoded directly inside `antivirusd.sh`. 
They are located near the top of the file inside the `scan_directory()` function, written directly into the `if` statement condition.