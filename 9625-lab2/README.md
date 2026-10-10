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
`cd "9625-lab2"`
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

## 5. Bonus Features Implemented

**Bonus 1: Cron Job Scheduling**
I created a separate script (`antivirus-cron.sh`) just for cron. It sleeps for 23 seconds at the start so it runs at exactly second 23 of the minute. It does not have an infinite loop.

**Prerequisites:**
1. The cron service must be running (`sudo service cron start`).
2. Make the script executable by running `chmod +x antivirus-cron.sh`.

**Step-by-Step Manual (Run every minute at second 23):**
1. Type `crontab -e` in your terminal to open the editor.
2. Add this exact line to the bottom. (Replace the paths with your actual absolute paths. Keep the quotes because of the space in "lab 2"):
`* * * * * "/path/to/9625-lab2/antivirus-cron.sh" "/path/to/9625-lab2/tester" "/path/to/9625-lab2/malicious_dir" "/path/to/9625-lab2/whitelist.txt" >> "/path/to/9625-lab2/cron.log" 2>&1`
3. Save and close the editor.

**Cron Expression: 3rd Friday of the month at 12:31 AM**
The 3rd Friday always falls between the 15th and 21st of the month. I used a bash test to check if today is Friday (day 5) before running.
`31 0 15-21 * * [ "$(date +\%u)" -eq 5 ] && "/path/to/9625-lab2/antivirus-cron.sh" "/path/to/9625-lab2/tester" "/path/to/9625-lab2/malicious_dir" "/path/to/9625-lab2/whitelist.txt" >> "/path/to/9625-lab2/cron.log" 2>&1`

**Bonus 2: Whitelist Implementation**
A whitelist was added so safe files can be ignored by the antivirus.

- **How the daemon checks it:** 
Inside `antivirusd.sh`, before the scanner checks a file for bad extensions or keywords, it runs `if [ -f "whitelist.txt" ] && grep -qix "$filter" whitelist.txt`. The `grep` command silently checks for an exact, case-insensitive match of the filename inside the text file. If a match is found, the script uses `continue` to skip that file entirely.

- **How a file gets added:**
In `restore.sh`, when the user selects Option 1 to restore a file, the script moves the file back to the watched folder and immediately runs `echo "$target_name" >> whitelist.txt`. This automatically appends the filename to the bottom of the whitelist on a new line so the daemon won't re-quarantine it.