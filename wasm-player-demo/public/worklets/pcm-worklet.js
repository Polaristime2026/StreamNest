const STATE_READ = 0;
const STATE_WRITE = 1;
const STATE_PLAYED = 3;

class PcmWorkletProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();

    const opts = options.processorOptions || {};
    this.channels = opts.channels || 2;
    this.capacityFrames = opts.capacityFrames || 16384;
    this.startThresholdFrames = opts.startThresholdFrames || 2048;

    this.state = new Int32Array(opts.stateSAB);
    this.data = new Float32Array(opts.dataSAB);

    this.started = false;
  }

  process(inputs, outputs) {
    const output = outputs[0];
    if (!output || output.length === 0) {
      return true;
    }

    const blockFrames = output[0].length;

    for (let ch = 0; ch < output.length; ch += 1) {
      output[ch].fill(0);
    }

    let read = Atomics.load(this.state, STATE_READ);
    const write = Atomics.load(this.state, STATE_WRITE);
    let available = write - read;

    if (!this.started) {
      if (available < this.startThresholdFrames) {
        return true;
      }
      this.started = true;
    }

    const canRead = Math.min(blockFrames, available);

    for (let i = 0; i < canRead; i += 1) {
      const srcFrame = (read + i) % this.capacityFrames;
      const srcOffset = srcFrame * this.channels;
      for (let ch = 0; ch < this.channels; ch += 1) {
        output[ch][i] = this.data[srcOffset + ch];
      }
    }

    read += canRead;
    Atomics.store(this.state, STATE_READ, read);
    Atomics.add(this.state, STATE_PLAYED, canRead);

    available -= canRead;
    if (available <= 0) {
      this.started = false;
    }

    return true;
  }
}

registerProcessor('pcm-worklet-processor', PcmWorkletProcessor);
