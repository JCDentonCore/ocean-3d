# ocean-3d — Changelog

## 2026-09-07 — v1.2 (cardume: más peces, velocidad y profundidad ajustables, huida)

### Features
- **Población 14 → 48 peces** (misma geometría ~28 verts/pez; 48 draw calls
  son triviales).
- **Peces ahora integrados en CPU** (posición/velocidad por pez) en vez de
  función pura del tiempo:
  - "Casa" = punto de la órbita alrededor de un centro 26 m delante de la
    cámara (el cardume sigue al jugador, como antes).
  - Muelle de vuelta a la casa (`FISH_SPRING` 2.0 1/s²) + amortiguación
    (`FISH_DAMP` 1.5 1/s).
  - **Huida:** repulsión radial desde la cámara cuando distancia <
    `FLEE_RADIUS` (12 m); fuerza ∝ (1−d/R)², pico `FLEE_FORCE` 24 m/s²,
    tope de velocidad 14 m/s. El cardume se dispersa al acercarse y
    reagrupa en la órbita al alejarse.
  - **Orientación** = vector velocidad (fallback a la tangente de la
    órbita); **tail-wag** 3× más rápido durante la huida.
  - **Orden lejante→cercaño** por frame (sin depth test entre peces, el
    orden de dibujo es su único orden de profundidad).
  - Limitados a estar bajo el agua (−0.3…−14 m).
- **Ajuste en tiempo real** (se muestra en el título de la ventana):
  - `[` / `]`: velocidad de nado `gFishSwim` (0.4–8 m/s, default 2.2).
  - `Y` / `T`: profundidad del cardume `gSchoolDepth` (1–10 m, default
    4 m; spread vertical ±2.5 m) — mantener pulsado para ajustar.
- **`--screenshot --time T`:** el simulacro del cardume hace fast-forward
  hasta T (pasos de 60 fps), así la imagen corresponde a ese instante.
- **Verificación:** 0 errores GL en llvmpipe y RX 7900 XT (radeonsi).
  Detector de clusters (`verify_fish.py`): ~108–114 clusters en la región
  subacuática; posiciones idénticas entre drivers a t=2.0 (determinista) y
  distintas entre t=2.0 y t=3.5 (peces en movimiento). El cluster grande
  junto a la cámara (~3000 px) = pez huyendo cerca del lente. Demo en
  `cardume.gif`.

### Bug
- El fast-forward inicial estaba gateado en `--headless`, pero
  `--screenshot` corre windowed: el screenshot salía congelado en t=0.
  Fix: gatear solo en `--time`.

## 2026-09-07 — v1.1 (peces)

### Feature: cardume de peces bajo la superficie
- **14 peces** low-poly (cuerpo hexagonal de 5 anillos + cola en diamante,
  ~28 vértices cada uno) nadando en órbitas alrededor de un punto 26 m
  adelante de la cámara y 2–6 m bajo la superficie — el cardume sigue al
  jugador por donde vaya.
- **Movimiento:** cada pez tiene radio/velocidad/fase de órbita, bob
  vertical, escala, paleta plateada azulada con varianza, y **tail-wag**
  animado en el vertex shader (la cola se flexiona con `sin(uTail)`).
- **Orientación:** matriz 3×3 por pez (right/up/fwd) calculada en CPU desde
  la tangente de la órbita — el pez apunta hacia donde nada.
- **Vista subacuática:** los peces se dibujan *después* del agua opaca con
  `glDisable(GL_DEPTH_TEST)` + tinte de profundidad en el fragment shader
  (más azul/oscuro con la profundidad). El truco clásico de "ver a través
  del agua" — no requiere alpha blending.
- **Bug de Mesa 26.2.2 descubierto en el camino:** `glEnable(GL_BLEND)` y
  `glDisable(GL_BLEND)` devuelven `GL_INVALID_OPERATION` (0x500) en **todos**
  los contextos (3.3/4.6 core) y **ambos** drivers (llvmpipe y radeonsi) en
  esta build de CachyOS. `glBlendFunc`/`glBlendEquation`/`glGetIntegerv(GL_BLEND)`
  funcionan — solo el enable/disable del cap está roto. Workaround: no usar
  blending (diseño de los peces opacos + depth off). Probado con
  `dlopen` directo a libGL y GLX puro: mismo error → es del driver, no del
  loader de GLFW.
- **Verificación:** llvmpipe y RX 7900 XT, 0 errores GL. Detección de
  componentes conectados en el screenshot: 13 clusters alargados compactos
  (64–2216 px) en la región subacuática = peces renderizados (misma
  detección en ambos drivers; la escena es determinista).

## 2026-09-07 — v1.0 (funcional)

### Fix crítico: el océano no se renderizaba
- **Síntoma:** `ocean.png` era solo el clear color celeste plano (153,194,224).
  Sky y water no se dibujaban; `glDraw*` fallaba con `GL_INVALID_OPERATION` (0x502).
- **Root cause:** Mesa/llvmpipe rechaza `glVertexAttribPointer` cuando se llama
  contra el **VAO por defecto** (buffer de estado implícito no soportado por el
  driver software). `glGenBuffers`/`glBindBuffer`/`glBufferData` y
  `glEnableVertexAttribArray` funcionaban, pero el pointer del atributo 0 nunca
  se aplicaba → draw → 0x502.
- **Fix:** VAOs explícitos.
  - `skyVAO`: fullscreen triangle, atributo 0 = vec2 NDC, `glDrawArrays`.
  - `waterVAO`: grid 256×256, atributo 0 = vec3 pos, IBO de índices, `glDrawElements`.
  - `glVertexAttribPointer` + `glEnableVertexAttribArray` se configuran una vez
    al setup dentro de cada VAO; en el frame loop solo `glBindVertexArray` + draw.
  - Agregados PFN + carga runtime de `glGenVertexArrays`, `glBindVertexArray`,
    `glDeleteVertexArrays`.
- **Limpieza:** quitados todos los diagnostics (DIAG binds, glGetError spam).
- **Verificación:**
  - llvmpipe: 0 errores GL, 496 colores únicos en screenshot, agua azul profundo
    dominante (29,113,142) + cielo.
  - radeonsi (RX 7900 XT, ACO, navi31): 0 errores GL, 1282 colores únicos,
    0.3% de la imagen es el clear color (el resto es agua/cielo renderizados).

## 2026-09-02 — v0.9 (primera iteración)

### Features
- Océano infinit: grid 256×256 (SPAN 800) centrado en el entero XZ de la
  cámara — sin uploads por frame.
- 5 olas de Gerstner, desplazamiento en GPU (vertex shader), normales
  analíticas en el fragment shader.
- Sky shader: gradiente zenit/horizonte + sol (halo + disco).
- Water shader: mezcla deep/shallow por normal, fresnel, especular solar,
  niebla lineal (FOG_NEAR 180, FOG_FAR 398).
- Cámara libre: WASD + QE, Shift boost, mouse look/pan, rueda velocidad,
  R reset, contador FPS en el título.
- GL 3.3 core cargado 100% a runtime vía `glfwGetProcAddress`
  (sin GLEW, sin `<GL/gl.h>`, `GLFW_INCLUDE_NONE`).
- Matrices 4×4 propias (perspective, lookAt, translate, inverse, mul).
- PNG writer propio (RGBA8 + zlib) para screenshots.
- Modo headless: `--screenshot` / `--headless` / `--time T`.

### Bug (abierto al cierre)
- Océano invisible: solo se veía el fondo celeste. Investigado como
  ACO/radeonsi driver bug o VRAM; descartado VRAM (llvmpipe lo reproduce).
  Causa real hallada el 2026-09-07 (ver arriba).
