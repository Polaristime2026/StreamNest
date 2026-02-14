import { SharedPcmRingBuffer } from './RingBuffer';

export class AudioEngine {
  readonly channels: number;

  private context: AudioContext | null = null;
  private node: AudioWorkletNode | null = null;
  private gainNode: GainNode | null = null;
  private ring: SharedPcmRingBuffer | null = null;

  constructor(channels = 2) {
    this.channels = channels;
  }

  async init(): Promise<void> {
    if (this.context) {
      return;
    }

    this.context = new AudioContext({
      latencyHint: 'interactive'
    });

    this.ring = new SharedPcmRingBuffer(16384, this.channels);

    await this.context.audioWorklet.addModule('/worklets/pcm-worklet.js');

    this.node = new AudioWorkletNode(this.context, 'pcm-worklet-processor', {
      numberOfInputs: 0,
      numberOfOutputs: 1,
      outputChannelCount: [this.channels],
      processorOptions: {
        ...this.ring.getState(),
        startThresholdFrames: 2048
      }
    });

    this.gainNode = this.context.createGain();
    this.gainNode.gain.value = 1.0;

    this.node.connect(this.gainNode).connect(this.context.destination);
  }

  getSampleRate(): number {
    return this.context?.sampleRate ?? 48000;
  }

  async resume(): Promise<void> {
    await this.context?.resume();
  }

  async suspend(): Promise<void> {
    await this.context?.suspend();
  }

  setVolume(volume: number): void {
    if (!this.gainNode) {
      return;
    }
    this.gainNode.gain.value = Math.max(0, Math.min(volume, 1));
  }

  clear(): void {
    this.ring?.clear();
  }

  getWritableFrames(): number {
    return this.ring?.getWritableFrames() ?? 0;
  }

  getBufferedFrames(): number {
    return this.ring?.getReadableFrames() ?? 0;
  }

  getPlayedTimeSec(): number {
    if (!this.ring || !this.context) {
      return 0;
    }
    return this.ring.getPlayedFrames() / this.context.sampleRate;
  }

  enqueuePcm(pcmInterleaved: Float32Array, frames: number): number {
    if (!this.ring || frames <= 0) {
      return 0;
    }
    return this.ring.pushInterleaved(pcmInterleaved, frames);
  }
}
