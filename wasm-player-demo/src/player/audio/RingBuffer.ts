const STATE_READ = 0;
const STATE_WRITE = 1;
const STATE_DROPPED = 2;
const STATE_PLAYED = 3;

export interface PcmRingState {
  dataSAB: SharedArrayBuffer;
  stateSAB: SharedArrayBuffer;
  capacityFrames: number;
  channels: number;
}

export class SharedPcmRingBuffer {
  readonly dataSAB: SharedArrayBuffer;
  readonly stateSAB: SharedArrayBuffer;
  readonly capacityFrames: number;
  readonly channels: number;

  private readonly data: Float32Array;
  private readonly state: Int32Array;

  constructor(capacityFrames: number, channels: number) {
    if (typeof SharedArrayBuffer === 'undefined') {
      throw new Error('SharedArrayBuffer unavailable: requires COOP/COEP + secure context');
    }
    this.capacityFrames = capacityFrames;
    this.channels = channels;

    this.dataSAB = new SharedArrayBuffer(Float32Array.BYTES_PER_ELEMENT * capacityFrames * channels);
    this.stateSAB = new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 4);

    this.data = new Float32Array(this.dataSAB);
    this.state = new Int32Array(this.stateSAB);
  }

  getState(): PcmRingState {
    return {
      dataSAB: this.dataSAB,
      stateSAB: this.stateSAB,
      capacityFrames: this.capacityFrames,
      channels: this.channels
    };
  }

  getReadableFrames(): number {
    const read = Atomics.load(this.state, STATE_READ);
    const write = Atomics.load(this.state, STATE_WRITE);
    return Math.max(0, write - read);
  }

  getWritableFrames(): number {
    return this.capacityFrames - this.getReadableFrames();
  }

  getDroppedFrames(): number {
    return Atomics.load(this.state, STATE_DROPPED);
  }

  getPlayedFrames(): number {
    return Atomics.load(this.state, STATE_PLAYED);
  }

  clear(): void {
    Atomics.store(this.state, STATE_READ, 0);
    Atomics.store(this.state, STATE_WRITE, 0);
  }

  pushInterleaved(input: Float32Array, frames: number): number {
    const read = Atomics.load(this.state, STATE_READ);
    const write = Atomics.load(this.state, STATE_WRITE);
    const used = write - read;
    const free = this.capacityFrames - used;
    const toWrite = Math.max(0, Math.min(frames, free));

    for (let i = 0; i < toWrite; i += 1) {
      const dstFrame = (write + i) % this.capacityFrames;
      const dstOffset = dstFrame * this.channels;
      const srcOffset = i * this.channels;
      this.data.set(input.subarray(srcOffset, srcOffset + this.channels), dstOffset);
    }

    Atomics.store(this.state, STATE_WRITE, write + toWrite);

    const dropped = frames - toWrite;
    if (dropped > 0) {
      Atomics.add(this.state, STATE_DROPPED, dropped);
    }
    return toWrite;
  }
}
