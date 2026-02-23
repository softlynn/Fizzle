const prefersReducedMotion = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
const hasFinePointer = window.matchMedia("(pointer:fine)").matches;
const clamp01 = (value) => Math.min(1, Math.max(0, value));
const smooth = (from, to, alpha) => from + (to - from) * alpha;

setupReveals();
setupCtaGlow();
setupTiltCard();
setupAmbientGradient();
void initBackdropVisual();

function setupReveals() {
  const reveals = document.querySelectorAll(".reveal");
  if (prefersReducedMotion) {
    reveals.forEach((node) => node.classList.add("show"));
    return;
  }

  const io = new IntersectionObserver(
    (entries) => {
      entries.forEach((entry, index) => {
        if (!entry.isIntersecting) return;
        setTimeout(() => entry.target.classList.add("show"), index * 45);
        io.unobserve(entry.target);
      });
    },
    { threshold: 0.12 },
  );

  reveals.forEach((node) => io.observe(node));
}

function setupCtaGlow() {
  const cta = document.getElementById("downloadBtn");
  if (!cta) return;

  cta.addEventListener("mousemove", (event) => {
    const rect = cta.getBoundingClientRect();
    const x = event.clientX - rect.left;
    const y = event.clientY - rect.top;
    cta.style.background = `radial-gradient(circle at ${x}px ${y}px, #dff6ff 0%, #adddff 40%, #6dbbff 74%)`;
  });

  cta.addEventListener("mouseleave", () => {
    cta.style.background = "linear-gradient(90deg, #6dbbff, #a4ddff)";
  });
}

function setupTiltCard() {
  const tiltCard = document.getElementById("tiltCard");
  if (!tiltCard || prefersReducedMotion || !hasFinePointer) return;

  const damp = 9;
  tiltCard.addEventListener("mousemove", (event) => {
    const rect = tiltCard.getBoundingClientRect();
    const px = (event.clientX - rect.left) / rect.width;
    const py = (event.clientY - rect.top) / rect.height;
    const rx = (0.5 - py) * damp;
    const ry = (px - 0.5) * damp;
    tiltCard.style.transform = `perspective(900px) rotateX(${rx}deg) rotateY(${ry}deg) translateZ(0)`;
  });

  tiltCard.addEventListener("mouseleave", () => {
    tiltCard.style.transform = "perspective(900px) rotateX(0deg) rotateY(0deg)";
  });
}

function setupAmbientGradient() {
  if (prefersReducedMotion) return;

  const root = document.documentElement;
  let targetX = 50;
  let targetY = 20;
  let viewX = 50;
  let viewY = 20;

  window.addEventListener(
    "pointermove",
    (event) => {
      targetX = (event.clientX / Math.max(1, window.innerWidth)) * 100;
      targetY = (event.clientY / Math.max(1, window.innerHeight)) * 100;
    },
    { passive: true },
  );

  const step = () => {
    viewX = smooth(viewX, targetX, 0.08);
    viewY = smooth(viewY, targetY, 0.08);
    root.style.setProperty("--mx", `${viewX.toFixed(2)}%`);
    root.style.setProperty("--my", `${viewY.toFixed(2)}%`);
    requestAnimationFrame(step);
  };

  requestAnimationFrame(step);
}

async function initBackdropVisual() {
  const canvas = document.getElementById("siteGpuBackdrop");
  if (!canvas) return;

  const setFallback = () => {
    document.body.classList.add("gpu-fallback");
  };

  if (!("gpu" in navigator)) {
    setFallback();
    return;
  }

  try {
    const module = await import("https://esm.sh/typegpu?bundle");
    const tgpu = module.default ?? module.tgpu ?? module;
    if (!tgpu || typeof tgpu.init !== "function") throw new Error("runtime unavailable");

    const runtime = await tgpu.init();
    const device = runtime?.device;
    if (!device) throw new Error("GPU device unavailable");

    const context = canvas.getContext("webgpu");
    if (!context) throw new Error("WebGPU context unavailable");

    const format = navigator.gpu.getPreferredCanvasFormat();
    const shaderCode = `
struct Uniforms {
  resolution: vec2<f32>,
  pointer: vec2<f32>,
  time: f32,
  intensity: f32,
  pointerMix: f32,
  pad0: vec3<f32>,
};

@group(0) @binding(0) var<uniform> u: Uniforms;

fn hash21(p: vec2<f32>) -> f32 {
  return fract(sin(dot(p, vec2<f32>(127.1, 311.7))) * 43758.5453123);
}

fn attractor(posIn: vec2<f32>, t: f32) -> f32 {
  var z = posIn;
  let a = 1.42 + 0.18 * sin(t * 0.23);
  let b = -2.31 + 0.17 * cos(t * 0.19);
  let c = 2.21 + 0.15 * sin(t * 0.17);
  let d = -2.07 + 0.13 * cos(t * 0.11);

  var acc = 0.0;
  for (var i: i32 = 0; i < 18; i = i + 1) {
    let x = z.x;
    let y = z.y;
    z = vec2<f32>(sin(a * y) - cos(b * x), sin(c * x) - cos(d * y));
    let q = vec2<f32>(
      0.5 + 0.42 * z.x,
      0.5 + 0.42 * z.y
    );
    let dd = distance(q, posIn * 0.5 + vec2<f32>(0.25, 0.25));
    acc += exp(-dd * 8.0);
  }
  return acc / 18.0;
}

struct VSOut {
  @builtin(position) pos: vec4<f32>,
};

@vertex
fn vs(@builtin(vertex_index) i: u32) -> VSOut {
  var verts = array<vec2<f32>, 6>(
    vec2<f32>(-1.0, -1.0),
    vec2<f32>( 1.0, -1.0),
    vec2<f32>(-1.0,  1.0),
    vec2<f32>(-1.0,  1.0),
    vec2<f32>( 1.0, -1.0),
    vec2<f32>( 1.0,  1.0)
  );
  var out: VSOut;
  out.pos = vec4<f32>(verts[i], 0.0, 1.0);
  return out;
}

@fragment
fn fs(@builtin(position) frag: vec4<f32>) -> @location(0) vec4<f32> {
  let uv = frag.xy / u.resolution;
  let aspect = u.resolution.x / max(1.0, u.resolution.y);
  let p = (uv - 0.5) * vec2<f32>(aspect, 1.0);

  let t = u.time;
  let drift = vec2<f32>(
    0.12 * sin(t * 0.13),
    0.11 * cos(t * 0.11)
  );

  let fieldA = attractor(uv + drift, t * 0.9);
  let fieldB = attractor(uv * vec2<f32>(1.06, 0.98) - drift * 0.7, t * 0.7 + 1.8);
  let field = 0.62 * fieldA + 0.38 * fieldB;

  let ripple = sin((p.x * 5.0 + t * 0.9) + cos(p.y * 6.5 - t * 0.6)) * 0.5 + 0.5;
  let pointerGlow = exp(-distance(uv, u.pointer) * 4.6) * u.pointerMix;

  let energy = clamp(field * 0.95 + ripple * 0.25 + pointerGlow * 0.65, 0.0, 1.0);

  let deep = vec3<f32>(0.02, 0.06, 0.13);
  let mid = vec3<f32>(0.10, 0.24, 0.48);
  let bright = vec3<f32>(0.45, 0.79, 1.0);
  let foam = vec3<f32>(0.74, 0.94, 1.0);

  var color = mix(deep, mid, smoothstep(0.05, 0.50, energy));
  color = mix(color, bright, smoothstep(0.36, 0.82, energy));
  color += foam * smoothstep(0.82, 1.16, energy) * 0.28;

  let grain = (hash21(uv * u.resolution + vec2<f32>(t * 12.0, t * 7.0)) - 0.5) * 0.03;
  color = clamp(color + grain, vec3<f32>(0.0), vec3<f32>(1.0));

  let alpha = clamp(0.14 + u.intensity * 0.48, 0.0, 0.9);
  return vec4<f32>(color, alpha);
}
`;

    const shader = device.createShaderModule({ code: shaderCode });
    const bindGroupLayout = device.createBindGroupLayout({
      entries: [{ binding: 0, visibility: GPUShaderStage.FRAGMENT, buffer: { type: "uniform" } }],
    });

    const pipeline = device.createRenderPipeline({
      layout: device.createPipelineLayout({ bindGroupLayouts: [bindGroupLayout] }),
      vertex: { module: shader, entryPoint: "vs" },
      fragment: { module: shader, entryPoint: "fs", targets: [{ format }] },
      primitive: { topology: "triangle-list" },
    });

    const uniformBuffer = device.createBuffer({
      size: 8 * 4,
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
    });

    const bindGroup = device.createBindGroup({
      layout: bindGroupLayout,
      entries: [{ binding: 0, resource: { buffer: uniformBuffer } }],
    });

    let pointerTargetX = 0.5;
    let pointerTargetY = 0.5;
    let pointerX = 0.5;
    let pointerY = 0.5;
    let pointerMix = 0;

    window.addEventListener(
      "pointermove",
      (event) => {
        pointerTargetX = clamp01(event.clientX / Math.max(1, window.innerWidth));
        pointerTargetY = clamp01(event.clientY / Math.max(1, window.innerHeight));
        pointerMix = 1;
      },
      { passive: true },
    );

    window.addEventListener("pointerleave", () => {
      pointerMix = 0;
    });

    let pixelWidth = 0;
    let pixelHeight = 0;

    const resize = () => {
      const dpr = Math.min(window.devicePixelRatio || 1, 1.2);
      const w = Math.max(1, Math.floor(window.innerWidth * dpr));
      const h = Math.max(1, Math.floor(window.innerHeight * dpr));
      if (w === pixelWidth && h === pixelHeight) return;

      pixelWidth = w;
      pixelHeight = h;
      canvas.width = w;
      canvas.height = h;
      context.configure({ device, format, alphaMode: "premultiplied" });
    };

    window.addEventListener("resize", resize, { passive: true });
    resize();

    const uniforms = new Float32Array(8);
    const start = performance.now();
    let rafId = 0;
    let lastFrame = 0;
    const minFrameStep = prefersReducedMotion ? 34 : 16;

    const frame = (now) => {
      rafId = requestAnimationFrame(frame);
      if (document.visibilityState !== "visible") return;
      if (now - lastFrame < minFrameStep) return;
      lastFrame = now;

      pointerX = smooth(pointerX, pointerTargetX, 0.12);
      pointerY = smooth(pointerY, pointerTargetY, 0.12);

      uniforms[0] = pixelWidth;
      uniforms[1] = pixelHeight;
      uniforms[2] = pointerX;
      uniforms[3] = pointerY;
      uniforms[4] = (now - start) * 0.001;
      uniforms[5] = prefersReducedMotion ? 0.28 : 0.72;
      uniforms[6] = pointerMix;
      uniforms[7] = 0;
      device.queue.writeBuffer(uniformBuffer, 0, uniforms);

      const encoder = device.createCommandEncoder();
      const texture = context.getCurrentTexture();
      const pass = encoder.beginRenderPass({
        colorAttachments: [
          {
            view: texture.createView(),
            clearValue: { r: 0, g: 0, b: 0, a: 0 },
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      });

      pass.setPipeline(pipeline);
      pass.setBindGroup(0, bindGroup);
      pass.draw(6, 1, 0, 0);
      pass.end();
      device.queue.submit([encoder.finish()]);
    };

    rafId = requestAnimationFrame(frame);

    window.addEventListener("beforeunload", () => {
      cancelAnimationFrame(rafId);
      if (typeof runtime.destroy === "function") runtime.destroy();
    });
  } catch (error) {
    console.error("[Fizzle Site] Background visual failed:", error);
    setFallback();
  }
}
