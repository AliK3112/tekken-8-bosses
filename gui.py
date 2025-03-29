import tkinter as tk
from tkinter import ttk
import ctypes
import psutil # type: ignore
import subprocess
import time
import threading
import os

dll_path = "path/to/your.dll"
game_name = "Polaris-Win64-Shipping.exe"
pipe_name = r"\\.\pipe\BossSelectorPipe"

# Boss Codes Mapping
boss_mapping = {
    -1: "No Boss Selected",
    0: "Jin (Boosted)",
    1: "Jin (Nerfed)",
    2: "Jin (Mishima)",
    3: "Jin (Kazama)",
    4: "Jin (Ultimate)",
    11: "Jin (Chained)",
    32: "Azazel",
    97: "Kazuya (Devil)",
    117: "Jin (Angel)",
    118: "Kazuya (True Devil)",
    121: "Jin (Devil)",
    244: "Kazuya (Final)",
    351: "Heihachi (Monk)",
    352: "Heihachi (Shadow)",
    353: "Heihachi (Final)",
}

boss_code_list = list(boss_mapping.keys())
boss_label_list = list(boss_mapping.values())


def get_game_pid():
    for proc in psutil.process_iter(["pid", "name"]):
        if proc.info["name"] and game_name.lower() in proc.info["name"].lower():
            return proc.info["pid"]
    return None


def inject_dll():
    pid = get_game_pid()
    if not pid:
        log_message("Game not found!", error=True)
        return

    # Inject the DLL (comment out for GUI-only testing)
    # subprocess.run(["injector.exe", str(pid), dll_path])
    log_message("DLL Injected!")

    # Start named pipe communication thread
    threading.Thread(target=connect_pipe, daemon=True).start()


def connect_pipe():
    """Connect to the named pipe created by the DLL"""
    global pipe
    log_message("Connecting to named pipe...")
    while True:
        try:
            pipe = open(pipe_name, "w")
            log_message("Connected to DLL!")
            return
        except Exception as e:
            time.sleep(1)


def send_boss_selection():
    """Send selected boss values to the DLL"""
    if not os.path.exists(pipe_name):
        log_message("Pipe not available, waiting...", error=True)
        return

    boss1_code = boss_code_list[player1_var.current()]
    boss2_code = boss_code_list[player2_var.current()]
    message = f"{boss1_code},{boss2_code}\n"

    try:
        with open(pipe_name, "w") as pipe:
            pipe.write(message)
            log_message(f"Sent to DLL: {message.strip()}")
    except Exception as e:
        log_message(f"Error writing to pipe: {e}", error=True)


def log_message(msg, error=False):
    log_box.insert(tk.END, ("[ERROR] " if error else "") + msg + "\n")
    log_box.see(tk.END)


# Create main window
root = tk.Tk()
root.title("Boss Selector")
root.geometry("600x200")
root.minsize(600,200)
root.maxsize(1200,400)
root.configure(bg="#2E2E2E")

frame = tk.Frame(root, bg="#2E2E2E")
frame.pack(pady=20)

player1_label = ttk.Label(
    frame, text="Player 1:", background="#2E2E2E", foreground="white"
)
player1_label.grid(row=0, column=0, padx=10)
player1_var = ttk.Combobox(frame, values=boss_label_list, state="readonly", width=20)
player1_var.current(0)
player1_var.grid(row=0, column=1, padx=10)
player1_var.bind("<<ComboboxSelected>>", lambda e: send_boss_selection())

player2_label = ttk.Label(
    frame, text="Player 2:", background="#2E2E2E", foreground="white"
)
player2_label.grid(row=0, column=2, padx=10)
player2_var = ttk.Combobox(frame, values=boss_label_list, state="readonly", width=20)
player2_var.current(0)
player2_var.grid(row=0, column=3, padx=10)
player2_var.bind("<<ComboboxSelected>>", lambda e: send_boss_selection())

inject_button = ttk.Button(root, text="Inject DLL", command=inject_dll)
inject_button.pack(pady=10)

log_box = tk.Text(root, height=5, width=70, bg="black", fg="white")
log_box.pack()

root.mainloop()
