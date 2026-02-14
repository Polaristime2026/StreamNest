import {
  BT709_MATRIX,
  BT709_OFFSET,
  fragmentShaderSource,
  vertexShaderSource
} from './shaders';

export interface YuvFrame {
  yPlane: Uint8Array;
  uPlane: Uint8Array;
  vPlane: Uint8Array;
  width: number;
  height: number;
  yStride: number;
  uvStride: number;
  pts: number;
}

export class YuvRenderer {
  private readonly gl: WebGL2RenderingContext;
  private readonly program: WebGLProgram;
  private readonly vao: WebGLVertexArrayObject;
  private readonly texY: WebGLTexture;
  private readonly texU: WebGLTexture;
  private readonly texV: WebGLTexture;
  private readonly uColorMatrix: WebGLUniformLocation;
  private readonly uColorOffset: WebGLUniformLocation;

  private tightScratch = new Uint8Array(0);

  constructor(private readonly canvas: HTMLCanvasElement) {
    const gl = canvas.getContext('webgl2', {
      alpha: false,
      antialias: false,
      desynchronized: true
    });
    if (!gl) {
      throw new Error('WebGL2 not supported');
    }
    this.gl = gl;

    const vs = this.compile(gl.VERTEX_SHADER, vertexShaderSource);
    const fs = this.compile(gl.FRAGMENT_SHADER, fragmentShaderSource);
    this.program = this.link(vs, fs);
    this.vao = this.createQuad();

    this.texY = this.createPlaneTexture();
    this.texU = this.createPlaneTexture();
    this.texV = this.createPlaneTexture();

    gl.useProgram(this.program);
    gl.uniform1i(this.uniform('uTexY'), 0);
    gl.uniform1i(this.uniform('uTexU'), 1);
    gl.uniform1i(this.uniform('uTexV'), 2);

    this.uColorMatrix = this.uniform('uColorMatrix');
    this.uColorOffset = this.uniform('uColorOffset');
    gl.uniformMatrix3fv(this.uColorMatrix, false, BT709_MATRIX);
    gl.uniform3fv(this.uColorOffset, BT709_OFFSET);

    gl.clearColor(0, 0, 0, 1);
  }

  render(frame: YuvFrame): void {
    const gl = this.gl;

    this.resizeCanvasToDisplaySize();
    gl.viewport(0, 0, this.canvas.width, this.canvas.height);

    this.uploadPlane(this.texY, frame.yPlane, frame.width, frame.height, frame.yStride);
    this.uploadPlane(
      this.texU,
      frame.uPlane,
      frame.width >> 1,
      frame.height >> 1,
      frame.uvStride
    );
    this.uploadPlane(
      this.texV,
      frame.vPlane,
      frame.width >> 1,
      frame.height >> 1,
      frame.uvStride
    );

    gl.useProgram(this.program);
    gl.bindVertexArray(this.vao);
    gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
    gl.bindVertexArray(null);
  }

  private uploadPlane(
    texture: WebGLTexture,
    src: Uint8Array,
    width: number,
    height: number,
    stride: number
  ): void {
    const gl = this.gl;
    const tight = stride === width ? src.subarray(0, width * height) : this.tighten(src, width, height, stride);

    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.R8, width, height, 0, gl.RED, gl.UNSIGNED_BYTE, tight);
  }

  private tighten(src: Uint8Array, width: number, height: number, stride: number): Uint8Array {
    const need = width * height;
    if (this.tightScratch.length < need) {
      this.tightScratch = new Uint8Array(need);
    }
    const out = this.tightScratch.subarray(0, need);

    let dstOff = 0;
    for (let row = 0; row < height; row += 1) {
      const srcStart = row * stride;
      out.set(src.subarray(srcStart, srcStart + width), dstOff);
      dstOff += width;
    }
    return out;
  }

  private createQuad(): WebGLVertexArrayObject {
    const gl = this.gl;
    const vao = gl.createVertexArray();
    if (!vao) {
      throw new Error('failed to create vao');
    }
    gl.bindVertexArray(vao);

    const vertices = new Float32Array([
      -1, -1, 0, 0,
      1, -1, 1, 0,
      -1, 1, 0, 1,
      1, 1, 1, 1
    ]);

    const vbo = gl.createBuffer();
    if (!vbo) {
      throw new Error('failed to create vbo');
    }
    gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
    gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);

    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 16, 0);

    gl.enableVertexAttribArray(1);
    gl.vertexAttribPointer(1, 2, gl.FLOAT, false, 16, 8);

    gl.bindVertexArray(null);
    return vao;
  }

  private createPlaneTexture(): WebGLTexture {
    const gl = this.gl;
    const tex = gl.createTexture();
    if (!tex) {
      throw new Error('failed to create texture');
    }

    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    return tex;
  }

  private compile(type: number, source: string): WebGLShader {
    const gl = this.gl;
    const shader = gl.createShader(type);
    if (!shader) {
      throw new Error('failed to create shader');
    }
    gl.shaderSource(shader, source);
    gl.compileShader(shader);

    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
      const log = gl.getShaderInfoLog(shader) ?? '';
      gl.deleteShader(shader);
      throw new Error(`shader compile failed: ${log}`);
    }
    return shader;
  }

  private link(vs: WebGLShader, fs: WebGLShader): WebGLProgram {
    const gl = this.gl;
    const program = gl.createProgram();
    if (!program) {
      throw new Error('failed to create program');
    }

    gl.attachShader(program, vs);
    gl.attachShader(program, fs);
    gl.linkProgram(program);

    gl.deleteShader(vs);
    gl.deleteShader(fs);

    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
      const log = gl.getProgramInfoLog(program) ?? '';
      gl.deleteProgram(program);
      throw new Error(`program link failed: ${log}`);
    }

    return program;
  }

  private uniform(name: string): WebGLUniformLocation {
    const location = this.gl.getUniformLocation(this.program, name);
    if (!location) {
      throw new Error(`uniform not found: ${name}`);
    }
    return location;
  }

  private resizeCanvasToDisplaySize(): void {
    const dpr = window.devicePixelRatio || 1;
    const width = Math.round(this.canvas.clientWidth * dpr);
    const height = Math.round(this.canvas.clientHeight * dpr);
    if (this.canvas.width !== width || this.canvas.height !== height) {
      this.canvas.width = width;
      this.canvas.height = height;
    }
  }
}
