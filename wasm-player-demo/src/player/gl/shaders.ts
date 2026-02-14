export const vertexShaderSource = `#version 300 es
precision highp float;

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;
out vec2 vUv;

void main() {
  vUv = aUv;
  gl_Position = vec4(aPos, 0.0, 1.0);
}
`;

export const fragmentShaderSource = `#version 300 es
precision highp float;

in vec2 vUv;
out vec4 fragColor;

uniform sampler2D uTexY;
uniform sampler2D uTexU;
uniform sampler2D uTexV;
uniform mat3 uColorMatrix;
uniform vec3 uColorOffset;

void main() {
  float y = texture(uTexY, vUv).r;
  float u = texture(uTexU, vUv).r;
  float v = texture(uTexV, vUv).r;

  vec3 yuv = vec3(y, u, v) + uColorOffset;
  vec3 rgb = uColorMatrix * yuv;
  fragColor = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
`;

export const BT709_MATRIX = new Float32Array([
  1.0, 1.0, 1.0,
  0.0, -0.1873, 1.8556,
  1.5748, -0.4681, 0.0
]);

export const BT709_OFFSET = new Float32Array([0.0, -0.5, -0.5]);

export const BT601_MATRIX = new Float32Array([
  1.0, 1.0, 1.0,
  0.0, -0.344136, 1.772,
  1.402, -0.714136, 0.0
]);
