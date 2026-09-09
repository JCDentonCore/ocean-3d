# ocean-3d — Estado (actualizado 2026-09-07, v1.2)

## Estado actual: v1.2 — Océano + cardume de 48 peces con huida ✅

### Cómo ver
```bash
cd ~/.openclaw/workspace/ocean-3d
g++ -O2 -std=c++17 src/ocean.cpp -o ocean $(pkg-config --cflags --libs glfw3) -lz
./ocean                 # ventana interactiva
./ocean --screenshot --time 2.0        # screenshot headless (GPU)
LIBGL_ALWAYS_SOFTWARE=1 ./ocean --screenshot --time 2.0   # sin GPU
```

### Lo que hay
- Océano infinito con 5 olas de Gerstner en GPU, sky con sol, niebla.
- **48 peces low-poly** en cardume orbital 26 m adelante de la cámara,
  ~4 m bajo la superficie (spread ±2.5 m). Integrados en CPU: muelle a la
  órbita + amortiguación + **repulsión radial al acercarse la cámara**
  (radio 12 m, fuerza ∝ (1−d/R)², pico 24 m/s², tope 14 m/s). Se dispersan
  al acercarte y reagrupan al alejarte. Tail-wag 3× más rápido huyendo.
  Orientación = vector velocidad; orden lejante→cercaño; tinte de
  profundidad en fragment shader.
- **Ajustes en vivo:** `[`/`]` velocidad de nado (0.4–8 m/s, default 2.2);
  `Y`/`T` profundidad del cardume (1–10 m, default 4). Aparecen en el
  título de la ventana.
- `--screenshot --time T` hace fast-forward del simulacro hasta T.
- Los peces se dibujan tras el agua opaca con depth test OFF (ver bug de Mesa).

### Bugs de Mesa 26.2.2 documentados (CachyOS, kernel 7.2.x)
1. **VAO por defecto:** `glVertexAttribPointer` contra el VAO por defecto →
   `GL_INVALID_OPERATION` (0x502). Fix: VAOs explícitos (v1.0).
2. **Blend cap roto:** `glEnable(GL_BLEND)`/`glDisable(GL_BLEND)` →
   `GL_INVALID_OPERATION` (0x500) en todos los perfiles/drivers.
   `glBlendFunc`/`glBlendEquation`/query funcionan. Workaround: sin blending,
   peces opacos + depth off (v1.1). Probado llvmpipe, softpipe, radeonsi,
   GLX puro + dlopen → es del driver.

### Archivos
- `src/ocean.cpp` — programa completo (GL 3.3 core, loader runtime, PNG writer)
- `ocean` — binario compilado
- `ocean.png` — último screenshot
- `cardume.gif` — demo de 4 frames de la huida (GPU)
- `verify_fish.py` — detector de clusters de peces en screenshots
- `CHANGELOG.md` — historia completa
- `NOTES.md` — este archivo
