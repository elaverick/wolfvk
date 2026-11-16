/*
 * Checks the viewer's MagicaVoxel parser and mesher against data the tool
 * wrote: every subject file must parse into the animations the manifest
 * lists, and every pose must mesh into whole triangles.
 *
 * Usage: node test_viewer.js <viewer.js> <viewer_data.js>
 */
'use strict';

const fs = require('fs');
const vm = require('vm');

const [viewerPath, dataPath] = process.argv.slice(2);
const viewerSource = fs.readFileSync(viewerPath, 'utf8');
/* Only the parsing and meshing half of the viewer runs outside a browser. */
const parsingSource = viewerSource.split('/* Matrices')[0];
const viewer = { atob, TextDecoder, Uint8Array, DataView, Float32Array, Math, Number, String, Error };
vm.createContext(viewer);
vm.runInContext(`${parsingSource}\nthis.parseVox = parseVox; this.buildMesh = buildMesh; this.decodeBase64 = decodeBase64;`, viewer);

const dataContext = { window: {} };
vm.createContext(dataContext);
vm.runInContext(fs.readFileSync(dataPath, 'utf8'), dataContext);
const data = dataContext.window.WOLF_VOXEL_DATA;

const failures = [];
let poses = 0;
for (const subject of data.manifest.subjects) {
  const vox = viewer.parseVox(viewer.decodeBase64(data.vox[subject.name]));
  if (vox.objects.length !== subject.animations.length) {
    failures.push(`${subject.name}: ${vox.objects.length} objects, manifest lists ${subject.animations.length}`);
    continue;
  }
  subject.animations.forEach((animation, index) => {
    const object = vox.objects[index];
    if (object.name !== animation.name || object.models.length !== animation.poses.length) {
      failures.push(`${subject.name}: object ${object.name} does not match animation ${animation.name}`);
      return;
    }
    object.models.forEach((modelId) => {
      const mesh = viewer.buildMesh(vox.models[modelId], vox.palette);
      poses += 1;
      if (mesh.vertexCount % 6 !== 0) {
        failures.push(`${animation.name}: mesh is not whole quads`);
      }
    });
  });
  for (const animation of subject.animations) {
    for (const pose of animation.poses) {
      for (const sprite of pose.sprites) {
        if (typeof data.sprites[sprite.index] !== 'string') {
          failures.push(`${pose.model}: original frame ${sprite.name} missing`);
        }
      }
    }
  }
}

if (failures.length > 0) {
  console.error(failures.join('\n'));
  process.exit(1);
}
console.log(`viewer parsed ${data.manifest.subjects.length} subjects and meshed ${poses} poses`);
