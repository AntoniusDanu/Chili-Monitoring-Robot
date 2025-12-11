import os
from uuid import uuid4
from fastapi import FastAPI, UploadFile, File, HTTPException
from fastapi.responses import HTMLResponse, FileResponse
from fastapi.middleware.cors import CORSMiddleware
from ultralytics import YOLO
from pydantic import BaseModel  # <-- tambahan untuk POST JSON
from PIL import Image           # <-- DITAMBAHKAN
import uuid

# ============================================
# KONFIGURASI
# ============================================
UPLOAD_DIR = "chili_uploads"
os.makedirs(UPLOAD_DIR, exist_ok=True)

# Load YOLO Model
chili_model = YOLO("bestchili.pt")

# State Global
chili_state = {
    "last_image": None,
    "last_pred": None,
    "count_total": 0,
    "count_ripe": 0,
    "count_unripe": 0
}

# ============================================
# Fungsi preprocessing (ROTATE 90° CLOCKWISE)
# ============================================
def preprocess_and_rotate(image_path):
    img = Image.open(image_path)
    img = img.rotate(-90, expand=True)  # ROTATE CLOCKWISE 90°
    img.save(image_path)                # overwrite file
    return image_path


# ============================================
# FastAPI App
# ============================================
app = FastAPI(title="Chili Ripeness Detection")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"]
)

# ============================================
# Fungsi klasifikasi
# ============================================
def analyze_chili_boxes(result):
    boxes = result.boxes

    if len(boxes) == 0:
        return -1, 0, 0, 0

    total = len(boxes)
    ripe = sum(1 for b in boxes if int(b.cls) == 0)
    unripe = sum(1 for b in boxes if int(b.cls) == 1)

    best = max(boxes, key=lambda x: float(x.conf))
    best_class = int(best.cls)

    return best_class, total, ripe, unripe


# ============================================
# Auto Cleanup
# ============================================
def cleanup_uploads(max_files=100):
    files = sorted(
        [os.path.join(UPLOAD_DIR, f) for f in os.listdir(UPLOAD_DIR)],
        key=os.path.getctime
    )
    if len(files) > max_files:
        for f in files[:-max_files]:
            os.remove(f)


# ============================================
# Endpoint Upload Gambar (ESP32)
# ============================================
@app.post("/chili/upload")
async def upload_chili(file: UploadFile = File(...)):
    filename = f"{uuid4()}.jpg"
    filepath = os.path.join(UPLOAD_DIR, filename)

    # Simpan file asli
    with open(filepath, "wb") as f:
        f.write(await file.read())

    cleanup_uploads()

    # ==== PREPROCESSING: ROTATE 90° CLOCKWISE ====
    preprocess_and_rotate(filepath)

    # ==== YOLO DETECTION ====
    try:
        result = chili_model.predict(filepath, verbose=False)[0]
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Inference error: {str(e)}")

    ripeness, total, ripe, unripe = analyze_chili_boxes(result)

    chili_state["last_image"] = filepath
    chili_state["last_pred"] = ripeness
    chili_state["count_total"] = total
    chili_state["count_ripe"] = ripe
    chili_state["count_unripe"] = unripe

    return {
        "status": "ok",
        "ripeness": ripeness,
        "total_detected": total,
        "ripe": ripe,
        "unripe": unripe,
        "note": "0=ripe, 1=unripe, -1=no chili"
    }


# ============================================
# Endpoint Status (JSON Only)
# ============================================
@app.get("/chili/status")
def chili_status():
    return {
        "last_pred": chili_state["last_pred"],
        "total_detected": chili_state["count_total"],
        "ripe": chili_state["count_ripe"],
        "unripe": chili_state["count_unripe"],
        "note": "0=ripe, 1=unripe, -1=no chili"
    }


# ============================================
# Endpoint Gambar Terakhir
# ============================================
@app.get("/chili/image")
def chili_image():
    if chili_state["last_image"] is None:
        raise HTTPException(status_code=404, detail="No image yet")
    return FileResponse(chili_state["last_image"])


# ============================================
# Endpoint Web Viewer
# ============================================
@app.get("/chili/view", response_class=HTMLResponse)
def chili_view():
    return """
    <html>
    <head><title>Chili Viewer</title>
    <style>
        body { font-family: Arial; }
        .container {
            display: flex;
            flex-direction: row;
            gap: 30px;
            align-items: flex-start;
        }
        .card {
            padding: 15px;
            border: 1px solid #ccc;
            border-radius: 10px;
            width: 300px;
            background: #f8f8f8;
        }
        img {
            border-radius: 10px;
            border: 1px solid #ccc;
        }
    </style>
    </head>

    <body>
        <h2>Chili Ripeness Detection</h2>

        <div class="container">

            <!-- KIRI: Gambar + Data Deteksi -->
            <div class="card">
                <img id="img" src="/chili/image" width="280" />
                <p>Status: <span id="status">Loading...</span></p>
                <p>Total Detected: <span id="total">0</span></p>
                <p>Ripe (Merah): <span id="ripe">0</span></p>
                <p>Unripe (Hijau): <span id="unripe">0</span></p>
            </div>

            <!-- KANAN: Data Waktu + Sensor -->
            <div class="card">
                <h3>📌 Device Time</h3>
                <p>Device: <span id="t_device">-</span></p>
                <p>Datetime: <span id="t_datetime">-</span></p>

                <h3>🌡️ Sensor DHT</h3>
                <p>Temperature: <span id="s_temp">-</span> °C</p>
                <p>Humidity: <span id="s_hum">-</span> %</p>
                <p>Sensor Device: <span id="s_dev">-</span></p>
            </div>

        </div>

        <script>
            async function update() {
                try {
                    // === Chili status ===
                    const res = await fetch('/chili/status');
                    const data = await res.json();

                    let text = "";
                    if (data.last_pred === 0) text = "RIPE (Merah)";
                    else if (data.last_pred === 1) text = "UNRIPE (Hijau)";
                    else text = "No chili detected";

                    document.getElementById("status").innerHTML = text;
                    document.getElementById("total").innerHTML = data.total_detected;
                    document.getElementById("ripe").innerHTML = data.ripe;
                    document.getElementById("unripe").innerHTML = data.unripe;

                    document.getElementById("img").src = '/chili/image?t=' + Date.now();

                    // === Waktu device ===
                    const t = await fetch('/device/time');
                    const tData = await t.json();

                    document.getElementById("t_device").innerHTML = tData.last_device ?? "-";
                    document.getElementById("t_datetime").innerHTML = tData.last_datetime ?? "-";

                    // === DHT Sensor ===
                    const d = await fetch('/sensor/dht');
                    const dData = await d.json();

                    document.getElementById("s_temp").innerHTML = dData.temperature ?? "-";
                    document.getElementById("s_hum").innerHTML = dData.humidity ?? "-";
                    document.getElementById("s_dev").innerHTML = dData.device ?? "-";

                } catch (err) {
                    document.getElementById("status").innerHTML = "No Image";
                }
            }
            setInterval(update, 2000);
            update();
        </script>

    </body>
    </html>
    """

# =====================================================================
# 🆕 ENDPOINT POST UNTUK TIME & SENSOR DHT
# =====================================================================
class TimeData(BaseModel):
    device: str
    datetime: str

time_state = {
    "last_device": None,
    "last_datetime": None
}

@app.post("/device/time")
def post_time(data: TimeData):
    time_state["last_device"] = data.device
    time_state["last_datetime"] = data.datetime
    return {
        "status": "ok",
        "message": "Time received",
        "device": data.device,
        "datetime": data.datetime
    }

@app.get("/device/time")
def get_time():
    return time_state


class DHTData(BaseModel):
    device: str
    temperature: float
    humidity: float

dht_state = {
    "device": None,
    "temperature": None,
    "humidity": None
}

@app.post("/sensor/dht")
def post_dht(data: DHTData):
    dht_state["device"] = data.device
    dht_state["temperature"] = data.temperature
    dht_state["humidity"] = data.humidity
    return {
        "status": "ok",
        "message": "DHT data received",
        "device": data.device,
        "temperature": data.temperature,
        "humidity": data.humidity
    }

@app.get("/sensor/dht")
def get_dht():
    return dht_state
