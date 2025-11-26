FROM python:3.10-slim

# ============================================================
# Install system dependencies for OpenCV, Numpy, YOLO, Torch
# ============================================================
RUN apt-get update && apt-get install -y --no-install-recommends \
    libgl1 \
    libglib2.0-0 \
    libstdc++6 \
    libgomp1 \
    libatlas3-base \
    libopenblas-dev \
    liblapack-dev \
    gfortran \
    build-essential \
    python3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# ============================================================
# Install PyTorch (CPU Version)
# ============================================================
RUN pip install --no-cache-dir --upgrade pip \
    && pip install --no-cache-dir \
        torch==2.1.0 \
        torchvision \
        torchaudio \
        --index-url https://download.pytorch.org/whl/cpu

# ============================================================
# Install other Python dependencies
# ============================================================
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt \
    && rm -rf /root/.cache/pip

COPY . .

EXPOSE 8000

# ============================================================
# Run FastAPI using uvicorn
# ============================================================
CMD ["uvicorn", "app:app", "--host", "0.0.0.0", "--port", "8000"]
