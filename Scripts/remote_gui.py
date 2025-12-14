import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import paramiko
import os
import datetime
import threading
import json

class MotionViewerApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Pi Motion Video Manager")
        self.root.geometry("1280x720")

        # SSH Client variables
        self.ssh_client = None
        self.sftp = None
        self.stop_threads = False # Flag to stop bulk operations if needed

        # --- Load Configuration ---
        config = self.load_config()

        # --- GUI Layout ---

        # 1. Connection Details Frame
        conn_frame = ttk.LabelFrame(root, text="Connection Details", padding=10)
        conn_frame.pack(fill="x", padx=10, pady=5)

        # Make columns 1 and 3 expand to fill width
        conn_frame.columnconfigure(1, weight=1)
        conn_frame.columnconfigure(3, weight=1)

        # Host
        ttk.Label(conn_frame, text="Pi IP Address:").grid(row=0, column=0, padx=5, sticky="e")
        self.entry_host = ttk.Entry(conn_frame)
        self.entry_host.insert(0, config.get("host", "192.168.1.X")) 
        self.entry_host.grid(row=0, column=1, padx=5, sticky="ew")

        # User
        ttk.Label(conn_frame, text="Username:").grid(row=0, column=2, padx=5, sticky="e")
        self.entry_user = ttk.Entry(conn_frame)
        self.entry_user.insert(0, config.get("user", "pi"))
        self.entry_user.grid(row=0, column=3, padx=5, sticky="ew")

        # Password
        ttk.Label(conn_frame, text="Password:").grid(row=1, column=0, padx=5, sticky="e")
        self.entry_pass = ttk.Entry(conn_frame, show="*")
        self.entry_pass.insert(0, config.get("password", ""))
        self.entry_pass.grid(row=1, column=1, padx=5, sticky="ew")

        # Path
        ttk.Label(conn_frame, text="Video Path:").grid(row=1, column=2, padx=5, sticky="e")
        self.entry_path = ttk.Entry(conn_frame)
        self.entry_path.insert(0, config.get("path", "/home/pi/motion_videos")) 
        self.entry_path.grid(row=1, column=3, padx=5, sticky="ew")

        # --- UPDATED BUTTON LAYOUT ---
        # Connect Button (Left half)
        self.btn_connect = ttk.Button(conn_frame, text="Connect & List", command=self.connect_and_list)
        self.btn_connect.grid(row=2, column=0, columnspan=2, pady=10, sticky="ew", padx=5)

        # Update Button (Right half) - New
        self.btn_update = ttk.Button(conn_frame, text="Update List", command=self.update_list_only, state="disabled")
        self.btn_update.grid(row=2, column=2, columnspan=2, pady=10, sticky="ew", padx=5)

        # 2. File List Frame
        list_frame = ttk.LabelFrame(root, text="Captured Videos", padding=10)
        list_frame.pack(fill="both", expand=True, padx=10, pady=5)

        # Columns: Date (decoded), Size
        self.tree = ttk.Treeview(list_frame, columns=("date", "size"), show="headings", selectmode="extended") # Changed to extended for multi-select support in future
        self.tree.heading("date", text="Detected Time")
        self.tree.heading("size", text="Size (MB)")
        self.tree.column("date", width=250)
        self.tree.column("size", width=100)
        
        # Scrollbar
        scrollbar = ttk.Scrollbar(list_frame, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=scrollbar.set)
        
        self.tree.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        # 3. Action Buttons Frame
        action_frame = ttk.Frame(root, padding=10)
        action_frame.pack(fill="x", padx=10, pady=5)

        # -- Left Side: Downloads --
        self.btn_download = ttk.Button(action_frame, text="Download Selected", command=self.download_video, state="disabled")
        self.btn_download.pack(side="left", padx=5)

        # New Download All
        self.btn_download_all = ttk.Button(action_frame, text="Download All", command=self.download_all_videos, state="disabled")
        self.btn_download_all.pack(side="left", padx=5)

        # -- Right Side: Deletions --
        self.btn_delete = ttk.Button(action_frame, text="Delete Selected", command=self.delete_video, state="disabled")
        self.btn_delete.pack(side="right", padx=5)

        # New Delete All with Checkbox
        self.btn_delete_all = ttk.Button(action_frame, text="DELETE ALL", command=self.delete_all_videos, state="disabled")
        self.btn_delete_all.pack(side="right", padx=5)

        self.confirm_del_var = tk.IntVar()
        self.chk_confirm = ttk.Checkbutton(action_frame, text="Confirm Delete All", variable=self.confirm_del_var, state="disabled")
        self.chk_confirm.pack(side="right", padx=5)

        # Status Bar
        self.status_var = tk.StringVar()
        self.status_var.set("Ready to connect.")
        status_bar = ttk.Label(root, textvariable=self.status_var, relief="sunken", anchor="w")
        status_bar.pack(fill="x", side="bottom")

    def load_config(self):
        if os.path.exists("connection.json"):
            try:
                with open("connection.json", "r") as f:
                    return json.load(f)
            except Exception as e:
                print(f"Error loading connection.json: {e}")
        return {}

    def log(self, message):
        self.status_var.set(message)
        self.root.update_idletasks()

    def toggle_controls(self, state):
        """Helper to enable/disable buttons based on connection state"""
        self.btn_update.config(state=state)
        self.btn_download.config(state=state)
        self.btn_download_all.config(state=state)
        self.btn_delete.config(state=state)
        self.btn_delete_all.config(state=state)
        self.chk_confirm.config(state=state)

    # --- Connection Logic ---

    def connect_and_list(self):
        host = self.entry_host.get()
        user = self.entry_user.get()
        password = self.entry_pass.get()
        path = self.entry_path.get()
        threading.Thread(target=self._connect_thread, args=(host, user, password, path)).start()

    def update_list_only(self):
        """Refreshes the list without reconnecting entirely."""
        if self.sftp:
            self.log("Refreshing list...")
            # Run in thread to prevent UI freeze during network listing
            threading.Thread(target=self._refresh_thread).start()
        else:
            messagebox.showerror("Error", "Not connected.")

    def _connect_thread(self, host, user, password, path):
        self.log(f"Connecting to {host}...")
        try:
            if self.ssh_client: self.ssh_client.close()
            
            self.ssh_client = paramiko.SSHClient()
            self.ssh_client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            self.ssh_client.connect(host, username=user, password=password, timeout=5)
            self.sftp = self.ssh_client.open_sftp()
            
            try:
                self.sftp.chdir(path)
            except IOError:
                messagebox.showerror("Error", f"Remote path not found: {path}")
                return

            self.log("Connected. Retrieving file list...")
            self.refresh_list()
            
            # Enable buttons on main thread
            self.root.after(0, lambda: self.toggle_controls("normal"))

        except Exception as e:
            self.log("Connection failed.")
            messagebox.showerror("Connection Error", str(e))

    def _refresh_thread(self):
        try:
            self.refresh_list()
        except Exception as e:
             self.log(f"Error refreshing: {e}")

    def refresh_list(self):
        # Clear current list
        for item in self.tree.get_children():
            self.tree.delete(item)

        try:
            files = self.sftp.listdir_attr()
            video_extensions = ('.mp4', '.avi', '.mkv', '.h264', '.mov')
            files.sort(key=lambda x: x.st_mtime, reverse=True)

            count = 0
            for file_attr in files:
                filename = file_attr.filename
                if filename.lower().endswith(video_extensions):
                    size_mb = f"{file_attr.st_size / (1024 * 1024):.2f} MB"
                    display_time = self.decode_filename(filename)
                    self.tree.insert("", "end", iid=filename, text=filename, values=(display_time, size_mb))
                    count += 1
            
            self.log(f"List refreshed. {count} videos found.")
        except Exception as e:
            self.log("Error retrieving list.")
            print(e)

    def decode_filename(self, filename):
        try:
            name_body = os.path.splitext(filename)[0]
            dt = datetime.datetime.strptime(name_body, "%Y-%m-%d_%H-%M-%S")
            return dt.strftime("%d %b %Y at %I:%M:%S %p")
        except:
            return filename

    # --- Download Logic ---

    def download_video(self):
        selected_items = self.tree.selection()
        if not selected_items: return

        # Just take the first one for single download, or handle loop if multiple
        filename = selected_items[0]
        save_path = filedialog.asksaveasfilename(initialfile=filename, title="Save Video As")
        if save_path:
            threading.Thread(target=self._download_thread, args=(filename, save_path)).start()

    def download_all_videos(self):
        # 1. Ask for a folder, not a file
        folder_path = filedialog.askdirectory(title="Select Folder to Save All Videos")
        if not folder_path:
            return

        # 2. Get all file names
        all_files = self.tree.get_children() # returns list of IIDs (filenames)
        if not all_files:
            messagebox.showinfo("Info", "No files to download.")
            return

        # 3. Start Bulk Thread
        threading.Thread(target=self._bulk_download_thread, args=(all_files, folder_path)).start()

    def _download_thread(self, remote_file, local_path):
        self.log(f"Downloading {remote_file}...")
        try:
            self.sftp.get(remote_file, local_path)
            self.log(f"Download complete: {remote_file}")
            messagebox.showinfo("Success", f"Downloaded {remote_file}")
        except Exception as e:
            self.log("Download failed.")
            messagebox.showerror("Error", str(e))

    def _bulk_download_thread(self, files_list, local_folder):
        total = len(files_list)
        success_count = 0
        
        for index, filename in enumerate(files_list):
            self.log(f"Downloading {index+1}/{total}: {filename}")
            local_path = os.path.join(local_folder, filename)
            
            try:
                # Basic progress callback could go here, but keeping it simple for bulk
                self.sftp.get(filename, local_path)
                success_count += 1
            except Exception as e:
                print(f"Failed to download {filename}: {e}")
        
        self.log(f"Bulk download complete. {success_count}/{total} files downloaded.")
        self.root.after(0, lambda: messagebox.showinfo("Complete", f"Downloaded {success_count} of {total} files."))

    # --- Delete Logic ---

    def delete_video(self):
        selected_item = self.tree.selection()
        if not selected_item: return
        filename = selected_item[0]
        
        if messagebox.askyesno("Confirm Delete", f"Delete '{filename}'?"):
            try:
                self.sftp.remove(filename)
                self.tree.delete(filename)
                self.log(f"Deleted {filename}")
            except Exception as e:
                messagebox.showerror("Error", str(e))

    def delete_all_videos(self):
        # 1. Check Safety Checkbox
        if self.confirm_del_var.get() != 1:
            messagebox.showwarning("Safety Lock", "Please check the 'Confirm Delete All' box to proceed.")
            return

        # 2. Final Confirmation
        all_files = self.tree.get_children()
        if not all_files: return

        if not messagebox.askyesno("FINAL WARNING", f"This will delete ALL {len(all_files)} videos from the Pi.\nThis cannot be undone.\nAre you sure?"):
            return

        # 3. Start Bulk Delete Thread
        threading.Thread(target=self._bulk_delete_thread, args=(all_files,)).start()

    def _bulk_delete_thread(self, files_list):
        total = len(files_list)
        count = 0
        
        for index, filename in enumerate(files_list):
            self.log(f"Deleting {index+1}/{total}: {filename}")
            try:
                self.sftp.remove(filename)
                # Remove from treeview safely using after
                self.root.after(0, lambda f=filename: self.tree.delete(f))
                count += 1
            except Exception as e:
                print(f"Failed to delete {filename}: {e}")
        
        self.log(f"Bulk delete complete. Removed {count} files.")
        
        # Reset checkbox and refresh list to be sure
        self.root.after(0, lambda: self.confirm_del_var.set(0))
        self.root.after(0, self.refresh_list)

    def __del__(self):
        if self.sftp: self.sftp.close()
        if self.ssh_client: self.ssh_client.close()

if __name__ == "__main__":
    root = tk.Tk()
    app = MotionViewerApp(root)
    root.mainloop()