import data from "../../data/bart_viz.json"

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
    const v = data.vertices[id];
    return [mapLon(parseFloat(v.lon)), mapLat(parseFloat(v.lat))];
  };

  return (
    <main>
      <svg height={yMax} width={xMax}>
        {Object.keys(data.vertices).map((id) => {
          const v = data.vertices[id];
          const [x, y] = vertexXy(id);
          return <>
            <circle key={id} cx={x} cy={y} r={2} fill="black" />
            <text x={x + 4} y={y + 4} fontSize={10}>{v.name}</text>
          </>;
        })}
        {data.edges.map((e) => {
          const [x1, y1] = vertexXy(e.origin_id);
          const [x2, y2] = vertexXy(e.destination_id);
          return <line key={`${e.origin_id}-${e.destination_id}`} x1={x1} y1={y1} x2={x2} y2={y2} stroke="black" />;
        })}
      </svg>
    </main>
  );
}
