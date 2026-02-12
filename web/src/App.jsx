import { DspFunctionGraph, maxMinusAbs, invertedMagnitude, absoluteValue } from './components/DspFunctionGraph';
import './App.css';

function App() {
  return (
    <div className="app">
      <h1>DSP Interpolation Functions</h1>
      <p className="intro">
        Visualization of three bitwise DSP operations used for signal processing.
        All three use the same operations (<code>~</code> and <code>&amp; 0x7FFFFF</code>)
        but differ in which values get the bitwise NOT applied.
      </p>

      <div className="graphs-container">
        <DspFunctionGraph
          title="max - abs(x)"
          func={maxMinusAbs}
          description="if (x >= 0) x = ~x; x &= 0x7fffff; — Triangle peak at zero. Inverts only positive values."
        />
        <DspFunctionGraph
          title="~x & mask"
          func={invertedMagnitude}
          description="x = ~x & 0x7fffff; — V-shape with minimum at zero. Inverts all values."
        />
        <DspFunctionGraph
          title="abs(x)"
          func={absoluteValue}
          description="if (x < 0) x = ~x; x &= 0x7fffff; — Standard absolute value. Inverts only negative values."
        />
      </div>

      <div className="code-block">
        <h3>C++ Code Variations</h3>
        <pre>{`// Variation 1: max - abs(x)
if (mulInputA_24 >= 0) mulInputA_24 = ~mulInputA_24;
mulInputA_24 &= 0x7fffff;

// Variation 2: ~x & mask
mulInputA_24 = (~mulInputA_24 & 0x7fffff);

// Variation 3: abs(x)
if (mulInputA_24 < 0) mulInputA_24 = ~mulInputA_24;
mulInputA_24 &= 0x7fffff;`}</pre>
      </div>

      <div className="value-table">
        <h3>Detailed Value Comparison</h3>
        <table>
          <thead>
            <tr>
              <th>Input</th>
              <th>max - abs(x)</th>
              <th>~x & mask</th>
              <th>abs(x)</th>
            </tr>
          </thead>
          <tbody>
            <tr><td>+1.0</td><td>0.0</td><td>0.0</td><td>1.0</td></tr>
            <tr><td>+0.5</td><td>0.5</td><td>0.5</td><td>0.5</td></tr>
            <tr><td>0</td><td>1.0</td><td>1.0</td><td>0.0</td></tr>
            <tr><td>-0.5</td><td>0.5</td><td className="highlight">0.5</td><td>0.5</td></tr>
            <tr><td>-1.0</td><td>0.0</td><td className="highlight">1.0</td><td>1.0</td></tr>
          </tbody>
        </table>
        <p className="table-note">
          Note: The only difference is at <strong>-1.0</strong> (negative max).
          <code>~x & mask</code> returns 1.0 while <code>max - abs(x)</code> returns 0.0.
        </p>
      </div>
    </div>
  );
}

export default App;
