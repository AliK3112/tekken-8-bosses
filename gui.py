import tkinter as tk
from tkinter import ttk
import ctypes
import psutil
import subprocess

DLL_PATH = "path/to/your.dll"
GAME_NAME = "Polaris-Win64-Shipping.exe"

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

# Reverse lookup for value retrieval
boss_code_list = list(boss_mapping.keys())
boss_label_list = list(boss_mapping.values())


def get_game_pid(game_name):
    for proc in psutil.process_iter(["pid", "name"]):
        if proc.info["name"] and game_name.lower() in proc.info["name"].lower():
            return proc.info["pid"]
    return None


def inject_dll():
    pid = get_game_pid(GAME_NAME)

    if not pid:
        status_label.config(text="Game not found!", foreground="red")
        return

    boss1_code = boss_code_list[player1_var.current()]
    boss2_code = boss_code_list[player2_var.current()]

    print("Boss code 1: %d" % boss1_code)
    print("Boss code 2: %d" % boss2_code)

    # Inject the DLL and pass boss selections (handled inside the DLL)
    # subprocess.run(
    #     ["injector.exe", str(pid), DLL_PATH, str(boss1_code), str(boss2_code)]
    # )

    status_label.config(text="DLL Injected!", foreground="green")


# Create main window
root = tk.Tk()
root.title("Boss Selector")
root.geometry("550x150")
root.maxsize(550, 150)
root.minsize(550, 150)
root.configure(bg="#2E2E2E")

frame = tk.Frame(root, bg="#2E2E2E")
frame.pack(pady=20)

# Dropdowns
player1_label = ttk.Label(
    frame, text="Player 1:", background="#2E2E2E", foreground="white"
)
player1_label.grid(row=0, column=0, padx=10)

player1_var = ttk.Combobox(frame, values=boss_label_list, state="readonly", width=20)
player1_var.current(0)
player1_var.grid(row=0, column=1, padx=10)

player2_label = ttk.Label(
    frame, text="Player 2:", background="#2E2E2E", foreground="white"
)
player2_label.grid(row=0, column=2, padx=10)

player2_var = ttk.Combobox(frame, values=boss_label_list, state="readonly", width=20)
player2_var.current(0)
player2_var.grid(row=0, column=3, padx=10)

# Inject button
inject_button = ttk.Button(root, text="Inject DLL", command=inject_dll)
inject_button.pack(pady=10)

# Status label
status_label = ttk.Label(root, text="", background="#2E2E2E", foreground="white")
status_label.pack()

root.mainloop()
