/*
 * Wolf3D voxel viewer.
 *
 * Shows the MagicaVoxel files written by wolf_sprite_voxelizer: every subject
 * (actor, object, weapon), each of its animations and their poses, with the
 * original sprite frames beside them for comparison. All data comes from
 * viewer_data.js, which the tool writes; nothing is fetched from the network.
 */
'use strict';

const VOXEL_DIM = 64;
const TIC_RATE_HZ = 70;
const UNIFORM_FRAME_SECONDS = 1 / 6;

/* ------------------------------------------------------------------ */
/* MagicaVoxel parsing                                                 */
/* ------------------------------------------------------------------ */

function decodeBase64(text) {
  const binary = atob(text);
  const bytes = new Uint8Array(binary.length);
  for (let index = 0; index < binary.length; index += 1) {
    bytes[index] = binary.charCodeAt(index);
  }
  return bytes;
}

/*
 * Read the models, palette and named animated objects of a .vox file. Each
 * object is a transform node (carrying "_name") pointing at a shape node
 * whose models are keyframes ("_f").
 */
function parseVox(bytes) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const ascii = (offset) => String.fromCharCode(bytes[offset], bytes[offset + 1], bytes[offset + 2], bytes[offset + 3]);
  if (ascii(0) !== 'VOX ' || ascii(8) !== 'MAIN') {
    throw new Error('Not a MagicaVoxel file');
  }
  const readString = (state) => {
    const length = view.getUint32(state.offset, true);
    const text = new TextDecoder().decode(bytes.subarray(state.offset + 4, state.offset + 4 + length));
    state.offset += 4 + length;
    return text;
  };
  const readDictionary = (state) => {
    const pairs = view.getUint32(state.offset, true);
    const dictionary = {};
    state.offset += 4;
    for (let pair = 0; pair < pairs; pair += 1) {
      const key = readString(state);
      dictionary[key] = readString(state);
    }
    return dictionary;
  };

  const models = [];
  const transforms = [];
  const shapes = [];
  let palette = null;
  let size = null;
  let offset = 20 + view.getUint32(12, true);
  const end = offset + view.getUint32(16, true);

  while (offset + 12 <= end) {
    const id = ascii(offset);
    const contentSize = view.getUint32(offset + 4, true);
    const childrenSize = view.getUint32(offset + 8, true);
    const content = offset + 12;

    if (id === 'SIZE') {
      size = [view.getUint32(content, true), view.getUint32(content + 4, true), view.getUint32(content + 8, true)];
    } else if (id === 'XYZI') {
      const count = view.getUint32(content, true);
      models.push({ size, voxels: bytes.subarray(content + 4, content + 4 + count * 4) });
    } else if (id === 'RGBA') {
      palette = bytes.slice(content, content + 1024);
    } else if (id === 'nTRN') {
      const state = { offset: content + 4 };
      const attributes = readDictionary(state);
      transforms.push({ id: view.getUint32(content, true), name: attributes._name || '', child: view.getUint32(state.offset, true) });
    } else if (id === 'nSHP') {
      const state = { offset: content + 4 };
      readDictionary(state);
      const count = view.getUint32(state.offset, true);
      const frames = [];
      state.offset += 4;
      for (let model = 0; model < count; model += 1) {
        const modelId = view.getUint32(state.offset, true);
        state.offset += 4;
        const attributes = readDictionary(state);
        frames.push({ model: modelId, frame: Number(attributes._f || model) });
      }
      frames.sort((a, b) => a.frame - b.frame);
      shapes.push({ id: view.getUint32(content, true), frames });
    }
    offset += 12 + contentSize + childrenSize;
  }

  /* A file without a scene graph holds one unnamed object of all its models. */
  const objects = shapes.length > 0
    ? shapes.map((shape) => ({
      name: (transforms.find((transform) => transform.child === shape.id) || {}).name || 'object',
      models: shape.frames.map((entry) => entry.model),
    }))
    : [{ name: 'object', models: models.map((_, index) => index) }];
  return { models, palette, objects };
}

/* ------------------------------------------------------------------ */
/* Meshing                                                             */
/* ------------------------------------------------------------------ */

/* Corner offsets of the six cube faces in VOX coordinates (x right, y away, z up). */
const FACES = [
  { normal: [1, 0, 0], corners: [[1, 0, 0], [1, 1, 0], [1, 1, 1], [1, 0, 1]] },
  { normal: [-1, 0, 0], corners: [[0, 0, 0], [0, 0, 1], [0, 1, 1], [0, 1, 0]] },
  { normal: [0, 1, 0], corners: [[0, 1, 0], [0, 1, 1], [1, 1, 1], [1, 1, 0]] },
  { normal: [0, -1, 0], corners: [[0, 0, 0], [1, 0, 0], [1, 0, 1], [0, 0, 1]] },
  { normal: [0, 0, 1], corners: [[0, 0, 1], [1, 0, 1], [1, 1, 1], [0, 1, 1]] },
  { normal: [0, 0, -1], corners: [[0, 0, 0], [0, 1, 0], [1, 1, 0], [1, 0, 0]] },
];

/*
 * VOX is z-up with the actor facing -y; WebGL here is y-up with the default
 * viewer on +z. (x, y, z) maps to (x - 32, z, 32 - y): a proper rotation, so
 * the front of the actor faces the default camera and the floor is y = 0.
 */
function toWorld(x, y, z) {
  return [x - VOXEL_DIM / 2, z, VOXEL_DIM / 2 - y];
}

/* Emit two triangles for every voxel face that touches empty space. */
function buildMesh(model, palette) {
  const [sizeX, sizeY, sizeZ] = model.size;
  const grid = new Uint8Array(sizeX * sizeY * sizeZ);
  const index = (x, y, z) => (z * sizeY + y) * sizeX + x;
  const filled = (x, y, z) => x >= 0 && y >= 0 && z >= 0 && x < sizeX && y < sizeY && z < sizeZ && grid[index(x, y, z)] !== 0;

  for (let voxel = 0; voxel < model.voxels.length; voxel += 4) {
    grid[index(model.voxels[voxel], model.voxels[voxel + 1], model.voxels[voxel + 2])] = model.voxels[voxel + 3];
  }

  const positions = [];
  const normals = [];
  const colours = [];
  for (let voxel = 0; voxel < model.voxels.length; voxel += 4) {
    const x = model.voxels[voxel];
    const y = model.voxels[voxel + 1];
    const z = model.voxels[voxel + 2];
    const colourIndex = model.voxels[voxel + 3] - 1;
    const rgb = [palette[colourIndex * 4], palette[colourIndex * 4 + 1], palette[colourIndex * 4 + 2]];

    for (const face of FACES) {
      if (filled(x + face.normal[0], y + face.normal[1], z + face.normal[2])) {
        continue;
      }
      const corners = face.corners.map((corner) => toWorld(x + corner[0], y + corner[1], z + corner[2]));
      const normal = [face.normal[0], face.normal[2], -face.normal[1]];
      for (const corner of [0, 1, 2, 0, 2, 3]) {
        positions.push(...corners[corner]);
        normals.push(...normal);
        colours.push(...rgb);
      }
    }
  }
  return {
    positions: new Float32Array(positions),
    normals: new Float32Array(normals),
    colours: new Uint8Array(colours),
    vertexCount: positions.length / 3,
  };
}

/* ------------------------------------------------------------------ */
/* Matrices                                                            */
/* ------------------------------------------------------------------ */

function perspective(fieldOfView, aspect, near, far) {
  const f = 1 / Math.tan(fieldOfView / 2);
  return [f / aspect, 0, 0, 0, 0, f, 0, 0, 0, 0, (far + near) / (near - far), -1, 0, 0, (2 * far * near) / (near - far), 0];
}

function orthographic(halfHeight, aspect, near, far) {
  const halfWidth = halfHeight * aspect;
  return [1 / halfWidth, 0, 0, 0, 0, 1 / halfHeight, 0, 0, 0, 0, -2 / (far - near), 0, 0, 0, -(far + near) / (far - near), 1];
}

function lookAt(eye, target, up) {
  const subtract = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
  const normalise = (v) => { const length = Math.hypot(...v) || 1; return v.map((c) => c / length); };
  const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
  const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  const forward = normalise(subtract(eye, target));
  const right = normalise(cross(up, forward));
  const trueUp = cross(forward, right);
  return [right[0], trueUp[0], forward[0], 0, right[1], trueUp[1], forward[1], 0, right[2], trueUp[2], forward[2], 0,
    -dot(right, eye), -dot(trueUp, eye), -dot(forward, eye), 1];
}

function multiply(a, b) {
  const result = new Array(16).fill(0);
  for (let column = 0; column < 4; column += 1) {
    for (let row = 0; row < 4; row += 1) {
      for (let k = 0; k < 4; k += 1) {
        result[column * 4 + row] += a[k * 4 + row] * b[column * 4 + k];
      }
    }
  }
  return result;
}

/* ------------------------------------------------------------------ */
/* Rendering                                                           */
/* ------------------------------------------------------------------ */

const VERTEX_SHADER = `#version 300 es
in vec3 position;
in vec3 normal;
in vec3 colour;
uniform mat4 viewProjection;
out vec3 vertexNormal;
out vec3 vertexColour;
void main() {
  vertexNormal = normal;
  vertexColour = colour;
  gl_Position = viewProjection * vec4(position, 1.0);
}`;

/* Flat key and fill lighting; voxel faces are axis aligned, so every face reads clearly. */
const FRAGMENT_SHADER = `#version 300 es
precision mediump float;
in vec3 vertexNormal;
in vec3 vertexColour;
uniform float lit;
out vec4 fragmentColour;
void main() {
  vec3 key = normalize(vec3(0.45, 0.8, 0.55));
  vec3 fill = normalize(vec3(-0.6, 0.3, -0.4));
  float light = 0.42 + 0.48 * max(dot(vertexNormal, key), 0.0) + 0.18 * max(dot(vertexNormal, fill), 0.0);
  fragmentColour = vec4(vertexColour * mix(1.0, light, lit), 1.0);
}`;

class Renderer {
  constructor(canvas) {
    this.canvas = canvas;
    this.gl = canvas.getContext('webgl2', { antialias: true });
    if (!this.gl) {
      throw new Error('WebGL2 is not available in this browser.');
    }
    const gl = this.gl;
    const compile = (type, source) => {
      const shader = gl.createShader(type);
      gl.shaderSource(shader, source);
      gl.compileShader(shader);
      if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
        throw new Error(gl.getShaderInfoLog(shader));
      }
      return shader;
    };
    this.program = gl.createProgram();
    gl.attachShader(this.program, compile(gl.VERTEX_SHADER, VERTEX_SHADER));
    gl.attachShader(this.program, compile(gl.FRAGMENT_SHADER, FRAGMENT_SHADER));
    gl.linkProgram(this.program);
    this.locations = {
      position: gl.getAttribLocation(this.program, 'position'),
      normal: gl.getAttribLocation(this.program, 'normal'),
      colour: gl.getAttribLocation(this.program, 'colour'),
      viewProjection: gl.getUniformLocation(this.program, 'viewProjection'),
      lit: gl.getUniformLocation(this.program, 'lit'),
    };
    this.floor = this.upload(this.buildFloor(), gl.LINES);
    gl.enable(gl.DEPTH_TEST);
  }

  /* One map tile (64 voxels) outlined, with an 8-voxel grid, on the floor plane. */
  buildFloor() {
    const positions = [];
    const half = VOXEL_DIM / 2;
    for (let line = -half; line <= half; line += 8) {
      positions.push(line, 0, -half, line, 0, half, -half, 0, line, half, 0, line);
    }
    const count = positions.length / 3;
    const colours = [];
    for (let vertex = 0; vertex < count; vertex += 1) {
      colours.push(70, 76, 96);
    }
    return {
      positions: new Float32Array(positions),
      normals: new Float32Array(count * 3).map((_, i) => (i % 3 === 1 ? 1 : 0)),
      colours: new Uint8Array(colours),
      vertexCount: count,
    };
  }

  upload(mesh, mode) {
    const gl = this.gl;
    const vertexArray = gl.createVertexArray();
    gl.bindVertexArray(vertexArray);
    const attribute = (location, data, size, type, normalised) => {
      const buffer = gl.createBuffer();
      gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
      gl.bufferData(gl.ARRAY_BUFFER, data, gl.STATIC_DRAW);
      gl.enableVertexAttribArray(location);
      gl.vertexAttribPointer(location, size, type, normalised, 0, 0);
    };
    attribute(this.locations.position, mesh.positions, 3, gl.FLOAT, false);
    attribute(this.locations.normal, mesh.normals, 3, gl.FLOAT, false);
    attribute(this.locations.colour, mesh.colours, 3, gl.UNSIGNED_BYTE, true);
    gl.bindVertexArray(null);
    return { vertexArray, count: mesh.vertexCount, mode: mode === undefined ? gl.TRIANGLES : mode };
  }

  resize() {
    const ratio = window.devicePixelRatio || 1;
    const width = Math.max(1, Math.round(this.canvas.clientWidth * ratio));
    const height = Math.max(1, Math.round(this.canvas.clientHeight * ratio));
    if (this.canvas.width !== width || this.canvas.height !== height) {
      this.canvas.width = width;
      this.canvas.height = height;
    }
  }

  draw(meshHandle, camera, showFloor) {
    const gl = this.gl;
    this.resize();
    gl.viewport(0, 0, this.canvas.width, this.canvas.height);
    gl.clearColor(0.086, 0.094, 0.122, 1);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    gl.useProgram(this.program);
    gl.uniformMatrix4fv(this.locations.viewProjection, false, camera.matrix(this.canvas.width / this.canvas.height));
    if (showFloor) {
      gl.uniform1f(this.locations.lit, 0);
      gl.bindVertexArray(this.floor.vertexArray);
      gl.drawArrays(this.floor.mode, 0, this.floor.count);
    }
    if (meshHandle) {
      gl.uniform1f(this.locations.lit, 1);
      gl.bindVertexArray(meshHandle.vertexArray);
      gl.drawArrays(meshHandle.mode, 0, meshHandle.count);
    }
    gl.bindVertexArray(null);
  }
}

/*
 * Orbit camera around the model. Yaw 0 is the front view; yaw 45*k degrees is
 * the original rotation k, matching the tool's convention.
 */
class OrbitCamera {
  constructor() { this.reset(); }

  reset() {
    this.yaw = -0.6;
    this.pitch = 0.35;
    this.distance = 150;
    this.target = [0, 28, 0];
    this.orthographic = false;
  }

  snapToRotation(rotation) {
    this.yaw = (rotation * Math.PI) / 4;
    this.pitch = 0;
    this.target = [0, VOXEL_DIM / 2, 0];
    this.distance = 150;
    this.orthographic = true;
  }

  eye() {
    return [
      this.target[0] + this.distance * Math.sin(this.yaw) * Math.cos(this.pitch),
      this.target[1] + this.distance * Math.sin(this.pitch),
      this.target[2] + this.distance * Math.cos(this.yaw) * Math.cos(this.pitch),
    ];
  }

  matrix(aspect) {
    const projection = this.orthographic
      ? orthographic((this.distance / 150) * VOXEL_DIM * 0.55, aspect, 1, 1000)
      : perspective(Math.PI / 4, aspect, 1, 2000);
    return multiply(projection, lookAt(this.eye(), this.target, [0, 1, 0]));
  }

  /* Move the target within the screen plane. */
  pan(deltaX, deltaY) {
    const scale = this.distance / 400;
    const right = [Math.cos(this.yaw), 0, -Math.sin(this.yaw)];
    this.target[0] -= right[0] * deltaX * scale;
    this.target[2] -= right[2] * deltaX * scale;
    this.target[1] += deltaY * scale;
  }
}

/* ------------------------------------------------------------------ */
/* Application                                                         */
/* ------------------------------------------------------------------ */

class Viewer {
  constructor(data) {
    this.data = data;
    this.manifest = data.manifest;
    this.subjects = this.manifest.subjects;
    this.parsed = new Map();
    this.meshes = new Map();
    this.renderer = new Renderer(document.getElementById('view'));
    this.camera = new OrbitCamera();
    this.showFloor = true;
    this.subjectIndex = 0;
    this.animationIndex = 0;
    this.poseIndex = 0;
    this.rotation = 0;
    this.playing = false;
    this.playback = null;
    this.bindControls();
    this.renderSubjectList();
    this.selectSubject(Math.max(0, this.subjects.findIndex((subject) => subject.name === 'grd')));
    this.applyLocationHash();
    requestAnimationFrame((time) => this.frame(time));
  }

  /*
   * Deep links such as #subject=dog&animation=1&pose=2&yaw=90 open a pose
   * directly; rotation=k snaps to original rotation k (0-7), and zoom and
   * height frame a detail (#subject=ofc&yaw=-40&zoom=4&height=46).
   */
  applyLocationHash() {
    const parameters = new URLSearchParams(window.location.hash.slice(1));
    const subjectIndex = this.subjects.findIndex((subject) => subject.name === parameters.get('subject'));
    if (subjectIndex >= 0) {
      this.selectSubject(subjectIndex);
    }
    const animation = Number(parameters.get('animation'));
    if (Number.isInteger(animation) && animation > 0 && animation < this.subject.animations.length) {
      this.selectAnimation(animation);
    }
    const pose = Number(parameters.get('pose'));
    if (Number.isInteger(pose) && pose > 0 && pose < this.animation.poses.length) {
      this.poseIndex = pose;
    }
    if (parameters.has('rotation')) {
      this.snapToRotation(Number(parameters.get('rotation')));
    } else if (parameters.has('yaw')) {
      this.camera.yaw = (Number(parameters.get('yaw')) * Math.PI) / 180;
    }
    /* zoom=2 halves the camera distance; height=48 aims the camera at that height (a head). */
    if (parameters.has('zoom')) {
      this.camera.distance /= Math.max(0.1, Number(parameters.get('zoom')));
    }
    if (parameters.has('height')) {
      this.camera.target[1] = Number(parameters.get('height'));
    }
    this.updatePanels();
  }

  /* Parsed VOX file for a subject, decoded on first use. */
  voxFor(subject) {
    if (!this.parsed.has(subject.name)) {
      this.parsed.set(subject.name, parseVox(decodeBase64(this.data.vox[subject.name])));
    }
    return this.parsed.get(subject.name);
  }

  meshFor(subject, animationIndex, poseIndex) {
    const key = `${subject.name}/${animationIndex}/${poseIndex}`;
    if (!this.meshes.has(key)) {
      const vox = this.voxFor(subject);
      const object = vox.objects[animationIndex];
      const model = vox.models[object.models[poseIndex]];
      this.meshes.set(key, this.renderer.upload(buildMesh(model, vox.palette)));
    }
    return this.meshes.get(key);
  }

  get subject() { return this.subjects[this.subjectIndex]; }
  get animation() { return this.subject.animations[this.animationIndex]; }
  get pose() { return this.animation.poses[this.poseIndex]; }

  bindControls() {
    const canvas = this.renderer.canvas;
    let dragging = null;
    canvas.addEventListener('contextmenu', (event) => event.preventDefault());
    canvas.addEventListener('pointerdown', (event) => {
      dragging = { x: event.clientX, y: event.clientY, pan: event.button === 2 || event.shiftKey };
      canvas.setPointerCapture(event.pointerId);
    });
    canvas.addEventListener('pointermove', (event) => {
      if (!dragging) {
        return;
      }
      const deltaX = event.clientX - dragging.x;
      const deltaY = event.clientY - dragging.y;
      dragging.x = event.clientX;
      dragging.y = event.clientY;
      if (dragging.pan) {
        this.camera.pan(deltaX, deltaY);
      } else {
        this.camera.yaw -= deltaX * 0.008;
        this.camera.pitch = Math.max(-1.45, Math.min(1.45, this.camera.pitch + deltaY * 0.008));
      }
    });
    canvas.addEventListener('pointerup', () => { dragging = null; });
    canvas.addEventListener('wheel', (event) => {
      event.preventDefault();
      this.camera.distance = Math.max(30, Math.min(600, this.camera.distance * Math.exp(event.deltaY * 0.001)));
    }, { passive: false });
    canvas.addEventListener('dblclick', () => { this.camera.reset(); this.setProjection(false); });

    document.getElementById('filter').addEventListener('input', () => this.renderSubjectList());
    document.getElementById('animation').addEventListener('change', (event) => this.selectAnimation(Number(event.target.value)));
    document.getElementById('previous').addEventListener('click', () => this.step(-1));
    document.getElementById('next').addEventListener('click', () => this.step(1));
    document.getElementById('play').addEventListener('click', () => this.togglePlay());
    document.getElementById('timing').addEventListener('change', () => { if (this.playing) { this.startPlayback(); } });
    document.getElementById('perspective').addEventListener('click', () => this.setProjection(false));
    document.getElementById('orthographic').addEventListener('click', () => this.setProjection(true));
    document.getElementById('floorToggle').addEventListener('click', (event) => {
      this.showFloor = !this.showFloor;
      event.target.classList.toggle('active', this.showFloor);
    });
    window.addEventListener('keydown', (event) => {
      if (event.target.tagName === 'INPUT') {
        return;
      }
      if (event.key === 'ArrowRight') { this.step(1); }
      if (event.key === 'ArrowLeft') { this.step(-1); }
      if (event.key === ' ') { event.preventDefault(); this.togglePlay(); }
      if (event.key >= '1' && event.key <= '8') { this.snapToRotation(Number(event.key) - 1); }
    });
  }

  setProjection(orthographicProjection) {
    this.camera.orthographic = orthographicProjection;
    document.getElementById('perspective').classList.toggle('active', !orthographicProjection);
    document.getElementById('orthographic').classList.toggle('active', orthographicProjection);
  }

  renderSubjectList() {
    const filter = document.getElementById('filter').value.trim().toLowerCase();
    const list = document.getElementById('subjects');
    list.innerHTML = '';
    this.subjects.forEach((subject, index) => {
      const description = subject.animations[0].poses[0].description || '';
      if (filter && !subject.name.includes(filter) && !description.toLowerCase().includes(filter)) {
        return;
      }
      const item = document.createElement('li');
      item.className = index === this.subjectIndex ? 'selected' : '';
      const poseCount = subject.animations.reduce((total, animation) => total + animation.poses.length, 0);
      const label = subject.name.startsWith('stat_') ? `${subject.name} · ${description}` : subject.name;
      item.innerHTML = `<span></span><span class="count">${subject.animations.length}/${poseCount}</span>`;
      item.firstChild.textContent = label;
      item.title = `${subject.animations.length} animations, ${poseCount} poses`;
      item.addEventListener('click', () => this.selectSubject(index));
      list.appendChild(item);
    });
  }

  selectSubject(index) {
    this.stopPlayback();
    this.subjectIndex = index;
    const select = document.getElementById('animation');
    select.innerHTML = '';
    this.subject.animations.forEach((animation, animationIndex) => {
      const option = document.createElement('option');
      option.value = String(animationIndex);
      option.textContent = `${animation.name} (${animation.poses.length})`;
      select.appendChild(option);
    });
    const timing = document.getElementById('timing');
    timing.innerHTML = '<option value="uniform">Uniform 6 fps</option>';
    this.manifest.sequences.filter((sequence) => sequence.subject === this.subject.name).forEach((sequence) => {
      const option = document.createElement('option');
      option.value = sequence.name;
      option.textContent = `Original: ${sequence.name}`;
      timing.appendChild(option);
    });
    this.renderSubjectList();
    this.selectAnimation(0);
  }

  selectAnimation(index) {
    this.animationIndex = index;
    this.poseIndex = 0;
    document.getElementById('animation').value = String(index);
    this.rotation = Math.min(this.rotation, this.pose.sprites.length - 1);
    this.updatePanels();
  }

  step(direction) {
    const count = this.animation.poses.length;
    this.poseIndex = (this.poseIndex + direction + count) % count;
    this.updatePanels();
  }

  snapToRotation(rotation) {
    if (rotation >= this.pose.sprites.length && rotation !== 0) {
      return;
    }
    this.rotation = rotation;
    this.camera.snapToRotation(rotation);
    this.setProjection(true);
    this.updatePanels();
  }

  togglePlay() {
    if (this.playing) {
      this.stopPlayback();
    } else {
      this.playing = true;
      document.getElementById('play').textContent = 'Pause';
      this.startPlayback();
    }
  }

  stopPlayback() {
    this.playing = false;
    this.playback = null;
    document.getElementById('play').textContent = 'Play';
  }

  /*
   * Uniform timing cycles the selected animation's poses. Original timing
   * follows a state sequence from the game, which may switch animations
   * (a walk cycle holds each stride for its own number of 70 Hz tics).
   */
  startPlayback() {
    const timing = document.getElementById('timing').value;
    const sequence = this.manifest.sequences.find((entry) => entry.name === timing && entry.subject === this.subject.name);
    this.playback = { sequence: sequence || null, step: 0, elapsed: 0 };
    if (sequence) {
      this.applySequenceStep();
    }
  }

  applySequenceStep() {
    const step = this.playback.sequence.frames[this.playback.step];
    const animationIndex = this.subject.animations.findIndex((animation) => animation.name === step.animation);
    if (animationIndex >= 0) {
      this.animationIndex = animationIndex;
      document.getElementById('animation').value = String(animationIndex);
      this.poseIndex = step.pose;
      this.updatePanels();
    }
  }

  advancePlayback(seconds) {
    const playback = this.playback;
    playback.elapsed += seconds;
    if (!playback.sequence) {
      if (playback.elapsed >= UNIFORM_FRAME_SECONDS) {
        playback.elapsed = 0;
        this.step(1);
      }
      return;
    }
    const frames = playback.sequence.frames;
    /* Zero-tic states hold until the game changes state; show them for half a second. */
    const duration = frames[playback.step].tics > 0 ? frames[playback.step].tics / TIC_RATE_HZ : 0.5;
    if (playback.elapsed < duration) {
      return;
    }
    playback.elapsed = 0;
    playback.step += 1;
    if (playback.step >= frames.length) {
      playback.step = playback.sequence.loop_frame >= 0 ? playback.sequence.loop_frame : 0;
    }
    this.applySequenceStep();
  }

  updatePanels() {
    const pose = this.pose;
    document.getElementById('frameLabel').textContent = `pose ${this.poseIndex + 1} / ${this.animation.poses.length}`;

    const rotations = document.getElementById('rotations');
    rotations.innerHTML = '';
    for (let rotation = 0; rotation < 8; rotation += 1) {
      const button = document.createElement('button');
      button.textContent = String(rotation + 1);
      button.title = `Rotation ${rotation + 1} (${rotation * 45}°)`;
      button.disabled = rotation >= pose.sprites.length && rotation !== 0;
      button.classList.toggle('active', this.camera.orthographic && rotation === this.rotation && this.camera.pitch === 0);
      button.addEventListener('click', () => this.snapToRotation(rotation));
      rotations.appendChild(button);
    }

    const compare = document.getElementById('compare');
    compare.innerHTML = '';
    const sprite = pose.sprites[Math.min(this.rotation, pose.sprites.length - 1)];
    const figure = document.createElement('figure');
    const image = document.createElement('img');
    image.src = `data:image/png;base64,${this.data.sprites[sprite.index]}`;
    image.alt = sprite.name;
    const caption = document.createElement('figcaption');
    caption.textContent = sprite.name;
    figure.append(image, caption);
    compare.appendChild(figure);

    const validation = pose.validation;
    const view = validation.views[Math.min(this.rotation, validation.views.length - 1)];
    const rows = [
      ['Model', pose.model],
      ['Method', pose.method],
      ['Shows', pose.description],
      ['Voxels', pose.voxels],
      ['Depth', `${pose.depth_extent} voxels`],
      ['Validation', `<span class="${validation.passed ? 'pass' : 'fail'}">${validation.passed ? 'PASS' : 'FAIL'}</span>`],
      ['Silhouette IoU', `${validation.mean_silhouette_iou.toFixed(3)} mean, ${validation.minimum_silhouette_iou.toFixed(3)} min`],
      ['This rotation', `IoU ${view.silhouette_iou.toFixed(3)}, colour within 1px ${(view.near_colour_accuracy * 100).toFixed(1)}%`],
      ['Shadow removed', `${sprite.shadow_pixels_removed} pixels`],
    ];
    if (validation.failure) {
      rows.push(['Why', validation.failure]);
    }
    const details = document.getElementById('details');
    details.innerHTML = '';
    for (const [label, value] of rows) {
      const term = document.createElement('dt');
      const definition = document.createElement('dd');
      term.textContent = label;
      if (label === 'Validation') {
        definition.innerHTML = value;
      } else {
        definition.textContent = String(value);
      }
      details.append(term, definition);
    }
  }

  frame(time) {
    const seconds = this.lastTime === undefined ? 0 : (time - this.lastTime) / 1000;
    this.lastTime = time;
    if (this.playing && this.playback) {
      this.advancePlayback(seconds);
    }
    this.renderer.draw(this.meshFor(this.subject, this.animationIndex, this.poseIndex), this.camera, this.showFloor);
    requestAnimationFrame((next) => this.frame(next));
  }
}

/* ------------------------------------------------------------------ */
/* Start-up                                                            */
/* ------------------------------------------------------------------ */

function start() {
  try {
    window.viewer = new Viewer(window.WOLF_VOXEL_DATA);
  } catch (error) {
    document.getElementById('loader').style.display = 'grid';
    document.getElementById('loadError').textContent = error.message;
  }
}

/*
 * viewer_data.js is loaded by a script tag. When it is not at the default
 * location, the user picks it and it is loaded the same way from a blob URL,
 * which works for pages opened straight from disk.
 */
if (window.WOLF_VOXEL_DATA) {
  start();
} else {
  document.getElementById('loader').style.display = 'grid';
  document.getElementById('dataFile').addEventListener('change', (event) => {
    const file = event.target.files[0];
    if (!file) {
      return;
    }
    const script = document.createElement('script');
    script.src = URL.createObjectURL(file);
    script.onload = () => {
      if (window.WOLF_VOXEL_DATA) {
        document.getElementById('loader').style.display = 'none';
        start();
      } else {
        document.getElementById('loadError').textContent = 'That file did not contain voxel data.';
      }
    };
    document.head.appendChild(script);
  });
}
