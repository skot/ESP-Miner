import tkinter as tk
from tkinter import messagebox, filedialog
import requests

# Variable to store the base API URL (starts empty to allow for full user configuration)
api_url = ""

# Function to set the API URL based on user input
def set_api_url():
    global api_url
    base_url = api_url_entry.get().strip()
    if base_url.startswith("http://"):
        api_url = base_url
        messagebox.showinfo("Success", f"API URL set to {api_url}")
    else:
        messagebox.showerror("Error", "Please enter a valid HTTP URL starting with 'http://'")

# Partially upload www.bin file to BitAxe and then stop
def partial_www_upload():
    if not api_url:
        messagebox.showerror("Error", "API URL is not set. Please set it first.")
        return
    
    www_file = filedialog.askopenfilename(title="Select www.bin File")
    if not www_file:
        return  # User canceled the file selection

    try:
        # Open the file and partially upload a chunk
        with open(www_file, 'rb') as file:
            partial_data = file.read(1000)  # Read only part of the file, e.g., 1000 bytes
            response = requests.post(f"{api_url}/api/system/OTAWWW", data=partial_data)
            response.raise_for_status()
        messagebox.showinfo("Partial Upload", "Partial upload of www.bin completed. Now restart the device.")
    except requests.RequestException as e:
        messagebox.showerror("Error", f"Failed to partially upload www.bin: {e}")

# Restart BitAxe device to trigger recovery mode
def restart_device():
    if not api_url:
        messagebox.showerror("Error", "API URL is not set. Please set it first.")
        return
    try:
        response = requests.post(f"{api_url}/api/system/restart")
        response.raise_for_status()
        messagebox.showinfo("Restart Device", "Device restarted. Check the BitAxe display screen for the IP address to access the recovery GUI.")
    except requests.RequestException as e:
        messagebox.showerror("Error", f"Failed to restart device: {e}")

# Guide the user through the recovery process
def show_recovery_instructions():
    instructions = (
        "Recovery Process Instructions:\n\n"
        "1. Partially upload the `www.bin` file. This process stops midway.\n"
        "2. Click 'Restart Device' to trigger BitAxe recovery mode.\n"
        "3. Check the BitAxe display screen for the IP address.\n"
        "4. Open the device’s recovery GUI using the displayed IP to fully upload `www.bin`.\n"
        "5. After `www.bin` upload completes, use the normal update function from the standard bitaxe interface to upload `esp-miner.bin` as usual. No more grayed out save button.\n\n"
        "Note: This process is required to address the issue on some firmware versions where the 'Save' button is grayed out.\n"
    )
    messagebox.showinfo("Recovery Instructions", instructions)

# Display the donation address and provide copy option
def show_donate():
    donate_address = "1CNg32ut3ABbtGVqfcsTeWvXHHFJtGy1mE"
    root.clipboard_clear()
    root.clipboard_append(donate_address)
    messagebox.showinfo("Donate Bitcoin", f"Bitcoin Address: {donate_address}\n\nAddress copied to clipboard.")

# GUI setup
root = tk.Tk()
root.title("BitAxe Recovery and Firmware Update")

# API URL configuration
tk.Label(root, text="API Base URL").grid(row=0, column=0, padx=5, pady=5, sticky=tk.W)
api_url_entry = tk.Entry(root, width=30)
api_url_entry.grid(row=0, column=1, padx=5, pady=5)
set_api_url_button = tk.Button(root, text="Set API URL", command=set_api_url)
set_api_url_button.grid(row=0, column=2, padx=5, pady=5)

# Buttons for partial upload, restart, recovery instructions, and donation
partial_upload_button = tk.Button(root, text="Partial Upload www.bin", command=partial_www_upload)
partial_upload_button.grid(row=1, column=0, padx=5, pady=5)

restart_button = tk.Button(root, text="Restart Device", command=restart_device)
restart_button.grid(row=1, column=1, padx=5, pady=5)

instructions_button = tk.Button(root, text="Show Recovery Instructions", command=show_recovery_instructions)
instructions_button.grid(row=2, column=0, columnspan=2, padx=5, pady=5)

donate_button = tk.Button(root, text="Donate Bitcoin", command=show_donate)
donate_button.grid(row=3, column=0, columnspan=2, padx=5, pady=5)

root.mainloop()
