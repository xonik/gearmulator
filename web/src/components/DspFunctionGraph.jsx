import { useRef, useEffect } from 'react';

const MAX_23BIT = 0x7FFFFF;

// Sign-extend a 24-bit value to JavaScript's 32-bit signed integer
function signExtend24(val) {
  if (val & 0x800000) {
    return val | 0xFF000000;
  }
  return val;
}

// Variation 1: max - abs(x) — Triangle peak at zero
function maxMinusAbs(val) {
  val = signExtend24(val);
  if (val >= 0) val = ~val;
  return val & MAX_23BIT;
}

// Variation 2: ~x & mask — V-shape, minimum at zero
function invertedMagnitude(val) {
  val = signExtend24(val);
  return (~val) & MAX_23BIT;
}

// Variation 3: abs(x) — Standard absolute value
function absoluteValue(val) {
  val = signExtend24(val);
  if (val < 0) val = ~val;
  return val & MAX_23BIT;
}

// Convert normalized value (-1 to 1) to 24-bit representation
function normalizedTo24Bit(normalized) {
  if (normalized >= 0) {
    return Math.round(normalized * MAX_23BIT);
  } else {
    // Negative values in 24-bit two's complement
    return (Math.round(normalized * (MAX_23BIT + 1)) & 0xFFFFFF);
  }
}

// Convert 24-bit result to normalized (0 to 1)
function result24BitToNormalized(val) {
  return val / MAX_23BIT;
}

function DspFunctionGraph({ title, func, description }) {
  const canvasRef = useRef(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    const ctx = canvas.getContext('2d');
    const width = canvas.width;
    const height = canvas.height;
    const padding = 50;
    const graphWidth = width - padding * 2;
    const graphHeight = height - padding * 2;

    // Clear canvas
    ctx.fillStyle = '#1a1a2e';
    ctx.fillRect(0, 0, width, height);

    // Draw grid
    ctx.strokeStyle = '#333355';
    ctx.lineWidth = 1;

    // Vertical gridlines (every 25% of input range: -1, -0.5, 0, 0.5, 1)
    for (let i = 0; i <= 4; i++) {
      const x = padding + (i / 4) * graphWidth;
      ctx.beginPath();
      ctx.moveTo(x, padding);
      ctx.lineTo(x, height - padding);
      ctx.stroke();
    }

    // Horizontal gridlines (every 25% of output range: 0, 0.25, 0.5, 0.75, 1)
    for (let i = 0; i <= 4; i++) {
      const y = padding + (i / 4) * graphHeight;
      ctx.beginPath();
      ctx.moveTo(padding, y);
      ctx.lineTo(width - padding, y);
      ctx.stroke();
    }

    // Draw axes
    ctx.strokeStyle = '#8888aa';
    ctx.lineWidth = 2;

    // X-axis
    ctx.beginPath();
    ctx.moveTo(padding, height - padding);
    ctx.lineTo(width - padding, height - padding);
    ctx.stroke();

    // Y-axis
    ctx.beginPath();
    ctx.moveTo(padding, padding);
    ctx.lineTo(padding, height - padding);
    ctx.stroke();

    // Draw axis labels
    ctx.fillStyle = '#ccccdd';
    ctx.font = '12px monospace';
    ctx.textAlign = 'center';

    // X-axis labels
    const xLabels = ['-1', '-0.5', '0', '0.5', '1'];
    for (let i = 0; i <= 4; i++) {
      const x = padding + (i / 4) * graphWidth;
      ctx.fillText(xLabels[i], x, height - padding + 20);
    }

    // Y-axis labels
    ctx.textAlign = 'right';
    const yLabels = ['1', '0.75', '0.5', '0.25', '0'];
    for (let i = 0; i <= 4; i++) {
      const y = padding + (i / 4) * graphHeight;
      ctx.fillText(yLabels[i], padding - 10, y + 4);
    }

    // Axis titles
    ctx.fillStyle = '#aaaacc';
    ctx.font = '14px sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText('Input (x)', width / 2, height - 10);

    ctx.save();
    ctx.translate(15, height / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.fillText('Output', 0, 0);
    ctx.restore();

    // Generate data points (at least 10 points, using 21 for smooth curve)
    const numPoints = 21;
    const points = [];

    for (let i = 0; i < numPoints; i++) {
      const normalizedInput = -1 + (i / (numPoints - 1)) * 2; // -1 to 1
      const input24Bit = normalizedTo24Bit(normalizedInput);
      const output24Bit = func(input24Bit);
      const normalizedOutput = result24BitToNormalized(output24Bit);

      const x = padding + ((normalizedInput + 1) / 2) * graphWidth;
      const y = height - padding - normalizedOutput * graphHeight;

      points.push({ x, y, inputVal: normalizedInput, outputVal: normalizedOutput });
    }

    // Draw the function line
    ctx.strokeStyle = '#00ff88';
    ctx.lineWidth = 3;
    ctx.lineJoin = 'round';
    ctx.lineCap = 'round';

    ctx.beginPath();
    ctx.moveTo(points[0].x, points[0].y);
    for (let i = 1; i < points.length; i++) {
      ctx.lineTo(points[i].x, points[i].y);
    }
    ctx.stroke();

    // Draw data points
    ctx.fillStyle = '#ff6688';
    for (const point of points) {
      ctx.beginPath();
      ctx.arc(point.x, point.y, 4, 0, Math.PI * 2);
      ctx.fill();
    }

  }, [func]);

  return (
    <div className="graph-container">
      <h2>{title}</h2>
      <canvas ref={canvasRef} width={400} height={400} />
      <p className="description">{description}</p>
    </div>
  );
}

export { DspFunctionGraph, maxMinusAbs, invertedMagnitude, absoluteValue };

