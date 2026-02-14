import { Player } from './player/Player';

async function bootstrap(): Promise<void> {
  const canvas = document.getElementById('videoCanvas');
  const playBtn = document.getElementById('playBtn');
  const pauseBtn = document.getElementById('pauseBtn');
  const seekBar = document.getElementById('seekBar');
  const volumeBar = document.getElementById('volumeBar');
  const rateSel = document.getElementById('rateSel');
  const timeLabel = document.getElementById('timeLabel');

  if (
    !(canvas instanceof HTMLCanvasElement) ||
    !(playBtn instanceof HTMLButtonElement) ||
    !(pauseBtn instanceof HTMLButtonElement) ||
    !(seekBar instanceof HTMLInputElement) ||
    !(volumeBar instanceof HTMLInputElement) ||
    !(rateSel instanceof HTMLSelectElement) ||
    !(timeLabel instanceof HTMLDivElement)
  ) {
    throw new Error('player dom not found');
  }

  const player = new Player({
    canvas,
    playBtn,
    pauseBtn,
    seekBar,
    volumeBar,
    rateSel,
    timeLabel,
    sourceUrl: '/assets/sample.mp4'
  });

  await player.init();
}

bootstrap().catch((err) => {
  // eslint-disable-next-line no-console
  console.error('[bootstrap] failed', err);
});
