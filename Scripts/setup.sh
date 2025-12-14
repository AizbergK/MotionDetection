set -e  # Exit immediately if any command fails

# Config
REPO="https://github.com/AizbergK/MotionDetection.git"
APP_DIR="/home/pi/motion_app"
VID_DIR="/home/pi/motion_videos"
BIN_NAME="motionApp"
SVC_NAME="motion_detector"

echo "Starting setup..."

# 1. Install Dependencies (Quietly)
sudo apt-get update -qq
sudo apt-get install -y -qq git build-essential pkg-config libopencv-dev libcamera-dev

# 2. Setup Folders
mkdir -p "$VID_DIR" "$APP_DIR"
sudo chown -R $USER:$USER "$VID_DIR"

# 3. Clone or Update Repo
if [ -d "$APP_DIR/.git" ]; then
    cd "$APP_DIR" && git pull
else
    git clone "$REPO" "$APP_DIR" && cd "$APP_DIR"
fi

# 4. Compile (C++17, Optimized for Pi)
echo "Compiling..."
g++ -std=c++17 -O3 -march=native -pthread -flto ./MotionDetectionApp/*.cpp -o "$BIN_NAME" $(pkg-config --cflags --libs opencv4)

# 5. Create Systemd Service
sudo bash -c "cat > /etc/systemd/system/motion_detector.service" <<EOF
[Unit]
Description=Motion Detection Service
After=network.target

[Service]
ExecStart=$APP_DIR/$BIN_NAME
WorkingDirectory=$VID_DIR
Restart=always
User=$USER

[Install]
WantedBy=multi-user.target
EOF

# 6. Start Service
sudo systemctl daemon-reload
sudo systemctl enable --now "$SVC_NAME.service"

echo "Done. Service is running."