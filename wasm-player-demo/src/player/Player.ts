import { AudioEngine } from './audio/AudioEngine';
import { YuvRenderer } from './gl/YuvRenderer';

interface PlayerDom {
  canvas: HTMLCanvasElement;
  playBtn: HTMLButtonElement;
  pauseBtn: HTMLButtonElement;
  seekBar: HTMLInputElement;
  volumeBar: HTMLInputElement;
  rateSel: HTMLSelectElement;
  timeLabel: HTMLDivElement;
  sourceUrl: string;
}

interface WasmPlayerModule {
  HEAPU8: Uint8Array;
  HEAPU32: Uint32Array;
  HEAP32: Int32Array;
  HEAPF32: Float32Array;
  HEAPF64: Float64Array;
  _malloc(size: number): number;
  _free(ptr: number): void;
  _player_init(sampleRate: number, channels: number): number;
  _player_feed(ptr: number, len: number): number;
  _player_decode_step(): number;
  _player_poll_video_frame(
    yPtr: number,
    uPtr: number,
    vPtr: number,
    wPtr: number,
    hPtr: number,
    yStridePtr: number,
    uvStridePtr: number,
    ptsPtr: number
  ): number;
  _player_poll_audio(pcmPtr: number, framesPtr: number, ptsPtr: number): number;
  _player_seek_ms(ms: number): number;
  _player_flush(): void;
  _player_close(): void;
  _player_get_duration_sec(): number;
}

interface OutPtrs {
  y: number;
  u: number;
  v: number;
  w: number;
  h: number;
  yStride: number;
  uvStride: number;
  videoPts: number;
  pcm: number;
  frames: number;
  audioPts: number;
}

export class Player {
  private readonly dom: PlayerDom;
  private readonly renderer: YuvRenderer;
  private readonly audio = new AudioEngine(2);

  private wasm: WasmPlayerModule | null = null;
  private outPtrs: OutPtrs | null = null;

  private decodeTimer: number | null = null;
  private rafId: number | null = null;
  private isPlaying = false;
  private isSeeking = false;

  private playbackRate = 1;
  private durationSec = 0;
  private currentSec = 0;

  constructor(dom: PlayerDom) {
    this.dom = dom;
    this.renderer = new YuvRenderer(dom.canvas);
    this.bindEvents();
  }

  async init(): Promise<void> {
    await this.audio.init();

    this.wasm = await this.loadWasm();
    const initRet = this.wasm._player_init(this.audio.getSampleRate(), this.audio.channels);
    if (initRet < 0) {
      throw new Error(`player_init failed: ${initRet}`);
    }

    this.outPtrs = this.allocOutPtrs(this.wasm);
    this.startDecodeLoop();

    void this.streamSource(this.dom.sourceUrl);
  }

  private bindEvents(): void {
    this.dom.playBtn.addEventListener('click', () => {
      void this.play();
    });

    this.dom.pauseBtn.addEventListener('click', () => {
      void this.pause();
    });

    this.dom.volumeBar.addEventListener('input', () => {
      this.audio.setVolume(Number(this.dom.volumeBar.value));
    });

    this.dom.rateSel.addEventListener('change', () => {
      this.playbackRate = Number(this.dom.rateSel.value);
    });

    this.dom.seekBar.addEventListener('input', () => {
      this.isSeeking = true;
      const sec = this.seekValueToSeconds(Number(this.dom.seekBar.value));
      this.setTimeLabel(sec, this.durationSec);
    });

    this.dom.seekBar.addEventListener('change', () => {
      const sec = this.seekValueToSeconds(Number(this.dom.seekBar.value));
      this.seek(sec);
      this.isSeeking = false;
    });
  }

  private async play(): Promise<void> {
    if (!this.wasm) {
      return;
    }
    this.isPlaying = true;
    await this.audio.resume();
    if (this.rafId === null) {
      this.rafId = requestAnimationFrame(this.renderLoop);
    }
  }

  private async pause(): Promise<void> {
    this.isPlaying = false;
    if (this.rafId !== null) {
      cancelAnimationFrame(this.rafId);
      this.rafId = null;
    }
    await this.audio.suspend();
  }

  private seek(targetSec: number): void {
    if (!this.wasm) {
      return;
    }

    const clamped = Math.max(0, Math.min(targetSec, this.durationSec || targetSec));
    const ms = Math.floor(clamped * 1000);
    const ret = this.wasm._player_seek_ms(ms);
    if (ret < 0) {
      // eslint-disable-next-line no-console
      console.warn('seek failed', ret);
      return;
    }

    this.wasm._player_flush();
    this.audio.clear();
    this.currentSec = clamped;
    this.setTimeLabel(this.currentSec, this.durationSec);
  }

  private readonly renderLoop = (): void => {
    if (!this.wasm || !this.outPtrs || !this.isPlaying) {
      this.rafId = null;
      return;
    }

    this.pullVideoFrames();
    this.updateUi();

    const delay = this.playbackRate <= 0 ? 1 : 1 / this.playbackRate;
    if (delay === 1) {
      this.rafId = requestAnimationFrame(this.renderLoop);
    } else {
      window.setTimeout(() => {
        this.rafId = requestAnimationFrame(this.renderLoop);
      }, Math.max(0, (delay - 1) * 16.7));
    }
  };

  private startDecodeLoop(): void {
    if (this.decodeTimer !== null) {
      return;
    }

    this.decodeTimer = window.setInterval(() => {
      if (!this.wasm) {
        return;
      }

      for (let i = 0; i < 8; i += 1) {
        const ret = this.wasm._player_decode_step();
        if (ret < 0) {
          break;
        }
        if (ret === 0) {
          break;
        }
      }

      this.pullAudioFrames();
      this.updateDurationFromWasm();

      if (this.isPlaying && this.rafId === null) {
        this.rafId = requestAnimationFrame(this.renderLoop);
      }
    }, 8);
  }

  private pullVideoFrames(): void {
    if (!this.wasm || !this.outPtrs) {
      return;
    }

    let rendered = false;
    for (let i = 0; i < 3; i += 1) {
      const ret = this.wasm._player_poll_video_frame(
        this.outPtrs.y,
        this.outPtrs.u,
        this.outPtrs.v,
        this.outPtrs.w,
        this.outPtrs.h,
        this.outPtrs.yStride,
        this.outPtrs.uvStride,
        this.outPtrs.videoPts
      );
      if (ret !== 1) {
        break;
      }

      const yPtr = this.wasm.HEAPU32[this.outPtrs.y >> 2] >>> 0;
      const uPtr = this.wasm.HEAPU32[this.outPtrs.u >> 2] >>> 0;
      const vPtr = this.wasm.HEAPU32[this.outPtrs.v >> 2] >>> 0;

      const width = this.wasm.HEAP32[this.outPtrs.w >> 2];
      const height = this.wasm.HEAP32[this.outPtrs.h >> 2];
      const yStride = this.wasm.HEAP32[this.outPtrs.yStride >> 2];
      const uvStride = this.wasm.HEAP32[this.outPtrs.uvStride >> 2];
      const pts = this.wasm.HEAPF64[this.outPtrs.videoPts >> 3];

      if (width <= 0 || height <= 0 || yStride <= 0 || uvStride <= 0) {
        continue;
      }

      const ySize = yStride * height;
      const uvHeight = height >> 1;
      const uvSize = uvStride * uvHeight;

      const yPlane = this.wasm.HEAPU8.slice(yPtr, yPtr + ySize);
      const uPlane = this.wasm.HEAPU8.slice(uPtr, uPtr + uvSize);
      const vPlane = this.wasm.HEAPU8.slice(vPtr, vPtr + uvSize);

      this.renderer.render({
        yPlane,
        uPlane,
        vPlane,
        width,
        height,
        yStride,
        uvStride,
        pts
      });
      this.currentSec = Number.isFinite(pts) ? pts : this.currentSec;
      rendered = true;
    }

    if (!rendered) {
      this.currentSec = Math.max(this.currentSec, this.audio.getPlayedTimeSec());
    }
  }

  private pullAudioFrames(): void {
    if (!this.wasm || !this.outPtrs) {
      return;
    }

    while (this.audio.getWritableFrames() > 512) {
      const ret = this.wasm._player_poll_audio(this.outPtrs.pcm, this.outPtrs.frames, this.outPtrs.audioPts);
      if (ret !== 1) {
        break;
      }

      const pcmPtr = this.wasm.HEAPU32[this.outPtrs.pcm >> 2] >>> 0;
      const frames = this.wasm.HEAP32[this.outPtrs.frames >> 2];
      if (frames <= 0) {
        continue;
      }

      const samples = frames * this.audio.channels;
      const start = pcmPtr >> 2;
      const pcm = this.wasm.HEAPF32.slice(start, start + samples);
      this.audio.enqueuePcm(pcm, frames);
    }
  }

  private updateDurationFromWasm(): void {
    if (!this.wasm) {
      return;
    }
    const duration = this.wasm._player_get_duration_sec();
    if (Number.isFinite(duration) && duration > 0) {
      this.durationSec = duration;
    }
  }

  private updateUi(): void {
    if (!this.isSeeking) {
      const seekValue = this.secondsToSeekValue(this.currentSec);
      this.dom.seekBar.value = String(seekValue);
      this.setTimeLabel(this.currentSec, this.durationSec);
    }
  }

  private setTimeLabel(current: number, duration: number): void {
    this.dom.timeLabel.textContent = `${formatTime(current)} / ${duration > 0 ? formatTime(duration) : '--:--'}`;
  }

  private seekValueToSeconds(value: number): number {
    if (!this.durationSec || this.durationSec <= 0) {
      return 0;
    }
    return (value / 1000) * this.durationSec;
  }

  private secondsToSeekValue(sec: number): number {
    if (!this.durationSec || this.durationSec <= 0) {
      return 0;
    }
    return Math.round((Math.max(0, Math.min(sec, this.durationSec)) / this.durationSec) * 1000);
  }

  private async streamSource(url: string): Promise<void> {
    if (!this.wasm) {
      return;
    }

    const resp = await fetch(url);
    if (!resp.ok || !resp.body) {
      throw new Error(`fetch failed: ${resp.status}`);
    }

    const reader = resp.body.getReader();
    while (true) {
      const { value, done } = await reader.read();
      if (done) {
        break;
      }
      if (!value || value.byteLength === 0) {
        continue;
      }
      this.feedChunk(value);
    }

    this.wasm._player_feed(0, 0);
  }

  private feedChunk(chunk: Uint8Array): void {
    if (!this.wasm) {
      return;
    }

    const ptr = this.wasm._malloc(chunk.byteLength);
    this.wasm.HEAPU8.set(chunk, ptr);
    const ret = this.wasm._player_feed(ptr, chunk.byteLength);
    this.wasm._free(ptr);

    if (ret < 0) {
      throw new Error(`player_feed failed: ${ret}`);
    }
  }

  private allocOutPtrs(wasm: WasmPlayerModule): OutPtrs {
    return {
      y: wasm._malloc(4),
      u: wasm._malloc(4),
      v: wasm._malloc(4),
      w: wasm._malloc(4),
      h: wasm._malloc(4),
      yStride: wasm._malloc(4),
      uvStride: wasm._malloc(4),
      videoPts: wasm._malloc(8),
      pcm: wasm._malloc(4),
      frames: wasm._malloc(4),
      audioPts: wasm._malloc(8)
    };
  }

  private async loadWasm(): Promise<WasmPlayerModule> {
    const mod = await import('/wasm/player.js');
    const factory = (mod as { default: (opts?: Record<string, unknown>) => Promise<WasmPlayerModule> }).default;
    return factory({
      locateFile: (name: string) => `/wasm/${name}`
    });
  }
}

function formatTime(sec: number): string {
  if (!Number.isFinite(sec) || sec < 0) {
    return '00:00';
  }
  const s = Math.floor(sec);
  const mm = Math.floor(s / 60)
    .toString()
    .padStart(2, '0');
  const ss = (s % 60).toString().padStart(2, '0');
  return `${mm}:${ss}`;
}
