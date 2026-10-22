/** Prepare official team artwork once, preserving its shape and original colors. */
export function prepareLogo(image) {
  // Bound processing work even for unusually large source images.
  const scale = Math.min(1, 512 / Math.max(image.naturalWidth, image.naturalHeight));
  const canvas = document.createElement('canvas');
  canvas.width = Math.max(1, Math.round(image.naturalWidth * scale));
  canvas.height = Math.max(1, Math.round(image.naturalHeight * scale));
  const context = canvas.getContext('2d', { willReadFrequently: true });
  context.drawImage(image, 0, 0, canvas.width, canvas.height);
  const { data } = context.getImageData(0, 0, canvas.width, canvas.height);
  let left = canvas.width, top = canvas.height, right = -1, bottom = -1;
  for (let y = 0; y < canvas.height; y++) {
    for (let x = 0; x < canvas.width; x++) {
      if (data[(y * canvas.width + x) * 4 + 3] > 8) {
        left = Math.min(left, x); right = Math.max(right, x);
        top = Math.min(top, y); bottom = Math.max(bottom, y);
      }
    }
  }
  if (right < left) return null;
  const sprites = new Map();
  return size => {
    if (sprites.has(size)) return sprites.get(size);
    const sprite = document.createElement('canvas');
    sprite.width = sprite.height = size;
    const target = sprite.getContext('2d');
    target.imageSmoothingEnabled = true;
    target.imageSmoothingQuality = 'high';
    const width = right - left + 1, height = bottom - top + 1;
    const ratio = Math.min(size / width, size / height);
    const w = Math.max(1, Math.round(width * ratio));
    const h = Math.max(1, Math.round(height * ratio));
    target.drawImage(canvas, left, top, width, height,
      Math.floor((size - w) / 2), Math.floor((size - h) / 2), w, h);
    sprites.set(size, sprite);
    return sprite;
  };
}
