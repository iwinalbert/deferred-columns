import onnxruntime as ort
import numpy as np
import time

session = ort.InferenceSession("model.onnx", providers=['CPUExecutionProvider'])
# 500K rows (5% of 10M)
X = np.random.rand(500000, 3).astype(np.float32)

t0 = time.time()
res = session.run(["variable"], {"float_input": X})[0]
t1 = time.time()

print(f"ONNX inference time for 500K rows (batch size 500K): {(t1-t0)*1000:.1f} ms")

t2 = time.time()
batch_size = 2048
for i in range(0, len(X), batch_size):
    batch = X[i:i+batch_size]
    session.run(["variable"], {"float_input": batch})
t3 = time.time()

print(f"ONNX inference time for 500K rows (batch size 2048): {(t3-t2)*1000:.1f} ms")
