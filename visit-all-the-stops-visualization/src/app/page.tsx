"use client"

import { useState } from "react";
import data from "../../data/bart_viz_faster.json"

function parseTime(time: string): number {
  return 60 * parseInt(time.substring(0, 2), 10) + parseInt(time.substring(4, 6));
}

function filterData(d: typeof data): typeof data {
  // const firstBart: Record<string, number> = {};
  // const lastBart: Record<string, number> = {};
  // for (const edge of d.edges) {
  //   const origin = edge.origin_id;
  //   for (const seg of edge.segments) {
  //     if (!seg.trips.every((t) => t.startsWith("BA:"))) {
  //       continue;
  //     }
  //     const time = parseTime(seg.departure_time);
  //     console.log(time);
  //     if (!(origin in firstBart)) { firstBart[origin] = time; }
  //     if (!(origin in lastBart)) { lastBart[origin] = time; }
  //     firstBart[origin] = Math.min(firstBart[origin], time);
  //     lastBart[origin] = Math.max(lastBart[origin], time);
  //   }
  // }

  const startTime = parseTime("08:00");
  const endTime = parseTime("22:00");

  const newEdges = d.edges.flatMap((edge) => {
    const newSegments = edge.segments.filter((seg) => (
        parseTime(seg.departure_time) >= startTime && parseTime(seg.arrival_time) <= endTime
      )).sort((a, b) => (parseTime(a.departure_time) - parseTime(b.departure_time)));
      if (newSegments.length === 0) {
        return [];
      }
      return [
        {
          ...edge,
          segments: newSegments,
        }
      ];
  });

  return {
    ...d,
    edges: newEdges
  };
}

const fData = filterData(data);

function projectToLineSegmentFromOrigin(
  x: number,
  y: number,
  lx: number,
  ly: number
): [number, number] {
  // L(t) = (lx * t, ly * t)
  //
  // d2(t) = ||L(t) - (x, y)||^2
  // = ||(lx * t - x, ly * t - y)||^2
  // = (lx * t - x)^2 + (ly * t - y)^2
  //
  // d2'(t) = 2 lx (lx t - x) + 2 ly (ly t - y)
  // = 2 lx^2 t - 2 lx x + 2 ly^2 t - 2 ly y
  // = t (2 lx^2 + 2 ly^2) - 2 lx x - 2 ly y
  //
  // So d2(t) is minimized at
  // t = (lx x + ly y) / (lx^2 + ly^2)
  let t = (lx * x + ly * y) / (lx * lx + ly * ly)
  if (t < 0) {
    t = 0;
  }
  if (t > 1) {
    t = 1;
  }
  return [lx * t, ly * t];
}

function projectToLineSegment(
  x: number,
  y: number,
  x1: number,
  y1: number,
  x2: number,
  y2: number
): [number, number] {
  const [px, py] = projectToLineSegmentFromOrigin(x - x1, y - y1, x2 - x1, y2 - y1);
  return [px + x1, py + y1];
}

function sqDistToLineSegment(
  x: number,
  y: number,
  x1: number,
  y1: number,
  x2: number,
  y2: number
): number {
  const [px, py] = projectToLineSegment(x, y, x1, y1, x2, y2);
  return (x - px) * (x - px) + (y - py) * (y - py);
}

export default function Home() {
  const latMin = 37.3;
  const latMax = 38.1;
  const lonMin = -122.5;
  const lonMax = -121.7;

  const xMax = 1200;
  const yMax = 800;

  const mapLat = (lat: number) => yMax - (lat - latMin) / (latMax - latMin) * yMax;
  const mapLon = (lon: number) => (lon - lonMin) / (lonMax - lonMin) * xMax;

  const vertexXy = (id: string) => {
    const v = fData.vertices[id];
    return [mapLon(parseFloat(v.lon)), mapLat(parseFloat(v.lat))];
  };

  const closestEdgeIndex = (x: number, y: number): number | undefined => {
    let closestSqDist = 20 * 20;
    let closestIndex = undefined;
    fData.edges.forEach((e, index) => {
      const [x1, y1] = vertexXy(e.origin_id);
      const [x2, y2] = vertexXy(e.destination_id);
      const sqDist = sqDistToLineSegment(x, y, x1, y1, x2, y2);
      if (sqDist < closestSqDist) {
        closestSqDist = sqDist;
        closestIndex = index;
      }
    });
    return closestIndex;
  }

  const [hoverEdgeIndex, setHoverEdgeIndex] = useState<number | undefined>();

  const handleMouseMove = (e) => {
    const index = closestEdgeIndex(e.nativeEvent.offsetX, e.nativeEvent.offsetY);
    setHoverEdgeIndex(index);
  }

  const [selectedEdgeIndex, setSelectedEdgeIndex] = useState<number | undefined>();
  const selectedEdge = selectedEdgeIndex != undefined ? fData.edges[selectedEdgeIndex] : undefined;

  const handleClick = () => {
    setSelectedEdgeIndex(hoverEdgeIndex);
  };

  const handleFlip = () => {
    let newIndex;
    fData.edges.forEach((e, i) => {
      if (e.destination_id == selectedEdge?.origin_id && e.origin_id == selectedEdge.destination_id) {
        newIndex = i;
      }
    });
    setSelectedEdgeIndex(newIndex);
  };

  return (
    <main style={{display: "flex", flexDirection: "row", height: "100vh"}}>
      <svg height={yMax} width={xMax} onMouseMove={handleMouseMove} onClick={handleClick} style={{cursor: "default"}}>
        {Object.keys(fData.vertices).map((id) => {
          const v = fData.vertices[id];
          const [x, y] = vertexXy(id);
          return <g key={id} onClick={() => console.log(id)}>
            <circle cx={x} cy={y} r={2} fill="black" />
            <text x={x + 4} y={y + 4} fontSize={10}>{v.name}</text>
          </g>;
        })}
        {fData.edges.map((e, index) => {
          const [x1, y1] = vertexXy(e.origin_id);
          const [x2, y2] = vertexXy(e.destination_id);
          const stroke = e.segments[0].trips.every((t) => t.startsWith("BA:")) ? "black" : "red";
          const strokeWidth = hoverEdgeIndex == index ? 3 : 1;
          return <g key={`${e.origin_id}-${e.destination_id}`}>
            <line x1={x1} y1={y1} x2={x2} y2={y2} stroke={stroke} strokeWidth={strokeWidth} />
          </g>
        })}
      </svg>
      {selectedEdge && (
        <div style={{flex: 1, overflowY: "auto", fontFamily: "monospace", whiteSpace: "pre"}}>
          <button onClick={handleFlip}>Flip</button>
          <p>{`${selectedEdge.origin_id} -> ${selectedEdge.destination_id}`}</p>
          {selectedEdge.segments.map((seg, i) => (
            <div key={i}>
              <p>
                {`${seg.departure_time} -> ${seg.arrival_time}`}
              </p>
              {seg.trips.map((trip, j) => (
                <p key={j}>
                  {`  ${trip}`}
                </p>
              ))}
            </div>
          ))}
        </div>
      )}
    </main>
  );
}
