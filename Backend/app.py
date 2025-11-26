import os
from uuid import uuid4
from fastapi import FastAPI, UploadFile, File, HTTPException
from fastapi.responses import HTMLResponse, FileResponse
from fastapi.middleware.cors import CORSMiddleware
from ultralytics import YOLO

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
    """
    Menghitung jumlah cabai matang & mentah.
    - Kelas 0 = ripe
    - Kelas 1 = unripe
    """
    boxes = result.boxes

    if len(boxes) == 0:
        return -1, 0, 0, 0

    total = len(boxes)
    ripe = sum(1 for b in boxes if int(b.cls) == 0)
    unripe = sum(1 for b in boxes if int(b.cls) == 1)

    # pilihan utama (pakai box confidence tertinggi)
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

    with open(filepath, "wb") as f:
        f.write(await file.read())

    cleanup_uploads()

    try:
        result = chili_model.predict(filepath, verbose=False)[0]
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Inference error: {str(e)}")

    # analisis jumlah & kategori
    ripeness, total, ripe, unripe = analyze_chili_boxes(result)

    # simpan state terbaru
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
    <head><title>Chili Viewer</title></head>
    <body style="font-family:Arial;">
        <h2>Chili Ripeness Detection</h2>

        <img id="img" src="/chili/image" width="320" />
        <p>Status: <span id="status">Loading...</span></p>
        <p>Total Detected: <span id="total">0</span></p>
        <p>Ripe (Merah): <span id="ripe">0</span></p>
        <p>Unripe (Hijau): <span id="unripe">0</span></p>

        <script>
            async function update() {
                try {
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
